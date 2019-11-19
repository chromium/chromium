// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_INFO_COLLECTOR_H_
#define GPU_CONFIG_GPU_INFO_COLLECTOR_H_

#include <stdint.h>

#include "build/build_config.h"
#include "gpu/config/gpu_extra_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/gpu_export.h"

namespace angle {
struct SystemInfo;
}

namespace base {
class CommandLine;
}

namespace gpu {

// Collects basic GPU info without creating a GL/DirectX context (and without
// the danger of crashing), including vendor_id and device_id.
// This is called at browser process startup time.
// The subset each platform collects may be different.
GPU_EXPORT bool CollectBasicGraphicsInfo(GPUInfo* gpu_info);

// Similar to above, except it handles the case where the software renderer of
// the platform is used.
GPU_EXPORT bool CollectBasicGraphicsInfo(const base::CommandLine* command_line,
                                         GPUInfo* gpu_info);

// Create a GL/DirectX context and collect related info.
// This is called at GPU process startup time.
GPU_EXPORT bool CollectContextGraphicsInfo(GPUInfo* gpu_info);

#if defined(OS_WIN)
// Collect the DirectX Disagnostics information about the attached displays.
GPU_EXPORT bool GetDxDiagnostics(DxDiagNode* output);
GPU_EXPORT void RecordGpuSupportedRuntimeVersionHistograms(
    Dx12VulkanVersionInfo* dx12_vulkan_version_info);
#endif  // OS_WIN

// Create a GL context and collect GL strings and versions.
GPU_EXPORT bool CollectGraphicsInfoGL(GPUInfo* gpu_info);

// If more than one GPUs are identified, and GL strings are available,
// identify the active GPU based on GL strings.
GPU_EXPORT void IdentifyActiveGPU(GPUInfo* gpu_info);

// Helper function to convert data from ANGLE's system info gathering library
// into a GPUInfo
void FillGPUInfoFromSystemInfo(GPUInfo* gpu_info,
                               angle::SystemInfo* system_info);

// On Android, this calls CollectContextGraphicsInfo().
// On other platforms, this calls CollectBasicGraphicsInfo().
GPU_EXPORT void CollectGraphicsInfoForTesting(GPUInfo* gpu_info);

// Collect Graphics info related to the current process
GPU_EXPORT bool CollectGpuExtraInfo(GpuExtraInfo* gpu_extra_info);

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_INFO_COLLECTOR_H_
