// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/fake_bluetooth_gatt_descriptor_client.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "dbus/bus.h"
#include "dbus/property.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace bluez {

const char FakeBluetoothGattDescriptorClient::
    kClientCharacteristicConfigurationPathComponent[] = "desc0000";
const char FakeBluetoothGattDescriptorClient::
    kClientCharacteristicConfigurationUUID[] =
        "00002902-0000-1000-8000-00805f9b34fb";

FakeBluetoothGattDescriptorClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : BluetoothGattDescriptorClient::Properties(
          NULL,
          bluetooth_gatt_descriptor::kBluetoothGattDescriptorInterface,
          callback) {}

FakeBluetoothGattDescriptorClient::Properties::~Properties() = default;

void FakeBluetoothGattDescriptorClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  DVLOG(1) << "Get " << property->name();
  std::move(callback).Run(true);
}

void FakeBluetoothGattDescriptorClient::Properties::GetAll() {
  DVLOG(1) << "GetAll";
}

void FakeBluetoothGattDescriptorClient::Properties::Set(
    dbus::PropertyBase* property,
    dbus::PropertySet::SetCallback callback) {
  DVLOG(1) << "Set " << property->name();
  std::move(callback).Run(false);
}

FakeBluetoothGattDescriptorClient::DescriptorData::DescriptorData() = default;

FakeBluetoothGattDescriptorClient::DescriptorData::~DescriptorData() = default;

FakeBluetoothGattDescriptorClient::FakeBluetoothGattDescriptorClient() {}

FakeBluetoothGattDescriptorClient::~FakeBluetoothGattDescriptorClient() {
  for (auto iter = properties_.begin(); iter != properties_.end(); iter++)
    delete iter->second;
}

void FakeBluetoothGattDescriptorClient::Init(
    dbus::Bus* bus,
    const std::string& bluetooth_service_name) {}

void FakeBluetoothGattDescriptorClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBluetoothGattDescriptorClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<dbus::ObjectPath>
FakeBluetoothGattDescriptorClient::GetDescriptors() {
  std::vector<dbus::ObjectPath> descriptors;
  for (PropertiesMap::const_iterator iter = properties_.begin();
       iter != properties_.end(); ++iter) {
    descriptors.push_back(iter->first);
  }
  return descriptors;
}

FakeBluetoothGattDescriptorClient::Properties*
FakeBluetoothGattDescriptorClient::GetProperties(
    const dbus::ObjectPath& object_path) {
  PropertiesMap::const_iterator iter = properties_.find(object_path);
  if (iter == properties_.end())
    return NULL;
  return iter->second->properties.get();
}

void FakeBluetoothGattDescriptorClient::ReadValue(
    const dbus::ObjectPath& object_path,
    ValueCallback callback,
    ErrorCallback error_callback) {
  auto iter = properties_.find(object_path);
  if (iter == properties_.end()) {
    std::move(error_callback).Run(kUnknownDescriptorError, "");
    return;
  }

  // Assign the value of the descriptor as necessary
  Properties* properties = iter->second->properties.get();
  if (properties->uuid.value() == kClientCharacteristicConfigurationUUID) {
    BluetoothGattCharacteristicClient::Properties* chrc_props =
        bluez::BluezDBusManager::Get()
            ->GetBluetoothGattCharacteristicClient()
            ->GetProperties(properties->characteristic.value());
    DCHECK(chrc_props);

    uint8_t value_byte = chrc_props->notifying.value() ? 0x01 : 0x00;
    const std::vector<uint8_t>& cur_value = properties->value.value();

    if (cur_value.empty() || cur_value[0] != value_byte) {
      std::vector<uint8_t> value = {value_byte, 0x00};
      properties->value.ReplaceValue(value);
    }
  }

  std::move(callback).Run(/*error_code=*/std::nullopt,
                          iter->second->properties->value.value());
}

void FakeBluetoothGattDescriptorClient::WriteValue(
    const dbus::ObjectPath& object_path,
    const std::vector<uint8_t>& value,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  if (!base::Contains(properties_, object_path)) {
    std::move(error_callback).Run(kUnknownDescriptorError, "");
    return;
  }

  // Since the only fake descriptor is "Client Characteristic Configuration"
  // and BlueZ doesn't allow writing to it, return failure.
  std::move(error_callback)
      .Run(bluetooth_gatt_service::kErrorNotPermitted,
           "Writing to the Client Characteristic Configuration "
           "descriptor not allowed");
}

dbus::ObjectPath FakeBluetoothGattDescriptorClient::ExposeDescriptor(
    const dbus::ObjectPath& characteristic_path,
    const std::string& uuid) {
  if (uuid != kClientCharacteristicConfigurationUUID) {
    DVLOG(2) << "Unsupported UUID: " << uuid;
    return dbus::ObjectPath();
  }

  // CCC descriptor is the only one supported at the moment.
  DCHECK(characteristic_path.IsValid());
  dbus::ObjectPath object_path(characteristic_path.value() + "/" +
                               kClientCharacteristicConfigurationPathComponent);
  DCHECK(object_path.IsValid());
  PropertiesMap::const_iterator iter = properties_.find(object_path);
  if (iter != properties_.end()) {
    DVLOG(1) << "Descriptor already exposed: " << object_path.value();
    return dbus::ObjectPath();
  }

  Properties* properties = new Properties(
      base::BindRepeating(&FakeBluetoothGattDescriptorClient::OnPropertyChanged,
                          weak_ptr_factory_.GetWeakPtr(), object_path));
  properties->uuid.ReplaceValue(uuid);
  properties->characteristic.ReplaceValue(characteristic_path);

  DescriptorData* data = new DescriptorData();
  data->properties.reset(properties);

  properties_[object_path] = data;

  NotifyDescriptorAdded(object_path);

  return object_path;
}

void FakeBluetoothGattDescriptorClient::HideDescriptor(
    const dbus::ObjectPath& descriptor_path) {
  auto iter = properties_.find(descriptor_path);
  if (iter == properties_.end()) {
    DVLOG(1) << "Descriptor not exposed: " << descriptor_path.value();
    return;
  }

  NotifyDescriptorRemoved(descriptor_path);

  delete iter->second;
  properties_.erase(iter);
}

void FakeBluetoothGattDescriptorClient::OnPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  DVLOG(2) << "Descriptor property changed: " << object_path.value() << ": "
           << property_name;

  for (auto& observer : observers_)
    observer.GattDescriptorPropertyChanged(object_path, property_name);
}

void FakeBluetoothGattDescriptorClient::NotifyDescriptorAdded(
    const dbus::ObjectPath& object_path) {
  for (auto& observer : observers_)
    observer.GattDescriptorAdded(object_path);
}

void FakeBluetoothGattDescriptorClient::NotifyDescriptorRemoved(
    const dbus::ObjectPath& object_path) {
  for (auto& observer : observers_)
    observer.GattDescriptorRemoved(object_path);
}

}  // namespace bluez
