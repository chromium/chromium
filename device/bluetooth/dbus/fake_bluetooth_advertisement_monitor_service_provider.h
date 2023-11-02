// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_ADVERTISEMENT_MONITOR_SERVICE_PROVIDER_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_ADVERTISEMENT_MONITOR_SERVICE_PROVIDER_H_

#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_application_service_provider.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_service_provider.h"

namespace bluez {

// FakeBluetoothAdvertisementMonitorServiceProvider simulates behavior of an
// Advertisement Monitor Service Provider object and is used in test cases.
class DEVICE_BLUETOOTH_EXPORT FakeBluetoothAdvertisementMonitorServiceProvider
    : public BluetoothAdvertisementMonitorServiceProvider {
 public:
  FakeBluetoothAdvertisementMonitorServiceProvider(
      const dbus::ObjectPath& object_path,
      base::WeakPtr<Delegate> delegate);
  FakeBluetoothAdvertisementMonitorServiceProvider(
      const FakeBluetoothAdvertisementMonitorServiceProvider&) = delete;
  FakeBluetoothAdvertisementMonitorServiceProvider& operator=(
      const FakeBluetoothAdvertisementMonitorServiceProvider&) = delete;
  ~FakeBluetoothAdvertisementMonitorServiceProvider() override;

  // BluetoothAdvertisementMonitorServiceProvider override:
  const dbus::ObjectPath& object_path() const override;

  BluetoothAdvertisementMonitorServiceProvider::Delegate* delegate() {
    return delegate_.get();
  }

 private:
  dbus::ObjectPath object_path_;

  base::WeakPtr<BluetoothAdvertisementMonitorServiceProvider::Delegate>
      delegate_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_ADVERTISEMENT_MONITOR_SERVICE_PROVIDER_H_
