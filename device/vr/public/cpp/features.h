// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_CPP_FEATURES_H_
#define DEVICE_VR_PUBLIC_CPP_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "device/vr/buildflags/buildflags.h"

namespace device::features {
COMPONENT_EXPORT(VR_FEATURES) BASE_DECLARE_FEATURE(kWebXrHandInput);
COMPONENT_EXPORT(VR_FEATURES) BASE_DECLARE_FEATURE(kWebXrIncubations);
COMPONENT_EXPORT(VR_FEATURES) BASE_DECLARE_FEATURE(kWebXrInternals);
COMPONENT_EXPORT(VR_FEATURES) BASE_DECLARE_FEATURE(kWebXrLayers);
COMPONENT_EXPORT(VR_FEATURES)
BASE_DECLARE_FEATURE(kWebXrOrientationSensorDevice);

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(VR_FEATURES) BASE_DECLARE_FEATURE(kWebXrSharedBuffers);
COMPONENT_EXPORT(VR_FEATURES)
BASE_DECLARE_FEATURE(kUseTargetTexture2DForSharedBuffers);
#endif

#if BUILDFLAG(ENABLE_OPENXR)
// Note that this feature can be overridden by logic contained within
// `IsOpenXrEnabled` and therefore should generally not be queried directly.
COMPONENT_EXPORT(VR_FEATURES) BASE_DECLARE_FEATURE(kOpenXR);
// Note that this feature can be overridden by logic contained within
// `IsOpenXrArEnabled` and therefore should generally not be queried directly.
COMPONENT_EXPORT(VR_FEATURES)
BASE_DECLARE_FEATURE(kOpenXrExtendedFeatureSupport);
COMPONENT_EXPORT(VR_FEATURES) BASE_DECLARE_FEATURE(kOpenXRSharedImages);
COMPONENT_EXPORT(VR_FEATURES)
BASE_DECLARE_FEATURE(kAllowOpenXrWithImmersiveFeature);

// Helper method to check if OpenXR should be enabled. It takes into account
// both the kOpenXR feature, as well as the state of the system features on
// Android and the `kAllowOpenXrWithImmersiveFeature` flag, and should be
// checked instead of a direct query on the kOpenXR feature being enabled.
COMPONENT_EXPORT(VR_FEATURES) bool IsOpenXrEnabled();

// Helper method to check if OpenXR AR should be enabled. It takes into account
// both the `kOpenXrExtendedFeatureSupport` feature, as well as the state of the
// system features on Android and the `kAllowOpenXrWithImmersiveFeature` flag,
// and should be checked instead of a direct query on the kOpenXR feature being
// enabled.
COMPONENT_EXPORT(VR_FEATURES) bool IsOpenXrArEnabled();

#endif  // ENABLE_OPENXR
COMPONENT_EXPORT(VR_FEATURES) bool HasImmersiveFeature();

COMPONENT_EXPORT(VR_FEATURES) bool IsHandTrackingEnabled();

}  // namespace device::features

#endif  // DEVICE_VR_PUBLIC_CPP_FEATURES_H_
