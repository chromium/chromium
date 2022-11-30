// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_SWITCHING_H_
#define GPU_CONFIG_GPU_SWITCHING_H_

#include <stdint.h>

#include <vector>

#include "build/build_config.h"
#include "gpu/gpu_export.h"

namespace base {
class CommandLine;
}

namespace gpu {

struct GPUInfo;

// Returns true if GPU dynamic switching inside Chrome is supported.
// Currently it's only for Mac with switchable dual GPUs.
GPU_EXPORT bool SwitchableGPUsSupported(const GPUInfo& gpu_info,
                                        const base::CommandLine& command_line);

// Depending on the GPU driver bug workarounds, if needed, force onto the
// discrete GPU or try best to stay on the integrated GPU.
// This should only be called if SwitchableGPUsSupported() returns true.
GPU_EXPORT void InitializeSwitchableGPUs(
    const std::vector<int32_t>& driver_bug_workarounds);

// Destroy the CGLPixelFormatObj that's used to force discrete GPU.
GPU_EXPORT void StopForceDiscreteGPU();

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_SWITCHING_H_
