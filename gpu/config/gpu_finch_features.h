// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the gpu
// module.

#ifndef GPU_CONFIG_GPU_FINCH_FEATURES_H_
#define GPU_CONFIG_GPU_FINCH_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "gpu/config/gpu_config_export.h"

namespace base {
class CommandLine;
}  // namespace base

namespace gpu {
struct GpuFeatureInfo;
}  // namespace gpu

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kAggressiveShaderCacheLimits);

#if BUILDFLAG(IS_ANDROID)
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kAndroidSurfaceControl);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebViewSurfaceControl);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebViewSurfaceControlForTV);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kLimitAImageReaderMaxSizeToOne);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebViewThreadSafeMediaDefault);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kUseHardwareBufferUsageFlagsFromVulkan);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(
    kAllowHardwareBufferUsageFlagsFromVulkanForScanout);
#endif  // BUILDFLAG(IS_ANDROID)

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kDefaultEnableGpuRasterization);

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kEnableMSAAOnNewIntelGPUs);

#if BUILDFLAG(IS_WIN)
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kNoUndamagedOverlayPromotion);
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_IOS)
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kAdjustGpuProcessPriority);
#endif

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kClearGrShaderDiskCacheOnInvalidPrefix);

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kGpuShaderDiskCache);
GPU_CONFIG_EXPORT bool IsShaderDiskCacheEnabled(
    const base::CommandLine* command_line);

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kVulkan);

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kSkiaGraphite);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kSkiaGraphitePrecompilation);
GPU_CONFIG_EXPORT extern const base::FeatureParam<bool>
    kSkiaGraphiteDawnSkipValidation;
GPU_CONFIG_EXPORT extern const base::FeatureParam<bool>
    kSkiaGraphiteDawnBackendValidation;
GPU_CONFIG_EXPORT extern const base::FeatureParam<bool>
    kSkiaGraphiteDawnBackendDebugLabels;
GPU_CONFIG_EXPORT extern const base::FeatureParam<bool>
    kSkiaGraphiteDawnUsePersistentCache;
GPU_CONFIG_EXPORT extern const base::FeatureParam<int>
    kSkiaGraphiteMaxPendingRecordings;
GPU_CONFIG_EXPORT extern const base::FeatureParam<bool>
    kSkiaGraphiteEnableDeferredSubmit;
GPU_CONFIG_EXPORT extern const base::FeatureParam<bool>
    kSkiaGraphiteEnableMSAAOnNewerIntel;

#if BUILDFLAG(IS_WIN)
GPU_CONFIG_EXPORT extern const base::FeatureParam<bool>
    kSkiaGraphiteDawnDumpWCOnD3DError;
GPU_CONFIG_EXPORT extern const base::FeatureParam<bool>
    kSkiaGraphiteDawnDisableD3DShaderOptimizations;
GPU_CONFIG_EXPORT extern const base::FeatureParam<bool>
    kSkiaGraphiteDawnD3D11DelayFlush;

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kSkiaGraphiteDawnUseD3D12);
#endif

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kSkiaGraphiteSmallPathAtlas);
GPU_CONFIG_EXPORT extern const base::FeatureParam<int>
    kSkiaGraphiteMinPathSizeForMsaa;

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kGpuPersistentCache);

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kConditionallySkipGpuChannelFlush);

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kNoDiscardableMemoryForGpuDecodePath);

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kEnableDrDc);

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kPruneOldTransferCacheEntries);

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kRemoveGPULegacyIPC);

#if BUILDFLAG(IS_ANDROID)
// This flag is use additionally with kEnableDrDc to enable the feature for
// vulkan enabled android devices.
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kEnableDrDcVulkan);
#endif  // BUILDFLAG(IS_ANDROID)

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebGPUService);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kAAPMBlocksWebGPU);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebGPUBlobCache);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebGPUEnableRangeAnalysisForRobustness);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebGPUAndroidOpenGLES);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebGPUUseSpirv14);
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebGPUDecomposeUniformBuffers);
#if BUILDFLAG(IS_WIN)
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebGPUQualcommWindows);
#endif
GPU_CONFIG_EXPORT extern const base::FeatureParam<std::string>
    kWebGPUDisabledToggles;
GPU_CONFIG_EXPORT extern const base::FeatureParam<std::string>
    kWebGPUEnabledToggles;
GPU_CONFIG_EXPORT extern const base::FeatureParam<std::string>
    kWebGPUUnsafeFeatures;
GPU_CONFIG_EXPORT extern const base::FeatureParam<bool>
    kWebGPUSpontaneousWireServer;
GPU_CONFIG_EXPORT extern const base::FeatureParam<std::string>
    kWGSLUnsafeFeatures;

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kIncreasedCmdBufferParseSlice);

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kDeferredOverlaysRelease);

#if BUILDFLAG(IS_WIN)
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kD3DBackingUploadWithUpdateSubresource);
#endif

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kHandleOverlaysSwapFailure);

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kGPUBlockListTestGroup);
GPU_CONFIG_EXPORT extern const base::FeatureParam<int> kGPUBlockListTestGroupId;
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kGPUDriverBugListTestGroup);
GPU_CONFIG_EXPORT extern const base::FeatureParam<int>
    kGPUDriverBugListTestGroupId;

GPU_CONFIG_EXPORT bool IsUsingVulkan();
GPU_CONFIG_EXPORT bool IsDrDcEnabled(
    const gpu::GpuFeatureInfo& gpu_feature_info);
GPU_CONFIG_EXPORT bool ShouldEnableDrDc();
GPU_CONFIG_EXPORT bool NeedThreadSafeAndroidMedia();
GPU_CONFIG_EXPORT bool IsSkiaGraphiteEnabled(
    const base::CommandLine* command_line);
GPU_CONFIG_EXPORT bool IsSkiaGraphitePrecompilationEnabled(
    const base::CommandLine* command_line);
GPU_CONFIG_EXPORT bool EnablePurgeGpuImageDecodeCache();
GPU_CONFIG_EXPORT bool EnablePruneOldTransferCacheEntries();
GPU_CONFIG_EXPORT bool IsLegacyIpcDisabled();

#if BUILDFLAG(IS_ANDROID)
GPU_CONFIG_EXPORT bool IsAndroidSurfaceControlEnabled();
GPU_CONFIG_EXPORT bool LimitAImageReaderMaxSizeToOne();
GPU_CONFIG_EXPORT bool IncreaseBufferCountForHighFrameRate();
GPU_CONFIG_EXPORT bool IncreaseBufferCountForWebViewOverlays();
#endif

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kSyncPointGraphValidation);

GPU_CONFIG_EXPORT bool IsSyncPointGraphValidationEnabled();

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kANGLEPerContextBlobCache);

#if BUILDFLAG(IS_APPLE)
GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kIOSurfaceMultiThreading);
#endif

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kConfigurableGPUWatchdogTimeout);
GPU_CONFIG_EXPORT extern const base::FeatureParam<int>
    kConfigurableGPUWatchdogTimeoutSeconds;

GPU_CONFIG_EXPORT BASE_DECLARE_FEATURE(kWebGPUCompatibilityMode);

GPU_CONFIG_EXPORT bool IsGraphiteContextThreadSafe();
}  // namespace features

#endif  // GPU_CONFIG_GPU_FINCH_FEATURES_H_
