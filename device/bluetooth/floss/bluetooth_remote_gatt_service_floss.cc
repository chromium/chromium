// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/bluetooth/floss/bluetooth_remote_gatt_service_floss.h"

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
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

  if (primary_) {
    for (GattService& s : remote_service_.included_services) {
      included_services_.push_back(Create(adapter, device, s, false));
    }
  }
}

BluetoothRemoteGattServiceFloss::~BluetoothRemoteGattServiceFloss() {
  characteristics_.clear();
  included_services_.clear();
}

std::string BluetoothRemoteGattServiceFloss::GetIdentifier() const {
  return base::StringPrintf("%s/%d", device_->GetAddress().c_str(),
                            remote_service_.instance_id);
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
  std::vector<device::BluetoothRemoteGattService*> services;

  // TODO(b/193686564) - It's likely that we need to surface this up to
  // BluetoothDevice in some other way and let it take ownership of these
  // services since |GetIncludedServices| doesn't seem to be correctly used
  // everywhere.
  for (const auto& s : included_services_) {
    services.push_back(
        static_cast<device::BluetoothRemoteGattService*>(s.get()));
  }

  return services;
}

}  // namespace floss
