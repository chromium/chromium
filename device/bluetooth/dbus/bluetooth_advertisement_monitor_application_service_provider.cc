// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_application_service_provider.h"

#include "base/memory/ptr_util.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_application_service_provider_impl.h"

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
  return std::make_unique<
      BluetoothAdvertisementMonitorApplicationServiceProviderImpl>(bus,
                                                                   object_path);
}

}  // namespace bluez
