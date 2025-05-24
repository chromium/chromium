// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_PROFILE_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_PROFILE_MANAGER_CLIENT_H_

#include <map>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_profile_manager_client.h"

namespace bluez {

class FakeBluetoothProfileServiceProvider;

// FakeBluetoothProfileManagerClient simulates the behavior of the Bluetooth
// Daemon's profile manager object and is used both in test cases in place of a
// mock and on the Linux desktop.
class DEVICE_BLUETOOTH_EXPORT FakeBluetoothProfileManagerClient
    : public BluetoothProfileManagerClient {
 public:
  FakeBluetoothProfileManagerClient();
  ~FakeBluetoothProfileManagerClient() override;

  // BluetoothProfileManagerClient overrides
  void Init(dbus::Bus* bus, const std::string& bluetooth_service_name) override;
  void RegisterProfile(const dbus::ObjectPath& profile_path,
                       const std::string& uuid,
                       const Options& options,
                       base::OnceClosure callback,
                       ErrorCallback error_callback) override;
  void UnregisterProfile(const dbus::ObjectPath& profile_path,
                         base::OnceClosure callback,
                         ErrorCallback error_callback) override;

  // Register, unregister and retrieve pointers to profile server providers.
  void RegisterProfileServiceProvider(
      FakeBluetoothProfileServiceProvider* service_provider);
  void UnregisterProfileServiceProvider(
      FakeBluetoothProfileServiceProvider* service_provider);
  FakeBluetoothProfileServiceProvider* GetProfileServiceProvider(
      const std::string& uuid);

  // UUIDs recognised for testing.
  static const char kL2capUuid[];
  static const char kRfcommUuid[];
  static const char kUnregisterableUuid[];

 private:
  // Map of a D-Bus object path to the FakeBluetoothProfileServiceProvider
  // registered for it; maintained by RegisterProfileServiceProvider() and
  // UnregisterProfileServiceProvicer() called by the constructor and
  // destructor of FakeBluetoothProfileServiceProvider.
  typedef std::map<
      dbus::ObjectPath,
      raw_ptr<FakeBluetoothProfileServiceProvider, CtnExperimental>>
      ServiceProviderMap;
  ServiceProviderMap service_provider_map_;

  // Map of Profile UUID to the D-Bus object path of the service provider
  // in |service_provider_map_|. Maintained by RegisterProfile() and
  // UnregisterProfile() in response to BluetoothProfile methods.
  typedef std::map<std::string, dbus::ObjectPath> ProfileMap;
  ProfileMap profile_map_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_PROFILE_MANAGER_CLIENT_H_
