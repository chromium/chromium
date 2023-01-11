// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_BATTERY_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_BATTERY_CLIENT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluez_dbus_client.h"

namespace bluez {

// BluetoothBatteryClient is used to communicate with objects on BlueZ's
// org.bluez.Battery1 interface.
class DEVICE_BLUETOOTH_EXPORT BluetoothBatteryClient : public BluezDBusClient {
 public:
  // Structure of properties on org.bluez.Battery1 interface.
  struct Properties : public dbus::PropertySet {
    // The percentage (0-100) of the battery. Read-only.
    dbus::Property<uint8_t> percentage;

    Properties(dbus::ObjectProxy* object_proxy,
               const std::string& interface_name,
               const PropertyChangedCallback& callback);
    ~Properties() override;
  };

  // Interface for observing changes from a remote bluetooth battery.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when the remote battery with object path |object_path| is added
    // to the set of known batteries.
    virtual void BatteryAdded(const dbus::ObjectPath& object_path) {}

    // Called when the remote battery with object path |object_path| is removed
    // from the set of known batteries.
    virtual void BatteryRemoved(const dbus::ObjectPath& object_path) {}

    // Called when the battery with object path |object_path| has a
    // change in value of the property named |property_name|.
    virtual void BatteryPropertyChanged(const dbus::ObjectPath& object_path,
                                        const std::string& property_name) {}
  };

  BluetoothBatteryClient(const BluetoothBatteryClient&) = delete;
  BluetoothBatteryClient& operator=(const BluetoothBatteryClient&) = delete;

  ~BluetoothBatteryClient() override;

  // Adds and removes observers for events on all remote bluetooth
  // batteries. Check the |object_path| parameter of observer methods to
  // determine which battery is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Obtain the properties for the battery with object path |object_path|,
  // any values should be copied if needed.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path) = 0;

  // Creates the instance.
  static BluetoothBatteryClient* Create();

 protected:
  BluetoothBatteryClient();
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_BATTERY_CLIENT_H_
