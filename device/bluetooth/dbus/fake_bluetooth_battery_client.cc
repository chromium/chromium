// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_battery_client.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

FakeBluetoothBatteryClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : BluetoothBatteryClient::Properties(
          nullptr,
          bluetooth_battery::kBluetoothBatteryInterface,
          callback) {}

FakeBluetoothBatteryClient::Properties::~Properties() = default;

void FakeBluetoothBatteryClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  DVLOG(1) << "Get " << property->name();
  std::move(callback).Run(false);
}

void FakeBluetoothBatteryClient::Properties::GetAll() {
  DVLOG(1) << "GetAll";
}

void FakeBluetoothBatteryClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  DVLOG(1) << "Set " << property->name();
  std::move(callback).Run(false);
}

FakeBluetoothBatteryClient::FakeBluetoothBatteryClient() = default;

FakeBluetoothBatteryClient::~FakeBluetoothBatteryClient() = default;

void FakeBluetoothBatteryClient::CreateBattery(const dbus::ObjectPath& path,
                                               uint8_t percentage) {
  DCHECK(!base::Contains(battery_list_, path));

  auto properties = std::make_unique<Properties>(
      base::BindRepeating(&FakeBluetoothBatteryClient::OnPropertyChanged,
                          base::Unretained(this), path));
  properties->percentage.ReplaceValue(percentage);
  properties->percentage.set_valid(true);

  properties_map_.insert(std::make_pair(path, std::move(properties)));
  battery_list_.push_back(path);

  for (auto& observer : observers_)
    observer.BatteryAdded(path);
}

void FakeBluetoothBatteryClient::ChangeBatteryPercentage(
    const dbus::ObjectPath& path,
    uint8_t percentage) {
  DCHECK(base::Contains(battery_list_, path));
  DCHECK(base::Contains(properties_map_, path));

  properties_map_[path]->percentage.ReplaceValue(percentage);

  for (auto& observer : observers_) {
    observer.BatteryPropertyChanged(path,
                                    bluetooth_battery::kPercentageProperty);
  }
}

void FakeBluetoothBatteryClient::RemoveBattery(const dbus::ObjectPath& path) {
  DCHECK(base::Contains(battery_list_, path));

  properties_map_.erase(path);
  battery_list_.erase(base::ranges::find(battery_list_, path));

  for (auto& observer : observers_)
    observer.BatteryRemoved(path);
}

void FakeBluetoothBatteryClient::Init(
    dbus::Bus* bus,
    const std::string& bluetooth_service_name) {}

void FakeBluetoothBatteryClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBluetoothBatteryClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

FakeBluetoothBatteryClient::Properties*
FakeBluetoothBatteryClient::GetProperties(const dbus::ObjectPath& object_path) {
  PropertiesMap::const_iterator iter = properties_map_.find(object_path);
  if (iter != properties_map_.end())
    return iter->second.get();
  return nullptr;
}

void FakeBluetoothBatteryClient::OnPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  DVLOG(2) << "Fake Bluetooth battery property changed: " << object_path.value()
           << ": " << property_name;
  for (auto& observer : observers_)
    observer.BatteryPropertyChanged(object_path, property_name);
}

}  // namespace bluez
