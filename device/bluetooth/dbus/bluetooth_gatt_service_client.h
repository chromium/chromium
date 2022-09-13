// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_SERVICE_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_SERVICE_CLIENT_H_

#include <string>
#include <vector>

#include "dbus/object_path.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluez_dbus_client.h"

namespace bluez {

// BluetoothGattServiceClient is used to communicate with remote GATT service
// objects exposed by the Bluetooth daemon.
class DEVICE_BLUETOOTH_EXPORT BluetoothGattServiceClient
    : public BluezDBusClient {
 public:
  // Structure of properties associated with GATT services.
  struct Properties : public dbus::PropertySet {
    // The 128-bit service UUID. [read-only]
    dbus::Property<std::string> uuid;

    // Object path of the Bluetooth device that the GATT service belongs to.
    dbus::Property<dbus::ObjectPath> device;

    // Whether or not this service is a primary service.
    dbus::Property<bool> primary;

    // Array of object paths representing the included services of this service.
    // [read-only]
    dbus::Property<std::vector<dbus::ObjectPath>> includes;

    Properties(dbus::ObjectProxy* object_proxy,
               const std::string& interface_name,
               const PropertyChangedCallback& callback);
    ~Properties() override;
  };

  // Interface for observing changes from a remote GATT service.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the GATT service with object path |object_path| is added to
    // the system.
    virtual void GattServiceAdded(const dbus::ObjectPath& object_path) {}

    // Called when the GATT service with object path |object_path| is removed
    // from the system.
    virtual void GattServiceRemoved(const dbus::ObjectPath& object_path) {}

    // Called when the GATT service with object path |object_path| has a change
    // in the value of the property named |property_name|.
    virtual void GattServicePropertyChanged(const dbus::ObjectPath& object_path,
                                            const std::string& property_name) {}
  };

  BluetoothGattServiceClient(const BluetoothGattServiceClient&) = delete;
  BluetoothGattServiceClient& operator=(const BluetoothGattServiceClient&) =
      delete;

  ~BluetoothGattServiceClient() override;

  // Adds and removes observers for events on all remote GATT services. Check
  // the |object_path| parameter of observer methods to determine which GATT
  // service is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the list of GATT service object paths known to the system.
  virtual std::vector<dbus::ObjectPath> GetServices() = 0;

  // Obtain the properties for the GATT service with object path |object_path|.
  // Values should be copied if needed.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path) = 0;

  // Creates the instance.
  static BluetoothGattServiceClient* Create();

 protected:
  BluetoothGattServiceClient();
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_SERVICE_CLIENT_H_
