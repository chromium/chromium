// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "gpu/config/gpu_finch_features.h"

namespace features {

// Enable GPU Rasterization by default. This can still be overridden by
// --force-gpu-rasterization or --disable-gpu-rasterization.
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_CHROMEOS) || \
    defined(OS_ANDROID)
// DefaultEnableGpuRasterization has launched on Mac, Windows, ChromeOS, and
// Android.
const base::Feature kDefaultEnableGpuRasterization{
    "DefaultEnableGpuRasterization", base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kDefaultEnableGpuRasterization{
    "DefaultEnableGpuRasterization", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Enable out of process rasterization by default.  This can still be overridden
// by --enable-oop-rasterization or --disable-oop-rasterization.
const base::Feature kDefaultEnableOopRasterization{
    "DefaultEnableOopRasterization", base::FEATURE_DISABLED_BY_DEFAULT};

// Use the passthrough command decoder by default.  This can be overridden with
// the --use-cmd-decoder=passthrough or --use-cmd-decoder=validating flags.
const base::Feature kDefaultPassthroughCommandDecoder{
    "DefaultPassthroughCommandDecoder", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Use android AImageReader when playing videos with MediaPlayer.
const base::Feature kAImageReaderMediaPlayer{"AImageReaderMediaPlayer",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Use android SurfaceControl API for managing display compositor's buffer queue
// and using overlays on Android.
// Note that the feature only works with VizDisplayCompositor enabled.
const base::Feature kAndroidSurfaceControl{"AndroidSurfaceControl",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Overrides preferred overlay format to NV12 instead of YUY2.
const base::Feature kDirectCompositionPreferNV12Overlays{
    "DirectCompositionPreferNV12Overlays", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls the decode acceleration of JPEG images (as opposed to camera
// captures) in Chrome OS using the VA-API.
// TODO(andrescj): remove or enable by default in Chrome OS once
// https://crbug.com/868400 is resolved.
const base::Feature kVaapiJpegImageDecodeAcceleration{
    "VaapiJpegImageDecodeAcceleration", base::FEATURE_DISABLED_BY_DEFAULT};

// Causes us to use the SharedImageManager, removing support for the old
// mailbox system. Any consumers of the GPU process using the old mailbox
// system will experience undefined results.
const base::Feature kSharedImageManager{"SharedImageManager",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
