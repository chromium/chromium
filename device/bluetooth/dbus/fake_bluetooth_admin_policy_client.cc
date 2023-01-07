// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_admin_policy_client.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

FakeBluetoothAdminPolicyClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : BluetoothAdminPolicyClient::Properties(
          nullptr,
          bluetooth_admin_policy::kBluetoothAdminPolicyStatusInterface,
          callback) {}

FakeBluetoothAdminPolicyClient::Properties::~Properties() = default;

void FakeBluetoothAdminPolicyClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  DVLOG(1) << "Get " << property->name();
  std::move(callback).Run(false);
}

void FakeBluetoothAdminPolicyClient::Properties::GetAll() {
  DVLOG(1) << "GetAll";
}

void FakeBluetoothAdminPolicyClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  DVLOG(1) << "Set " << property->name();
  std::move(callback).Run(false);
}

FakeBluetoothAdminPolicyClient::FakeBluetoothAdminPolicyClient() = default;

FakeBluetoothAdminPolicyClient::~FakeBluetoothAdminPolicyClient() = default;

void FakeBluetoothAdminPolicyClient::Init(
    dbus::Bus* bus,
    const std::string& bluetooth_service_name) {}

void FakeBluetoothAdminPolicyClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBluetoothAdminPolicyClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeBluetoothAdminPolicyClient::SetServiceAllowList(
    const dbus::ObjectPath& object_path,
    const UUIDList& service_uuids,
    base::OnceClosure callback,
    ErrorCallback error_callback) {}

FakeBluetoothAdminPolicyClient::Properties*
FakeBluetoothAdminPolicyClient::GetProperties(
    const dbus::ObjectPath& object_path) {
  PropertiesMap::const_iterator iter = properties_map_.find(object_path);
  if (iter != properties_map_.end())
    return iter->second.get();
  return nullptr;
}

void FakeBluetoothAdminPolicyClient::CreateAdminPolicy(
    const dbus::ObjectPath& path,
    bool is_blocked_by_policy) {
  DCHECK(!base::Contains(properties_map_, path));

  auto properties = std::make_unique<Properties>(
      base::BindRepeating(&FakeBluetoothAdminPolicyClient::OnPropertyChanged,
                          base::Unretained(this), path));
  properties->is_blocked_by_policy.ReplaceValue(is_blocked_by_policy);
  properties->is_blocked_by_policy.set_valid(true);

  properties_map_.emplace(path, std::move(properties));

  for (auto& observer : observers_)
    observer.AdminPolicyAdded(path);
}

void FakeBluetoothAdminPolicyClient::ChangeAdminPolicy(
    const dbus::ObjectPath& path,
    bool is_blocked_by_policy) {
  DCHECK(base::Contains(properties_map_, path));

  properties_map_[path]->is_blocked_by_policy.ReplaceValue(
      is_blocked_by_policy);

  for (auto& observer : observers_) {
    observer.AdminPolicyPropertyChanged(
        path, bluetooth_admin_policy::kIsBlockedByPolicyProperty);
  }
}

void FakeBluetoothAdminPolicyClient::RemoveAdminPolicy(
    const dbus::ObjectPath& path) {
  DCHECK(base::Contains(properties_map_, path));
  properties_map_.erase(path);

  for (auto& observer : observers_)
    observer.AdminPolicyRemoved(path);
}

void FakeBluetoothAdminPolicyClient::OnPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  DVLOG(2) << "Fake Bluetooth admin_policy property changed: "
           << object_path.value() << ": " << property_name;
  for (auto& observer : observers_)
    observer.AdminPolicyPropertyChanged(object_path, property_name);
}

}  // namespace bluez
