// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADVERTISEMENT_MONITOR_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADVERTISEMENT_MONITOR_MANAGER_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluez_dbus_client.h"

namespace bluez {

// BluetoothAdvertisementMonitorManagerClient is used to communicate with the
// advertisement monitor manager object of the Bluetooth daemon.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdvertisementMonitorManagerClient
    : public BluezDBusClient {
 public:
  ~BluetoothAdvertisementMonitorManagerClient() override;

  using ErrorCallback = base::OnceCallback<void(std::string error_name,
                                                std::string error_message)>;

  // Registers an advertisement monitor manager at the D-bus object path
  // |application| with the remote BlueZ advertisement monitor manager. After
  // successful registration applications can register advertisement monitors
  // under the root |application|.
  virtual void RegisterMonitor(const dbus::ObjectPath& application,
                               const dbus::ObjectPath& adapter,
                               base::OnceClosure callback,
                               ErrorCallback error_callback) = 0;

  // Unregisters the advertisement monitor manager with the D-Bus object path
  // |application| from the remote advertisement monitor manager.
  virtual void UnregisterMonitor(const dbus::ObjectPath& application,
                                 const dbus::ObjectPath& adapter,
                                 base::OnceClosure callback,
                                 ErrorCallback error_callback) = 0;

  // Creates the instance.
  static std::unique_ptr<BluetoothAdvertisementMonitorManagerClient> Create();

 protected:
  BluetoothAdvertisementMonitorManagerClient();
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADVERTISEMENT_MONITOR_MANAGER_CLIENT_H_