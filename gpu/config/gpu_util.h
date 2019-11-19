// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_UTIL_H_
#define GPU_CONFIG_GPU_UTIL_H_

#include "build/build_config.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/gpu_export.h"

namespace base {
class CommandLine;
}

namespace gpu {

struct GPUInfo;
struct GpuPreferences;
enum class GpuSeriesType;

// Set GPU feature status if hardware acceleration is disabled.
GPU_EXPORT GpuFeatureInfo
ComputeGpuFeatureInfoWithHardwareAccelerationDisabled();

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

#if defined(OS_ANDROID)
// Check if GL bindings are initialized. If not, initializes GL
// bindings, create a GL context, collects GPUInfo, make blacklist and
// GPU driver bug workaround decisions. This is intended to be called
// by Android WebView render thread and in-process GPU thread.
GPU_EXPORT bool InitializeGLThreadSafe(base::CommandLine* command_line,
                                       const GpuPreferences& gpu_preferences,
                                       GPUInfo* out_gpu_info,
                                       GpuFeatureInfo* out_gpu_feature_info);
#endif  // OS_ANDROID

// Returns whether SwiftShader should be enabled. If true, the proper command
// line switch to enable SwiftShader will be appended to 'command_line'.
GPU_EXPORT bool EnableSwiftShaderIfNeeded(
    base::CommandLine* command_line,
    const GpuFeatureInfo& gpu_feature_info,
    bool disable_software_rasterizer,
    bool blacklist_needs_more_info);

GPU_EXPORT GpuSeriesType GetGpuSeriesType(uint32_t vendor_id,
                                          uint32_t device_id);

GPU_EXPORT std::string GetIntelGpuGeneration(uint32_t vendor_id,
                                             uint32_t device_id);

#if defined(OS_WIN)
GPU_EXPORT std::string D3DFeatureLevelToString(uint32_t d3d_feature_level);
GPU_EXPORT std::string VulkanVersionToString(uint32_t vulkan_version);
#endif  // OS_WIN

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This should match enum VulkanVersion in \tools\metrics\histograms\enums.xml
enum class VulkanVersion {
  kVulkanVersionUnknown = 0,
  kVulkanVersion_1_0_0 = 1,
  kVulkanVersion_1_1_0 = 2,
  kMaxValue = kVulkanVersion_1_1_0,
};

GPU_EXPORT VulkanVersion
ConvertToHistogramVulkanVersion(uint32_t vulkan_version);

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_UTIL_H_
