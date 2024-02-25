// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_DESCRIPTOR_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_DESCRIPTOR_CLIENT_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/dbus/bluez_dbus_client.h"

namespace bluez {

// BluetoothGattDescriptorClient is used to communicate with remote GATT
// characteristic descriptor objects exposed by the Bluetooth daemon.
class DEVICE_BLUETOOTH_EXPORT BluetoothGattDescriptorClient
    : public BluezDBusClient {
 public:
  // Structure of properties associated with GATT descriptors.
  struct Properties : public dbus::PropertySet {
    // The 128-bit characteristic descriptor UUID. [read-only]
    dbus::Property<std::string> uuid;

    // Object path of the GATT characteristic the descriptor belongs to.
    // [read-only]
    dbus::Property<dbus::ObjectPath> characteristic;

    // The cached value of the descriptor. This property gets updated only after
    // a successful read request. [read-only]
    dbus::Property<std::vector<uint8_t>> value;

    Properties(dbus::ObjectProxy* object_proxy,
               const std::string& interface_name,
               const PropertyChangedCallback& callback);
    ~Properties() override;
  };

  // Interface for observing changes from a remote GATT characteristic
  // descriptor.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the GATT descriptor with object path |object_path| is added
    // to the system.
    virtual void GattDescriptorAdded(const dbus::ObjectPath& object_path) {}

    // Called when the GATT descriptor with object path |object_path| is removed
    // from the system.
    virtual void GattDescriptorRemoved(const dbus::ObjectPath& object_path) {}

    // Called when the GATT descriptor with object path |object_path| has a
    // change in the value of the property named |property_name|.
    virtual void GattDescriptorPropertyChanged(
        const dbus::ObjectPath& object_path,
        const std::string& property_name) {}
  };

  // Callbacks used to report the result of asynchronous methods.
  using ErrorCallback =
      base::OnceCallback<void(const std::string& error_name,
                              const std::string& error_message)>;
  using ValueCallback = base::OnceCallback<void(
      std::optional<device::BluetoothGattService::GattErrorCode> error_code,
      const std::vector<uint8_t>& value)>;

  BluetoothGattDescriptorClient(const BluetoothGattDescriptorClient&) = delete;
  BluetoothGattDescriptorClient& operator=(
      const BluetoothGattDescriptorClient&) = delete;

  ~BluetoothGattDescriptorClient() override;

  // Adds and removes observers for events on all remote GATT descriptors. Check
  // the |object_path| parameter of observer methods to determine which GATT
  // descriptor is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the list of GATT descriptor object paths known to the system.
  virtual std::vector<dbus::ObjectPath> GetDescriptors() = 0;

  // Obtain the properties for the GATT descriptor with object path
  // |object_path|. Values should be copied if needed.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path) = 0;

  // Issues a request to read the value of GATT descriptor with object path
  // |object_path| and returns the value in |callback| on success. On error,
  // invokes |error_callback|.
  virtual void ReadValue(const dbus::ObjectPath& object_path,
                         ValueCallback callback,
                         ErrorCallback error_callback) = 0;

  // Issues a request to write the value of GATT descriptor with object path
  // |object_path| with value |value|. Invokes |callback| on success and
  // |error_callback| on failure.
  virtual void WriteValue(const dbus::ObjectPath& object_path,
                          const std::vector<uint8_t>& value,
                          base::OnceClosure callback,
                          ErrorCallback error_callback) = 0;

  // Creates the instance.
  static BluetoothGattDescriptorClient* Create();

  // Constants used to indicate exceptional error conditions.
  static const char kNoResponseError[];
  static const char kUnknownDescriptorError[];

 protected:
  BluetoothGattDescriptorClient();
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_DESCRIPTOR_CLIENT_H_
