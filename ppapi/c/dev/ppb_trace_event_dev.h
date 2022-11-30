/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From dev/ppb_trace_event_dev.idl modified Tue Jun 25 16:12:08 2013. */

#ifndef PPAPI_C_DEV_PPB_TRACE_EVENT_DEV_H_
#define PPAPI_C_DEV_PPB_TRACE_EVENT_DEV_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_TRACE_EVENT_DEV_INTERFACE_0_1 "PPB_Trace_Event(Dev);0.1"
#define PPB_TRACE_EVENT_DEV_INTERFACE_0_2 "PPB_Trace_Event(Dev);0.2"
#define PPB_TRACE_EVENT_DEV_INTERFACE PPB_TRACE_EVENT_DEV_INTERFACE_0_2

/**
 * @file
 * This file defines the <code>PPB_Trace_Event</code> interface. It is meant
 * to be used in plugins as the API that trace macros from trace_event.h use.
 */


/**
 * @addtogroup Typedefs
 * @{
 */
/**
 * A trace event timestamp.
 */
typedef int64_t PP_TraceEventTime;
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPB_Trace_Event_Dev_0_2 {
  /**
   * Gets a pointer to a character for identifying a category name in the
   * tracing system as well as for being able to early exit in client-side
   * tracing code.
   *
   * NB: This mem_t return value should technically be const, but return values
   * for Pepper IDL of mem_t type are not const.  The same is true for the arg
   * |category_enabled| for AddTraceEvent.
   */
  void* (*GetCategoryEnabled)(const char* category_name);
  /**
   * Adds a trace event to the platform tracing system. This function call is
   * usually the result of a TRACE_* macro from trace_event.h when tracing and
   * the category of the particular trace are enabled. It is not advisable to
   * call this function on its own; it is really only meant to be used by the
   * trace macros.
   */
  void (*AddTraceEvent)(int8_t phase,
                        const void* category_enabled,
                        const char* name,
                        uint64_t id,
                        uint32_t num_args,
                        const char* arg_names[],
                        const uint8_t arg_types[],
                        const uint64_t arg_values[],
                        uint8_t flags);
  /**
   * Version of the above interface that allows specifying a custom thread id
   * and timestamp. This is useful for when tracing data cannot be registered
   * in real time. For example, this could be used by storing timestamps
   * internally and then registering the events retroactively.
   */
  void (*AddTraceEventWithThreadIdAndTimestamp)(int8_t phase,
                                                const void* category_enabled,
                                                const char* name,
                                                uint64_t id,
                                                int32_t thread_id,
                                                PP_TraceEventTime timestamp,
                                                uint32_t num_args,
                                                const char* arg_names[],
                                                const uint8_t arg_types[],
                                                const uint64_t arg_values[],
                                                uint8_t flags);
  /**
   * Get the current clock value. Since this uses the same function as the trace
   * events use internally, it can be used to create events with explicit time
   * stamps.
   */
  PP_TraceEventTime (*Now)(void);
  /**
   * Sets the thread name of the calling thread in the tracing system so it will
   * show up properly in chrome://tracing.
   */
  void (*SetThreadName)(const char* thread_name);
};

typedef struct PPB_Trace_Event_Dev_0_2 PPB_Trace_Event_Dev;

struct PPB_Trace_Event_Dev_0_1 {
  void* (*GetCategoryEnabled)(const char* category_name);
  void (*AddTraceEvent)(int8_t phase,
                        const void* category_enabled,
                        const char* name,
                        uint64_t id,
                        uint32_t num_args,
                        const char* arg_names[],
                        const uint8_t arg_types[],
                        const uint64_t arg_values[],
                        uint8_t flags);
  void (*SetThreadName)(const char* thread_name);
};
/**
 * @}
 */

#endif  /* PPAPI_C_DEV_PPB_TRACE_EVENT_DEV_H_ */

