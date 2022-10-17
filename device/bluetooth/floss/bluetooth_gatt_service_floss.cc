// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_gatt_service_floss.h"

#include "base/containers/contains.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_gatt_client.h"

namespace floss {

BluetoothGattServiceFloss::BluetoothGattServiceFloss(
    BluetoothAdapterFloss* adapter)
    : adapter_(adapter) {
  FlossDBusManager::Get()->GetGattClient()->AddObserver(this);
}

BluetoothGattServiceFloss::~BluetoothGattServiceFloss() {
  FlossDBusManager::Get()->GetGattClient()->RemoveObserver(this);
}

BluetoothAdapterFloss* BluetoothGattServiceFloss::GetAdapter() const {
  return adapter_;
}

// static
device::BluetoothGattService::GattErrorCode
BluetoothGattServiceFloss::GattStatusToServiceError(const GattStatus status) {
  DCHECK(status != GattStatus::kSuccess);

  // TODO(b/193686564) - Translate remote service gatt errors to correct values.
  return GattErrorCode::kUnknown;
}

void BluetoothGattServiceFloss::AddObserverForHandle(
    int32_t handle,
    FlossGattClientObserver* observer) {
  DCHECK(!base::Contains(observer_by_handle_, handle));
  DCHECK(observer);

  if (observer)
    observer_by_handle_[handle] = observer;
}

void BluetoothGattServiceFloss::RemoveObserverForHandle(int32_t handle) {
  DCHECK(base::Contains(observer_by_handle_, handle));

  observer_by_handle_.erase(handle);
}

void BluetoothGattServiceFloss::GattCharacteristicRead(
    std::string address,
    GattStatus status,
    int32_t handle,
    const std::vector<uint8_t>& data) {
  if (base::Contains(observer_by_handle_, handle)) {
    observer_by_handle_[handle]->GattCharacteristicRead(address, status, handle,
                                                        data);
  }
}

void BluetoothGattServiceFloss::GattCharacteristicWrite(std::string address,
                                                        GattStatus status,
                                                        int32_t handle) {
  if (base::Contains(observer_by_handle_, handle)) {
    observer_by_handle_[handle]->GattCharacteristicWrite(address, status,
                                                         handle);
  }
}
void BluetoothGattServiceFloss::GattDescriptorRead(
    std::string address,
    GattStatus status,
    int32_t handle,
    const std::vector<uint8_t>& data) {
  if (base::Contains(observer_by_handle_, handle)) {
    observer_by_handle_[handle]->GattDescriptorRead(address, status, handle,
                                                    data);
  }
}

void BluetoothGattServiceFloss::GattDescriptorWrite(std::string address,
                                                    GattStatus status,
                                                    int32_t handle) {
  if (base::Contains(observer_by_handle_, handle)) {
    observer_by_handle_[handle]->GattDescriptorWrite(address, status, handle);
  }
}

void BluetoothGattServiceFloss::GattNotify(std::string address,
                                           int32_t handle,
                                           const std::vector<uint8_t>& data) {
  if (base::Contains(observer_by_handle_, handle)) {
    observer_by_handle_[handle]->GattNotify(address, handle, data);
  }
}

}  // namespace floss
