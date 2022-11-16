// Copyright 2017 The Chromium Authors
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
DEVICE_BASE_EXPORT BASE_DECLARE_FEATURE(kNewUsbBackend);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
DEVICE_BASE_EXPORT BASE_DECLARE_FEATURE(kNewBLEWinImplementation);
DEVICE_BASE_EXPORT BASE_DECLARE_FEATURE(kNewBLEGattSessionHandling);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_VR)
DEVICE_BASE_EXPORT BASE_DECLARE_FEATURE(kWebXrOrientationSensorDevice);
#endif  // BUILDFLAG(ENABLE_VR)

// New features should be added to the device::features namespace.

namespace features {
#if BUILDFLAG(ENABLE_OPENXR)
DEVICE_BASE_EXPORT BASE_DECLARE_FEATURE(kOpenXR);
DEVICE_BASE_EXPORT BASE_DECLARE_FEATURE(kOpenXrExtendedFeatureSupport);
DEVICE_BASE_EXPORT BASE_DECLARE_FEATURE(kOpenXRSharedImages);
#endif  // ENABLE_OPENXR

DEVICE_BASE_EXPORT BASE_DECLARE_FEATURE(kWebXrHandInput);
DEVICE_BASE_EXPORT BASE_DECLARE_FEATURE(kWebXrHitTest);
DEVICE_BASE_EXPORT BASE_DECLARE_FEATURE(kWebXrIncubations);
DEVICE_BASE_EXPORT BASE_DECLARE_FEATURE(kWebXrLayers);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
DEVICE_BASE_EXPORT BASE_DECLARE_FEATURE(kWebBluetoothConfirmPairingSupport);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)

}  // namespace features
}  // namespace device

#endif  // DEVICE_BASE_FEATURES_H_
