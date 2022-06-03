// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_AGENT_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_AGENT_MANAGER_CLIENT_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/observer_list.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_agent_manager_client.h"

namespace bluez {

class FakeBluetoothAgentServiceProvider;

// FakeBluetoothAgentManagerClient simulates the behavior of the Bluetooth
// Daemon's agent manager object and is used both in test cases in place of a
// mock and on the Linux desktop.
class DEVICE_BLUETOOTH_EXPORT FakeBluetoothAgentManagerClient
    : public BluetoothAgentManagerClient {
 public:
  FakeBluetoothAgentManagerClient();
  ~FakeBluetoothAgentManagerClient() override;

  // BluetoothAgentManagerClient overrides
  void Init(dbus::Bus* bus, const std::string& bluetooth_service_name) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void RegisterAgent(const dbus::ObjectPath& agent_path,
                     const std::string& capability,
                     base::OnceClosure callback,
                     ErrorCallback error_callback) override;
  void UnregisterAgent(const dbus::ObjectPath& agent_path,
                       base::OnceClosure callback,
                       ErrorCallback error_callback) override;
  void RequestDefaultAgent(const dbus::ObjectPath& agent_path,
                           base::OnceClosure callback,
                           ErrorCallback error_callback) override;

  // Register, unregister and retrieve pointers to agent service providers.
  void RegisterAgentServiceProvider(
      FakeBluetoothAgentServiceProvider* service_provider);
  void UnregisterAgentServiceProvider(
      FakeBluetoothAgentServiceProvider* service_provider);
  FakeBluetoothAgentServiceProvider* GetAgentServiceProvider();

 private:
  // The single agent service provider we permit, owned by the application
  // using it.
  FakeBluetoothAgentServiceProvider* service_provider_;

  // List of observers interested in event notifications from us.
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_AGENT_MANAGER_CLIENT_H_
