// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/device_features.h"

namespace features {

// Enables an extra set of concrete sensors classes based on Generic Sensor API,
// which expose previously unexposed platform features, e.g. ALS or Magnetometer
BASE_FEATURE(kGenericSensorExtraClasses,
             "GenericSensorExtraClasses",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables usage of the Windows.Devices.Geolocation WinRT API for the
// LocationProvider instead of the NetworkLocationProvider on Windows.
BASE_FEATURE(kWinrtGeolocationImplementation,
             "WinrtGeolocationImplementation",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enables usage of the CoreLocation API for LocationProvider instead of
// NetworkLocationProvider for macOS or iOS.
BASE_FEATURE(kMacCoreLocationBackend,
             "MacCoreLocationBackend",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Reduces the interval between WiFi polls to fetch new WiFi data sooner. This
// can be useful in situations where new WiFi data is needed frequently.
BASE_FEATURE(kCrOSGeolocationReducedWifiPollingInterval,
             "ReducedWifiPollingInterval",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enable serial communication for SPP devices.
BASE_FEATURE(kEnableBluetoothSerialPortProfileInSerialApi,
             "EnableBluetoothSerialPortProfileInSerialApi",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features
