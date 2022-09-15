// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_GPU_STREAM_CONSTANTS_H_
#define CONTENT_PUBLIC_COMMON_GPU_STREAM_CONSTANTS_H_

#include "gpu/command_buffer/common/scheduling_priority.h"

namespace content {

enum {
  kGpuStreamIdDefault,
  kGpuStreamIdWorker,
  kGpuStreamIdMedia,
};

// Used for renderer compositor thread context, WebGL (when high priority is
// not requested by workaround), canvas, etc.
const gpu::SchedulingPriority kGpuStreamPriorityDefault =
    gpu::SchedulingPriority::kNormal;

// Used for UI context and all other browser contexts in the same stream.
const gpu::SchedulingPriority kGpuStreamPriorityUI =
    gpu::SchedulingPriority::kHigh;

// Used for renderer video media context.
const gpu::SchedulingPriority kGpuStreamPriorityMedia =
    gpu::SchedulingPriority::kHigh;

// Used for renderer raster worker context.
const gpu::SchedulingPriority kGpuStreamPriorityWorker =
    gpu::SchedulingPriority::kLow;

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_GPU_STREAM_CONSTANTS_H_
