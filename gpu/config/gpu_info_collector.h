// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_INFO_COLLECTOR_H_
#define GPU_CONFIG_GPU_INFO_COLLECTOR_H_

#include <stdint.h>

#include "build/build_config.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/gpu_export.h"
#include "ui/gfx/gpu_extra_info.h"

#if BUILDFLAG(IS_WIN)
#include <d3dcommon.h>
#endif  // BUILDFLAG(IS_WIN)

namespace gl {
class GLDisplay;
}

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

#if BUILDFLAG(IS_WIN)
// Collect the DirectX Disagnostics information about the attached displays.
GPU_EXPORT void GetGpuSupportedDirectXVersion(
    uint32_t& d3d12_feature_level,
    uint32_t& highest_shader_model_version,
    uint32_t& directml_feature_level);
GPU_EXPORT void RecordGpuSupportedDx12VersionHistograms(
    uint32_t d3d12_feature_level,
    uint32_t highest_shader_model_version);
GPU_EXPORT uint32_t
GetGpuSupportedVulkanVersion(const gpu::GPUInfo::GPUDevice& gpu_device);

// Iterate through all adapters and create a hardware D3D11 device on each
// adapter. If succeeded, query the highest feature level it supports and
// weather it's a discrete GPU.
// Set |d3d11_feature_level| to the highest from all adapters.
// Set |is_discrete_gpu| to true if one of the adapters is discrete.
// Return false if info collection fails.
GPU_EXPORT bool CollectD3D11FeatureInfo(D3D_FEATURE_LEVEL* d3d11_feature_level,
                                        bool* has_discrete_gpu);

// Collect the hardware overlay support flags.
GPU_EXPORT void CollectHardwareOverlayInfo(OverlayInfo* overlay_info);

// Identify the active GPU based on LUIDs.
bool IdentifyActiveGPUWithLuid(GPUInfo* gpu_info);
#endif  // BUILDFLAG(IS_WIN)

// Create a GL context and collect GL strings and versions.
GPU_EXPORT bool CollectGraphicsInfoGL(GPUInfo* gpu_info,
                                      gl::GLDisplay* display);

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
GPU_EXPORT bool CollectGpuExtraInfo(gfx::GpuExtraInfo* gpu_extra_info,
                                    const GpuPreferences& prefs);

// Collect Dawn Toggle name info for about:gpu
GPU_EXPORT void CollectDawnInfo(const gpu::GpuPreferences& gpu_preferences,
                                bool collect_metrics,
                                std::vector<std::string>* dawn_info_list);

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_INFO_COLLECTOR_H_
