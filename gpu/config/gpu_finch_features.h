// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the gpu
// module.

#ifndef GPU_CONFIG_GPU_FEATURES_H_
#define GPU_CONFIG_GPU_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "gpu/gpu_export.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
#if defined(OS_ANDROID)
GPU_EXPORT extern const base::Feature kUseGles2ForOopR;
GPU_EXPORT extern const base::Feature kAndroidSurfaceControl;
GPU_EXPORT extern const base::Feature kWebViewSurfaceControl;
GPU_EXPORT extern const base::Feature kAImageReader;
GPU_EXPORT extern const base::Feature kWebViewVulkan;
GPU_EXPORT extern const base::Feature kLimitAImageReaderMaxSizeToOne;
GPU_EXPORT extern const base::Feature kWebViewZeroCopyVideo;
#endif  // defined(OS_ANDROID)

GPU_EXPORT extern const base::Feature kDefaultEnableGpuRasterization;

GPU_EXPORT extern const base::Feature kDefaultEnableOopRasterization;

GPU_EXPORT extern const base::Feature kCanvasOopRasterization;

#if defined(OS_WIN)
GPU_EXPORT extern const base::Feature kGpuProcessHighPriorityWin;

GPU_EXPORT extern const base::Feature kDirectCompositionUseOverlayDamageList;
#endif

GPU_EXPORT extern const base::Feature kGpuUseDisplayThreadPriority;

#if defined(OS_MAC)
GPU_EXPORT extern const base::Feature kMetal;
#endif

GPU_EXPORT extern const base::Feature kOopRasterizationDDL;

GPU_EXPORT extern const base::Feature kSharedImageManager;

GPU_EXPORT extern const base::Feature kVaapiJpegImageDecodeAcceleration;

GPU_EXPORT extern const base::Feature kVaapiWebPImageDecodeAcceleration;

GPU_EXPORT extern const base::Feature kVulkan;

GPU_EXPORT extern const base::Feature kSkiaDawn;

GPU_EXPORT extern const base::Feature kEnableSharedImageForWebview;

GPU_EXPORT extern const base::Feature kEnableGrShaderCacheForVulkan;

GPU_EXPORT extern const base::Feature kEnableVkPipelineCache;

GPU_EXPORT bool IsUsingVulkan();
#if defined(OS_ANDROID)
GPU_EXPORT bool IsAImageReaderEnabled();
GPU_EXPORT bool IsAndroidSurfaceControlEnabled();
GPU_EXPORT bool LimitAImageReaderMaxSizeToOne();
GPU_EXPORT bool IsWebViewZeroCopyVideoEnabled();
#endif

}  // namespace features

#endif  // GPU_CONFIG_GPU_FEATURES_H_
