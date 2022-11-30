// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_advertisement_monitor_service_provider.h"

namespace bluez {

FakeBluetoothAdvertisementMonitorServiceProvider::
    FakeBluetoothAdvertisementMonitorServiceProvider(
        const dbus::ObjectPath& object_path,
        base::WeakPtr<Delegate> delegate)
    : object_path_(object_path), delegate_(delegate) {}

FakeBluetoothAdvertisementMonitorServiceProvider::
    ~FakeBluetoothAdvertisementMonitorServiceProvider() = default;

const dbus::ObjectPath&
FakeBluetoothAdvertisementMonitorServiceProvider::object_path() const {
  return object_path_;
}

}  // namespace bluez
