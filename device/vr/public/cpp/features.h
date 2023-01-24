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
COMPONENT_EXPORT(VR_FEATURES) BASE_DECLARE_FEATURE(kWebXrLayers);
COMPONENT_EXPORT(VR_FEATURES)
BASE_DECLARE_FEATURE(kWebXrOrientationSensorDevice);

#if BUILDFLAG(ENABLE_CARDBOARD)
COMPONENT_EXPORT(VR_FEATURES) BASE_DECLARE_FEATURE(kEnableCardboard);
#endif  // ENABLE_CARDBOARD

#if BUILDFLAG(ENABLE_OPENXR)
COMPONENT_EXPORT(VR_FEATURES) BASE_DECLARE_FEATURE(kOpenXR);
COMPONENT_EXPORT(VR_FEATURES)
BASE_DECLARE_FEATURE(kOpenXrExtendedFeatureSupport);
COMPONENT_EXPORT(VR_FEATURES) BASE_DECLARE_FEATURE(kOpenXRSharedImages);
#endif  // ENABLE_OPENXR
}  // namespace device::features

#endif  // DEVICE_VR_PUBLIC_CPP_FEATURES_H_
