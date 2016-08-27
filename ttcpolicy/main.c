//
//  main.cpp
//  ttcpolicy
//
//  Created by pebble8888 on 2016/08/27.
//  Copyright © 2016年 pebble8888. All rights reserved.
//
// このプログラムが意味するところは、
// スレッドで50msスリープした後200us時間以内にはスレッドにCPUが必ず割り当てられるということである。
// policyを設定しなかった場合は平均2192us時間以内、最小277us、最大5148usにスレッドにCPUが割り当てられている。
//

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>

#define PROGNAME "ttcpolicy"

#define SLEEP_NS  500000 // sleep for 50ms

// OSX 10.11.6 では 52us〜 120us程度の誤差があるので50usではエラーになってしまう
//#define ERROR_THRESH_NS ((double)50000) // 50us
#define ERROR_THRESH_NS ((double)200000) //

static double abs2clock;
static unsigned long long nerrors = 0, nsamples = 0;
static struct timespec rqt = { 0, SLEEP_NS };

void atexit_handler(void)
{
    printf("%llu errors in %llu samples\n", nerrors, nsamples);
}

void * timestamper(void *arg)
{
    int ret;
    double diff_ns;
    u_int64_t t1, t2, diff;
    while(1){
        t1 = mach_absolute_time();   // get time
        ret = nanosleep(&rqt, NULL); // sleep 50ms    50ms * 200 = 1sec
        t2 = mach_absolute_time();   // get time
        if (ret != 0)
            exit(1);
        diff = t2 - t1;
        
        diff_ns = ((double)SLEEP_NS) - (double)diff * abs2clock;
        
        // 絶対値
        if (diff_ns < 0)
            diff_ns *= -1;
        
        if (diff_ns > ERROR_THRESH_NS){
            nerrors++;
            printf("diff_ns %f\n", diff_ns);
        }
        
        nsamples++;
    }
    return NULL;
}

int main(int argc, const char * argv[]) {
    int ret;
    kern_return_t kr;
    static double clock2abs;
    
    pthread_t t1;
    ret = pthread_create(&t1, (pthread_attr_t *)0, timestamper, (void *)0);
    ret = atexit(atexit_handler);
    
    mach_timebase_info_data_t tbinfo;
    (void)mach_timebase_info(&tbinfo);
    abs2clock = ((double)tbinfo.numer / (double)tbinfo.denom);
    
    if (argc > 1){
        clock2abs = ((double)tbinfo.denom / (double)tbinfo.numer) * 1000000;
        
        {
            thread_time_constraint_policy_data_t policy;
            //policy.period = 50 * clock2abs;
            policy.period = 0;
            policy.computation = 0.25 * clock2abs;
            policy.constraint = 0.5 * clock2abs;
            policy.preemptible = TRUE;
            
            kr = thread_policy_set(pthread_mach_thread_np(t1),
                                   THREAD_TIME_CONSTRAINT_POLICY,
                                   (thread_policy_t)&policy,
                                   THREAD_TIME_CONSTRAINT_POLICY_COUNT);
            if (kr != KERN_SUCCESS){
                mach_error("thread_policy_set:", kr);
                goto OUT;
            }
        }
    }
    ret = pthread_detach(t1);
    printf("waiting 10 seconds...\n");
    sleep(10);
    
OUT:
    exit(0);
}
