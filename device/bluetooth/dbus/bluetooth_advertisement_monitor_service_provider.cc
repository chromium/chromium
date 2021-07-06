// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_service_provider.h"

#include "base/memory/ptr_util.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_service_provider_impl.h"

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
  return std::make_unique<BluetoothAdvertisementMonitorServiceProviderImpl>(
      bus, object_path, std::move(filter), delegate);
}

}  // namespace bluez
