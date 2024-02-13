// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/cast/bluetooth_remote_gatt_descriptor_cast.h"

#include <stdint.h>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chromecast/device/bluetooth/le/remote_descriptor.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/cast/bluetooth_remote_gatt_characteristic_cast.h"
#include "device/bluetooth/cast/bluetooth_utils.h"

namespace device {

BluetoothRemoteGattDescriptorCast::BluetoothRemoteGattDescriptorCast(
    BluetoothRemoteGattCharacteristicCast* characteristic,
    scoped_refptr<chromecast::bluetooth::RemoteDescriptor> remote_descriptor)
    : characteristic_(characteristic),
      remote_descriptor_(remote_descriptor),
      weak_factory_(this) {}

BluetoothRemoteGattDescriptorCast::~BluetoothRemoteGattDescriptorCast() {}

std::string BluetoothRemoteGattDescriptorCast::GetIdentifier() const {
  return GetUUID().canonical_value();
}

BluetoothUUID BluetoothRemoteGattDescriptorCast::GetUUID() const {
  return UuidToBluetoothUUID(remote_descriptor_->uuid());
}

BluetoothGattCharacteristic::Permissions
BluetoothRemoteGattDescriptorCast::GetPermissions() const {
  NOTIMPLEMENTED();
  return BluetoothGattCharacteristic::Permissions();
}

const std::vector<uint8_t>& BluetoothRemoteGattDescriptorCast::GetValue()
    const {
  return value_;
}

BluetoothRemoteGattCharacteristic*
BluetoothRemoteGattDescriptorCast::GetCharacteristic() const {
  return characteristic_;
}

void BluetoothRemoteGattDescriptorCast::ReadRemoteDescriptor(
    ValueCallback callback) {
  remote_descriptor_->Read(
      base::BindOnce(&BluetoothRemoteGattDescriptorCast::OnReadRemoteDescriptor,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BluetoothRemoteGattDescriptorCast::WriteRemoteDescriptor(
    const std::vector<uint8_t>& new_value,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  remote_descriptor_->Write(
      new_value,
      base::BindOnce(
          &BluetoothRemoteGattDescriptorCast::OnWriteRemoteDescriptor,
          weak_factory_.GetWeakPtr(), new_value, std::move(callback),
          std::move(error_callback)));
}

void BluetoothRemoteGattDescriptorCast::OnReadRemoteDescriptor(
    ValueCallback callback,
    bool success,
    const std::vector<uint8_t>& result) {
  if (success) {
    value_ = result;
    std::move(callback).Run(/*error_code=*/std::nullopt, result);
    return;
  }
  std::move(callback).Run(BluetoothGattService::GattErrorCode::kFailed,
                          /*value=*/std::vector<uint8_t>());
}

void BluetoothRemoteGattDescriptorCast::OnWriteRemoteDescriptor(
    const std::vector<uint8_t>& written_value,
    base::OnceClosure callback,
    ErrorCallback error_callback,
    bool success) {
  if (success) {
    value_ = written_value;
    std::move(callback).Run();
    return;
  }
  std::move(error_callback).Run(BluetoothGattService::GattErrorCode::kFailed);
}

}  // namespace device
