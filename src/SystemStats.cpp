/*
* Licensed to the Apache Software Foundation (ASF) under one
* or more contributor license agreements.  See the NOTICE file
* distributed with this work for additional information
* regarding copyright ownership.  The ASF licenses this file
* to you under the Apache License, Version 2.0 (the
* "License"); you may not use this file except in compliance
* with the License.  You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an
* "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.  See the License for the
* specific language governing permissions and limitations
* under the License.
*/

#include <SystemStats.h>

#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/times.h>
#include <sys/vtimes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

using namespace stats;

int parseLine(char* line){
    // This assumes that a digit will be found and the line ends in " Kb".
    int i = strlen(line);
    const char* p = line;
    while (*p <'0' || *p > '9') p++;
    line[i-3] = '\0';
    i = atoi(p);
    return i;
}

uint64_t SystemStats::totalVirtualMemory()
{
    struct sysinfo memInfo;

    sysinfo (&memInfo);
    u_int64_t totalVirtualMem = memInfo.totalram;
    //Add other values in next statement to avoid int overflow on right hand side...
    totalVirtualMem += memInfo.totalswap;
    totalVirtualMem *= memInfo.mem_unit;
    return totalVirtualMem;
}

uint64_t SystemStats::usedVirtualMemory()
{
    struct sysinfo memInfo;

    sysinfo (&memInfo);

    uint64_t virtualMemUsed = memInfo.totalram - memInfo.freeram;
    //Add other values in next statement to avoid int overflow on right hand side...
    virtualMemUsed += memInfo.totalswap - memInfo.freeswap;
    virtualMemUsed *= memInfo.mem_unit;
    return virtualMemUsed;
}

uint64_t SystemStats::virtualMemoryUsedByThis()
{
    FILE* file = fopen("/proc/self/status", "r");
    uint64_t result = 0;
    char line[128];

    while (fgets(line, 128, file) != NULL){
        if (strncmp(line, "VmSize:", 7) == 0){
            result = parseLine(line);
            break;
        }
    }
    fclose(file);
    return result * 1000;
}

uint64_t SystemStats::totalPhysicalMemory()
{
    struct sysinfo memInfo;

    sysinfo (&memInfo);
    uint64_t totalPhysMem = memInfo.totalram;
    totalPhysMem *= memInfo.mem_unit;
    return totalPhysMem;
}

uint64_t SystemStats::usedPhysicalMemory()
{
    struct sysinfo memInfo;

    sysinfo (&memInfo);
    uint64_t physMemUsed = memInfo.totalram - memInfo.freeram;
    //Multiply in next statement to avoid int overflow on right hand side...
    physMemUsed *= memInfo.mem_unit;
    return physMemUsed;
}

uint64_t SystemStats::physicalMemoryUsedByThis()
{
    FILE* file = fopen("/proc/self/status", "r");
    uint64_t result = 0;
    char line[128];

    while (fgets(line, 128, file) != NULL){
        if (strncmp(line, "VmRSS:", 6) == 0){
            result = parseLine(line);
            break;
        }
    }
    fclose(file);
    return result * 1000;   
}

struct TotalCPUStats
{
    TotalCPUStats()
    {
        FILE* file = fopen("/proc/stat", "r");
        fscanf(file, "cpu %llu %llu %llu %llu", &totalUser, &totalUserLow, &totalSys, &totalIdle);
        fclose(file);
    }
    long long unsigned int totalUser = 0, totalUserLow = 0, totalSys = 0, totalIdle = 0;
};

struct SelfCPUStats
{
    SelfCPUStats()
    {
        FILE* file;
        struct tms timeSample;
        char line[128];

        lastCPU = times(&timeSample);
        lastSysCPU = timeSample.tms_stime;
        lastUserCPU = timeSample.tms_utime;

        file = fopen("/proc/cpuinfo", "r");
        numProcessors = 0;
        while(fgets(line, 128, file) != NULL){
            if (strncmp(line, "processor", 9) == 0) numProcessors++;
        }
        fclose(file);
    }

    clock_t lastCPU = 0, lastSysCPU = 0, lastUserCPU = 0;
    int numProcessors;
};

float SystemStats::cpuUsage()
{
    static TotalCPUStats last;
    TotalCPUStats current;

    float percent;
    int64_t total;

    if (current.totalUser < last.totalUser || current.totalUserLow < last.totalUserLow ||
        current.totalSys < last.totalSys || current.totalIdle < last.totalIdle){
        //Overflow detection. Just skip this value.
        percent = -1.0;
    }
    else{
        total = (current.totalUser - last.totalUser) + (current.totalUserLow - last.totalUserLow) +
            (current.totalSys - last.totalSys);
        percent = total;
        total += (current.totalIdle - last.totalIdle);
        percent /= total;
        percent *= 100;
    }

    last = current;

    return percent;
}

float SystemStats::cpuUsageByThis()
{
    static SelfCPUStats stats;
    struct tms timeSample;
    clock_t now;
    double percent;

    now = times(&timeSample);
    if (now <= stats.lastCPU || timeSample.tms_stime < stats.lastSysCPU ||
        timeSample.tms_utime < stats.lastUserCPU){
        //Overflow detection. Just skip this value.
        percent = -1.0;
    }
    else{
        percent = (timeSample.tms_stime - stats.lastSysCPU) +
            (timeSample.tms_utime - stats.lastUserCPU);
        percent /= (now - stats.lastCPU);
        percent /= stats.numProcessors;
        percent *= 100;
    }
    stats.lastCPU = now;
    stats.lastSysCPU = timeSample.tms_stime;
    stats.lastUserCPU = timeSample.tms_utime;

    return percent;
}