// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_SWITCHES_H_
#define GPU_CONFIG_GPU_SWITCHES_H_

#include "gpu/config/gpu_config_export.h"

namespace switches {

GPU_CONFIG_EXPORT extern const char kDisableGpuRasterization[];
GPU_CONFIG_EXPORT extern const char kDisableMipmapGeneration[];
GPU_CONFIG_EXPORT extern const char kEnableGpuRasterization[];
GPU_CONFIG_EXPORT extern const char kGpuBlocklistTestGroup[];
GPU_CONFIG_EXPORT extern const char kGpuDriverBugListTestGroup[];
GPU_CONFIG_EXPORT extern const char kGpuPreferences[];
GPU_CONFIG_EXPORT extern const char kIgnoreGpuBlocklist[];
GPU_CONFIG_EXPORT extern const char kGpuDiskCacheSizeKB[];
GPU_CONFIG_EXPORT extern const char kDisableGpuShaderDiskCache[];
GPU_CONFIG_EXPORT extern const char kDisableGpuProcessForDX12InfoCollection[];
GPU_CONFIG_EXPORT extern const char kEnableUnsafeWebGPU[];
GPU_CONFIG_EXPORT extern const char kForceHighPerformanceGPU[];
GPU_CONFIG_EXPORT extern const char kEnableWebGPUDeveloperFeatures[];
GPU_CONFIG_EXPORT extern const char kEnableDawnBackendValidation[];
GPU_CONFIG_EXPORT extern const char kUseWebGPUAdapter[];
GPU_CONFIG_EXPORT extern const char kUseWebGPUPowerPreference[];
GPU_CONFIG_EXPORT extern const char kForceWebGPUCompat[];
GPU_CONFIG_EXPORT extern const char kEnableDawnFeatures[];
GPU_CONFIG_EXPORT extern const char kDisableDawnFeatures[];
GPU_CONFIG_EXPORT extern const char kCollectDawnInfoEagerly[];
GPU_CONFIG_EXPORT extern const char kNoDelayForDX12VulkanInfoCollection[];
GPU_CONFIG_EXPORT extern const char kEnableGpuBlockedTime[];
GPU_CONFIG_EXPORT extern const char kGpuVendorId[];
GPU_CONFIG_EXPORT extern const char kGpuDeviceId[];
GPU_CONFIG_EXPORT extern const char kGpuSubSystemId[];
GPU_CONFIG_EXPORT extern const char kGpuRevision[];
GPU_CONFIG_EXPORT extern const char kGpuDriverVersion[];
GPU_CONFIG_EXPORT extern const char kWebViewDrawFunctorUsesVulkan[];
GPU_CONFIG_EXPORT extern const char kEnableVulkanProtectedMemory[];
GPU_CONFIG_EXPORT extern const char kDisableVulkanFallbackToGLForTesting[];
GPU_CONFIG_EXPORT extern const char kVulkanHeapMemoryLimitMb[];
GPU_CONFIG_EXPORT extern const char kVulkanSyncCpuMemoryLimitMb[];
GPU_CONFIG_EXPORT extern const char kForceBrowserCrashOnGpuCrash[];
GPU_CONFIG_EXPORT extern const char kGpuWatchdogTimeoutSeconds[];
GPU_CONFIG_EXPORT extern const char kForceSeparateEGLDisplayForWebGLTesting[];
GPU_CONFIG_EXPORT extern const char kSkiaGraphiteBackend[];
GPU_CONFIG_EXPORT extern const char kSkiaGraphiteBackendDawn[];
GPU_CONFIG_EXPORT extern const char kSkiaGraphiteBackendDawnD3D11[];
GPU_CONFIG_EXPORT extern const char kSkiaGraphiteBackendDawnD3D12[];
GPU_CONFIG_EXPORT extern const char kSkiaGraphiteBackendDawnMetal[];
GPU_CONFIG_EXPORT extern const char kSkiaGraphiteBackendDawnOpenGLES[];
GPU_CONFIG_EXPORT extern const char kSkiaGraphiteBackendDawnSwiftshader[];
GPU_CONFIG_EXPORT extern const char kSkiaGraphiteBackendDawnVulkan[];
GPU_CONFIG_EXPORT extern const char kSkiaGraphiteBackendMetal[];
GPU_CONFIG_EXPORT extern const char kDisableSkiaGraphite[];
GPU_CONFIG_EXPORT extern const char kEnableSkiaGraphite[];
GPU_CONFIG_EXPORT extern const char kDisableSkiaGraphitePrecompilation[];
GPU_CONFIG_EXPORT extern const char kEnableSkiaGraphitePrecompilation[];
GPU_CONFIG_EXPORT extern const char kUseRedistributableDirectML[];
GPU_CONFIG_EXPORT extern const char kEnableGpuMainTimeKeeperMetrics[];
GPU_CONFIG_EXPORT extern const char kSuppressPerformanceLogs[];

}  // namespace switches

#endif  // GPU_CONFIG_GPU_SWITCHES_H_
