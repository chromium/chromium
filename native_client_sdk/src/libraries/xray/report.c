/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */


/* XRay -- a simple profiler for Native Client */

#include <alloca.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "xray/xray_priv.h"

#if defined(XRAY)

struct XRayTotal {
  int index;
  int frame;
  uint64_t ticks;
};


/* Dumps the trace report for a given frame. */
void XRayTraceReport(struct XRayTraceCapture* capture,
                     FILE* f,
                     int frame,
                     char* label,
                     float percent_cutoff,
                     int ticks_cutoff) {
  int index;
  int start;
  int end;
  float total;
  char space[257];
  struct XRaySymbolTable* symbols = XRayGetSymbolTable(capture);
  memset(space, ' ', 256);
  space[256] = 0;
  if (NULL == f) {
    f = stdout;
  }
  fprintf(f,
      "====================================================================\n");
  if (NULL != label)
    fprintf(f, "label %s\n", label);
  fprintf(f, "\n");
  fprintf(f,
      "   Address          Ticks   Percent      Function    [annotation...]\n");
  fprintf(f,
      "--------------------------------------------------------------------\n");
  total = XRayFrameGetTotalTicks(capture, frame);
  start = XRayFrameGetTraceStartIndex(capture, frame);
  end = XRayFrameGetTraceEndIndex(capture, frame);
  index = start;
  while (index != end) {
    if (!XRayTraceIsAnnotation(capture, index)) {
      const char* symbol_name;
      char annotation[XRAY_TRACE_ANNOTATION_LENGTH];
      struct XRayTraceBufferEntry* e = XRayTraceGetEntry(capture, index);
      uint32_t depth = XRAY_EXTRACT_DEPTH(e->depth_addr);
      uint32_t addr = XRAY_EXTRACT_ADDR(e->depth_addr);
      uint32_t annotation_index = e->annotation_index;
      uint64_t ticks =
          e->end_tick > e->start_tick ? e->end_tick - e->start_tick : 0;
      float percent = 100.0f * (float)ticks / total;
      if (percent >= percent_cutoff && ticks >= ticks_cutoff) {
        struct XRaySymbol* symbol;
        symbol = XRaySymbolTableLookup(symbols, addr);
        symbol_name = XRaySymbolGetName(symbol);
        if (0 != annotation_index) {
          XRayTraceCopyToString(capture, annotation_index, annotation);
        } else {
          strcpy(annotation, "");
        }
        fprintf(f, "0x%08X   %12" PRIu64 "     %5.1f     %s%s %s\n",
                (unsigned int)addr, ticks, percent,
                &space[256 - depth], symbol_name, annotation);
      }
    }
    index = XRayTraceNextEntry(capture, index);
  }
  fflush(f);
}


int qcompare(const void* a, const void* b) {
  struct XRayTotal* ia = (struct XRayTotal*)a;
  struct XRayTotal* ib = (struct XRayTotal*)b;
  if (ib->ticks > ia->ticks)
    return 1;
  else if (ib->ticks < ia->ticks)
    return -1;
  return 0;
}


/* Dumps a frame report */
void XRayFrameReport(struct XRayTraceCapture* capture, FILE* f) {
  int i;
  int head = XRayFrameGetHead(capture);
  int frame = XRayFrameGetTail(capture);
  int counter = 0;
  int total_capture = 0;
  struct XRayTotal* totals;
  totals = (struct XRayTotal*)
    alloca(XRayFrameGetCount(capture) * sizeof(struct XRayTotal));
  fprintf(f, "\n");
  fprintf(f,
      "Frame#        Total Ticks      Capture size    Annotations   Label\n");
  fprintf(f,
      "--------------------------------------------------------------------\n");
  while (frame != head) {
    uint64_t total_ticks = XRayFrameGetTotalTicks(capture, frame);
    int capture_size = XRayFrameGetTraceCount(capture, frame);
    int annotation_count = XRayFrameGetAnnotationCount(capture, frame);
    bool valid = XRayFrameIsValid(capture, frame);
    char label[XRAY_MAX_LABEL];
    XRayFrameMakeLabel(capture, counter, label);
    fprintf(f, "   %3d %s     %12" PRIu64 "        %10d     %10d   %s\n",
      counter,
      valid ? " " : "*",
      total_ticks,
      capture_size,
      annotation_count,
      label);
    totals[counter].index = counter;
    totals[counter].frame = frame;
    totals[counter].ticks = total_ticks;
    total_capture += capture_size;
    ++counter;
    frame = XRayFrameGetNext(capture, frame);
  }
  fprintf(f,
      "--------------------------------------------------------------------\n");
  fprintf(f,
  "XRay: %d frame(s)    %d total capture(s)\n", counter, total_capture);
  fprintf(f, "\n");
  /* Sort and take average of the median cut */
  qsort(totals, counter, sizeof(struct XRayTotal), qcompare);
  fprintf(f, "\n");
  fprintf(f, "Sorted by total ticks (most expensive first):\n");
  fprintf(f, "\n");
  fprintf(f,
      "Frame#        Total Ticks      Capture size    Annotations   Label\n");
  fprintf(f,
      "--------------------------------------------------------------------\n");
  for (i = 0; i < counter; ++i) {
    int index = totals[i].index;
    int frame = totals[i].frame;
    uint64_t total_ticks = XRayFrameGetTotalTicks(capture, frame);
    int capture_size = XRayFrameGetTraceCount(capture, frame);
    int annotation_count = XRayFrameGetAnnotationCount(capture, frame);
    char label[XRAY_MAX_LABEL];
    XRayFrameMakeLabel(capture, index, label);
    fprintf(f, "   %3d       %12" PRIu64 "        %10d     %10d   %s\n",
        index,
        total_ticks,
        capture_size,
        annotation_count,
        label);
  }
  fflush(f);
}


/* Dump a frame report followed by trace report(s) for each frame. */
void XRayReport(struct XRayTraceCapture* capture,
                FILE* f,
                float percent_cutoff,
                int ticks_cutoff) {
  int head = XRayFrameGetHead(capture);
  int frame = XRayFrameGetTail(capture);
  int counter = 0;
  XRayFrameReport(capture, f);
  fprintf(f, "\n");
  while (frame != head) {
    char label[XRAY_MAX_LABEL];
    fprintf(f, "\n");
    XRayFrameMakeLabel(capture, counter, label);
    XRayTraceReport(capture, f, frame, label, percent_cutoff, ticks_cutoff);
    ++counter;
    frame = XRayFrameGetNext(capture, frame);
  }
  fprintf(f,
      "====================================================================\n");
#if defined(XRAY_OUTPUT_HASH_COLLISIONS)
  XRayHashTableHisto(capture, f);
#endif
  fflush(f);
}

/* Write a profile report to text file. */
void XRaySaveReport(struct XRayTraceCapture* capture,
                    const char* filename,
                    float percent_cutoff,
                    int ticks_cutoff) {
  FILE* f;
  f = fopen(filename, "wt");
  if (NULL != f) {
    XRayReport(capture, f, percent_cutoff, ticks_cutoff);
    fclose(f);
  }
}

#endif  /* XRAY */
