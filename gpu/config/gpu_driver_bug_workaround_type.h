// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_DRIVER_BUG_WORKAROUND_TYPE_H_
#define GPU_CONFIG_GPU_DRIVER_BUG_WORKAROUND_TYPE_H_

#include <string>

#include "gpu/config/gpu_driver_bug_workaround_autogen.h"
#include "gpu/gpu_export.h"

namespace gpu {

// Provides all types of GPU driver bug workarounds.
enum GpuDriverBugWorkaroundType {
#define GPU_OP(type, name) type,
  GPU_DRIVER_BUG_WORKAROUNDS(GPU_OP)
#undef GPU_OP
  NUMBER_OF_GPU_DRIVER_BUG_WORKAROUND_TYPES
};

GPU_EXPORT std::string GpuDriverBugWorkaroundTypeToString(
    GpuDriverBugWorkaroundType type);

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_DRIVER_BUG_WORKAROUND_TYPE_H_
