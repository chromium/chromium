// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_LE_ADVERTISEMENT_SERVICE_PROVIDER_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_LE_ADVERTISEMENT_SERVICE_PROVIDER_H_

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_le_advertisement_service_provider.h"

namespace bluez {

// FakeBluetoothAdvertisementServiceProvider simulates the behavior of a local
// Bluetooth agent object and is used both in test cases in place of a
// mock and on the Linux desktop.
class DEVICE_BLUETOOTH_EXPORT FakeBluetoothLEAdvertisementServiceProvider
    : public BluetoothLEAdvertisementServiceProvider {
 public:
  FakeBluetoothLEAdvertisementServiceProvider(
      const dbus::ObjectPath& object_path,
      Delegate* delegate);

  FakeBluetoothLEAdvertisementServiceProvider(
      const FakeBluetoothLEAdvertisementServiceProvider&) = delete;
  FakeBluetoothLEAdvertisementServiceProvider& operator=(
      const FakeBluetoothLEAdvertisementServiceProvider&) = delete;

  ~FakeBluetoothLEAdvertisementServiceProvider() override;

  // Each of these calls the equivalent
  // BluetoothAdvertisementServiceProvider::Delegate method on the object passed
  // on construction.
  void Release();

  const dbus::ObjectPath& object_path() { return object_path_; }

 private:
  friend class FakeBluetoothLEAdvertisingManagerClient;

  // All incoming method calls are passed on to the Delegate and a callback
  // passed to generate the reply. |delegate_| is generally the object that
  // owns this one, and must outlive it.
  raw_ptr<Delegate> delegate_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_LE_ADVERTISEMENT_SERVICE_PROVIDER_H_
