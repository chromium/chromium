// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_platform.h"

#include "base/trace_event/trace_arguments.h"
#include "base/trace_event/trace_event.h"

namespace gpu {
namespace webgpu {

DawnPlatform::DawnPlatform() = default;

DawnPlatform::~DawnPlatform() = default;

const unsigned char* DawnPlatform::GetTraceCategoryEnabledFlag(
    dawn_platform::TraceCategory category) {
  // For now, all Dawn trace categories are put under "gpu.dawn"
  return TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("gpu.dawn"));
}

double DawnPlatform::MonotonicallyIncreasingTime() {
  return (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
}

uint64_t DawnPlatform::AddTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    uint64_t id,
    double timestamp,
    int num_args,
    const char** arg_names,
    const unsigned char* arg_types,
    const uint64_t* arg_values,
    unsigned char flags) {
  base::TimeTicks timestamp_tt =
      base::TimeTicks() + base::TimeDelta::FromSecondsD(timestamp);

  base::trace_event::TraceArguments args(
      num_args, arg_names, arg_types,
      reinterpret_cast<const unsigned long long*>(arg_values));

  base::trace_event::TraceEventHandle handle =
      TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP(
          phase, category_group_enabled, name,
          trace_event_internal::kGlobalScope, id, trace_event_internal::kNoId,
          base::PlatformThread::CurrentId(), timestamp_tt, &args, flags);

  uint64_t result = 0;
  static_assert(sizeof(base::trace_event::TraceEventHandle) <= sizeof(result),
                "TraceEventHandle must be at most the size of uint64_t");
  static_assert(std::is_pod<base::trace_event::TraceEventHandle>(),
                "TraceEventHandle must be memcpy'able");
  memcpy(&result, &handle, sizeof(base::trace_event::TraceEventHandle));
  return result;
}

}  // namespace webgpu
}  // namespace gpu
