// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/device_features.h"

namespace features {

// Enables an extra set of concrete sensors classes based on Generic Sensor API,
// which expose previously unexposed platform features, e.g. ALS or Magnetometer
const base::Feature kGenericSensorExtraClasses{
    "GenericSensorExtraClasses", base::FEATURE_DISABLED_BY_DEFAULT};
// Enables usage of the Windows.Devices.Geolocation WinRT API for the
// LocationProvider instead of the NetworkLocationProvider on Windows.
const base::Feature kWinrtGeolocationImplementation{
    "WinrtGeolocationImplementation", base::FEATURE_DISABLED_BY_DEFAULT};
// Enables usage of the CoreLocation API for LocationProvider instead of
// NetworkLocationProvider for macOS. The |kMacCoreLocationImplementation| flag
// enables a permissions UX workflow that navigates the user to give the
// browser location permission in the macOS System Preferences. The
// |kMacCoreLocationBackend| flag switches to using the the macOS Core Location
// API instead of using the NetworkLocationProvider to gather location through
// WiFi scans.
const base::Feature kMacCoreLocationImplementation{
    "MacCoreLocationImplementation", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kMacCoreLocationBackend{"MacCoreLocationBackend",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
