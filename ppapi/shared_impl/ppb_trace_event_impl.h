// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_TRACE_EVENT_IMPL_H_
#define PPAPI_SHARED_IMPL_PPB_TRACE_EVENT_IMPL_H_

#include <stdint.h>

#include "ppapi/c/dev/ppb_trace_event_dev.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

// Contains the implementation of the PPB_Trace_Event_Dev functions. Since these
// functions are to be run from whatever plugin process/thread in which they
// originated, the implementation lives in shared_impl.
//
class PPAPI_SHARED_EXPORT TraceEventImpl {
 public:
  static void* GetCategoryEnabled(const char* category_name);
  static void AddTraceEvent(int8_t phase,
                            const void* category_enabled,
                            const char* name,
                            uint64_t id,
                            uint32_t num_args,
                            const char* arg_names[],
                            const uint8_t arg_types[],
                            const uint64_t arg_values[],
                            uint8_t flags);
  static void AddTraceEventWithThreadIdAndTimestamp(
      int8_t phase,
      const void* category_enabled,
      const char* name,
      uint64_t id,
      int32_t thread_id,
      int64_t timestamp,
      uint32_t num_args,
      const char* arg_names[],
      const uint8_t arg_types[],
      const uint64_t arg_values[],
      uint8_t flags);
  static int64_t Now();
  static void SetThreadName(const char* thread_name);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_TRACE_EVENT_IMPL_H_
