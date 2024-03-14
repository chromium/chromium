// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_gatt_service_floss.h"

#include "base/containers/contains.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_gatt_manager_client.h"

namespace floss {

using GattErrorCode = device::BluetoothGattService::GattErrorCode;

constexpr std::pair<GattStatus, device::BluetoothGattService::GattErrorCode>
    kGattStatusMap[] = {
        {GattStatus::kInvalidAttributeLen, GattErrorCode::kInvalidLength},
        {GattStatus::kReadNotPermitted, GattErrorCode::kNotPermitted},
        {GattStatus::kWriteNotPermitted, GattErrorCode::kNotPermitted},
        {GattStatus::kInsufficientAuthorization, GattErrorCode::kNotAuthorized},
        {GattStatus::kReqNotSupported, GattErrorCode::kNotSupported},
};

BluetoothGattServiceFloss::BluetoothGattServiceFloss(
    BluetoothAdapterFloss* adapter)
    : adapter_(adapter) {
  FlossDBusManager::Get()->GetGattManagerClient()->AddObserver(this);
  FlossDBusManager::Get()->GetGattManagerClient()->AddServerObserver(this);
}

BluetoothGattServiceFloss::~BluetoothGattServiceFloss() {
  FlossDBusManager::Get()->GetGattManagerClient()->RemoveObserver(this);
  FlossDBusManager::Get()->GetGattManagerClient()->RemoveServerObserver(this);
}

BluetoothAdapterFloss* BluetoothGattServiceFloss::GetAdapter() const {
  return adapter_;
}

// static
device::BluetoothGattService::GattErrorCode
BluetoothGattServiceFloss::GattStatusToServiceError(const GattStatus status) {
  DCHECK(status != GattStatus::kSuccess);

  for (auto& [source, target] : kGattStatusMap) {
    if (status == source) {
      return target;
    }
  }

  return GattErrorCode::kUnknown;
}

// static
GattStatus BluetoothGattServiceFloss::GattServiceErrorToStatus(
    device::BluetoothGattService::GattErrorCode error_code) {
  for (auto& [target, source] : kGattStatusMap) {
    if (error_code == source) {
      return target;
    }
  }

  return GattStatus::kError;
}

void BluetoothGattServiceFloss::AddObserverForHandle(
    int32_t handle,
    FlossGattClientObserver* observer) {
  DCHECK(!base::Contains(observer_by_handle_, handle));
  DCHECK(observer);

  if (observer)
    observer_by_handle_[handle] = observer;
}

void BluetoothGattServiceFloss::AddServerObserverForHandle(
    int32_t handle,
    FlossGattServerObserver* observer) {
  DCHECK(!base::Contains(server_observer_by_handle_, handle));
  DCHECK(observer);

  if (observer) {
    server_observer_by_handle_[handle] = observer;
  }
}

void BluetoothGattServiceFloss::RemoveObserverForHandle(int32_t handle) {
  DCHECK(base::Contains(observer_by_handle_, handle));

  observer_by_handle_.erase(handle);
}

void BluetoothGattServiceFloss::RemoveServerObserverForHandle(int32_t handle) {
  if (!base::Contains(server_observer_by_handle_, handle)) {
    return;
  }

  server_observer_by_handle_.erase(handle);
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

void BluetoothGattServiceFloss::GattServerCharacteristicReadRequest(
    std::string address,
    int32_t request_id,
    int32_t offset,
    bool is_long,
    int32_t handle) {
  if (base::Contains(server_observer_by_handle_, handle)) {
    server_observer_by_handle_[handle]->GattServerCharacteristicReadRequest(
        address, request_id, offset, is_long, handle);
  }
}

void BluetoothGattServiceFloss::GattServerDescriptorReadRequest(
    std::string address,
    int32_t request_id,
    int32_t offset,
    bool is_long,
    int32_t handle) {
  if (base::Contains(server_observer_by_handle_, handle)) {
    server_observer_by_handle_[handle]->GattServerDescriptorReadRequest(
        address, request_id, offset, is_long, handle);
  }
}

void BluetoothGattServiceFloss::GattServerCharacteristicWriteRequest(
    std::string address,
    int32_t request_id,
    int32_t offset,
    int32_t length,
    bool is_prepared_write,
    bool needs_response,
    int32_t handle,
    std::vector<uint8_t> value) {
  if (base::Contains(server_observer_by_handle_, handle)) {
    server_observer_by_handle_[handle]->GattServerCharacteristicWriteRequest(
        address, request_id, offset, length, is_prepared_write, needs_response,
        handle, value);
  }
}

void BluetoothGattServiceFloss::GattServerDescriptorWriteRequest(
    std::string address,
    int32_t request_id,
    int32_t offset,
    int32_t length,
    bool is_prepared_write,
    bool needs_response,
    int32_t handle,
    std::vector<uint8_t> value) {
  if (base::Contains(server_observer_by_handle_, handle)) {
    server_observer_by_handle_[handle]->GattServerDescriptorWriteRequest(
        address, request_id, offset, length, is_prepared_write, needs_response,
        handle, value);
  }
}

void BluetoothGattServiceFloss::GattServerExecuteWrite(std::string address,
                                                       int32_t request_id,
                                                       bool execute_write) {
  for (auto const& [_, observer] : server_observer_by_handle_) {
    observer->GattServerExecuteWrite(address, request_id, execute_write);
  }
}

}  // namespace floss
