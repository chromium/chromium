// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADVERTISEMENT_MONITOR_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADVERTISEMENT_MONITOR_MANAGER_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluez_dbus_client.h"

namespace bluez {

// BluetoothAdvertisementMonitorManagerClient is used to communicate with the
// advertisement monitor manager object of the Bluetooth daemon.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdvertisementMonitorManagerClient
    : public BluezDBusClient {
 public:
  // Structure of properties associated with the advertisement monitor manager.
  struct Properties : public dbus::PropertySet {
    Properties(dbus::ObjectProxy* object_proxy,
               const std::string& interface_name,
               const PropertyChangedCallback& callback);
    ~Properties() override;

    // This lists the supported types of advertisement monitors. [read-only]
    dbus::Property<std::vector<std::string>> supported_monitor_types;

    // This lists the features of advertisement monitoring supported by the
    // Bluetooth stack. [read-only]
    dbus::Property<std::vector<std::string>> supported_features;
  };

  // Interface for observing changes in advertisement monitor client properties.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;

    // Called when the advertisement monitoring supported features change.
    virtual void SupportedAdvertisementMonitorFeaturesChanged() = 0;
  };

  ~BluetoothAdvertisementMonitorManagerClient() override;

  using ErrorCallback = base::OnceCallback<void(std::string error_name,
                                                std::string error_message)>;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

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

  // Obtain the properties for the advertisement monitor manager associated with
  // object path |object_path|. Values should be copied if needed.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path) = 0;

  // Creates the instance.
  static std::unique_ptr<BluetoothAdvertisementMonitorManagerClient> Create();

 protected:
  BluetoothAdvertisementMonitorManagerClient();
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADVERTISEMENT_MONITOR_MANAGER_CLIENT_H_
