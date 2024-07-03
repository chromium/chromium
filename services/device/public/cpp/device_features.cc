// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/device_features.h"

#include "services/device/public/cpp/geolocation/buildflags.h"

namespace features {

// Enables mitigation algorithm to prevent attempt of calibration from an
// attacker.
BASE_FEATURE(kComputePressureBreakCalibrationMitigation,
             "ComputePressureBreakCalibrationMitigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables an extra set of concrete sensors classes based on Generic Sensor API,
// which expose previously unexposed platform features, e.g. ALS or Magnetometer
BASE_FEATURE(kGenericSensorExtraClasses,
             "GenericSensorExtraClasses",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Enable serial communication for SPP devices.
BASE_FEATURE(kEnableBluetoothSerialPortProfileInSerialApi,
             "EnableBluetoothSerialPortProfileInSerialApi",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Enable real-time diagnostic updates in chrome://location-internals.
BASE_FEATURE(kGeolocationDiagnosticsObserver,
             "GeolocationDiagnosticsObserver",
             base::FEATURE_ENABLED_BY_DEFAULT);
// Expose serial port logical connection state and dispatch connection events
// for Bluetooth serial ports when the Bluetooth device connection state
// changes.
BASE_FEATURE(kSerialPortConnected,
             "SerialPortConnected",
             base::FEATURE_DISABLED_BY_DEFAULT);
#if BUILDFLAG(IS_WIN)
// Enable integration with the Windows system-level location permission.
BASE_FEATURE(kWinSystemLocationPermission,
             "WinSystemLocationPermission",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)
// Enables usage of the location provider manager to select between
// the operating system's location API or our network-based provider
// as the source of location data for Geolocation API.
BASE_FEATURE(kLocationProviderManager,
             "LocationProviderManager",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS)
// Enables crash key logging for USB device open operations on ChromeOS. See
// crbug.com/332722607. Can be disabled as a kill switch if needed.
BASE_FEATURE(kUsbDeviceLinuxOpenCrashKey,
             "UsbDeviceLinuxOpenCrashKey",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

const base::FeatureParam<device::mojom::LocationProviderManagerMode>::Option
    location_provider_manager_mode_options[] = {
        {device::mojom::LocationProviderManagerMode::kNetworkOnly,
         "NetworkOnly"},
        {device::mojom::LocationProviderManagerMode::kPlatformOnly,
         "PlatformOnly"},
        {device::mojom::LocationProviderManagerMode::kHybridPlatform,
         "HybridPlatform"},
};

const base::FeatureParam<device::mojom::LocationProviderManagerMode>
    kLocationProviderManagerParam{
        &kLocationProviderManager, "LocationProviderManagerMode",
        device::mojom::LocationProviderManagerMode::kNetworkOnly,
        &location_provider_manager_mode_options};

bool IsOsLevelGeolocationPermissionSupportEnabled() {
#if BUILDFLAG(IS_WIN)
  return base::FeatureList::IsEnabled(features::kWinSystemLocationPermission);
#elif BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  return true;
#else
  return false;
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace features
