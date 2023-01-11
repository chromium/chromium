// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADMIN_POLICY_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADMIN_POLICY_CLIENT_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluez_dbus_client.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace bluez {

// BluetoothAdminPolicyClient is used to communicate with objects on BlueZ's
// org.bluez.AdminPolicy1 interface.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdminPolicyClient
    : public BluezDBusClient {
 public:
  // Creates the instance.
  static std::unique_ptr<BluetoothAdminPolicyClient> Create();

  // Structure of properties on org.bluez.AdminPolicy1 interface.
  struct Properties : public dbus::PropertySet {
    // The current value of service allow list, each service is described in
    // UUID-128 string, Read-only.
    dbus::Property<std::vector<std::string>> service_allow_list;

    // Whether the device is blocked by an admin policy, read-only.
    dbus::Property<bool> is_blocked_by_policy;

    Properties(dbus::ObjectProxy* object_proxy,
               const std::string& interface_name,
               const PropertyChangedCallback& callback);
    ~Properties() override;
  };

  // Interface for observing changes of AdminPolicy1.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when the AdminPolicy with object path |object_path| is added
    virtual void AdminPolicyAdded(const dbus::ObjectPath& object_path) = 0;

    // Called when the AdminPolicy with object path |object_path| is removed
    virtual void AdminPolicyRemoved(const dbus::ObjectPath& object_path) = 0;

    // Called when the AdminPolicy with object path |object_path| has a
    // change in value of the property named |property_name|.
    virtual void AdminPolicyPropertyChanged(
        const dbus::ObjectPath& object_path,
        const std::string& property_name) = 0;
  };

  ~BluetoothAdminPolicyClient() override;

  // Adds and removes observers for events on all local AdminPolicy.
  // Check the |object_path| parameter of observer methods to determine which
  // AdminPolicy is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Obtains the properties for the admin policy with object path |object_path|,
  // any values should be copied if needed.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path) = 0;

  // The ErrorCallback is used by adapter methods to indicate failure.
  // It receives two arguments: the name of the error in |error_name| and
  // an optional message in |error_message|.
  using ErrorCallback =
      base::OnceCallback<void(const std::string& error_name,
                              const std::string& error_message)>;

  using UUIDList = std::vector<device::BluetoothUUID>;

  // Sets ServiceAllowList to AdminPolicy with |object_path|
  virtual void SetServiceAllowList(const dbus::ObjectPath& object_path,
                                   const UUIDList& service_uuids,
                                   base::OnceClosure callback,
                                   ErrorCallback error_callback) = 0;

 protected:
  BluetoothAdminPolicyClient();
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADMIN_POLICY_CLIENT_H_
