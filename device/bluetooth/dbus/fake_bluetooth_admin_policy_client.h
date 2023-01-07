// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_ADMIN_POLICY_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_ADMIN_POLICY_CLIENT_H_

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "base/observer_list.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_admin_policy_client.h"

namespace bluez {

// FakeBluetoothAdminPolicyClient simulates the behavior of the Bluetooth Daemon
// admin policy objects and is used both in test cases in place of a mock and on
// the Linux desktop.
class DEVICE_BLUETOOTH_EXPORT FakeBluetoothAdminPolicyClient
    : public BluetoothAdminPolicyClient {
 public:
  struct Properties : public BluetoothAdminPolicyClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback);
    ~Properties() override;

    // dbus::PropertySet override
    void Get(dbus::PropertyBase* property,
             dbus::PropertySet::GetCallback callback) override;
    void GetAll() override;
    void Set(dbus::PropertyBase* property,
             dbus::PropertySet::SetCallback callback) override;
  };

  FakeBluetoothAdminPolicyClient();
  ~FakeBluetoothAdminPolicyClient() override;

  // BluetoothAdminPolicyClient overrides
  void Init(dbus::Bus* bus, const std::string& bluetooth_service_name) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  Properties* GetProperties(const dbus::ObjectPath& object_path) override;
  void SetServiceAllowList(const dbus::ObjectPath& object_path,
                           const UUIDList& service_uuids,
                           base::OnceClosure callback,
                           ErrorCallback error_callback) override;

  void CreateAdminPolicy(const dbus::ObjectPath& object_path,
                         bool is_blocked_by_policy);
  void ChangeAdminPolicy(const dbus::ObjectPath& object_path,
                         bool is_blocked_by_policy);
  void RemoveAdminPolicy(const dbus::ObjectPath& object_path);

 private:
  // Property callback passed when we create Properties* structures.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name);

  // List of observers interested in event notifications from us.
  base::ObserverList<Observer>::Unchecked observers_;

  using PropertiesMap =
      std::map<const dbus::ObjectPath, std::unique_ptr<Properties>>;
  PropertiesMap properties_map_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_ADMIN_POLICY_CLIENT_H_
