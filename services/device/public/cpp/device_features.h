// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the
// services/device module.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_DEVICE_FEATURES_H_
#define SERVICES_DEVICE_PUBLIC_CPP_DEVICE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "services/device/public/cpp/device_features_export.h"
#include "services/device/public/mojom/geolocation_internals.mojom-shared.h"

namespace features {

// The features should be documented alongside the definition of their values
// in the .cc file.
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(
    kComputePressureBreakCalibrationMitigation);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kGenericSensorExtraClasses);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(
    kEnableBluetoothSerialPortProfileInSerialApi);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kGeolocationDiagnosticsObserver);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kSerialPortConnected);
#if BUILDFLAG(IS_WIN)
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kWinSystemLocationPermission);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kHidGetFeatureReportFix);

extern const DEVICE_FEATURES_EXPORT base::FeatureParam<int>
    kWinSystemLocationPermissionPollingParam;
#endif  // BUILDFLAG(IS_WIN)
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kLocationProviderManager);

#if BUILDFLAG(IS_CHROMEOS)
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kUsbDeviceLinuxOpenCrashKey);
#endif  // BUILDFLAG(IS_CHROMEOS)

extern const DEVICE_FEATURES_EXPORT
    base::FeatureParam<device::mojom::LocationProviderManagerMode>
        kLocationProviderManagerParam;

DEVICE_FEATURES_EXPORT bool IsOsLevelGeolocationPermissionSupportEnabled();

}  // namespace features

#endif  // SERVICES_DEVICE_PUBLIC_CPP_DEVICE_FEATURES_H_
