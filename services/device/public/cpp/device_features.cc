// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/device_features.h"

#include "services/device/public/cpp/geolocation/buildflags.h"

namespace features {

// Enables mitigation algorithm to prevent attempt of calibration from an
// attacker.
BASE_FEATURE(kComputePressureBreakCalibrationMitigation,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables an extra set of concrete sensors classes based on Generic Sensor API,
// which expose previously unexposed platform features, e.g. ALS or Magnetometer
BASE_FEATURE(kGenericSensorExtraClasses, base::FEATURE_DISABLED_BY_DEFAULT);
// Expose serial port logical connection state and dispatch connection events
// for Bluetooth serial ports when the Bluetooth device connection state
// changes.
BASE_FEATURE(kSerialPortConnected,
#if !BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // !BUILDFLAG(IS_ANDROID)
);

// This feature allows to dynamically introduce an additional list of devices
// blocked by WebUSB via a Finch parameter. This parameter should be specified
// in the Finch configuration to manage the list of blocked devices.
BASE_FEATURE(kWebUsbBlocklist,
             "WebUSBBlocklist",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, accessing the navigator.hid attribute does not prevent the
// frame from entering the back forward cache.
BASE_FEATURE(kWebHidAttributeAllowsBackForwardCache,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// Enable integration with the Windows system-level location permission.
BASE_FEATURE(kWinSystemLocationPermission, base::FEATURE_ENABLED_BY_DEFAULT);
// Enables a fix for a HID issue where feature reports read from devices that
// do not use report IDs would incorrectly include an extra zero byte at the
// start of the report and truncate the last byte of the report.
BASE_FEATURE(kHidGetFeatureReportFix, base::FEATURE_ENABLED_BY_DEFAULT);

// Defines a feature parameter for the `kWinSystemLocationPermission` feature.
// This parameter controls the polling interval (in milliseconds) for checking
// the permission status. The default polling interval is set to 500
// milliseconds.
const base::FeatureParam<int> kWinSystemLocationPermissionPollingParam{
    &kWinSystemLocationPermission, "polling_interval_in_ms", 500};
#endif  // BUILDFLAG(IS_WIN)

// Enables usage of the location provider manager to select between
// the operating system's location API or our network-based provider
// as the source of location data for Geolocation API.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
BASE_FEATURE(kLocationProviderManager, base::FEATURE_ENABLED_BY_DEFAULT);
#else
BASE_FEATURE(kLocationProviderManager, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
// Enables crash key logging for USB device open operations on ChromeOS. See
// crbug.com/332722607. Can be disabled as a kill switch if needed.
BASE_FEATURE(kUsbDeviceLinuxOpenCrashKey, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
// Enables registering & unregistering of the Battery Status Manager broadcast
// receiver to the background thread.
BASE_FEATURE(kBatteryStatusManagerBroadcastReceiverInBackground,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
// Modifies the internal allowlist behavior that enables privileged extensions
// to bypass the HID blocklist when accessing FIDO devices. When enabled,
// privileged extensions can access non-FIDO interfaces on known security keys.
BASE_FEATURE(kSecurityKeyHidInterfacesAreFido,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID)

const base::FeatureParam<device::mojom::LocationProviderManagerMode>::Option
    location_provider_manager_mode_options[] = {
        {device::mojom::LocationProviderManagerMode::kNetworkOnly,
         "NetworkOnly"},
        {device::mojom::LocationProviderManagerMode::kPlatformOnly,
         "PlatformOnly"},
        {device::mojom::LocationProviderManagerMode::kHybridPlatform,
         "HybridPlatform"},
        {device::mojom::LocationProviderManagerMode::kHybridPlatform2,
         "HybridPlatform2"},
};

#if BUILDFLAG(IS_MAC)
const base::FeatureParam<device::mojom::LocationProviderManagerMode>
    kLocationProviderManagerParam{
        &kLocationProviderManager, "LocationProviderManagerMode",
        device::mojom::LocationProviderManagerMode::kHybridPlatform,
        &location_provider_manager_mode_options};
#elif BUILDFLAG(IS_WIN)
const base::FeatureParam<device::mojom::LocationProviderManagerMode>
    kLocationProviderManagerParam{
        &kLocationProviderManager, "LocationProviderManagerMode",
        device::mojom::LocationProviderManagerMode::kPlatformOnly,
        &location_provider_manager_mode_options};
#else
const base::FeatureParam<device::mojom::LocationProviderManagerMode>
    kLocationProviderManagerParam{
        &kLocationProviderManager, "LocationProviderManagerMode",
        device::mojom::LocationProviderManagerMode::kNetworkOnly,
        &location_provider_manager_mode_options};
#endif  // BUILDFLAG(IS_MAC)

bool IsOsLevelGeolocationPermissionSupportEnabled() {
#if BUILDFLAG(IS_WIN)
  return base::FeatureList::IsEnabled(features::kWinSystemLocationPermission);
#elif BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
  return true;
#else
  return false;
#endif  // BUILDFLAG(IS_WIN)
}

// Controls whether Chrome will try to automatically detach kernel drivers when
// a USB interface is busy.
#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kAutomaticUsbDetach, base::FEATURE_ENABLED_BY_DEFAULT);
#elif BUILDFLAG(IS_LINUX)
BASE_FEATURE(kAutomaticUsbDetach, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_WIN)
// Splits DTR and RTS control signals. See crbug.com/420689824.
// Can be disabled as a kill switch if needed.
BASE_FEATURE(kSerialSplitDtrAndRts, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kHidReportRequestExactLength, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

}  // namespace features
