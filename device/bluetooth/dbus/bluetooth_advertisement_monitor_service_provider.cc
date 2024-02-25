// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_service_provider.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_service_provider_impl.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_advertisement_monitor_service_provider.h"

namespace bluez {

BluetoothAdvertisementMonitorServiceProvider::
    BluetoothAdvertisementMonitorServiceProvider() = default;

BluetoothAdvertisementMonitorServiceProvider::
    ~BluetoothAdvertisementMonitorServiceProvider() = default;

// static
std::unique_ptr<BluetoothAdvertisementMonitorServiceProvider>
BluetoothAdvertisementMonitorServiceProvider::Create(
    dbus::Bus* bus,
    const dbus::ObjectPath& object_path,
    std::unique_ptr<device::BluetoothLowEnergyScanFilter> filter,
    base::WeakPtr<BluetoothAdvertisementMonitorServiceProvider::Delegate>
        delegate) {
  if (!bluez::BluezDBusManager::Get()->IsUsingFakes()) {
    return std::make_unique<BluetoothAdvertisementMonitorServiceProviderImpl>(
        bus, object_path, std::move(filter), delegate);
  }
#if defined(USE_REAL_DBUS_CLIENTS)
  LOG(FATAL) << "Fake is unavailable if USE_REAL_DBUS_CLIENTS is defined.";
#else
  return std::make_unique<FakeBluetoothAdvertisementMonitorServiceProvider>(
      object_path, delegate);
#endif  // defined(USE_REAL_DBUS_CLIENTS)
}

}  // namespace bluez
