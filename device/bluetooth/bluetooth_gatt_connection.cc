// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_gatt_connection.h"

#include "device/bluetooth/bluetooth_adapter.h"

namespace device {

BluetoothGattConnection::BluetoothGattConnection(
    scoped_refptr<device::BluetoothAdapter> adapter,
    const std::string& device_address)
    : adapter_(adapter), device_address_(device_address) {
  DCHECK(adapter_.get());
  DCHECK(!device_address_.empty());

  device_ = adapter_->GetDevice(device_address_);
  DCHECK(device_);
  owns_reference_for_connection_ = true;
  device_->AddGattConnection(this);
}

BluetoothGattConnection::~BluetoothGattConnection() {
  Disconnect();
}

const std::string& BluetoothGattConnection::GetDeviceAddress() const {
  return device_address_;
}

bool BluetoothGattConnection::IsConnected() {
  if (!owns_reference_for_connection_)
    return false;
  DCHECK(adapter_->GetDevice(device_address_));
  DCHECK(device_->IsGattConnected());
  return true;
}

void BluetoothGattConnection::Disconnect() {
  if (!owns_reference_for_connection_)
    return;

  owns_reference_for_connection_ = false;
  device_->RemoveGattConnection(this);
}

void BluetoothGattConnection::InvalidateConnectionReference() {
  owns_reference_for_connection_ = false;
}

}  // namespace device
