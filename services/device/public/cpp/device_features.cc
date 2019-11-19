// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/device_features.h"

namespace features {

// Enables an extra set of concrete sensors classes based on Generic Sensor API,
// which expose previously unexposed platform features, e.g. ALS or Magnetometer
const base::Feature kGenericSensorExtraClasses{
    "GenericSensorExtraClasses", base::FEATURE_DISABLED_BY_DEFAULT};
// Enables usage of the Windows.Devices.Sensors WinRT API for the sensor
// backend instead of the ISensor API on Windows.
const base::Feature kWinrtSensorsImplementation{
    "WinrtSensorsImplementation", base::FEATURE_DISABLED_BY_DEFAULT};
// Enables usage of the Windows.Devices.Geolocation WinRT API for the
// LocationProvider instead of the NetworkLocationProvider on Windows.
const base::Feature kWinrtGeolocationImplementation{
    "WinrtGeolocationImplementation", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
