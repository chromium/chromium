// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/cast/bluetooth_remote_gatt_service_cast.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "chromecast/device/bluetooth/le/remote_characteristic.h"
#include "chromecast/device/bluetooth/le/remote_service.h"
#include "device/bluetooth/cast/bluetooth_device_cast.h"
#include "device/bluetooth/cast/bluetooth_remote_gatt_characteristic_cast.h"
#include "device/bluetooth/cast/bluetooth_utils.h"

namespace device {

BluetoothRemoteGattServiceCast::BluetoothRemoteGattServiceCast(
    BluetoothDeviceCast* device,
    scoped_refptr<chromecast::bluetooth::RemoteService> remote_service)
    : device_(device), remote_service_(std::move(remote_service)) {
  std::vector<scoped_refptr<chromecast::bluetooth::RemoteCharacteristic>>
      characteristics = remote_service_->GetCharacteristics();
  characteristics_.reserve(characteristics.size());
  for (const auto& characteristic : characteristics) {
    AddCharacteristic(std::make_unique<BluetoothRemoteGattCharacteristicCast>(
        this, characteristic));
  }
  SetDiscoveryComplete(true);
}

BluetoothRemoteGattServiceCast::~BluetoothRemoteGattServiceCast() {}

std::string BluetoothRemoteGattServiceCast::GetIdentifier() const {
  return GetUUID().canonical_value();
}

BluetoothUUID BluetoothRemoteGattServiceCast::GetUUID() const {
  return UuidToBluetoothUUID(remote_service_->uuid());
}

bool BluetoothRemoteGattServiceCast::IsPrimary() const {
  return remote_service_->primary();
}

BluetoothDevice* BluetoothRemoteGattServiceCast::GetDevice() const {
  return device_;
}

std::vector<BluetoothRemoteGattService*>
BluetoothRemoteGattServiceCast::GetIncludedServices() const {
  NOTIMPLEMENTED();
  return std::vector<BluetoothRemoteGattService*>();
}

}  // namespace device
