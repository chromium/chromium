/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/* XRay -- a simple profiler for Native Client */

/* This header file is the private internal interface. */

#ifndef LIBRARIES_XRAY_XRAY_PRIV_H_
#define LIBRARIES_XRAY_XRAY_PRIV_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "xray/xray.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(XRAY)

#define XRAY_FORCE_INLINE __attribute__((always_inline, no_instrument_function))

#define XRAY_TRACE_STACK_SIZE (256)
#define XRAY_TRACE_ANNOTATION_LENGTH (2048)
#define XRAY_TRACE_BUFFER_SIZE (1048576)
#define XRAY_ANNOTATION_STACK_SIZE ((XRAY_TRACE_STACK_SIZE) * \
                                    (XRAY_TRACE_ANNOTATION_LENGTH))
#define XRAY_STRING_POOL_NODE_SIZE (32768)
#define XRAY_FRAME_MARKER (0xFFFFFFFF)
#define XRAY_NULL_ANNOTATION (0x0)
#define XRAY_FUNCTION_ALIGNMENT_BITS (4)
#define XRAY_ADDR_MASK (0xFFFFFF00)
#define XRAY_ADDR_SHIFT (4)
#define XRAY_DEPTH_MASK (0x000000FF)
#define XRAY_SYMBOL_TABLE_MAX_RATIO (0.66f)
#define XRAY_LINE_SIZE (1024)
#define XRAY_MAX_FRAMES (60)
#define XRAY_MAX_LABEL (64)
#define XRAY_DEFAULT_SYMBOL_TABLE_SIZE (4096)
#define XRAY_SYMBOL_POOL_NODE_SIZE (1024)
#define XRAY_GUARD_VALUE_0x12345678 (0x12345678)
#define XRAY_GUARD_VALUE_0x87654321 (0x87654321)
#define XRAY_EXTRACT_ADDR(x) (((x) & XRAY_ADDR_MASK) >> XRAY_ADDR_SHIFT)
#define XRAY_EXTRACT_DEPTH(x) ((x) & XRAY_DEPTH_MASK)
#define XRAY_PACK_ADDR(x) (((x) << XRAY_ADDR_SHIFT) & XRAY_ADDR_MASK)
#define XRAY_PACK_DEPTH(x) ((x) & XRAY_DEPTH_MASK)
#define XRAY_PACK_DEPTH_ADDR(d, a) (XRAY_PACK_DEPTH(d) | XRAY_PACK_ADDR(a))
#define XRAY_ALIGN64 __attribute((aligned(64)))

struct XRayStringPool;
struct XRayHashTable;
struct XRaySymbolPool;
struct XRaySymbol;
struct XRaySymbolTable;
struct XRayTraceCapture;

struct XRayTraceBufferEntry {
  uint32_t depth_addr;
  uint32_t annotation_index;
  uint64_t start_tick;
  uint64_t end_tick;
};


/* Important: don't instrument xray itself, so use       */
/*            XRAY_NO_INSTRUMENT on all xray functions   */

XRAY_NO_INSTRUMENT char* XRayStringPoolAppend(struct XRayStringPool* pool,
  const char* src);
XRAY_NO_INSTRUMENT struct XRayStringPool* XRayStringPoolCreate();
XRAY_NO_INSTRUMENT void XRayStringPoolFree(struct XRayStringPool* pool);

XRAY_NO_INSTRUMENT void* XRayHashTableLookup(struct XRayHashTable* table,
    uint32_t addr);
XRAY_NO_INSTRUMENT void* XRayHashTableInsert(struct XRayHashTable* table,
    void* data, uint32_t addr);
XRAY_NO_INSTRUMENT void* XRayHashTableAtIndex(
  struct XRayHashTable* table, int i);
XRAY_NO_INSTRUMENT int XRayHashTableGetCapacity(struct XRayHashTable* table);
XRAY_NO_INSTRUMENT int XRayHashTableGetCount(struct XRayHashTable* table);
XRAY_NO_INSTRUMENT struct XRayHashTable* XRayHashTableCreate(int capacity);
XRAY_NO_INSTRUMENT void XRayHashTableFree(struct XRayHashTable* table);
XRAY_NO_INSTRUMENT void XRayHashTableHisto(FILE* f);

XRAY_NO_INSTRUMENT struct XRaySymbol* XRaySymbolPoolAlloc(
    struct XRaySymbolPool* sympool);
XRAY_NO_INSTRUMENT struct XRaySymbolPool* XRaySymbolPoolCreate();
XRAY_NO_INSTRUMENT void XRaySymbolPoolFree(struct XRaySymbolPool* sympool);

XRAY_NO_INSTRUMENT const char* XRayDemangle(char* demangle, size_t buffersize,
    const char* symbol);

XRAY_NO_INSTRUMENT const char* XRaySymbolGetName(struct XRaySymbol* symbol);
XRAY_NO_INSTRUMENT struct XRaySymbol* XRaySymbolCreate(
    struct XRaySymbolPool* sympool, const char* name);
XRAY_NO_INSTRUMENT void XRaySymbolFree(struct XRaySymbol* symbol);
XRAY_NO_INSTRUMENT int XRaySymbolCount(struct XRaySymbolTable* symtab);

XRAY_NO_INSTRUMENT struct XRaySymbol* XRaySymbolTableCreateEntry(
    struct XRaySymbolTable* symtab, char* line);
XRAY_NO_INSTRUMENT void XRaySymbolTableParseMapfile(
    struct XRaySymbolTable* symbols, const char* mapfilename);

XRAY_NO_INSTRUMENT struct XRaySymbol* XRaySymbolTableAddByName(
    struct XRaySymbolTable* symtab, const char* name, uint32_t addr);

XRAY_NO_INSTRUMENT int XRaySymbolTableGetCount(struct XRaySymbolTable* symtab);
XRAY_NO_INSTRUMENT struct XRaySymbol* XRaySymbolTableLookup(
    struct XRaySymbolTable* symbols, uint32_t addr);
XRAY_NO_INSTRUMENT struct XRaySymbol* XRaySymbolTableAtIndex(
    struct XRaySymbolTable* symbols, int i);
XRAY_NO_INSTRUMENT struct XRaySymbolTable* XRaySymbolTableCreate(int size);
XRAY_NO_INSTRUMENT void XRaySymbolTableFree(struct XRaySymbolTable* symbtab);

XRAY_NO_INSTRUMENT struct XRaySymbolTable* XRayGetSymbolTable(
    struct XRayTraceCapture* capture);

XRAY_NO_INSTRUMENT void XRayCheckGuards(struct XRayTraceCapture* capture);

XRAY_NO_INSTRUMENT struct XRayTraceBufferEntry* XRayTraceGetEntry(
    struct XRayTraceCapture* capture, int index);
XRAY_NO_INSTRUMENT int XRayTraceIncrementIndex(
    struct XRayTraceCapture* capture, int i);
XRAY_NO_INSTRUMENT int XRayTraceDecrementIndex(
    struct XRayTraceCapture* capture, int i);
XRAY_NO_INSTRUMENT bool XRayTraceIsAnnotation(
    struct XRayTraceCapture* capture, int index);
XRAY_NO_INSTRUMENT void XRayTraceAppendString(
    struct XRayTraceCapture* capture, char* src);
XRAY_NO_INSTRUMENT int XRayTraceCopyToString(
    struct XRayTraceCapture* capture, int index, char* dst);
XRAY_NO_INSTRUMENT int XRayTraceSkipAnnotation(
    struct XRayTraceCapture* capture, int index);
XRAY_NO_INSTRUMENT int XRayTraceNextEntry(
    struct XRayTraceCapture* capture, int index);

XRAY_NO_INSTRUMENT void XRayFrameMakeLabel(struct XRayTraceCapture* capture,
                                           int counter,
                                           char* label);
XRAY_NO_INSTRUMENT int XRayFrameFindTail(struct XRayTraceCapture* capture);

XRAY_NO_INSTRUMENT int XRayFrameGetCount(struct XRayTraceCapture* capture);
XRAY_NO_INSTRUMENT int XRayFrameGetHead(struct XRayTraceCapture* capture);
XRAY_NO_INSTRUMENT int XRayFrameGetTail(struct XRayTraceCapture* capture);
XRAY_NO_INSTRUMENT int XRayFrameGetNext(
    struct XRayTraceCapture* capture, int index);
XRAY_NO_INSTRUMENT uint64_t XRayFrameGetTotalTicks(
    struct XRayTraceCapture* capture, int frame);
XRAY_NO_INSTRUMENT int XRayFrameGetTraceCount(
    struct XRayTraceCapture* capture, int frame);
XRAY_NO_INSTRUMENT int XRayFrameGetTraceStartIndex(
    struct XRayTraceCapture* capture, int frame);
XRAY_NO_INSTRUMENT int XRayFrameGetTraceEndIndex(
    struct XRayTraceCapture* capture, int frame);
XRAY_NO_INSTRUMENT int XRayFrameGetAnnotationCount(
    struct XRayTraceCapture* capture, int frame);
XRAY_NO_INSTRUMENT bool XRayFrameIsValid(
    struct XRayTraceCapture* capture, int frame);


XRAY_NO_INSTRUMENT void XRayTraceReport(struct XRayTraceCapture* capture,
                                        FILE* f,
                                        int frame,
                                        char* label,
                                        float percent_cutoff,
                                        int ticks_cutoff);
XRAY_NO_INSTRUMENT void XRayFrameReport(struct XRayTraceCapture* capture,
                                        FILE* f);

XRAY_NO_INSTRUMENT void* XRayMalloc(size_t t);
XRAY_NO_INSTRUMENT void XRayFree(void* data);

XRAY_NO_INSTRUMENT void XRaySetMaxStackDepth(
    struct XRayTraceCapture* capture, int stack_depth);
XRAY_NO_INSTRUMENT int XRayGetLastFrame(struct XRayTraceCapture* capture);
XRAY_NO_INSTRUMENT void XRayDisableCapture(struct XRayTraceCapture* capture);
XRAY_NO_INSTRUMENT void XRayEnableCapture(struct XRayTraceCapture* capture);
XRAY_NO_INSTRUMENT void XRayLoadMapfile(
    struct XRayTraceCapture* capture, const char* mapfilename);

struct XRayTimestampPair {
  uint64_t xray;   /* internal xray timestamp */
  int64_t pepper;  /* corresponding timestamp from PPAPI interface */
};

#ifndef XRAY_DISABLE_BROWSER_INTEGRATION
XRAY_NO_INSTRUMENT void XRayGetTSC(uint64_t* tsc);
XRAY_NO_INSTRUMENT int32_t XRayGetSavedThreadID(
    struct XRayTraceCapture* capture);
XRAY_NO_INSTRUMENT struct XRayTimestampPair XRayFrameGetStartTimestampPair(
    struct XRayTraceCapture* capture, int frame);
XRAY_NO_INSTRUMENT struct XRayTimestampPair XRayFrameGetEndTimestampPair(
    struct XRayTraceCapture* capture, int frame);
XRAY_NO_INSTRUMENT struct XRayTimestampPair XRayGenerateTimestampsNow(void);
#endif


#endif  /* defined(XRAY) */

#ifdef __cplusplus
}
#endif

#endif  // LIBRARIES_XRAY_XRAY_PRIV_H_
