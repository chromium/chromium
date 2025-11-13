// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_CPP_FEATURES_H_
#define DEVICE_VR_PUBLIC_CPP_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "device/vr/buildflags/buildflags.h"

namespace device::features {
COMPONENT_EXPORT(VR_FEATURES) BASE_DECLARE_FEATURE(kWebXRWebGPUBinding);
COMPONENT_EXPORT(VR_FEATURES) BASE_DECLARE_FEATURE(kWebXRIncubations);
COMPONENT_EXPORT(VR_FEATURES) BASE_DECLARE_FEATURE(kWebXrInternals);
COMPONENT_EXPORT(VR_FEATURES) BASE_DECLARE_FEATURE(kWebXRLayers);
COMPONENT_EXPORT(VR_FEATURES)
BASE_DECLARE_FEATURE(kWebXROrientationSensorDevice);
COMPONENT_EXPORT(VR_FEATURES) BASE_DECLARE_FEATURE(kWebXrVisibleBlurred);

#if BUILDFLAG(ENABLE_OPENXR)
// Note that this feature can be overridden by logic contained within
// `IsOpenXrEnabled` and therefore should generally not be queried directly.
COMPONENT_EXPORT(VR_FEATURES) BASE_DECLARE_FEATURE(kOpenXR);
// Note that this feature can be overridden by logic contained within
// `IsOpenXrArEnabled` and therefore should generally not be queried directly.
COMPONENT_EXPORT(VR_FEATURES)
BASE_DECLARE_FEATURE(kOpenXrExtendedFeatureSupport);
COMPONENT_EXPORT(VR_FEATURES) BASE_DECLARE_FEATURE(kOpenXrSpatialEntities);
COMPONENT_EXPORT(VR_FEATURES) BASE_DECLARE_FEATURE(kSpatialEntitesDepthHitTest);
#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(VR_FEATURES) BASE_DECLARE_FEATURE(kOpenXrAndroidSmoothDepth);
#endif

// Helper method to check if OpenXR should be enabled, this is because we want
// the feature enabled on XrDevices, but don't have a buildflag to cleanly set
// the feature by itself. This should be checked instead of a direct query on
// the kOpenXR feature being enabled.
COMPONENT_EXPORT(VR_FEATURES) bool IsOpenXrEnabled();

// Helper method to check if OpenXR AR should be enabled, this is because we
// want the feature enabled on XrDevices, but don't have a buildflag to cleanly
// set the feature by itself. This should be checked instead of a direct query
// on the kOpenXrExtendedFeatureSupport feature being enabled.
COMPONENT_EXPORT(VR_FEATURES) bool IsOpenXrArEnabled();

#endif  // ENABLE_OPENXR

COMPONENT_EXPORT(VR_FEATURES) bool IsXrDevice();

COMPONENT_EXPORT(VR_FEATURES) bool IsHandTrackingEnabled();

}  // namespace device::features

#endif  // DEVICE_VR_PUBLIC_CPP_FEATURES_H_
