//C++ BRSTM converter
//Copyright (C) 2020 Extrasklep
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include <cstring>
#include <math.h>

#include "lib/brstm.h"
#include "lib/brstm_encode.h"

//-------------------######### STRINGS

const char* helpString0 = "BRSTM/WAV/other converter\nCopyright (C) 2020 Extrasklep\nThis program is free software, see the license file for more information.\nUsage:\n";
const char* helpString1 = " [file to open.type] [options...]\nOptions:\n\n-o [output file name.type] - If this is not used the output will not be saved.\nSupported input formats:  WAV, BRSTM, BWAV\nSupported output formats: WAV, BRSTM\n\n-v - Verbose output\n\n--ffmpeg \"[ffmpeg arguments]\" - Use ffmpeg in the middle of reencoding to change the audio data with the passed ffmpeg arguments (as a single argument!)\nRequires FFMPEG to be installed and it may not work on non-unix systems.\nOnly usable in BRSTM/other -> BRSTM/other conversion.\n\nWAV -> BRSTM/other options:\n  -l --loop [loop point] - Creating looping file or -1 for no loop\n  -c --track-channels [1 or 2] - Number of channels for each track (default is 2)\n";

//------------------ Command line arguments

const char* opts[] = {"-v","-o","-l","-c","--ffmpeg"};
const char* opts_alt[] = {"--verbose","--output","--loop","--track-channels","--ffmpeg"};
const unsigned int optcount = 5;
const bool optrequiredarg[optcount] = {0,1,1,1,1};
bool  optused  [optcount];
char* optargstr[optcount];
//____________________________________
const char* inputFileName;
const char* outputFileName;
int inputFileExt;
int outputFileExt;

int  verb = 1;
bool saveFile = 0;

bool userLoop = 0; //1 if the loop option was used
signed char userLoopFlag = 0;
unsigned long userLoopPoint = 0;

//0 = option not used, 1 = 1ch, 2 = 2ch
unsigned char userTrackChannels = 0;

bool useFFMPEG = 0;
const char* ffmpegArgs;

//Modified functions from brstm_encode.h, used to write WAV files
void writebytes(unsigned char* buf,const unsigned char* data,unsigned int bytes,unsigned long& off) {
    for(unsigned int i=0;i<bytes;i++) {
        buf[i+off] = data[i];
    }
    off += bytes;
}
//Returns integer as little endian bytes
unsigned char* BEint;
unsigned char* getLEuint(uint64_t num,uint8_t bytes) {
    delete[] BEint;
    BEint = new unsigned char[bytes];
    unsigned long pwr;
    unsigned char pwn = bytes - 1;
    for(unsigned char i = 0; i < bytes; i++) {
        pwr = pow(256,pwn--);
        unsigned int pos = abs(i-bytes+1);
        BEint[pos]=0;
        while(num >= pwr) {
            BEint[pos]++;
            num -= pwr;
        }
    }
    return BEint;
}

//Program functions
char* strtolowerstr;
char* strtolower(const char* str) {
    delete[] strtolowerstr;
    unsigned int len = strlen(str);
    strtolowerstr = new char[len+1];
    strtolowerstr[len] = '\0';
    for(unsigned int i=0;i<len;i++) {
        strtolowerstr[i] = tolower(str[i]);
    }
    return strtolowerstr;
}

int compareFileExt(const char* filename, const char* ext) {
    unsigned int extlen = strlen(ext);
    unsigned int fnlen  = strlen(filename);
    //return no match if filename is shorter than extension
    if(fnlen < extlen) {return 1;}
    //return if extension in filename is not the same length
    if(filename[fnlen-1-extlen] != '.') {return 2;}
    char* lext = new char[extlen+1];
    memcpy(lext,strtolower(ext),extlen+1);
    //copy the end of filename
    char* fnext = new char[extlen+1];
    fnext[extlen] = '\0';
    for(unsigned int i=fnlen;i>=fnlen-extlen;i--) {
        fnext[(i-fnlen)+extlen] = tolower(filename[i]);
    }
    //compare
    int res = strcmp(fnext,lext);
    delete[] lext;
    delete[] fnext;
    return res;
}

//-1 = unsupported, 0 = WAV, 1+ = BRSTM lib formats
int getFileExt(const char* filename) {
    if(compareFileExt(filename,"WAV") == 0) return 0;
    for(unsigned int f=1;f<BRSTM_formats_count;f++) {
        if(compareFileExt(filename,BRSTM_formats_short_usr_str[f]) == 0) return f;
    }
    //No match
    return -1;
}

int main(int argc, char** args) {
    if(argc<2) {
        std::cout << helpString0 << args[0] << helpString1;
        return 0;
    }
    //Parse command line args
    for(unsigned int a=2;a<argc;a++) {
        int vOpt = -1;
        //Compare cmd arg against each known option
        for(unsigned int o=0;o<optcount;o++) {
            if( strcmp(args[a], opts[o]) == 0 || strcmp(args[a], opts_alt[o]) == 0 ) {
                //Matched
                vOpt = o;
                break;
            }
        }
        //No match
        if(vOpt < 0) {std::cout << "Unknown option '" << args[a] << "'.\n"; exit(255);}
        //Mark the options as used
        optused[vOpt] = 1;
        //Read the argument for the option if it requires it
        if(optrequiredarg[vOpt]) {
            if(a+1 < argc) {
                optargstr[vOpt] = args[++a];
            } else {
                std::cout << "Option " << opts[vOpt] << " requires an argument\n";
                exit(255);
            }
        }
    }
    //Apply the options
    //Input
    inputFileName = args[1];
    //Verbose
    if(optused[0]) verb=2;
    //Output
    if(optused[1]) {outputFileName=optargstr[1]; saveFile=1;}
    //Loop
    if(optused[2]) {
        unsigned long lp = atoi(optargstr[2]);
        if(lp == (unsigned long)(-1)) {
            std::cout << "user no loop\n";//debug remove later
            userLoop = 0;
        } else {
            userLoop = 1;
            userLoopFlag  = 1;
            userLoopPoint = lp;
        }
    }
    //Track channels
    if(optused[3]) {
        unsigned int tc = atoi(optargstr[3]);
        if(!(tc >= 1 && tc <= 2)) {std::cout << "Track channel count must be 1 or 2.\n"; exit(255);}
        userTrackChannels = tc;
    }
    //FFMPEG
    if(optused[4]) {ffmpegArgs=optargstr[4]; useFFMPEG=1;}
    
    //Check file extensions
    inputFileExt = getFileExt(inputFileName);
    if(inputFileExt == -1) {std::cout << "Unsupported input file extension.\n"; exit(255);}
    if(saveFile) {
        outputFileExt = getFileExt(outputFileName);
        if(outputFileExt == -1) {std::cout << "Unsupported output file extension.\n"; exit(255);}
    }
}
