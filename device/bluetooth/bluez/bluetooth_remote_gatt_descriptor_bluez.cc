// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_remote_gatt_descriptor_bluez.h"

#include <iterator>
#include <ostream>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluez/bluetooth_gatt_characteristic_bluez.h"
#include "device/bluetooth/bluez/bluetooth_gatt_descriptor_bluez.h"
#include "device/bluetooth/bluez/bluetooth_gatt_service_bluez.h"
#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"

namespace bluez {

namespace {

// Stream operator for logging vector<uint8_t>.
std::ostream& operator<<(std::ostream& out, const std::vector<uint8_t> bytes) {
  out << "[";
  for (auto iter = bytes.begin(); iter != bytes.end(); ++iter) {
    out << base::StringPrintf("%02X", *iter);
  }
  return out << "]";
}

}  // namespace

BluetoothRemoteGattDescriptorBlueZ::BluetoothRemoteGattDescriptorBlueZ(
    BluetoothRemoteGattCharacteristicBlueZ* characteristic,
    const dbus::ObjectPath& object_path)
    : BluetoothGattDescriptorBlueZ(object_path),
      characteristic_(characteristic) {
  DVLOG(1) << "Creating remote GATT descriptor with identifier: "
           << GetIdentifier() << ", UUID: " << GetUUID().canonical_value();
}

BluetoothRemoteGattDescriptorBlueZ::~BluetoothRemoteGattDescriptorBlueZ() =
    default;

device::BluetoothUUID BluetoothRemoteGattDescriptorBlueZ::GetUUID() const {
  bluez::BluetoothGattDescriptorClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattDescriptorClient()
          ->GetProperties(object_path());
  DCHECK(properties);
  return device::BluetoothUUID(properties->uuid.value());
}

const std::vector<uint8_t>& BluetoothRemoteGattDescriptorBlueZ::GetValue()
    const {
  bluez::BluetoothGattDescriptorClient::Properties* properties =
      bluez::BluezDBusManager::Get()
          ->GetBluetoothGattDescriptorClient()
          ->GetProperties(object_path());

  DCHECK(properties);

  return properties->value.value();
}

device::BluetoothRemoteGattCharacteristic*
BluetoothRemoteGattDescriptorBlueZ::GetCharacteristic() const {
  return characteristic_;
}

device::BluetoothRemoteGattCharacteristic::Permissions
BluetoothRemoteGattDescriptorBlueZ::GetPermissions() const {
  // TODO(armansito): Once BlueZ defines the permissions, return the correct
  // values here.
  return device::BluetoothRemoteGattCharacteristic::PERMISSION_NONE;
}

void BluetoothRemoteGattDescriptorBlueZ::ReadRemoteDescriptor(
    ValueCallback callback) {
  DVLOG(1) << "Sending GATT characteristic descriptor read request to "
           << "descriptor: " << GetIdentifier()
           << ", UUID: " << GetUUID().canonical_value();

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  bluez::BluezDBusManager::Get()->GetBluetoothGattDescriptorClient()->ReadValue(
      object_path(), std::move(split_callback.first),
      base::BindOnce(&BluetoothRemoteGattDescriptorBlueZ::OnReadError,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.second)));
}

void BluetoothRemoteGattDescriptorBlueZ::WriteRemoteDescriptor(
    const std::vector<uint8_t>& new_value,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  DVLOG(1) << "Sending GATT characteristic descriptor write request to "
           << "characteristic: " << GetIdentifier()
           << ", UUID: " << GetUUID().canonical_value()
           << ", with value: " << new_value << ".";

  bluez::BluezDBusManager::Get()
      ->GetBluetoothGattDescriptorClient()
      ->WriteValue(object_path(), new_value, std::move(callback),
                   base::BindOnce(&BluetoothRemoteGattDescriptorBlueZ::OnError,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(error_callback)));
}

void BluetoothRemoteGattDescriptorBlueZ::OnReadError(
    ValueCallback callback,
    const std::string& error_name,
    const std::string& error_message) {
  DVLOG(1) << "Operation failed: " << error_name
           << ", message: " << error_message;

  std::move(callback).Run(
      BluetoothGattServiceBlueZ::DBusErrorToServiceError(error_name),
      /*value=*/std::vector<uint8_t>());
}

void BluetoothRemoteGattDescriptorBlueZ::OnError(
    ErrorCallback error_callback,
    const std::string& error_name,
    const std::string& error_message) {
  DVLOG(1) << "Operation failed: " << error_name
           << ", message: " << error_message;

  std::move(error_callback)
      .Run(BluetoothGattServiceBlueZ::DBusErrorToServiceError(error_name));
}

}  // namespace bluez
