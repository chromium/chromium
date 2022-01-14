// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BASE_FEATURES_H_
#define DEVICE_BASE_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "device/base/device_base_export.h"
#include "device/vr/buildflags/buildflags.h"

namespace device {

#if BUILDFLAG(IS_MAC)
DEVICE_BASE_EXPORT extern const base::Feature kNewUsbBackend;
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
DEVICE_BASE_EXPORT extern const base::Feature kNewBLEWinImplementation;
DEVICE_BASE_EXPORT extern const base::Feature kNewBLEGattSessionHandling;
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_VR)
DEVICE_BASE_EXPORT extern const base::Feature kWebXrOrientationSensorDevice;
#endif  // BUILDFLAG(ENABLE_VR)

// New features should be added to the device::features namespace.

namespace features {
#if BUILDFLAG(ENABLE_OPENXR)
DEVICE_BASE_EXPORT extern const base::Feature kOpenXR;
DEVICE_BASE_EXPORT extern const base::Feature kOpenXrExtendedFeatureSupport;
DEVICE_BASE_EXPORT extern const base::Feature kOpenXRSharedImages;
#endif  // ENABLE_OPENXR

DEVICE_BASE_EXPORT extern const base::Feature kWebXrHandInput;
DEVICE_BASE_EXPORT extern const base::Feature kWebXrHitTest;
DEVICE_BASE_EXPORT extern const base::Feature kWebXrIncubations;

}  // namespace features
}  // namespace device

#endif  // DEVICE_BASE_FEATURES_H_
