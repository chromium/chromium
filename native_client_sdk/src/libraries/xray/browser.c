/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


/* XRay -- a simple profiler for Native Client */

#ifndef XRAY_DISABLE_BROWSER_INTEGRATION

#include <alloca.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ppapi/c/dev/ppb_trace_event_dev.h"
#include "xray/xray_priv.h"


#if defined(XRAY)
static PPB_Trace_Event_Dev* ppb_trace_event_interface = NULL;

static const char* XRayGetName(struct XRaySymbolTable* symbols,
      struct XRayTraceBufferEntry* e) {
  uint32_t addr = XRAY_EXTRACT_ADDR(e->depth_addr);
  struct XRaySymbol* symbol = XRaySymbolTableLookup(symbols, addr);
  return XRaySymbolGetName(symbol);
}

struct XRayTimestampPair XRayGenerateTimestampsNow(void) {
  struct XRayTimestampPair pair;
  assert(ppb_trace_event_interface);

  XRayGetTSC(&pair.xray);
  pair.pepper = ppb_trace_event_interface->Now();
  return pair;
}

/* see chromium/src/base/trace_event/trace_event.h */
#define TRACE_VALUE_TYPE_UINT (2)
#define TRACE_VALUE_TYPE_DOUBLE (4)
#define TRACE_VALUE_TYPE_COPY_STRING (7)

union TraceValue {
    bool as_bool;
    unsigned long long as_uint;
    long long as_int;
    double as_double;
    const void* as_pointer;
    const char* as_string;
  };

void XRayBrowserTraceReport(struct XRayTraceCapture* capture) {

  const void* cat_enabled = ppb_trace_event_interface->GetCategoryEnabled(
      "xray");
  struct XRaySymbolTable* symbols = XRayGetSymbolTable(capture);

  int32_t thread_id = XRayGetSavedThreadID(capture);

  int head = XRayFrameGetHead(capture);
  int frame = XRayFrameGetTail(capture);
  while(frame != head) {

    struct XRayTimestampPair start_time = XRayFrameGetStartTimestampPair(
        capture, frame);
    struct XRayTimestampPair end_time = XRayFrameGetEndTimestampPair(
        capture, frame);

    double pdiff = (end_time.pepper - start_time.pepper);
    double odiff = (end_time.xray - start_time.xray);
    double scale_a = pdiff / odiff;
    double scale_b = ((double)end_time.pepper) - (scale_a * end_time.xray);
    printf("Xray timestamp calibration frame %d: %f %f\n",
        frame, scale_a, scale_b);

    int start = XRayFrameGetTraceStartIndex(capture, frame);
    int end = XRayFrameGetTraceEndIndex(capture, frame);

    struct XRayTraceBufferEntry** stack_base = XRayMalloc(
      sizeof(struct XRayTraceBufferEntry*) * (XRAY_TRACE_STACK_SIZE + 1));
    struct XRayTraceBufferEntry** stack_top = stack_base;
    *stack_top = NULL;

    uint32_t num_args = 0;
    const char* arg_names[] = {"annotation"};
    uint8_t arg_types[] = {TRACE_VALUE_TYPE_COPY_STRING};
    uint64_t arg_values[] = {0};
    char annotation[XRAY_TRACE_ANNOTATION_LENGTH];

    int i;
    for(i = start; i != end; i = XRayTraceNextEntry(capture, i)) {
      if (XRayTraceIsAnnotation(capture, i)) {
        continue;
      }

      uint32_t depth = XRAY_EXTRACT_DEPTH(
          XRayTraceGetEntry(capture, i)->depth_addr);

      while(*stack_top &&
          XRAY_EXTRACT_DEPTH((*stack_top)->depth_addr) >= depth) {
        struct XRayTraceBufferEntry* e = *(stack_top--);
        ppb_trace_event_interface->AddTraceEventWithThreadIdAndTimestamp(
            'E', cat_enabled,
            XRayGetName(symbols, e),
            0, thread_id,
            (scale_a * e->end_tick) + scale_b,
            0, NULL, NULL, NULL, 0
        );
      }

      num_args = 0;
      struct XRayTraceBufferEntry* e = XRayTraceGetEntry(capture, i);
      uint32_t annotation_index = e->annotation_index;
      if (annotation_index) {
        XRayTraceCopyToString(capture, annotation_index, annotation);

        union TraceValue val;
        val.as_string = (const char*)annotation;

        arg_values[0] = val.as_uint;
        num_args = 1;
      }

      ppb_trace_event_interface->AddTraceEventWithThreadIdAndTimestamp(
          'B', cat_enabled,
          XRayGetName(symbols, e),
          0, thread_id,
          (scale_a * e->start_tick) + scale_b,
          num_args, arg_names, arg_types, arg_values, 0
      );

      *(++stack_top) = e;
    }

    while(*stack_top) {
      struct XRayTraceBufferEntry* e = *(stack_top--);
      ppb_trace_event_interface->AddTraceEventWithThreadIdAndTimestamp(
          'E', cat_enabled,
          XRayGetName(symbols, e),
          0, thread_id,
          (scale_a * e->end_tick) + scale_b,
          0, NULL, NULL, NULL, 0
      );
    }

    frame = XRayFrameGetNext(capture, frame);
    XRayFree(stack_base);
  }
}

void XRayRegisterBrowserInterface(PPB_GetInterface interface) {
  ppb_trace_event_interface = (PPB_Trace_Event_Dev*)interface(
      PPB_TRACE_EVENT_DEV_INTERFACE);
  assert(ppb_trace_event_interface);
}

#endif  /* XRAY */
#endif  /* XRAY_DISABLE_BROWSER_INTEGRATION */