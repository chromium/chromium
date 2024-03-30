// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_UTIL_H_
#define GPU_CONFIG_GPU_UTIL_H_

#include "build/build_config.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/gpu_export.h"
#include "ui/gl/gl_display.h"

namespace base {
class CommandLine;
}

namespace gpu {

struct DevicePerfInfo;
struct GPUInfo;
struct GpuPreferences;
enum class IntelGpuSeriesType;
enum class IntelGpuGeneration;

// Set GPU feature status if GPU is blocked.
GPU_EXPORT GpuFeatureInfo ComputeGpuFeatureInfoWithNoGpu();

// Set GPU feature status for SwiftShader.
GPU_EXPORT GpuFeatureInfo ComputeGpuFeatureInfoForSwiftShader();

// This function should only be called from the GPU process, or the Browser
// process while using in-process GPU. This function is safe to call at any
// point, and is not dependent on sandbox initialization.
// This function also appends a few commandline switches caused by driver bugs.
GPU_EXPORT GpuFeatureInfo
ComputeGpuFeatureInfo(const GPUInfo& gpu_info,
                      const GpuPreferences& gpu_preferences,
                      base::CommandLine* command_line,
                      bool* needs_more_info);

GPU_EXPORT void SetKeysForCrashLogging(const GPUInfo& gpu_info);

#if BUILDFLAG(IS_ANDROID)
// Cache GPUInfo so it can be accessed later.
GPU_EXPORT void CacheGPUInfo(const GPUInfo& gpu_info);

// If GPUInfo is cached, write into |gpu_info|, clear cache, and return true;
// otherwise, return false;
GPU_EXPORT bool PopGPUInfoCache(GPUInfo* gpu_info);

// Cache GpuFeatureInfo so it can be accessed later.
GPU_EXPORT void CacheGpuFeatureInfo(const GpuFeatureInfo& gpu_feature_info);

// If GpuFeatureInfo is cached, write into |gpu_feature_info|, clear cache, and
// return true; otherwise, return false;
GPU_EXPORT bool PopGpuFeatureInfoCache(GpuFeatureInfo* gpu_feature_info);

// Check if GL bindings are initialized. If not, initializes GL
// bindings, create a GL context, collects GPUInfo, make blocklist and
// GPU driver bug workaround decisions. This is intended to be called
// by Android WebView render thread and in-process GPU thread.
GPU_EXPORT gl::GLDisplay* InitializeGLThreadSafe(
    base::CommandLine* command_line,
    const GpuPreferences& gpu_preferences,
    GPUInfo* out_gpu_info,
    GpuFeatureInfo* out_gpu_feature_info);
#endif  // BUILDFLAG(IS_ANDROID)

// Returns whether SwiftShader should be enabled. If true, the proper command
// line switch to enable SwiftShader will be appended to 'command_line'.
GPU_EXPORT bool EnableSwiftShaderIfNeeded(
    base::CommandLine* command_line,
    const GpuFeatureInfo& gpu_feature_info,
    bool disable_software_rasterizer,
    bool blocklist_needs_more_info);

GPU_EXPORT IntelGpuSeriesType GetIntelGpuSeriesType(uint32_t vendor_id,
                                                    uint32_t device_id);

GPU_EXPORT std::string GetIntelGpuGeneration(uint32_t vendor_id,
                                             uint32_t device_id);

// If multiple Intel GPUs are detected, this returns the latest generation.
GPU_EXPORT IntelGpuGeneration GetIntelGpuGeneration(const GPUInfo& gpu_info);

// If this function is called in browser process (|in_browser_process| is set
// to true), don't collect total disk space (which may block) and D3D related
// info.
GPU_EXPORT void CollectDevicePerfInfo(DevicePerfInfo* device_perf_info,
                                      bool in_browser_process);
GPU_EXPORT void RecordDevicePerfInfoHistograms();

// In a multi-gpu device, record the discrete gpu device id.
// Currently only record for AMD/Nvidia GPUs.
GPU_EXPORT void RecordDiscreteGpuHistograms(const GPUInfo& gpu_info);

#if BUILDFLAG(IS_WIN)
GPU_EXPORT std::string DirectMLFeatureLevelToString(
    uint32_t directml_feature_level);
GPU_EXPORT std::string D3DFeatureLevelToString(uint32_t d3d_feature_level);
GPU_EXPORT std::string VulkanVersionToString(uint32_t vulkan_version);
#endif  // BUILDFLAG(IS_WIN)

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_UTIL_H_
