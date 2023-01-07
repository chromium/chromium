/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/* XRay -- a simple profiler for Native Client */


#ifndef LIBRARIES_XRAY_XRAY_H_
#define LIBRARIES_XRAY_XRAY_H_

#include <stdint.h>

#ifndef XRAY_DISABLE_BROWSER_INTEGRATION
#include "ppapi/c/ppb.h"
#endif

#if defined(__arm__)
#undef XRAY
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define XRAY_NO_INSTRUMENT  __attribute__((no_instrument_function))
#define XRAY_INLINE __attribute__((always_inline, no_instrument_function))

#if defined(XRAY)

/* Do not call __XRayAnnotate* directly; instead use the */
/* XRayAnnotate() macros below. */
XRAY_NO_INSTRUMENT void __XRayAnnotate(const char* str, ...)
  __attribute__ ((format(printf, 1, 2)));
XRAY_NO_INSTRUMENT void __XRayAnnotateFiltered(const uint32_t filter,
  const char* str, ...) __attribute__ ((format(printf, 2, 3)));

/* This is the beginning of the public XRay API */

/* Ok if mapfilename is NULL, no symbols will be loaded.  On glibc builds,
 * XRay will also attempt to populate the symbol table with dladdr()
 */
XRAY_NO_INSTRUMENT struct XRayTraceCapture* XRayInit(int stack_size,
                                                     int buffer_size,
                                                     int frame_count,
                                                     const char* mapfilename);
XRAY_NO_INSTRUMENT void XRayShutdown(struct XRayTraceCapture* capture);
XRAY_NO_INSTRUMENT void XRayStartFrame(struct XRayTraceCapture* capture);
XRAY_NO_INSTRUMENT void XRayEndFrame(struct XRayTraceCapture* capture);
XRAY_NO_INSTRUMENT void XRaySetAnnotationFilter(
    struct XRayTraceCapture* capture, uint32_t filter);
XRAY_NO_INSTRUMENT void XRaySaveReport(struct XRayTraceCapture* capture,
                                       const char* filename,
                                       float percent_cutoff,
                                       int cycle_cutoff);
XRAY_NO_INSTRUMENT void XRayReport(struct XRayTraceCapture* capture,
                                   FILE* f,
                                   float percent_cutoff,
                                   int ticks_cutoff);

#ifndef XRAY_DISABLE_BROWSER_INTEGRATION
XRAY_NO_INSTRUMENT void XRayBrowserTraceReport(
    struct XRayTraceCapture* capture);
XRAY_NO_INSTRUMENT void XRayRegisterBrowserInterface(
    PPB_GetInterface get_browser_interface);
#endif  /* XRAY_DISABLE_BROWSER_INTEGRATION */


#if defined(XRAY_ANNOTATE)
#define XRayAnnotate(...) __XRayAnnotate(__VA_ARGS__)
#define XRayAnnotateFiltered(...) __XRayAnnotateFiltered(__VA_ARGS__)
#else
#define XRayAnnotate(...)
#define XRayAnnotateFiltered(...)
#endif
/* This is the end of the public XRay API */

#else  /* defined(XRAY) */

/* Builds that don't define XRAY will use these 'null' functions instead. */

#define XRayAnnotate(...)
#define XRayAnnotateFiltered(...)

inline struct XRayTraceCapture* XRayInit(int stack_size,
                                         int buffer_size,
                                         int frame_count,
                                         const char* mapfilename) {
  return NULL;
}
inline void XRayShutdown(struct XRayTraceCapture* capture) {}
inline void XRayStartFrame(struct XRayTraceCapture* capture) {}
inline void XRayEndFrame(struct XRayTraceCapture* capture) {}
inline void XRaySetAnnotationFilter(struct XRayTraceCapture* capture,
                                    uint32_t filter) {}
inline void XRaySaveReport(struct XRayTraceCapture* capture,
                           const char* filename,
                           float percent_cutoff,
                           int cycle_cutoff) {}
inline void XRayReport(struct XRayTraceCapture* capture,
                       FILE* f,
                       float percent_cutoff,
                       int ticks_cutoff) {}

#ifndef XRAY_DISABLE_BROWSER_INTEGRATION
inline void XRayBrowserTraceReport(struct XRayTraceCapture* capture) {}
inline void XRayRegisterBrowserInterface(
    PPB_GetInterface get_browser_interface) {}
#endif  /* XRAY_DISABLE_BROWSER_INTEGRATION */


#endif  /* defined(XRAY) */

#ifdef __cplusplus
}
#endif

#endif  // LIBRARIES_XRAY_XRAY_H_
