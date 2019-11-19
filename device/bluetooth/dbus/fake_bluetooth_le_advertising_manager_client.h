// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_LE_ADVERTISING_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_LE_ADVERTISING_MANAGER_CLIENT_H_

#include <map>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_le_advertising_manager_client.h"

namespace bluez {

class FakeBluetoothLEAdvertisementServiceProvider;

// FakeBluetoothAdvertisementManagerClient simulates the behavior of the
// Bluetooth
// Daemon's profile manager object and is used both in test cases in place of a
// mock and on the Linux desktop.
class DEVICE_BLUETOOTH_EXPORT FakeBluetoothLEAdvertisingManagerClient
    : public BluetoothLEAdvertisingManagerClient {
 public:
  FakeBluetoothLEAdvertisingManagerClient();
  ~FakeBluetoothLEAdvertisingManagerClient() override;

  // DBusClient overrides:
  void Init(dbus::Bus* bus, const std::string& bluetooth_service_name) override;

  // BluetoothAdvertisingManagerClient overrides:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void RegisterAdvertisement(const dbus::ObjectPath& manager_object_path,
                             const dbus::ObjectPath& advertisement_object_path,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) override;
  void UnregisterAdvertisement(
      const dbus::ObjectPath& manager_object_path,
      const dbus::ObjectPath& advertisement_object_path,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;

  void SetAdvertisingInterval(const dbus::ObjectPath& manager_object_path,
                              uint16_t min_interval_ms,
                              uint16_t max_interval_ms,
                              base::OnceClosure callback,
                              ErrorCallback error_callback) override;

  void ResetAdvertising(const dbus::ObjectPath& manager_object_path,
                        base::OnceClosure callback,
                        ErrorCallback error_callback) override;

  // Register, unregister and retrieve pointers to profile server providers.
  void RegisterAdvertisementServiceProvider(
      FakeBluetoothLEAdvertisementServiceProvider* service_provider);
  void UnregisterAdvertisementServiceProvider(
      FakeBluetoothLEAdvertisementServiceProvider* service_provider);
  FakeBluetoothLEAdvertisementServiceProvider* GetAdvertisementServiceProvider(
      const std::string& uuid);

  int currently_registered() { return currently_registered_.size(); }

  enum : size_t { kMaxBluezAdvertisements = 5 };

 private:
  // Map of a D-Bus object path to the FakeBluetoothAdvertisementServiceProvider
  // registered for it; maintained by RegisterAdvertisementServiceProvider() and
  // UnregisterProfileServiceProvicer() called by the constructor and
  // destructor of FakeBluetoothAdvertisementServiceProvider.
  typedef std::map<dbus::ObjectPath,
                   FakeBluetoothLEAdvertisementServiceProvider*>
      ServiceProviderMap;
  ServiceProviderMap service_provider_map_;

  // Holds currently registered advertisements.
  std::vector<dbus::ObjectPath> currently_registered_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothLEAdvertisingManagerClient);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_LE_ADVERTISING_MANAGER_CLIENT_H_
