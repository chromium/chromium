// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_SCHEDULING_PRIORITY_H_
#define GPU_COMMAND_BUFFER_COMMON_SCHEDULING_PRIORITY_H_

#include "gpu/gpu_export.h"

namespace gpu {

enum class SchedulingPriority {
  // The High priority can be used by priveleged clients only. This priority is
  // used for UI contexts and by the scheduler for prioritizing contexts which
  // have outstanding sync token waits or client side waits.
  kHigh,
  // The following priorities can be used on unprivileged clients.
  // This priority is used as the default priority for all contexts.
  kNormal,
  // This priority is used for worker contexts.
  kLow,
  kLast = kLow
};

GPU_EXPORT const char* SchedulingPriorityToString(SchedulingPriority priority);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_SCHEDULING_PRIORITY_H_
