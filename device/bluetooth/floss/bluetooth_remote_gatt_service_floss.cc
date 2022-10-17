// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/bluetooth/floss/bluetooth_remote_gatt_service_floss.h"

#include "base/memory/ptr_util.h"
#include "device/bluetooth/floss/bluetooth_device_floss.h"
#include "device/bluetooth/floss/bluetooth_remote_gatt_characteristic_floss.h"

namespace floss {

// static
std::unique_ptr<BluetoothRemoteGattServiceFloss>
BluetoothRemoteGattServiceFloss::Create(BluetoothAdapterFloss* adapter,
                                        BluetoothDeviceFloss* device,
                                        GattService remote_service,
                                        bool primary) {
  return base::WrapUnique(new BluetoothRemoteGattServiceFloss(
      adapter, device, remote_service, primary));
}

BluetoothRemoteGattServiceFloss::BluetoothRemoteGattServiceFloss(
    BluetoothAdapterFloss* adapter,
    BluetoothDeviceFloss* device,
    GattService remote_service,
    bool primary)
    : BluetoothGattServiceFloss(adapter),
      primary_(primary),
      remote_service_(remote_service),
      device_(device) {
  for (GattCharacteristic& c : remote_service_.characteristics) {
    AddCharacteristic(BluetoothRemoteGattCharacteristicFloss::Create(this, &c));
  }
}

BluetoothRemoteGattServiceFloss::~BluetoothRemoteGattServiceFloss() = default;

std::string BluetoothRemoteGattServiceFloss::GetIdentifier() const {
  return device_->GetAddress() + remote_service_.uuid.canonical_value();
}

device::BluetoothUUID BluetoothRemoteGattServiceFloss::GetUUID() const {
  return remote_service_.uuid;
}

device::BluetoothDevice* BluetoothRemoteGattServiceFloss::GetDevice() const {
  return static_cast<device::BluetoothDevice*>(device_.get());
}

bool BluetoothRemoteGattServiceFloss::IsPrimary() const {
  return primary_;
}

std::vector<device::BluetoothRemoteGattService*>
BluetoothRemoteGattServiceFloss::GetIncludedServices() const {
  // TODO(b/193686564) - Iterate secondary services and create more.
  return {};
}

}  // namespace floss
