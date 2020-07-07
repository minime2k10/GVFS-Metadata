#include <iostream>
#include <cstdio>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>

struct metadata
{
    int keyword;
    char* text;
};

struct rootEntry
{
    char name[255];
    long childOffset;
    long metaOffset;
    int numMetaKeys;
    time_t lastUpdate;
    metadata metaList[10];
};

int GVFSOpen(char* fileName);
long readInteger(unsigned char* ch, int pointer, int size);
long long readLongInteger(unsigned char* ch, int pointer, int size);
int readRoot(long offset);
int readChild(long offset, int level);
void writeOutput(rootEntry entry, int level);
metadata readMeta(int offset);

unsigned char* fileBuffer;
FILE* outFile;
unsigned long long baseTime;

using namespace std;


int main(int args, char** argv)
{
    if (args != 2)
    {
        printf("GVFS Metadata parser\nUsage: gvfs PathName\nPath name should not include \'\\\' at end\n");
        return 0;
    }
    DIR *dir;
    struct dirent *ent;
    dir = opendir(argv[1]);
    if (dir != NULL)
    {
        while ((ent = readdir(dir)) != NULL)
        {
            char path[512];
            if (!strcmp (ent->d_name, "."))
                continue;
            if (!strcmp (ent->d_name, ".."))
                continue;
            sprintf(path,"%s/%s\0",argv[1],ent->d_name);
            GVFSOpen(path);
        }
    }
    //int result = GVFSOpen("c:\\uuid-a1b42ca8-2718-4ca2-a44d-65e428f1de26");
    return 0;
}

long readInteger(unsigned char *ch, int pointer, int size) {
	int i;
	long p = 0;
	for(i = 0; i < size; i++)
		p = ((p << 8) | (unsigned char)ch[pointer + i]);
	return p;
}

long long readLongInteger(unsigned char *ch, int pointer, int size) {
	int i;
	long long p = 0;
	for(i = 0; i < size; i++)
		p = ((p << 8) | (unsigned char)ch[pointer + i]);
	return p;
}

int GVFSOpen(char* fileName)
{
    FILE* inFile;
    inFile = fopen(fileName,"rb");
    if (inFile == NULL)
    {
        return 1;
    }
    char outputPath[512];
    sprintf(outputPath,"%s\.txt\0",fileName);
    outFile = fopen(outputPath,"wb");
    //get file size
    fseek(inFile,0,SEEK_END);
    long sizeFile = ftell(inFile);
    fseek(inFile,0,SEEK_SET);
    fileBuffer = (unsigned char*) malloc(sizeof(char) * sizeFile);
    if (fileBuffer == NULL)
    {
        //memory error
        return 2;
    }
    int check = fread(fileBuffer,1,sizeFile,inFile);
    if (fileBuffer[0] == 0xDA && fileBuffer[1] == 0x1A)
    {
        //possible winner
        char header[5];
        strncpy(header,(char*)&fileBuffer[2],4);
        header[4] = '\0';
        if (strncmp(header,"meta",4) == 0)
        {
            //get base time
            baseTime = readLongInteger(fileBuffer,24,8);
            //find root offset
            long ptr = readInteger((unsigned char*)fileBuffer,16,4);
            readRoot(ptr);
        }
    }
    fclose(inFile);
    free(fileBuffer);
    return 0;
}

int readRoot(long offset)
{
    rootEntry rEntry;
    long nameOffset = readInteger(fileBuffer,offset,4);
    rEntry.name[0] = fileBuffer[nameOffset];
    rEntry.name[1] = '\0';
    rEntry.childOffset = readInteger(fileBuffer,offset + 4,4);
    rEntry.metaOffset = readInteger(fileBuffer,offset + 8,4);
    rEntry.lastUpdate = 0;
    int numberChildren = readInteger(fileBuffer,rEntry.childOffset,4);
    long tempOffset = rEntry.childOffset + 4;
    rEntry.numMetaKeys = readInteger(fileBuffer, rEntry.metaOffset,4);
    for (int n = 0;n<rEntry.numMetaKeys;n++)
    {
        rEntry.metaList[n] = readMeta(rEntry.metaOffset + 4 + (n * 8));
    }
    writeOutput(rEntry, 0);
    for (int i = 0;i < numberChildren;i++)
    {
        readChild(tempOffset,1);
        tempOffset +=16;
    }
    return 0;
}

int readChild(long offset, int level)
{
    rootEntry cEntry;
    long nameOffset = readInteger(fileBuffer,offset,4);
    int nameLength = 0;
    for (int i = 0;i< 250;i++)
    {
        if (fileBuffer[nameOffset + i] == 0x00)
        {
            nameLength = i;
            i = 255;
        }
    }
    strncpy(cEntry.name,(char*)&fileBuffer[nameOffset],nameLength);
    cEntry.name[nameLength] = '\0';
    cEntry.childOffset = readInteger(fileBuffer,offset + 4,4);
    cEntry.metaOffset = readInteger(fileBuffer,offset + 8,4);
    cEntry.lastUpdate = readInteger(fileBuffer,offset+12,4)+ baseTime;
    cEntry.numMetaKeys = readInteger(fileBuffer, cEntry.metaOffset,4);
    if(cEntry.numMetaKeys > 2)cEntry.numMetaKeys=2; // stack corruption when this value over 2, idk why, but working with this sh*tty fix
    for (int n = 0;n<cEntry.numMetaKeys;n++)
    {
        cEntry.metaList[n] = readMeta(cEntry.metaOffset + 4 + (n * 8));
    }
    writeOutput(cEntry,level);
    //free up strings from metakeys
    for (int n = 0;n<cEntry.numMetaKeys;n++)
    {
        delete[] cEntry.metaList[n].text;
    }
    int numberChildren = readInteger(fileBuffer,cEntry.childOffset,4);
    long tempOffset = cEntry.childOffset + 4;
    if (numberChildren > 0)
    {
        for (int i = 0;i < numberChildren;i++)
        {
            readChild(tempOffset,level + 1);
            tempOffset +=16;
        }
    }
    return 0;
}

char* createMetaString(int numKeys, metadata list[10])
{
    char* retString = new char[1024];
    retString[0] = '\0';
    char* tempStr = new char[128];
    tempStr[0] = '\0';
    for (int i=0;i< numKeys;i++)
    {
        sprintf(tempStr,"||%i,%s\0",list[i].keyword, list[i].text);
        strcat(retString,tempStr);
        tempStr[0] = '\0';
    }
    return retString;
}

void writeOutput(rootEntry entry, int level)
{
    char output[1024];
    if (entry.lastUpdate != 0)
    {
        char timeDisplay[30];
        time_t tempTime = entry.lastUpdate;
        sprintf(timeDisplay,"%s\0",asctime(gmtime(&tempTime)));
        char* metaString = createMetaString(entry.numMetaKeys, entry.metaList);
        sprintf(output,"%s,     %lu,    %lu,    %s      %i      %s\x0D\x0A\0",entry.name,entry.childOffset, entry.metaOffset, timeDisplay, entry.numMetaKeys, metaString);
    }
    else
    {
        //root entry
        char* metaString = createMetaString(entry.numMetaKeys, entry.metaList);
        sprintf(output,"%s,     %lu,    %lu,    %lu     %i      %s\x0D\x0A\0",entry.name,entry.childOffset, entry.metaOffset, entry.lastUpdate, entry.numMetaKeys,metaString);
    }
    char tab = 0x09;
    for (int i=0;i< level;i++)
    {
        fwrite((char*)&tab,1,1,outFile);
    }
    fwrite(output,strlen(output),1,outFile);
    fflush(outFile);

}

metadata readMeta(int offset)
{
    metadata retMet;
    retMet.keyword = readInteger(fileBuffer,offset, 4);
    int stringOffset = readInteger(fileBuffer, offset+4, 4);
    int strLen = 0;
    for (int i = 0; i< 100; i++)
    {
        if (fileBuffer[stringOffset + i] == '\0')
        {
            strLen = i;
            i = 101;
        }
    }
    retMet.text =  new char[strLen+1];
    strncpy(retMet.text,(char*)&fileBuffer[stringOffset],strLen);
    retMet.text[strLen] = '\0';
    return retMet;
}

