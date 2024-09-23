// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_LE_ADVERTISING_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_LE_ADVERTISING_MANAGER_CLIENT_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluez_dbus_client.h"

namespace bluez {

// BluetoothAdvertisingManagerClient is used to communicate with the advertising
// manager object of the BlueZ daemon.
class DEVICE_BLUETOOTH_EXPORT BluetoothLEAdvertisingManagerClient
    : public BluezDBusClient {
 public:
  // Interface for observing changes to advertising managers.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when an advertising manager with object path |object_path| is
    // added to the system.
    virtual void AdvertisingManagerAdded(const dbus::ObjectPath& object_path) {}

    // Called when an advertising manager with object path |object_path| is
    // removed from the system.
    virtual void AdvertisingManagerRemoved(
        const dbus::ObjectPath& object_path) {}
  };

  BluetoothLEAdvertisingManagerClient(
      const BluetoothLEAdvertisingManagerClient&) = delete;
  BluetoothLEAdvertisingManagerClient& operator=(
      const BluetoothLEAdvertisingManagerClient&) = delete;

  ~BluetoothLEAdvertisingManagerClient() override;

  // Adds and removes observers for events which change the advertising
  // managers on the system.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // The ErrorCallback is used by advertising manager methods to indicate
  // failure. It receives two arguments: the name of the error in |error_name|
  // and an optional message in |error_message|.
  using ErrorCallback =
      base::OnceCallback<void(const std::string& error_name,
                              const std::string& error_message)>;

  // Registers an advertisement with the DBus object path
  // |advertisement_object_path| with BlueZ's advertising manager.
  virtual void RegisterAdvertisement(
      const dbus::ObjectPath& manager_object_path,
      const dbus::ObjectPath& advertisement_object_path,
      base::OnceClosure callback,
      ErrorCallback error_callback) = 0;

  // Unregisters an advertisement with the DBus object path
  // |advertisement_object_path| with BlueZ's advertising manager.
  virtual void UnregisterAdvertisement(
      const dbus::ObjectPath& manager_object_path,
      const dbus::ObjectPath& advertisement_object_path,
      base::OnceClosure callback,
      ErrorCallback error_callback) = 0;

  // Set's the advertising interval.
  virtual void SetAdvertisingInterval(
      const dbus::ObjectPath& manager_object_path,
      uint16_t min_interval_ms,
      uint16_t max_interval_ms,
      base::OnceClosure callback,
      ErrorCallback error_callback) = 0;

  // Resets advertising.
  virtual void ResetAdvertising(const dbus::ObjectPath& manager_object_path,
                                base::OnceClosure callback,
                                ErrorCallback error_callback) = 0;

  // Creates the instance.
  static BluetoothLEAdvertisingManagerClient* Create();

  // Constants used to indicate exceptional error conditions.
  static const char kNoResponseError[];

  // Structure of properties associated with bluetooth adapters.
  struct Properties : public dbus::PropertySet {
    // The supported Advertising features. Read-only.
    dbus::Property<std::vector<std::string>> supported_features;

    Properties(dbus::ObjectProxy* object_proxy,
               const std::string& interface_name,
               const PropertyChangedCallback& callback);
    ~Properties() override;
  };

  // Obtain the properties for the advertisingManager with object path
  // |object_path|, any values should be copied if needed.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path) = 0;

 protected:
  BluetoothLEAdvertisingManagerClient();
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_LE_ADVERTISING_MANAGER_CLIENT_H_
