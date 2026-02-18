// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the
// services/device module.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_DEVICE_FEATURES_H_
#define SERVICES_DEVICE_PUBLIC_CPP_DEVICE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "services/device/public/cpp/device_features_export.h"
#include "services/device/public/mojom/geolocation_internals.mojom-shared.h"

namespace features {

// The features should be documented alongside the definition of their values
// in the .cc file.
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kGenericSensorExtraClasses);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kSerialPortConnected);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kWebUsbBlocklist);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(
    kWebHidAttributeAllowsBackForwardCache);
#if BUILDFLAG(IS_WIN)
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kWinSystemLocationPermission);
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kHidGetFeatureReportFix);

extern const DEVICE_FEATURES_EXPORT base::FeatureParam<int>
    kWinSystemLocationPermissionPollingParam;
#endif  // BUILDFLAG(IS_WIN)
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kLocationProviderManager);

#if BUILDFLAG(IS_ANDROID)
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(
    kBatteryStatusManagerBroadcastReceiverInBackground);
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kSecurityKeyHidInterfacesAreFido);
#endif  // !BUILDFLAG(IS_ANDROID)

extern const DEVICE_FEATURES_EXPORT
    base::FeatureParam<device::mojom::LocationProviderManagerMode>
        kLocationProviderManagerParam;

DEVICE_FEATURES_EXPORT bool IsOsLevelGeolocationPermissionSupportEnabled();

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kAutomaticUsbDetach);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kProductNameOverHidName);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_WIN)
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kSerialSplitDtrAndRts);
#endif  // !BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kHidReportRequestExactLength);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_APPLE) && BUILDFLAG(USE_BLINK)
// Controls whether to use the ellipsoidal altitude from Core Location
// instead of the default altitude attribute.
DEVICE_FEATURES_EXPORT BASE_DECLARE_FEATURE(kEllipsoidalAltitude);
#endif  // BUILDFLAG(IS_APPLE) && BUILDFLAG(USE_BLINK)

}  // namespace features

#endif  // SERVICES_DEVICE_PUBLIC_CPP_DEVICE_FEATURES_H_
