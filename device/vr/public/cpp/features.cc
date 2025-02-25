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
// Enables access to articulated hand tracking sensor input.
BASE_FEATURE(kWebXrHandInput,
             "WebXRHandInput",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables rendering to WebXR sessions with the WebGPU API.
BASE_FEATURE(kWebXrWebGpuBinding,
             "WebXRWebGPUBinding",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables access to experimental WebXR features.
BASE_FEATURE(kWebXrIncubations,
             "WebXRIncubations",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Feature flag for the WebXRInternals debugging page.
BASE_FEATURE(kWebXrInternals,
             "WebXrInternals",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables access to WebXR composition layers.
BASE_FEATURE(kWebXrLayers, "WebXRLayers", base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the orientation sensor based device is enabled.
BASE_FEATURE(kWebXrOrientationSensorDevice,
             "WebXROrientationSensorDevice",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             // TODO(https://crbug.com/820308, https://crbug.com/773829): Enable
             // once platform specific bugs have been fixed.
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_ANDROID)
// Controls whether or not SharedBuffer support is enabled. This is enabled by
// default; but some platforms (e.g. below O) cannot support the feature; while
// on other GPUs there may be quirks that prevent using the shared buffers.
BASE_FEATURE(kWebXrSharedBuffers,
             "WebXrSharedBuffers",
             base::FEATURE_ENABLED_BY_DEFAULT);

#endif

#if BUILDFLAG(ENABLE_OPENXR)
// Controls WebXR support for the OpenXR Runtime.
BASE_FEATURE(kOpenXR,
             "OpenXR",
             BUILDFLAG(IS_WIN) ? base::FEATURE_ENABLED_BY_DEFAULT
                               : base::FEATURE_DISABLED_BY_DEFAULT);

// Some WebXR features may have been enabled for ARCore, but are not yet ready
// to be plumbed up from the OpenXR backend. This feature provides a mechanism
// to gate such support in a generic way. Note that this feature should not be
// used for features we intend to ship simultaneously on both OpenXR and ArCore.
// For those features, a feature-specific flag should be created if needed.
BASE_FEATURE(kOpenXrExtendedFeatureSupport,
             "OpenXrExtendedFeatureSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether shared images are used for OpenXR Runtime
BASE_FEATURE(kOpenXRSharedImages,
             "OpenXRSharedImages",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the XrFeatureStatus.isXrDevice check is allowed to
// be used to determine if OpenXR should be enabled or not. Functionally, this
// feature is intended to be used as a kill-switch when on an xr device.
BASE_FEATURE(kAllowOpenXrOnXrDevices,
             "AllowOpenXrOnXrDevices",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kOpenXrAndroidSmoothDepth,
             "OpenXrAndroidSmoothDepth",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Helper for enabling a feature if either the base flag is enabled or if the
// device is an xr device that can have the feature enabled.
// `xr_device_feature_guard` is thus used as a kill-switch for xr devices, since
// we ignore the usual feature flag in that case.
bool IsXrFeatureEnabled(const base::Feature& base_feature,
                        const base::Feature& xr_device_feature_guard) {
  // Generally a reboot is required to change the state of a feature; so we
  // use statics rather than const's here to give a slight optimization,
  // especially in the case of `is_xr_device`.
  static bool feature_enabled = base::FeatureList::IsEnabled(base_feature);
  static bool allow_on_xr_devices =
      base::FeatureList::IsEnabled(xr_device_feature_guard);
  static bool is_xr_device = IsXrDevice();

  return feature_enabled || (allow_on_xr_devices && is_xr_device);
}

bool IsOpenXrEnabled() {
  return IsXrFeatureEnabled(kOpenXR, kAllowOpenXrOnXrDevices);
}

bool IsOpenXrArEnabled() {
  return IsOpenXrEnabled() && IsXrFeatureEnabled(kOpenXrExtendedFeatureSupport,
                                                 kAllowOpenXrOnXrDevices);
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
  return IsOpenXrEnabled() && base::FeatureList::IsEnabled(kWebXrHandInput);
#else
  return false;
#endif
}
}  // namespace device::features
