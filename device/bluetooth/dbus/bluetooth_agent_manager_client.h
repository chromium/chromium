// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_AGENT_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_AGENT_MANAGER_CLIENT_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluez_dbus_client.h"

namespace bluez {

// BluetoothAgentManagerClient is used to communicate with the agent manager
// object of the Bluetooth daemon.
class DEVICE_BLUETOOTH_EXPORT BluetoothAgentManagerClient
    : public BluezDBusClient {
 public:
  // Interface for observing changes of agent manager.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override {}

    // Called when the agent manager with object path |object_path| is added to
    // the system.
    virtual void AgentManagerAdded(const dbus::ObjectPath& object_path) {}

    // Called when the agent manager with object path |object_path| is removed
    // from the system.
    virtual void AgentManagerRemoved(const dbus::ObjectPath& object_path) {}
  };

  BluetoothAgentManagerClient(const BluetoothAgentManagerClient&) = delete;
  BluetoothAgentManagerClient& operator=(const BluetoothAgentManagerClient&) =
      delete;

  ~BluetoothAgentManagerClient() override;

  // Adds and removes observers for events on agent manager.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // The ErrorCallback is used by agent manager methods to indicate failure.
  // It receives two arguments: the name of the error in |error_name| and
  // an optional message in |error_message|.
  typedef base::OnceCallback<void(const std::string& error_name,
                                  const std::string& error_message)>
      ErrorCallback;

  // Registers an agent within the local process at the D-bus object path
  // |agent_path| with the remote agent manager. The agent is used for pairing
  // and for authorization of incoming connection requests. |capability|
  // specifies the input and display capabilities of the agent and should be
  // one of the constants declared in the bluetooth_agent_manager:: namespace.
  virtual void RegisterAgent(const dbus::ObjectPath& agent_path,
                             const std::string& capability,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) = 0;

  // Unregisters the agent with the D-Bus object path |agent_path| from the
  // remote agent manager.
  virtual void UnregisterAgent(const dbus::ObjectPath& agent_path,
                               base::OnceClosure callback,
                               ErrorCallback error_callback) = 0;

  // Requests that the agent with the D-Bus object path |agent_path| be made
  // the default.
  virtual void RequestDefaultAgent(const dbus::ObjectPath& agent_path,
                                   base::OnceClosure callback,
                                   ErrorCallback error_callback) = 0;

  // Creates the instance.
  static BluetoothAgentManagerClient* Create();

  // Constants used to indicate exceptional error conditions.
  static const char kNoResponseError[];

 protected:
  BluetoothAgentManagerClient();
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_AGENT_MANAGER_CLIENT_H_
