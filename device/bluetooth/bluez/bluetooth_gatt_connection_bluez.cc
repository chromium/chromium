// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_gatt_connection_bluez.h"

#include "base/bind.h"
#include "base/logging.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"

namespace bluez {

BluetoothGattConnectionBlueZ::BluetoothGattConnectionBlueZ(
    scoped_refptr<device::BluetoothAdapter> adapter,
    const std::string& device_address,
    const dbus::ObjectPath& object_path)
    : BluetoothGattConnection(adapter.get(), device_address),
      connected_(true),
      object_path_(object_path) {
  DCHECK(adapter_.get());
  DCHECK(!device_address_.empty());
  DCHECK(object_path_.IsValid());

  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->AddObserver(this);
}

BluetoothGattConnectionBlueZ::~BluetoothGattConnectionBlueZ() {
  bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->RemoveObserver(
      this);
  Disconnect();
}

bool BluetoothGattConnectionBlueZ::IsConnected() {
  // Lazily determine the activity state of the connection. If already
  // marked as inactive, then return false. Otherwise, explicitly mark
  // |connected_| as false if the device is removed or disconnected. We do this,
  // so that if this method is called during a call to DeviceRemoved or
  // DeviceChanged somewhere else, it returns the correct status.
  if (!connected_)
    return false;

  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);
  if (!properties || !properties->connected.value())
    connected_ = false;

  return connected_;
}

void BluetoothGattConnectionBlueZ::Disconnect() {
  if (!connected_) {
    DVLOG(1) << "Connection already inactive.";
    return;
  }

  connected_ = false;
  BluetoothGattConnection::Disconnect();
}

void BluetoothGattConnectionBlueZ::DeviceRemoved(
    const dbus::ObjectPath& object_path) {
  if (object_path != object_path_)
    return;

  connected_ = false;
}

void BluetoothGattConnectionBlueZ::DevicePropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  if (object_path != object_path_)
    return;

  if (!connected_)
    return;

  bluez::BluetoothDeviceClient::Properties* properties =
      bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient()->GetProperties(
          object_path_);

  if (!properties) {
    connected_ = false;
    return;
  }

  if (property_name == properties->connected.name() &&
      !properties->connected.value())
    connected_ = false;

  // The remote device's bluetooth address may change if it is paired while
  // connected.
  if (property_name == properties->address.name())
    device_address_ = properties->address.value();
}

}  // namespace bluez
