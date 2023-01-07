/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */


/* XRay -- a simple profiler for Native Client */

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "xray/xray_priv.h"

#if defined(XRAY)

/* GTSC - Get Time Stamp Counter */
#if defined(__amd64__) && !defined(XRAY_NO_RDTSC)
XRAY_INLINE uint64_t RDTSC64();
uint64_t RDTSC64() {
  uint64_t a, d;
  __asm__ __volatile__("rdtsc" : "=a" (a), "=d" (d));
  return ((uint64_t)a) | (((uint64_t)d) << 32);
}
#define GTSC(_x) _x = RDTSC64()
#elif defined(__i386__) && !defined(XRAY_NO_RDTSC)
#define GTSC(_x)      __asm__ __volatile__ ("rdtsc" : "=A" (_x));
#else
XRAY_INLINE uint64_t GTOD();
uint64_t GTOD() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint64_t)tv.tv_sec * 1000000 + (uint64_t)tv.tv_usec;
}
#define GTSC(_x) _x = GTOD();
#endif

/* Use a TLS variable for cheap thread uid. */
__thread struct XRayTraceCapture* g_xray_capture = NULL;
__thread int g_xray_thread_id_placeholder = 0;


struct XRayTraceStackEntry {
  uint32_t depth_addr;
  uint64_t tsc;
  uint32_t dest;
  uint32_t annotation_index;
};


struct XRayTraceFrameEntry {
  /* Indices into global tracebuffer */
  int start;
  int end;
  uint64_t start_tsc;
  uint64_t end_tsc;
  uint64_t total_ticks;
  int annotation_count;
  bool valid;

#ifndef XRAY_DISABLE_BROWSER_INTEGRATION
  struct XRayTimestampPair start_time;
  struct XRayTimestampPair end_time;
#endif
};


struct XRayTraceFrame {
  struct XRayTraceFrameEntry* entry;
  int head;
  int tail;
  int count;
};


struct XRayTraceCapture {
  /* Common variables share cache line */
  bool recording;
  uint32_t stack_depth;
  uint32_t max_stack_depth;
  int buffer_index;
  int buffer_size;
  int disabled;
  int annotation_count;
  struct XRaySymbolTable* symbols;
  bool initialized;
  uint32_t annotation_filter;
  uint32_t guard0;
  struct XRayTraceStackEntry stack[XRAY_TRACE_STACK_SIZE] XRAY_ALIGN64;
  uint32_t guard1;
  uint32_t guard2;
  char annotation[XRAY_ANNOTATION_STACK_SIZE] XRAY_ALIGN64;
  uint32_t guard3;
  struct XRayTraceBufferEntry* buffer;
  struct XRayTraceFrame frame;

#ifndef XRAY_DISABLE_BROWSER_INTEGRATION
  int32_t thread_id;
#endif
} XRAY_ALIGN64;


#ifdef __cplusplus
extern "C" {
#endif

#if defined(__pnacl__)
XRAY_NO_INSTRUMENT void __pnacl_profile_func_enter(const char* fname);
XRAY_NO_INSTRUMENT void __pnacl_profile_func_exit(const char* fname);
#else
XRAY_NO_INSTRUMENT void __cyg_profile_func_enter(void* this_fn,
                                                 void* call_site);
XRAY_NO_INSTRUMENT void __cyg_profile_func_exit(void* this_fn,
                                                void* call_site);
#endif

XRAY_INLINE int XRayTraceDecrementIndexInline(
    struct XRayTraceCapture* capture, int index);
XRAY_INLINE int XRayTraceIncrementIndexInline(
    struct XRayTraceCapture* capture, int index);


XRAY_NO_INSTRUMENT void __xray_profile_append_annotation(
    struct XRayTraceCapture* capture,
    struct XRayTraceStackEntry* se,
    struct XRayTraceBufferEntry* be);

#ifdef __cplusplus
}
#endif

/* Asserts that the guard values haven't changed. */
void XRayCheckGuards(struct XRayTraceCapture* capture) {
  assert(capture->guard0 == XRAY_GUARD_VALUE_0x12345678);
  assert(capture->guard1 == XRAY_GUARD_VALUE_0x12345678);
  assert(capture->guard2 == XRAY_GUARD_VALUE_0x87654321);
  assert(capture->guard3 == XRAY_GUARD_VALUE_0x12345678);
}

/* Decrements the trace index, wrapping around if needed. */
int XRayTraceDecrementIndexInline(
    struct XRayTraceCapture* capture, int index) {
  --index;
  if (index < 0)
    index = capture->buffer_size - 1;
  return index;
}

/* Increments the trace index, wrapping around if needed. */
int XRayTraceIncrementIndexInline(
    struct XRayTraceCapture* capture, int index) {
  ++index;
  if (index >= capture->buffer_size)
    index = 0;
  return index;
}

/* Returns true if the trace entry is an annotation string. */
bool XRayTraceIsAnnotation(
    struct XRayTraceCapture* capture, int index) {
  struct XRayTraceBufferEntry* be = &capture->buffer[index];
  char* dst = (char*)be;
  return 0 == *dst;
}

int XRayTraceIncrementIndex(struct XRayTraceCapture* capture, int index) {
  return XRayTraceIncrementIndexInline(capture, index);
}

int XRayTraceDecrementIndex(struct XRayTraceCapture* capture, int index) {
  return XRayTraceDecrementIndexInline(capture, index);
}

/* The entry in the tracebuffer at index is an annotation string. */
/* Calculate the next index value representing the next trace entry. */
int XRayTraceSkipAnnotation(struct XRayTraceCapture* capture, int index) {
  /* Annotations are strings embedded in the trace buffer. */
  /* An annotation string can span multiple trace entries. */
  /* Skip over the string by looking for zero termination. */
  assert(capture);
  assert(XRayTraceIsAnnotation(capture, index));
  bool done = false;
  int start_index = 1;
  int i;
  while (!done) {
    char* str = (char*) &capture->buffer[index];
    const int num = sizeof(capture->buffer[index]);
    for (i = start_index; i < num; ++i) {
      if (0 == str[i]) {
        done = true;
        break;
      }
    }
    index = XRayTraceIncrementIndexInline(capture, index);
    start_index = 0;
  }
  return index;
}


struct XRayTraceBufferEntry* XRayTraceGetEntry(
    struct XRayTraceCapture* capture, int index) {
  return &capture->buffer[index];
}

/* Starting at index, return the index into the trace buffer */
/* for the next trace entry.  Index can wrap (ringbuffer) */
int XRayTraceNextEntry(struct XRayTraceCapture* capture, int index) {
  if (XRayTraceIsAnnotation(capture, index))
    index = XRayTraceSkipAnnotation(capture, index);
  else
    index = XRayTraceIncrementIndexInline(capture, index);
  return index;
}

int XRayFrameGetTraceStartIndex(struct XRayTraceCapture* capture, int frame) {
  assert(capture);
  assert(capture->initialized);
  assert(!capture->recording);
  return capture->frame.entry[frame].start;
}

int XRayFrameGetTraceEndIndex(struct XRayTraceCapture* capture, int frame) {
  assert(capture);
  assert(capture->initialized);
  assert(!capture->recording);
  return capture->frame.entry[frame].end;
}

/* Not very accurate, annotation strings will also be counted as "entries" */
int XRayFrameGetTraceCount(
    struct XRayTraceCapture* capture, int frame) {
  assert(true == capture->initialized);
  assert(frame >= 0);
  assert(frame < capture->frame.count);
  assert(!capture->recording);
  int start = capture->frame.entry[frame].start;
  int end = capture->frame.entry[frame].end;
  int num;
  if (start < end)
    num = end - start;
  else
    num = capture->buffer_size - (start - end);
  return num;
}

/* Append a string to trace buffer. */
void XRayTraceAppendString(struct XRayTraceCapture* capture, char* src) {
  int index = capture->buffer_index;
  bool done = false;
  int start_index = 1;
  int s = 0;
  int i;
  char* dst = (char*)&capture->buffer[index];
  const int num = sizeof(capture->buffer[index]);
  dst[0] = 0;
  while (!done) {
    for (i = start_index; i < num; ++i) {
      dst[i] = src[s];
      if (0 == src[s]) {
        dst[i] = 0;
        done = true;
        break;
      }
      ++s;
    }
    index = XRayTraceIncrementIndexInline(capture, index);
    dst = (char*)&capture->buffer[index];
    start_index = 0;
  }
  capture->buffer_index = index;
}

/* Copies annotation from trace buffer to output string. */
int XRayTraceCopyToString(
    struct XRayTraceCapture* capture, int index, char* dst) {
  assert(XRayTraceIsAnnotation(capture, index));
  bool done = false;
  int i;
  int d = 0;
  int start_index = 1;
  while (!done) {
    char* src = (char*) &capture->buffer[index];
    const int num = sizeof(capture->buffer[index]);
    for (i = start_index; i < num; ++i) {
      dst[d] = src[i];
      if (0 == src[i]) {
        done = true;
        break;
      }
      ++d;
    }
    index = XRayTraceIncrementIndexInline(capture, index);
    start_index = 0;
  }
  return index;
}


/* Generic memory malloc for XRay */
/* validates pointer returned by malloc */
/* memsets memory block to zero */
void* XRayMalloc(size_t t) {
  void* data;
  data = calloc(1, t);
  if (NULL == data) {
    printf("XRay: malloc(%d) failed, panic shutdown!\n", t);
    exit(-1);
  }
  return data;
}


/* Generic memory free for XRay */
void XRayFree(void* data) {
  assert(NULL != data);
  free(data);
}


/* Main profile capture function that is called at the start */
/* of every instrumented function.  This function is implicitly */
/* called when code is compilied with the -finstrument-functions option */
#if defined(__pnacl__)
void __pnacl_profile_func_enter(const char* this_fn) {
#else
void __cyg_profile_func_enter(void* this_fn, void* call_site) {
#endif
  struct XRayTraceCapture* capture = g_xray_capture;
  if (capture && capture->recording) {
    uint32_t depth = capture->stack_depth;
    if (depth < capture->max_stack_depth) {
      struct XRayTraceStackEntry* se = &capture->stack[depth];
      uint32_t addr = (uint32_t)(uintptr_t)this_fn;
      se->depth_addr = XRAY_PACK_DEPTH_ADDR(depth, addr);
      se->dest = capture->buffer_index;
      se->annotation_index = 0;
      GTSC(se->tsc);
      capture->buffer_index =
        XRayTraceIncrementIndexInline(capture, capture->buffer_index);
    }
    ++capture->stack_depth;
  }
}


/* Main profile capture function that is called at the exit of */
/* every instrumented function.  This function is implicity called */
/* when the code is compiled with the -finstrument-functions option */
#if defined(__pnacl__)
void __pnacl_profile_func_exit(const char* this_fn) {
#else
void __cyg_profile_func_exit(void* this_fn, void* call_site) {
#endif
  struct XRayTraceCapture* capture = g_xray_capture;
  if (capture && capture->recording) {
    --capture->stack_depth;
    if (capture->stack_depth < capture->max_stack_depth) {
      uint32_t depth = capture->stack_depth;
      struct XRayTraceStackEntry* se = &capture->stack[depth];
      uint32_t buffer_index = se->dest;
      uint64_t tsc;
      struct XRayTraceBufferEntry* be = &capture->buffer[buffer_index];
      GTSC(tsc);
      be->depth_addr = se->depth_addr;
      be->start_tick = se->tsc;
      be->end_tick = tsc;
      be->annotation_index = 0;
      if (0 != se->annotation_index)
        __xray_profile_append_annotation(capture, se, be);
    }
  }
}

#ifndef XRAY_DISABLE_BROWSER_INTEGRATION
void XRayGetTSC(uint64_t* tsc) {
  GTSC(*tsc);
}

int32_t XRayGetSavedThreadID(struct XRayTraceCapture* capture) {
  return capture->thread_id;
}

struct XRayTimestampPair XRayFrameGetStartTimestampPair(
    struct XRayTraceCapture* capture, int frame) {
  return capture->frame.entry[frame].start_time;
}

struct XRayTimestampPair XRayFrameGetEndTimestampPair(
    struct XRayTraceCapture* capture, int frame) {
  return capture->frame.entry[frame].end_time;
}
#endif

/* Special case appending annotation string to trace buffer */
/* this function should only ever be called from __cyg_profile_func_exit() */
void __xray_profile_append_annotation(struct XRayTraceCapture* capture,
                                      struct XRayTraceStackEntry* se,
                                      struct XRayTraceBufferEntry* be) {
  struct XRayTraceStackEntry* parent = se - 1;
  int start = parent->annotation_index;
  be->annotation_index = capture->buffer_index;
  char* str = &capture->annotation[start];
  XRayTraceAppendString(capture, str);
  *str = 0;
  ++capture->annotation_count;
}



/* Annotates the trace buffer. no filtering. */
void __XRayAnnotate(const char* fmt, ...) {
  va_list args;
  struct XRayTraceCapture* capture = g_xray_capture;
  /* Only annotate functions recorded in the trace buffer. */
  if (capture && capture->initialized) {
    if (0 == capture->disabled) {
      if (capture->recording) {
        char buffer[1024];
        int r;
        va_start(args, fmt);
        r = vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        {
          /* Get current string ptr */
          int depth = capture->stack_depth - 1;
          struct XRayTraceStackEntry* se = &capture->stack[depth];
          if (0 == se->annotation_index) {
            struct XRayTraceStackEntry* parent = se - 1;
            se->annotation_index = parent->annotation_index;
          }
          char* dst = &capture->annotation[se->annotation_index];
          strcpy(dst, buffer);
          int len = strlen(dst);
          se->annotation_index += len;
        }
      }
    }
  }
}


/* Annotates the trace buffer with user strings.  Can be filtered. */
void __XRayAnnotateFiltered(const uint32_t filter, const char* fmt, ...) {
  va_list args;
  struct XRayTraceCapture* capture = g_xray_capture;
  if (capture && capture->initialized) {
    if (0 != (filter & capture->annotation_filter)) {
      if (0 == capture->disabled) {
        if (capture->recording) {
          char buffer[XRAY_TRACE_ANNOTATION_LENGTH];
          int r;
          va_start(args, fmt);
          r = vsnprintf(buffer, sizeof(buffer), fmt, args);
          va_end(args);
          {
            /* get current string ptr */
            int depth = capture->stack_depth - 1;
            struct XRayTraceStackEntry* se = &capture->stack[depth];
            if (0 == se->annotation_index) {
              struct XRayTraceStackEntry* parent = se - 1;
              se->annotation_index = parent->annotation_index;
            }
            char* dst = &capture->annotation[se->annotation_index];
            strcpy(dst, buffer);
            int len = strlen(dst);
            se->annotation_index += len;
          }
        }
      }
    }
  }
}


/* Allows user to specify annotation filter value, a 32 bit mask. */
void XRaySetAnnotationFilter(struct XRayTraceCapture* capture,
                             uint32_t filter) {
  capture->annotation_filter = filter;
}


/* Reset xray profiler. */
void XRayReset(struct XRayTraceCapture* capture) {
  assert(capture);
  assert(capture->initialized);
  assert(!capture->recording);
  capture->buffer_index = 0;
  capture->stack_depth = 0;
  capture->disabled = 0;
  capture->frame.head = 0;
  capture->frame.tail = 0;
  memset(capture->frame.entry, 0,
    sizeof(capture->frame.entry[0]) * capture->frame.count);
  memset(&capture->stack, 0,
    sizeof(capture->stack[0]) * XRAY_TRACE_STACK_SIZE);
  XRayCheckGuards(capture);
}


/* Change the maximum stack depth captures are made. */
void XRaySetMaxStackDepth(struct XRayTraceCapture* capture, int stack_depth) {
  assert(capture);
  assert(capture->initialized);
  assert(!capture->recording);
  if (stack_depth < 1)
    stack_depth = 1;
  if (stack_depth >= XRAY_TRACE_STACK_SIZE)
    stack_depth = (XRAY_TRACE_STACK_SIZE - 1);
  capture->max_stack_depth = stack_depth;
}


int XRayFrameGetCount(struct XRayTraceCapture* capture) {
  return capture->frame.count;
}

int XRayFrameGetTail(struct XRayTraceCapture* capture) {
  return capture->frame.tail;
}

int XRayFrameGetHead(struct XRayTraceCapture* capture) {
  return capture->frame.head;
}

int XRayFrameGetPrev(struct XRayTraceCapture* capture, int i) {
  i = i - 1;
  if (i < 0)
    i = capture->frame.count - 1;
  return i;
}

int XRayFrameGetNext(struct XRayTraceCapture* capture, int i) {
  i = i + 1;
  if (i >= capture->frame.count)
    i = 0;
  return i;
}

bool XRayFrameIsValid(struct XRayTraceCapture* capture, int i) {
  return capture->frame.entry[i].valid;
}

uint64_t XRayFrameGetTotalTicks(struct XRayTraceCapture* capture, int i) {
  return capture->frame.entry[i].total_ticks;
}

int XRayFrameGetAnnotationCount(struct XRayTraceCapture* capture, int i) {
  return capture->frame.entry[i].annotation_count;
}

void XRayFrameMakeLabel(struct XRayTraceCapture* capture,
                        int counter,
                        char* label) {
  snprintf(label, XRAY_MAX_LABEL, "@@@frame%d@@@", counter);
}


/* Scans the ring buffer going backwards to find last valid complete frame. */
/* Will mark whether frames are valid or invalid during the traversal. */
int XRayFrameFindTail(struct XRayTraceCapture* capture) {
  int head = capture->frame.head;
  int index = XRayFrameGetPrev(capture, head);
  int total_capture = 0;
  int last_valid_frame = index;
  /* Check for no captures */
  if (capture->frame.head == capture->frame.tail)
    return capture->frame.head;
  /* Go back and invalidate all captures that have been stomped. */
  while (index != head) {
    bool valid = capture->frame.entry[index].valid;
    if (valid) {
      total_capture += XRayFrameGetTraceCount(capture, index) + 1;
      if (total_capture < capture->buffer_size) {
        last_valid_frame = index;
        capture->frame.entry[index].valid = true;
      } else {
        capture->frame.entry[index].valid = false;
      }
    }
    index = XRayFrameGetPrev(capture, index);
  }
  return last_valid_frame;
}


/* Starts a new frame and enables capturing, and must be paired with */
/* XRayEndFrame()  Trace capturing only occurs on the thread which called */
/* XRayBeginFrame() and each instance of capture can only trace one thread */
/* at a time. */
void XRayStartFrame(struct XRayTraceCapture* capture) {
  int i;
  assert(NULL == g_xray_capture);
  assert(capture->initialized);
  assert(!capture->recording);
  i = capture->frame.head;
  XRayCheckGuards(capture);
  /* Add a trace entry marker so we can detect wrap around stomping */
  struct XRayTraceBufferEntry* be = &capture->buffer[capture->buffer_index];
  be->depth_addr = XRAY_FRAME_MARKER;
  capture->buffer_index =
      XRayTraceIncrementIndex(capture, capture->buffer_index);
  /* Set start of the frame we're about to trace */
  capture->frame.entry[i].start = capture->buffer_index;
  capture->disabled = 0;
  capture->stack_depth = 1;

  /* The trace stack[0] is reserved */
  memset(&capture->stack[0], 0, sizeof(capture->stack[0]));
  /* Annotation index 0 is reserved to indicate no annotation */
  capture->stack[0].annotation_index = 1;
  capture->annotation[0] = 0;
  capture->annotation[1] = 0;
  capture->annotation_count = 0;
  capture->recording = true;
  GTSC(capture->frame.entry[i].start_tsc);
  g_xray_capture = capture;

#ifndef XRAY_DISABLE_BROWSER_INTEGRATION
  capture->frame.entry[i].start_time = XRayGenerateTimestampsNow();
#endif
}


/* Ends a frame and disables capturing. Advances to the next frame. */
/* Must be paired with XRayStartFrame(), and called from the same thread. */
void XRayEndFrame(struct XRayTraceCapture* capture) {
  int i;
  assert(capture);
  assert(capture->initialized);
  assert(capture->recording);
  assert(g_xray_capture == capture);
  assert(0 == capture->disabled);
  assert(1 == capture->stack_depth);
  i = capture->frame.head;
  GTSC(capture->frame.entry[i].end_tsc);
  capture->frame.entry[i].total_ticks =
    capture->frame.entry[i].end_tsc - capture->frame.entry[i].start_tsc;
  capture->recording = NULL;
  capture->frame.entry[i].end = capture->buffer_index;
  capture->frame.entry[i].valid = true;
  capture->frame.entry[i].annotation_count = capture->annotation_count;
  capture->frame.head = XRayFrameGetNext(capture, capture->frame.head);
  /* If the table is filled, bump the tail. */
  if (capture->frame.head == capture->frame.tail)
    capture->frame.tail = XRayFrameGetNext(capture, capture->frame.tail);
  capture->frame.tail = XRayFrameFindTail(capture);
  /* Check that we didn't stomp over trace entry marker. */
  int marker = XRayTraceDecrementIndex(capture, capture->frame.entry[i].start);
  struct XRayTraceBufferEntry* be = &capture->buffer[marker];
  if (be->depth_addr != XRAY_FRAME_MARKER) {
    fprintf(stderr,
      "XRay: XRayStopFrame() detects insufficient trace buffer size!\n");
    XRayReset(capture);
  } else {
    /* Replace marker with an empty annotation string. */
    be->depth_addr = XRAY_NULL_ANNOTATION;
    XRayCheckGuards(capture);
  }
  g_xray_capture = NULL;

#ifndef XRAY_DISABLE_BROWSER_INTEGRATION
  capture->frame.entry[i].end_time = XRayGenerateTimestampsNow();
#endif
}


/* Get the last frame captured.  Do not call while capturing. */
/* (ie call outside of XRayStartFrame() / XRayStopFrame() pair) */
int XRayGetLastFrame(struct XRayTraceCapture* capture) {
  assert(capture);
  assert(capture->initialized);
  assert(!capture->recording);
  assert(0 == capture->disabled);
  assert(1 == capture->stack_depth);
  int last_frame = XRayFrameGetPrev(capture, capture->frame.head);
  return last_frame;
}


/* Disables capturing until a paired XRayEnableCapture() is called */
/* This call can be nested, but must be paired with an enable */
/* (If you need to just exclude a specific function and not its */
/* children, the XRAY_NO_INSTRUMENT modifier might be better) */
/* Must be called from same thread as XRayBeginFrame() / XRayEndFrame() */
void XRayDisableCapture(struct XRayTraceCapture* capture) {
  assert(capture);
  assert(capture == g_xray_capture);
  assert(capture->initialized);
  ++capture->disabled;
  capture->recording = false;
}


/* Re-enables capture.  Must be paired with XRayDisableCapture() */
void XRayEnableCapture(struct XRayTraceCapture* capture) {
  assert(capture);
  assert(capture == g_xray_capture);
  assert(capture->initialized);
  assert(0 < capture->disabled);
  --capture->disabled;
  if (0 == capture->disabled) {
    capture->recording = true;
  }
}



struct XRaySymbolTable* XRayGetSymbolTable(struct XRayTraceCapture* capture) {
  return capture->symbols;
}


/* Initialize XRay */
struct XRayTraceCapture* XRayInit(int stack_depth,
                                  int buffer_size,
                                  int frame_count,
                                  const char* mapfilename) {
  struct XRayTraceCapture* capture;
  capture = (struct XRayTraceCapture*)XRayMalloc(
      sizeof(struct XRayTraceCapture));
  int adj_frame_count = frame_count + 1;
  size_t buffer_size_in_bytes =
      sizeof(capture->buffer[0]) * buffer_size;
  size_t frame_size_in_bytes =
      sizeof(capture->frame.entry[0]) * adj_frame_count;
  capture->buffer =
      (struct XRayTraceBufferEntry *)XRayMalloc(buffer_size_in_bytes);
  capture->frame.entry =
      (struct XRayTraceFrameEntry *)XRayMalloc(frame_size_in_bytes);
  capture->buffer_size = buffer_size;
  capture->frame.count = adj_frame_count;
  capture->frame.head = 0;
  capture->frame.tail = 0;
  capture->disabled = 0;
  capture->annotation_filter = 0xFFFFFFFF;
  capture->guard0 = XRAY_GUARD_VALUE_0x12345678;
  capture->guard1 = XRAY_GUARD_VALUE_0x12345678;
  capture->guard2 = XRAY_GUARD_VALUE_0x87654321;
  capture->guard3 = XRAY_GUARD_VALUE_0x12345678;
  capture->initialized = true;
  capture->recording = false;
  XRaySetMaxStackDepth(capture, stack_depth);
  XRayReset(capture);

  /* Mapfile is optional; we don't need it for captures, only for reports. */
  capture->symbols =
      XRaySymbolTableCreate(XRAY_DEFAULT_SYMBOL_TABLE_SIZE);
  if (NULL != mapfilename)
    XRaySymbolTableParseMapfile(capture->symbols, mapfilename);

#ifndef XRAY_DISABLE_BROWSER_INTEGRATION
  /* Use the address of a thread local variable as a fake thread id. */
  capture->thread_id = (int32_t)(&g_xray_thread_id_placeholder);
#endif

  return capture;
}


/* Shut down and free memory used by XRay. */
void XRayShutdown(struct XRayTraceCapture* capture) {
  assert(capture);
  assert(capture->initialized);
  assert(!capture->recording);
  XRayCheckGuards(capture);
  if (NULL != capture->symbols) {
    XRaySymbolTableFree(capture->symbols);
  }
  XRayFree(capture->frame.entry);
  XRayFree(capture->buffer);
  capture->initialized = false;
  XRayFree(capture);
}

#endif  /* XRAY */
