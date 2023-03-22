// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the gpu
// module.

#ifndef GPU_CONFIG_GPU_FINCH_FEATURES_H_
#define GPU_CONFIG_GPU_FINCH_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "gpu/gpu_export.h"

namespace features {

GPU_EXPORT BASE_DECLARE_FEATURE(kUseGles2ForOopR);

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
#if BUILDFLAG(IS_ANDROID)
GPU_EXPORT BASE_DECLARE_FEATURE(kAndroidSurfaceControl);
GPU_EXPORT BASE_DECLARE_FEATURE(kWebViewSurfaceControl);
GPU_EXPORT BASE_DECLARE_FEATURE(kAImageReader);
GPU_EXPORT BASE_DECLARE_FEATURE(kWebViewVulkan);
GPU_EXPORT BASE_DECLARE_FEATURE(kLimitAImageReaderMaxSizeToOne);
GPU_EXPORT BASE_DECLARE_FEATURE(kWebViewThreadSafeMediaDefault);
GPU_EXPORT BASE_DECLARE_FEATURE(kIncreaseBufferCountForHighFrameRate);
#endif  // BUILDFLAG(IS_ANDROID)

GPU_EXPORT BASE_DECLARE_FEATURE(kDefaultEnableGpuRasterization);

GPU_EXPORT BASE_DECLARE_FEATURE(kCanvasOopRasterization);

GPU_EXPORT BASE_DECLARE_FEATURE(kEnableMSAAOnNewIntelGPUs);

GPU_EXPORT BASE_DECLARE_FEATURE(kDefaultEnableANGLEValidation);

GPU_EXPORT BASE_DECLARE_FEATURE(kCanvasContextLostInBackground);

#if BUILDFLAG(IS_WIN)
GPU_EXPORT BASE_DECLARE_FEATURE(kGpuProcessHighPriorityWin);

GPU_EXPORT BASE_DECLARE_FEATURE(kDisableVideoOverlayIfMoving);

GPU_EXPORT BASE_DECLARE_FEATURE(kNoUndamagedOverlayPromotion);

GPU_EXPORT BASE_DECLARE_FEATURE(kDCompPresenter);
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_IOS)
GPU_EXPORT BASE_DECLARE_FEATURE(kMetal);

GPU_EXPORT BASE_DECLARE_FEATURE(kAdjustGpuProcessPriority);
#endif

GPU_EXPORT BASE_DECLARE_FEATURE(kSharedImageManager);

GPU_EXPORT BASE_DECLARE_FEATURE(kVaapiJpegImageDecodeAcceleration);

GPU_EXPORT BASE_DECLARE_FEATURE(kVaapiWebPImageDecodeAcceleration);

GPU_EXPORT BASE_DECLARE_FEATURE(kVulkan);

GPU_EXPORT BASE_DECLARE_FEATURE(kSkiaDawn);

GPU_EXPORT BASE_DECLARE_FEATURE(kEnableGrShaderCacheForVulkan);

GPU_EXPORT BASE_DECLARE_FEATURE(kEnableWatchdogReportOnlyModeOnGpuInit);

GPU_EXPORT BASE_DECLARE_FEATURE(kEnableVkPipelineCache);

GPU_EXPORT BASE_DECLARE_FEATURE(kReduceOpsTaskSplitting);

GPU_EXPORT BASE_DECLARE_FEATURE(kNoDiscardableMemoryForGpuDecodePath);

GPU_EXPORT BASE_DECLARE_FEATURE(kEnableDrDc);

GPU_EXPORT BASE_DECLARE_FEATURE(kForceGpuMainThreadToNormalPriorityDrDc);

GPU_EXPORT BASE_DECLARE_FEATURE(kForceRestartGpuKillSwitch);

GPU_EXPORT BASE_DECLARE_FEATURE(kUseGpuSchedulerDfs);

#if BUILDFLAG(IS_ANDROID)
// This flag is use additionally with kEnableDrDc to enable the feature for
// vulkan enabled android devices.
GPU_EXPORT BASE_DECLARE_FEATURE(kEnableDrDcVulkan);
#endif  // BUILDFLAG(IS_ANDROID)

GPU_EXPORT BASE_DECLARE_FEATURE(kWebGPUService);

GPU_EXPORT BASE_DECLARE_FEATURE(kIncreasedCmdBufferParseSlice);

GPU_EXPORT BASE_DECLARE_FEATURE(kPassthroughYuvRgbConversion);

GPU_EXPORT BASE_DECLARE_FEATURE(kCmdDecoderAlwaysGetSizeFromSourceTexture);

GPU_EXPORT BASE_DECLARE_FEATURE(kGpuCleanupInBackground);

GPU_EXPORT bool UseGles2ForOopR();
GPU_EXPORT bool IsUsingVulkan();
GPU_EXPORT bool IsDrDcEnabled();
GPU_EXPORT bool IsGpuMainThreadForcedToNormalPriorityDrDc();
GPU_EXPORT bool NeedThreadSafeAndroidMedia();
GPU_EXPORT bool IsANGLEValidationEnabled();

#if BUILDFLAG(IS_ANDROID)
GPU_EXPORT bool IsAImageReaderEnabled();
GPU_EXPORT bool IsAndroidSurfaceControlEnabled();
GPU_EXPORT bool LimitAImageReaderMaxSizeToOne();
GPU_EXPORT bool IncreaseBufferCountForHighFrameRate();
GPU_EXPORT bool IncreaseBufferCountForWebViewOverlays();
#endif

}  // namespace features

#endif  // GPU_CONFIG_GPU_FINCH_FEATURES_H_
