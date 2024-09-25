// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/emulation/fake_remote_gatt_descriptor.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "device/bluetooth/public/mojom/emulation/fake_bluetooth.mojom.h"

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
    const std::optional<std::vector<uint8_t>>& value) {
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
}

const std::vector<uint8_t>& FakeRemoteGattDescriptor::GetValue() const {
  NOTREACHED();
}

device::BluetoothRemoteGattCharacteristic*
FakeRemoteGattDescriptor::GetCharacteristic() const {
  return characteristic_;
}

void FakeRemoteGattDescriptor::ReadRemoteDescriptor(ValueCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeRemoteGattDescriptor::DispatchReadResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FakeRemoteGattDescriptor::WriteRemoteDescriptor(
    const std::vector<uint8_t>& value,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeRemoteGattDescriptor::DispatchWriteResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(error_callback), value));
}

void FakeRemoteGattDescriptor::DispatchReadResponse(ValueCallback callback) {
  DCHECK(next_read_response_);
  uint16_t gatt_code = next_read_response_->gatt_code();
  std::optional<std::vector<uint8_t>> value = next_read_response_->value();
  next_read_response_.reset();

  switch (gatt_code) {
    case mojom::kGATTSuccess:
      DCHECK(value);
      value_ = std::move(value.value());
      std::move(callback).Run(/*error_code=*/std::nullopt, value_);
      break;
    case mojom::kGATTInvalidHandle:
      DCHECK(!value);
      std::move(callback).Run(
          device::BluetoothGattService::GattErrorCode::kFailed,
          /*value=*/std::vector<uint8_t>());
      break;
    default:
      NOTREACHED();
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
          .Run(device::BluetoothGattService::GattErrorCode::kFailed);
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace bluetooth
