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
#include "gpu/gpu_export.h"

namespace base {
class CommandLine;
}  // namespace base

namespace features {

GPU_EXPORT BASE_DECLARE_FEATURE(kUseGles2ForOopR);

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
GPU_EXPORT BASE_DECLARE_FEATURE(kAggressiveShaderCacheLimits);

#if BUILDFLAG(IS_ANDROID)
GPU_EXPORT BASE_DECLARE_FEATURE(kAndroidSurfaceControl);
GPU_EXPORT BASE_DECLARE_FEATURE(kWebViewSurfaceControl);
GPU_EXPORT BASE_DECLARE_FEATURE(kWebViewSurfaceControlForTV);
GPU_EXPORT BASE_DECLARE_FEATURE(kLimitAImageReaderMaxSizeToOne);
GPU_EXPORT BASE_DECLARE_FEATURE(kWebViewThreadSafeMediaDefault);
GPU_EXPORT BASE_DECLARE_FEATURE(kUseHardwareBufferUsageFlagsFromVulkan);
GPU_EXPORT BASE_DECLARE_FEATURE(
    kAllowHardwareBufferUsageFlagsFromVulkanForScanout);
#endif  // BUILDFLAG(IS_ANDROID)

GPU_EXPORT BASE_DECLARE_FEATURE(kDefaultEnableGpuRasterization);

GPU_EXPORT BASE_DECLARE_FEATURE(kEnableMSAAOnNewIntelGPUs);

#if BUILDFLAG(IS_WIN)
GPU_EXPORT BASE_DECLARE_FEATURE(kNoUndamagedOverlayPromotion);
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_IOS)
GPU_EXPORT BASE_DECLARE_FEATURE(kAdjustGpuProcessPriority);
#endif

GPU_EXPORT BASE_DECLARE_FEATURE(kClearGrShaderDiskCacheOnInvalidPrefix);

GPU_EXPORT BASE_DECLARE_FEATURE(kVaapiJpegImageDecodeAcceleration);

GPU_EXPORT BASE_DECLARE_FEATURE(kVaapiWebPImageDecodeAcceleration);

GPU_EXPORT BASE_DECLARE_FEATURE(kVulkan);

GPU_EXPORT BASE_DECLARE_FEATURE(kSkiaGraphite);
GPU_EXPORT BASE_DECLARE_FEATURE(kSkiaGraphitePrecompilation);
GPU_EXPORT extern const base::FeatureParam<bool>
    kSkiaGraphiteDawnSkipValidation;
GPU_EXPORT extern const base::FeatureParam<bool>
    kSkiaGraphiteDawnBackendValidation;
GPU_EXPORT extern const base::FeatureParam<bool>
    kSkiaGraphiteDawnBackendDebugLabels;

#if BUILDFLAG(IS_WIN)
GPU_EXPORT extern const base::FeatureParam<bool>
    kSkiaGraphiteDawnDumpWCOnD3DError;

GPU_EXPORT BASE_DECLARE_FEATURE(kSkiaGraphiteDawnUseD3D12);
#endif

GPU_EXPORT BASE_DECLARE_FEATURE(kConditionallySkipGpuChannelFlush);

GPU_EXPORT BASE_DECLARE_FEATURE(kEnableVkPipelineCache);

GPU_EXPORT BASE_DECLARE_FEATURE(kNoDiscardableMemoryForGpuDecodePath);

GPU_EXPORT BASE_DECLARE_FEATURE(kEnableDrDc);

GPU_EXPORT BASE_DECLARE_FEATURE(kPruneOldTransferCacheEntries);

#if BUILDFLAG(IS_ANDROID)
// This flag is use additionally with kEnableDrDc to enable the feature for
// vulkan enabled android devices.
GPU_EXPORT BASE_DECLARE_FEATURE(kEnableDrDcVulkan);
#endif  // BUILDFLAG(IS_ANDROID)

GPU_EXPORT BASE_DECLARE_FEATURE(kWebGPUService);
GPU_EXPORT BASE_DECLARE_FEATURE(kWebGPUBlobCache);
GPU_EXPORT BASE_DECLARE_FEATURE(kWebGPUUseTintIR);
GPU_EXPORT BASE_DECLARE_FEATURE(kWebGPUUseVulkanMemoryModel);
GPU_EXPORT extern const base::FeatureParam<std::string> kWebGPUDisabledToggles;
GPU_EXPORT extern const base::FeatureParam<std::string> kWebGPUEnabledToggles;
GPU_EXPORT extern const base::FeatureParam<std::string> kWebGPUUnsafeFeatures;
GPU_EXPORT extern const base::FeatureParam<std::string> kWGSLUnsafeFeatures;

GPU_EXPORT BASE_DECLARE_FEATURE(kIncreasedCmdBufferParseSlice);

GPU_EXPORT BASE_DECLARE_FEATURE(kDeferredOverlaysRelease);

#if BUILDFLAG(IS_WIN)
GPU_EXPORT BASE_DECLARE_FEATURE(kD3DBackingUploadWithUpdateSubresource);
#endif

GPU_EXPORT BASE_DECLARE_FEATURE(kHandleOverlaysSwapFailure);

GPU_EXPORT BASE_DECLARE_FEATURE(kGPUBlockListTestGroup);
GPU_EXPORT extern const base::FeatureParam<int> kGPUBlockListTestGroupId;
GPU_EXPORT BASE_DECLARE_FEATURE(kGPUDriverBugListTestGroup);
GPU_EXPORT extern const base::FeatureParam<int> kGPUDriverBugListTestGroupId;

GPU_EXPORT bool UseGles2ForOopR();
GPU_EXPORT bool IsUsingVulkan();
GPU_EXPORT bool IsDrDcEnabled();
GPU_EXPORT bool NeedThreadSafeAndroidMedia();
GPU_EXPORT bool IsSkiaGraphiteEnabled(const base::CommandLine* command_line);
GPU_EXPORT bool IsSkiaGraphitePrecompilationEnabled(
    const base::CommandLine* command_line);
GPU_EXPORT bool EnablePurgeGpuImageDecodeCache();
GPU_EXPORT bool EnablePruneOldTransferCacheEntries();

#if BUILDFLAG(IS_ANDROID)
GPU_EXPORT bool IsAndroidSurfaceControlEnabled();
GPU_EXPORT bool LimitAImageReaderMaxSizeToOne();
GPU_EXPORT bool IncreaseBufferCountForHighFrameRate();
GPU_EXPORT bool IncreaseBufferCountForWebViewOverlays();
#endif

GPU_EXPORT BASE_DECLARE_FEATURE(kSyncPointGraphValidation);

GPU_EXPORT bool IsSyncPointGraphValidationEnabled();

GPU_EXPORT BASE_DECLARE_FEATURE(kANGLEPerContextBlobCache);

#if BUILDFLAG(IS_APPLE)
GPU_EXPORT BASE_DECLARE_FEATURE(kIOSurfaceMultiThreading);
#endif

GPU_EXPORT BASE_DECLARE_FEATURE(kWebGPUCompatibilityMode);

GPU_EXPORT bool IsGraphiteContextThreadSafe();
}  // namespace features

#endif  // GPU_CONFIG_GPU_FINCH_FEATURES_H_
