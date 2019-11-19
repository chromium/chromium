// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_remote_gatt_descriptor.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/bluetooth/public/mojom/test/fake_bluetooth.mojom.h"

namespace bluetooth {

FakeRemoteGattDescriptor::FakeRemoteGattDescriptor(
    const std::string& descriptor_id,
    const device::BluetoothUUID& descriptor_uuid,
    device::BluetoothRemoteGattCharacteristic* characteristic)
    : descriptor_id_(descriptor_id),
      descriptor_uuid_(descriptor_uuid),
      characteristic_(characteristic) {}

FakeRemoteGattDescriptor::~FakeRemoteGattDescriptor() = default;

void FakeRemoteGattDescriptor::SetNextReadResponse(
    uint16_t gatt_code,
    const base::Optional<std::vector<uint8_t>>& value) {
  DCHECK(!next_read_response_);
  next_read_response_.emplace(gatt_code, value);
}

void FakeRemoteGattDescriptor::SetNextWriteResponse(uint16_t gatt_code) {
  DCHECK(!next_write_response_);
  next_write_response_.emplace(gatt_code);
}

bool FakeRemoteGattDescriptor::AllResponsesConsumed() {
  return !next_read_response_ && !next_write_response_;
}

std::string FakeRemoteGattDescriptor::GetIdentifier() const {
  return descriptor_id_;
}

device::BluetoothUUID FakeRemoteGattDescriptor::GetUUID() const {
  return descriptor_uuid_;
}

device::BluetoothRemoteGattCharacteristic::Permissions
FakeRemoteGattDescriptor::GetPermissions() const {
  NOTREACHED();
  return device::BluetoothRemoteGattCharacteristic::PERMISSION_NONE;
}

const std::vector<uint8_t>& FakeRemoteGattDescriptor::GetValue() const {
  NOTREACHED();
  return value_;
}

device::BluetoothRemoteGattCharacteristic*
FakeRemoteGattDescriptor::GetCharacteristic() const {
  return characteristic_;
}

void FakeRemoteGattDescriptor::ReadRemoteDescriptor(
    ValueCallback callback,
    ErrorCallback error_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeRemoteGattDescriptor::DispatchReadResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(error_callback)));
}

void FakeRemoteGattDescriptor::WriteRemoteDescriptor(
    const std::vector<uint8_t>& value,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeRemoteGattDescriptor::DispatchWriteResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(error_callback), value));
}

void FakeRemoteGattDescriptor::DispatchReadResponse(
    ValueCallback callback,
    ErrorCallback error_callback) {
  DCHECK(next_read_response_);
  uint16_t gatt_code = next_read_response_->gatt_code();
  base::Optional<std::vector<uint8_t>> value = next_read_response_->value();
  next_read_response_.reset();

  if (gatt_code == mojom::kGATTSuccess) {
    DCHECK(value);
    value_ = std::move(value.value());
    std::move(callback).Run(value_);
    return;
  } else if (gatt_code == mojom::kGATTInvalidHandle) {
    DCHECK(!value);
    std::move(error_callback)
        .Run(device::BluetoothGattService::GATT_ERROR_FAILED);
    return;
  }
}

void FakeRemoteGattDescriptor::DispatchWriteResponse(
    base::OnceClosure callback,
    ErrorCallback error_callback,
    const std::vector<uint8_t>& value) {
  DCHECK(next_write_response_);
  uint16_t gatt_code = next_write_response_.value();
  next_write_response_.reset();

  switch (gatt_code) {
    case mojom::kGATTSuccess:
      last_written_value_ = value;
      std::move(callback).Run();
      break;
    case mojom::kGATTInvalidHandle:
      std::move(error_callback)
          .Run(device::BluetoothGattService::GATT_ERROR_FAILED);
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace bluetooth
