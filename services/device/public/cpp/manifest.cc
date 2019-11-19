// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "services/device/public/mojom/bluetooth_system.mojom.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/fingerprint.mojom.h"
#include "services/device/public/mojom/geolocation_config.mojom.h"
#include "services/device/public/mojom/geolocation_context.mojom.h"
#include "services/device/public/mojom/geolocation_control.mojom.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "services/device/public/mojom/input_service.mojom.h"
#include "services/device/public/mojom/nfc_provider.mojom.h"
#include "services/device/public/mojom/power_monitor.mojom.h"
#include "services/device/public/mojom/public_ip_address_geolocation_provider.mojom.h"
#include "services/device/public/mojom/screen_orientation.mojom.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "services/device/public/mojom/time_zone_monitor.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/public/mojom/usb_manager_test.mojom.h"
#include "services/device/public/mojom/vibration_manager.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

#if defined(USE_DBUS)
#include "services/device/public/mojom/mtp_manager.mojom.h"
#endif

namespace device {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
        .WithServiceName(mojom::kServiceName)
        .WithDisplayName("Device Service")
        .WithOptions(service_manager::ManifestOptionsBuilder()
                         .WithInstanceSharingPolicy(
                             service_manager::Manifest::InstanceSharingPolicy::
                                 kSharedAcrossGroups)
                         .Build())
        .ExposeCapability(
            "device:battery_monitor",
            service_manager::Manifest::InterfaceList<mojom::BatteryMonitor>())
        .ExposeCapability("device:bluetooth_system",
                          service_manager::Manifest::InterfaceList<
                              mojom::BluetoothSystemFactory>())
        .ExposeCapability(
            "device:fingerprint",
            service_manager::Manifest::InterfaceList<mojom::Fingerprint>())
        .ExposeCapability(
            "device:generic_sensor",
            service_manager::Manifest::InterfaceList<mojom::SensorProvider>())
        .ExposeCapability("device:geolocation",
                          service_manager::Manifest::InterfaceList<
                              mojom::GeolocationContext>())
        .ExposeCapability("device:geolocation_config",
                          service_manager::Manifest::InterfaceList<
                              mojom::GeolocationConfig>())
        .ExposeCapability("device:geolocation_control",
                          service_manager::Manifest::InterfaceList<
                              mojom::GeolocationControl>())
        .ExposeCapability(
            "device:hid",
            service_manager::Manifest::InterfaceList<mojom::HidManager>())
        .ExposeCapability("device:ip_geolocator",
                          service_manager::Manifest::InterfaceList<
                              mojom::PublicIpAddressGeolocationProvider>())
        .ExposeCapability("device:input_service",
                          service_manager::Manifest::InterfaceList<
                              mojom::InputDeviceManager>())
        .ExposeCapability(
            "device:nfc",
            service_manager::Manifest::InterfaceList<mojom::NFCProvider>())
        .ExposeCapability(
            "device:power_monitor",
            service_manager::Manifest::InterfaceList<mojom::PowerMonitor>())
        .ExposeCapability("device:screen_orientation",
                          service_manager::Manifest::InterfaceList<
                              mojom::ScreenOrientationListener>())
        .ExposeCapability("device:serial",
                          service_manager::Manifest::InterfaceList<
                              mojom::SerialPortManager>())
        .ExposeCapability(
            "device:time_zone_monitor",
            service_manager::Manifest::InterfaceList<mojom::TimeZoneMonitor>())
        .ExposeCapability(
            "device:usb",
            service_manager::Manifest::InterfaceList<mojom::UsbDeviceManager>())
        .ExposeCapability("device:usb_test",
                          service_manager::Manifest::InterfaceList<
                              mojom::UsbDeviceManagerTest>())
        .ExposeCapability(
            "device:vibration",
            service_manager::Manifest::InterfaceList<mojom::VibrationManager>())
        .ExposeCapability(
            "device:wake_lock",
            service_manager::Manifest::InterfaceList<mojom::WakeLockProvider>())
#if defined(USE_DBUS)
        .ExposeCapability(
            "device:mtp",
            service_manager::Manifest::InterfaceList<mojom::MtpManager>())
#endif
        .Build()
  };
  return *manifest;
}

}  // namespace device
