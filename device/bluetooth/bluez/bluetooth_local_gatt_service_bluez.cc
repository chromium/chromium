// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_local_gatt_service_bluez.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"

namespace bluez {

// static
base::WeakPtr<BluetoothLocalGattServiceBlueZ>
BluetoothLocalGattServiceBlueZ::Create(
    BluetoothAdapterBlueZ* adapter,
    const device::BluetoothUUID& uuid,
    bool is_primary,
    device::BluetoothLocalGattService::Delegate* delegate) {
  auto* service =
      new BluetoothLocalGattServiceBlueZ(adapter, uuid, is_primary, delegate);
  auto weak_ptr = service->weak_ptr_factory_.GetWeakPtr();
  adapter->AddLocalGattService(base::WrapUnique(service));
  return weak_ptr;
}

BluetoothLocalGattServiceBlueZ::BluetoothLocalGattServiceBlueZ(
    BluetoothAdapterBlueZ* adapter,
    const device::BluetoothUUID& uuid,
    bool is_primary,
    device::BluetoothLocalGattService::Delegate* delegate)
    : BluetoothGattServiceBlueZ(
          adapter,
          AddGuidToObjectPath(adapter->GetApplicationObjectPath().value() +
                              "/service")),
      uuid_(uuid),
      is_primary_(is_primary),
      delegate_(delegate) {
  DVLOG(1) << "Creating local GATT service with identifier: "
           << GetIdentifier();
}

BluetoothLocalGattServiceBlueZ::~BluetoothLocalGattServiceBlueZ() = default;

device::BluetoothUUID BluetoothLocalGattServiceBlueZ::GetUUID() const {
  return uuid_;
}

bool BluetoothLocalGattServiceBlueZ::IsPrimary() const {
  return is_primary_;
}

void BluetoothLocalGattServiceBlueZ::Register(base::OnceClosure callback,
                                              ErrorCallback error_callback) {
  GetAdapter()->RegisterGattService(this, std::move(callback),
                                    std::move(error_callback));
}

void BluetoothLocalGattServiceBlueZ::Unregister(base::OnceClosure callback,
                                                ErrorCallback error_callback) {
  DCHECK(GetAdapter());
  GetAdapter()->UnregisterGattService(this, std::move(callback),
                                      std::move(error_callback));
}

bool BluetoothLocalGattServiceBlueZ::IsRegistered() {
  return GetAdapter()->IsGattServiceRegistered(this);
}

void BluetoothLocalGattServiceBlueZ::Delete() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  GetAdapter()->RemoveLocalGattService(this);
}

device::BluetoothLocalGattCharacteristic*
BluetoothLocalGattServiceBlueZ::GetCharacteristic(
    const std::string& identifier) {
  const auto& service = characteristics_.find(dbus::ObjectPath(identifier));
  return service == characteristics_.end() ? nullptr : service->second.get();
}

base::WeakPtr<device::BluetoothLocalGattCharacteristic>
BluetoothLocalGattServiceBlueZ::CreateCharacteristic(
    const device::BluetoothUUID& uuid,
    device::BluetoothGattCharacteristic::Properties properties,
    device::BluetoothGattCharacteristic::Permissions permissions) {
  return bluez::BluetoothLocalGattCharacteristicBlueZ::Create(
      uuid, properties, permissions, /*service=*/this);
}

const std::map<dbus::ObjectPath,
               std::unique_ptr<BluetoothLocalGattCharacteristicBlueZ>>&
BluetoothLocalGattServiceBlueZ::GetCharacteristics() const {
  return characteristics_;
}

// static
dbus::ObjectPath BluetoothLocalGattServiceBlueZ::AddGuidToObjectPath(
    const std::string& path) {
  std::string GuidString = base::Uuid::GenerateRandomV4().AsLowercaseString();
  base::RemoveChars(GuidString, "-", &GuidString);

  return dbus::ObjectPath(path + GuidString);
}

void BluetoothLocalGattServiceBlueZ::AddCharacteristic(
    std::unique_ptr<BluetoothLocalGattCharacteristicBlueZ> characteristic) {
  characteristics_[characteristic->object_path()] = std::move(characteristic);
}

}  // namespace bluez
