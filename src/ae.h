/* A simple event-driven programming library. Originally I wrote this code
 * for the Jim's event-loop (Jim is a Tcl interpreter) but later translated
 * it in form of a library for easy reuse.
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * redis 事件
 *
 * 所谓事件驱动，就是当你输入一条命令并且按下回车，然后消息被组装成Redis协议的格式发送给Redis服务器，这就会产生一个事件，
 * Redis服务器会接收该命令，处理该命令和发送回复，而当你没有与服务器进行交互时，那么服务器就会处于阻塞等待状态，
 * 会让出CPU从而进入睡眠状态，当事件触发时，就会被操作系统唤醒。事件驱动使CPU更高效的利用。
 *
 *
 *
 * 在redis服务器中，处理了两类事件：
 *
 * 文件事件（file event）：Redis服务器通过套接字于客户端（或其他Redis服务器）进行连接，而文件事件就是服务器对套接字操作的抽象。
 * 时间事件（time event）：Redis服务器的一些操作需要在给定的事件点执行，而时间事件就是服务器对这类定时操作的抽象。
 *
 */

#ifndef __AE_H__
#define __AE_H__

#include <time.h>

#define AE_OK 0
#define AE_ERR -1

#define AE_NONE 0       /* No events registered. */
#define AE_READABLE 1   /* Fire when descriptor is readable. */
#define AE_WRITABLE 2   /* Fire when descriptor is writable. */
#define AE_BARRIER 4    /* With WRITABLE, never fire the event if the
                           READABLE event already fired in the same event
                           loop iteration. Useful when you want to persist
                           things to disk before sending replies, and want
                           to do that in a group fashion. */

#define AE_FILE_EVENTS 1
#define AE_TIME_EVENTS 2
#define AE_ALL_EVENTS (AE_FILE_EVENTS|AE_TIME_EVENTS)
#define AE_DONT_WAIT 4
#define AE_CALL_AFTER_SLEEP 8

#define AE_NOMORE -1
#define AE_DELETED_EVENT_ID -1

/* Macros */
#define AE_NOTUSED(V) ((void) V)

struct aeEventLoop;

/* Types and data structures */
typedef void aeFileProc(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask);
typedef int aeTimeProc(struct aeEventLoop *eventLoop, long long id, void *clientData);
typedef void aeEventFinalizerProc(struct aeEventLoop *eventLoop, void *clientData);
typedef void aeBeforeSleepProc(struct aeEventLoop *eventLoop);

/* File event structure
 *
 *  文件事件（file event）：Redis服务器通过套接字于客户端（或其他Redis服务器）进行连接，
 *  而文件事件就是服务器对套接字操作的抽象。
 */
typedef struct aeFileEvent {
    int mask; /* one of AE_(READABLE|WRITABLE|BARRIER) */

    //可读处理函数
    aeFileProc *rfileProc;

    //可写处理函数
    aeFileProc *wfileProc;

    //客户端传入的数据
    void *clientData;
} aeFileEvent;

/* Time event structure
 *
 *时间事件（time event）：Redis服务器的一些操作需要在给定的事件点执行，
 * 而时间事件就是服务器对这类定时操作的抽象
 *
 */
typedef struct aeTimeEvent {
    //事件事件id
    long long id; /* time event identifier. */

    // 时间事件到达的时间的秒数
    long when_sec; /* seconds */

    // 时间事件到达的时间的毫秒数
    long when_ms; /* milliseconds */

    // 时间事件处理函数
    aeTimeProc *timeProc;

    // 时间事件终结函数
    aeEventFinalizerProc *finalizerProc;

    // 客户端传入的数据
    void *clientData;

    // 指向上一个时间事件
    struct aeTimeEvent *prev;
    // 指向下一个时间事件
    struct aeTimeEvent *next;
} aeTimeEvent;


/* A fired event */
typedef struct aeFiredEvent {
    //就绪事件描述符
    int fd;
    //就绪事件类型
    int mask;
} aeFiredEvent;



/* State of an event based program
 *
 * 事件状态
 * */
typedef struct aeEventLoop {
    //当前已注册的最大的文件描述符
    int maxfd;

    //文件描述符监听集合大小
    int setsize; /* max number of file descriptors tracked */

    //下一个事件id
    long long timeEventNextId;

    //最后一次事件执行时间
    time_t lastTime;     /* Used to detect system clock skew */

    //注册的文件事件
    aeFileEvent *events; /* Registered events */

    //就绪文件事件
    aeFiredEvent *fired; /* Fired events */

    // 时间事件的头节点指针
    aeTimeEvent *timeEventHead;

    // 事件处理开关
    int stop;

    // 多路复用库的事件状态数据
    void *apidata; /* This is used for polling API specific data */

    // 执行处理事件之前的函数
    aeBeforeSleepProc *beforesleep;

    // 执行处理事件之后的函数
    aeBeforeSleepProc *aftersleep;
} aeEventLoop;

/* Prototypes */
aeEventLoop *aeCreateEventLoop(int setsize);

void aeDeleteEventLoop(aeEventLoop *eventLoop);


void aeStop(aeEventLoop *eventLoop);


int aeCreateFileEvent(aeEventLoop *eventLoop, int fd, int mask,
        aeFileProc *proc, void *clientData);


void aeDeleteFileEvent(aeEventLoop *eventLoop, int fd, int mask);


int aeGetFileEvents(aeEventLoop *eventLoop, int fd);


/*
 * 创建时间事件
 * 返回时间事件的id
 */
long long aeCreateTimeEvent(aeEventLoop *eventLoop, long long milliseconds,
        aeTimeProc *proc, void *clientData,
        aeEventFinalizerProc *finalizerProc);

/**
 * 删除给定id的时间事件
 * @param eventLoop
 * @param id
 * @return
 */
int aeDeleteTimeEvent(aeEventLoop *eventLoop, long long id);


int aeProcessEvents(aeEventLoop *eventLoop, int flags);


int aeWait(int fd, int mask, long long milliseconds);


void aeMain(aeEventLoop *eventLoop);


char *aeGetApiName(void);


void aeSetBeforeSleepProc(aeEventLoop *eventLoop, aeBeforeSleepProc *beforesleep);


void aeSetAfterSleepProc(aeEventLoop *eventLoop, aeBeforeSleepProc *aftersleep);


int aeGetSetSize(aeEventLoop *eventLoop);


int aeResizeSetSize(aeEventLoop *eventLoop, int setsize);

#endif
