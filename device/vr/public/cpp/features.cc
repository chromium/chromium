// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/public/cpp/features.h"

#include "base/feature_list.h"
#include "device/vr/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_OPENXR) && BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "device/vr/public/jni_headers/XrFeatureStatus_jni.h"
#endif

namespace device::features {
// Enables rendering to WebXR sessions with the WebGPU API.
BASE_FEATURE(kWebXRWebGPUBinding, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables access to experimental WebXR features.
BASE_FEATURE(kWebXRIncubations, base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag for the WebXRInternals debugging page.
BASE_FEATURE(kWebXrInternals, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables access to WebXR composition layers.
BASE_FEATURE(kWebXRLayers, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether the orientation sensor based device is enabled.
BASE_FEATURE(kWebXROrientationSensorDevice,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             // TODO(https://crbug.com/820308, https://crbug.com/773829): Enable
             // once platform specific bugs have been fixed.
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enables access to the WebXR plane-detection feature
BASE_FEATURE(kWebXRPlaneDetection, base::FEATURE_ENABLED_BY_DEFAULT);

// Allows blink to process the `visible-blurred` state.
BASE_FEATURE(kWebXrVisibleBlurred, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(ENABLE_OPENXR)
// Controls WebXR support for the OpenXR Runtime.
BASE_FEATURE(kOpenXR,
             BUILDFLAG(IS_WIN) ? base::FEATURE_ENABLED_BY_DEFAULT
                               : base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the spatial entities framework is allowed to use depth-based
// hit tests or only plane-based ones.
BASE_FEATURE(kSpatialEntitesDepthHitTest, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kOpenXrAndroidSmoothDepth, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

bool IsOpenXrEnabled() {
  static bool is_xr_device = IsXrDevice();
  return base::FeatureList::IsEnabled(kOpenXR) || is_xr_device;
}
#endif  // ENABLE_OPENXR

bool IsXrDevice() {
#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_OPENXR)
  return device::Java_XrFeatureStatus_isXrDevice(
      base::android::AttachCurrentThread());
#else
  return false;
#endif
}

bool IsHandTrackingEnabled() {
#if BUILDFLAG(ENABLE_OPENXR)
  return IsOpenXrEnabled();
#else
  return false;
#endif
}
}  // namespace device::features

#if BUILDFLAG(ENABLE_OPENXR) && BUILDFLAG(IS_ANDROID)
DEFINE_JNI(XrFeatureStatus)
#endif
