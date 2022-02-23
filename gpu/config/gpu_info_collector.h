// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

namespace angle {
struct SystemInfo;
}

namespace base {
class CommandLine;
}

namespace gpu {

#if BUILDFLAG(IS_WIN)
// TODO(magchen@): Remove D3D_FEATURE_LEVEL_CHROMIUM and
// D3D_SHADER_MODEL_CHROMIUM and use D3D_FEATURE_LEVEL directly once the Windows
// Kits is updated from version 19041 to a newer version 20170 or later.
// D3D_FEATURE_LEVEL is defined in
// third_party\depot_tools\win_toolchain\vs_files\ 20d5f2553f\Windows
// Kits\10\Include\10.0.19041.0\um\d3dcommon.h

// This is a temporary solution for adding D3D_FEATURE_LEVEL_12_2 to D3D12 API.
// Do not use enum D3D_FEATURE_LEVEL_CHROMIUM for D3D11 now. The support for
// D3D_FEATURE_LEVEL_12_2 has not been surfaced through D3D11 API.

typedef enum D3D_FEATURE_LEVEL_CHROMIUM {
  D3D12_FEATURE_LEVEL_1_0_CORE = 0x1000,
  D3D12_FEATURE_LEVEL_9_1 = 0x9100,
  D3D12_FEATURE_LEVEL_9_2 = 0x9200,
  D3D12_FEATURE_LEVEL_9_3 = 0x9300,
  D3D12_FEATURE_LEVEL_10_0 = 0xa000,
  D3D12_FEATURE_LEVEL_10_1 = 0xa100,
  D3D12_FEATURE_LEVEL_11_0 = 0xb000,
  D3D12_FEATURE_LEVEL_11_1 = 0xb100,
  D3D12_FEATURE_LEVEL_12_0 = 0xc000,
  D3D12_FEATURE_LEVEL_12_1 = 0xc100,
  D3D12_FEATURE_LEVEL_12_2 = 0xc200,
} D3D_FEATURE_LEVEL_CHROMIUM;

#endif  // BUILDFLAG(IS_WIN)

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
GPU_EXPORT bool GetDxDiagnostics(DxDiagNode* output);
GPU_EXPORT void GetGpuSupportedD3D12Version(
    uint32_t& d3d12_feature_level,
    uint32_t& highest_shader_model_version);
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
GPU_EXPORT bool CollectGpuExtraInfo(gfx::GpuExtraInfo* gpu_extra_info,
                                    const GpuPreferences& prefs);

// Collect Dawn Toggle name info for about:gpu
GPU_EXPORT void CollectDawnInfo(const gpu::GpuPreferences& gpu_preferences,
                                std::vector<std::string>* dawn_info_list);

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_INFO_COLLECTOR_H_
