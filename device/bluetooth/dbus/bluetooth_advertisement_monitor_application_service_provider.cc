// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_application_service_provider.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_application_service_provider_impl.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_advertisement_monitor_application_service_provider.h"

namespace bluez {

BluetoothAdvertisementMonitorApplicationServiceProvider::
    BluetoothAdvertisementMonitorApplicationServiceProvider() = default;

BluetoothAdvertisementMonitorApplicationServiceProvider::
    ~BluetoothAdvertisementMonitorApplicationServiceProvider() = default;

// static
std::unique_ptr<BluetoothAdvertisementMonitorApplicationServiceProvider>
BluetoothAdvertisementMonitorApplicationServiceProvider::Create(
    dbus::Bus* bus,
    const dbus::ObjectPath& object_path) {
  if (!bluez::BluezDBusManager::Get()->IsUsingFakes()) {
    return std::make_unique<
        BluetoothAdvertisementMonitorApplicationServiceProviderImpl>(
        bus, object_path);
  }
#if defined(USE_REAL_DBUS_CLIENTS)
  LOG(FATAL) << "Fake is unavailable if USE_REAL_DBUS_CLIENTS is defined.";
#else
  return std::make_unique<
      FakeBluetoothAdvertisementMonitorApplicationServiceProvider>(object_path);
#endif  // defined(USE_REAL_DBUS_CLIENTS)
}

}  // namespace bluez
