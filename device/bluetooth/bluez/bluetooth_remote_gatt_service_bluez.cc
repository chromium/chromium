// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <utility>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "dbus/property.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/bluez/bluetooth_device_bluez.h"
#include "device/bluetooth/bluez/bluetooth_gatt_characteristic_bluez.h"
#include "device/bluetooth/bluez/bluetooth_remote_gatt_characteristic_bluez.h"
#include "device/bluetooth/bluez/bluetooth_remote_gatt_descriptor_bluez.h"
#include "device/bluetooth/bluez/bluetooth_remote_gatt_service_bluez.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"

namespace bluez {

BluetoothRemoteGattServiceBlueZ::BluetoothRemoteGattServiceBlueZ(
    BluetoothAdapterBlueZ* adapter,
    BluetoothDeviceBlueZ* device,
    const dbus::ObjectPath& object_path)
    : BluetoothGattServiceBlueZ(adapter, object_path), device_(device) {
  DVLOG(1) << "Creating remote GATT service with identifier: "
           << object_path.value();
  DCHECK(GetAdapter());

  bluez::BluezDBusManager::Get()->GetBluetoothGattServiceClient()->AddObserver(
      this);
  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattCharacteristicClient()
      ->AddObserver(this);

  // Add all known GATT characteristics.
  const std::vector<dbus::ObjectPath>& gatt_chars =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattCharacteristicClient()
          ->GetCharacteristics();
  for (auto iter = gatt_chars.begin(); iter != gatt_chars.end(); ++iter)
    GattCharacteristicAdded(*iter);
}

BluetoothRemoteGattServiceBlueZ::~BluetoothRemoteGattServiceBlueZ() {
  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattServiceClient()
      ->RemoveObserver(this);
  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattCharacteristicClient()
      ->RemoveObserver(this);

  // Clean up all the characteristics. Move the characteristics list here and
  // clear the original so that when we send GattCharacteristicRemoved(),
  // GetCharacteristics() returns no characteristics.
  CharacteristicMap characteristics = std::move(characteristics_);
  characteristics_.clear();

  for (const auto& characteristic : characteristics) {
    DCHECK(GetAdapter());
    GetAdapter()->NotifyGattCharacteristicRemoved(characteristic.second.get());
  }
}

device::BluetoothUUID BluetoothRemoteGattServiceBlueZ::GetUUID() const {
  bluez::BluetoothGattServiceClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattServiceClient()
          ->GetProperties(object_path());
  DCHECK(properties);
  return device::BluetoothUUID(properties->uuid.value());
}

bool BluetoothRemoteGattServiceBlueZ::IsPrimary() const {
  bluez::BluetoothGattServiceClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattServiceClient()
          ->GetProperties(object_path());
  DCHECK(properties);
  return properties->primary.value();
}

device::BluetoothDevice* BluetoothRemoteGattServiceBlueZ::GetDevice() const {
  return device_;
}

std::vector<device::BluetoothRemoteGattService*>
BluetoothRemoteGattServiceBlueZ::GetIncludedServices() const {
  // TODO(armansito): Return the actual included services here.
  return std::vector<device::BluetoothRemoteGattService*>();
}

void BluetoothRemoteGattServiceBlueZ::NotifyServiceChanged() {
  // Don't send service changed unless we know that all characteristics have
  // already been discovered. This is to prevent spammy events before sending
  // out the first Gatt
  if (!device_->IsGattServicesDiscoveryComplete())
    return;

  DCHECK(GetAdapter());
  GetAdapter()->NotifyGattServiceChanged(this);
}

void BluetoothRemoteGattServiceBlueZ::NotifyDescriptorAddedOrRemoved(
    BluetoothRemoteGattCharacteristicBlueZ* characteristic,
    BluetoothRemoteGattDescriptorBlueZ* descriptor,
    bool added) {
  DCHECK(characteristic->GetService() == this);
  DCHECK(descriptor->GetCharacteristic() == characteristic);
  DCHECK(GetAdapter());

  if (added) {
    GetAdapter()->NotifyGattDescriptorAdded(descriptor);
    return;
  }

  GetAdapter()->NotifyGattDescriptorRemoved(descriptor);
}

void BluetoothRemoteGattServiceBlueZ::NotifyDescriptorValueChanged(
    BluetoothRemoteGattCharacteristicBlueZ* characteristic,
    BluetoothRemoteGattDescriptorBlueZ* descriptor,
    const std::vector<uint8_t>& value) {
  DCHECK(characteristic->GetService() == this);
  DCHECK(descriptor->GetCharacteristic() == characteristic);
  DCHECK(GetAdapter());
  GetAdapter()->NotifyGattDescriptorValueChanged(descriptor, value);
}

void BluetoothRemoteGattServiceBlueZ::GattServicePropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  if (object_path != this->object_path())
    return;

  DVLOG(1) << "Service property changed: \"" << property_name << "\", "
           << object_path.value();
  bluez::BluetoothGattServiceClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattServiceClient()
          ->GetProperties(object_path);
  DCHECK(properties);

  NotifyServiceChanged();
}

void BluetoothRemoteGattServiceBlueZ::GattCharacteristicAdded(
    const dbus::ObjectPath& object_path) {
  if (base::Contains(characteristics_, object_path.value())) {
    DVLOG(1) << "Remote GATT characteristic already exists: "
             << object_path.value();
    return;
  }

  bluez::BluetoothGattCharacteristicClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattCharacteristicClient()
          ->GetProperties(object_path);
  DCHECK(properties);
  if (properties->service.value() != this->object_path()) {
    DVLOG(2) << "Remote GATT characteristic does not belong to this service.";
    return;
  }

  DVLOG(1) << "Adding new remote GATT characteristic for GATT service: "
           << GetIdentifier() << ", UUID: " << GetUUID().canonical_value();

  // NOTE: Can't use std::make_unique due to private constructor.
  BluetoothRemoteGattCharacteristicBlueZ* characteristic =
      new BluetoothRemoteGattCharacteristicBlueZ(this, object_path);
  AddCharacteristic(base::WrapUnique(characteristic));
  DCHECK(characteristic->GetIdentifier() == object_path.value());
  DCHECK(characteristic->GetUUID().IsValid());

  DCHECK(GetAdapter());
  GetAdapter()->NotifyGattCharacteristicAdded(characteristic);
}

void BluetoothRemoteGattServiceBlueZ::GattCharacteristicRemoved(
    const dbus::ObjectPath& object_path) {
  auto iter = characteristics_.find(object_path.value());
  if (iter == characteristics_.end()) {
    DVLOG(2) << "Unknown GATT characteristic removed: " << object_path.value();
    return;
  }

  DVLOG(1) << "Removing remote GATT characteristic from service: "
           << GetIdentifier() << ", UUID: " << GetUUID().canonical_value();

  auto characteristic = std::move(iter->second);
  DCHECK(
      static_cast<BluetoothRemoteGattCharacteristicBlueZ*>(characteristic.get())
          ->object_path() == object_path);
  characteristics_.erase(iter);

  DCHECK(GetAdapter());
  GetAdapter()->NotifyGattCharacteristicRemoved(characteristic.get());
}

void BluetoothRemoteGattServiceBlueZ::GattCharacteristicPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  auto* characteristic_bluez =
      static_cast<BluetoothRemoteGattCharacteristicBlueZ*>(
          GetCharacteristic(object_path.value()));

  if (!characteristic_bluez) {
    DVLOG(3) << "Properties of unknown characteristic changed";
    return;
  }

  // We may receive a property changed event in certain cases, e.g. when the
  // characteristic "Flags" property has been updated with values from the
  // "Characteristic Extended Properties" descriptor. In this case, kick off
  // a service changed observer event to let observers refresh the
  // characteristics.
  bluez::BluetoothGattCharacteristicClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattCharacteristicClient()
          ->GetProperties(object_path);

  DCHECK(properties);
  DCHECK(GetAdapter());

  if (property_name == properties->flags.name()) {
    NotifyServiceChanged();
  } else if (property_name == properties->value.name()) {
    DCHECK_GE(
        characteristic_bluez->num_of_characteristic_value_read_in_progress_, 0);
    if (characteristic_bluez->num_of_characteristic_value_read_in_progress_ >
        0) {
      --characteristic_bluez->num_of_characteristic_value_read_in_progress_;
    } else {
      GetAdapter()->NotifyGattCharacteristicValueChanged(
          characteristic_bluez, properties->value.value());
    }
  }
}

}  // namespace bluez
