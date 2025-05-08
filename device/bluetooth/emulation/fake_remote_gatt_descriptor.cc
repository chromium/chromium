// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/emulation/fake_remote_gatt_descriptor.h"

#include <utility>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "device/bluetooth/emulation/fake_central.h"
#include "device/bluetooth/emulation/fake_peripheral.h"
#include "device/bluetooth/emulation/fake_remote_gatt_characteristic.h"
#include "device/bluetooth/public/mojom/emulation/fake_bluetooth.mojom.h"

namespace bluetooth {

namespace {

FakeRemoteGattDescriptor::ResponseCallback CreateResponseCallback(
    base::OnceClosure callback,
    device::BluetoothGattDescriptor::ErrorCallback error_callback) {
  return base::BindOnce(
      [](base::OnceClosure callback,
         device::BluetoothGattDescriptor::ErrorCallback error_callback,
         uint16_t gatt_code) {
        switch (gatt_code) {
          case mojom::kGATTSuccess:
            std::move(callback).Run();
            break;
          default:
            std::move(error_callback)
                .Run(device::BluetoothGattService::GattErrorCode::kFailed);
        }
      },
      std::move(callback), std::move(error_callback));
}

}  // namespace

FakeRemoteGattDescriptor::FakeRemoteGattDescriptor(
    const std::string& descriptor_id,
    const device::BluetoothUUID& descriptor_uuid,
    FakeRemoteGattCharacteristic* characteristic)
    : descriptor_id_(descriptor_id),
      descriptor_uuid_(descriptor_uuid),
      fake_characteristic_(*characteristic) {}

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

void FakeRemoteGattDescriptor::SimulateReadResponse(
    uint16_t gatt_code,
    const std::optional<std::vector<uint8_t>>& value) {
  std::optional<device::BluetoothGattService::GattErrorCode> response_code;
  std::vector<uint8_t> response_value;
  switch (gatt_code) {
    case mojom::kGATTSuccess:
      if (value) {
        response_value = std::move(value.value());
      }
      break;
    default:
      response_code = device::BluetoothGattService::GattErrorCode::kFailed;
  }

  auto callbacks = std::move(read_callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(response_code, response_value);
  }
}

void FakeRemoteGattDescriptor::SimulateWriteResponse(uint16_t gatt_code) {
  auto callbacks = std::move(write_callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(gatt_code);
  }
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
  return &fake_characteristic_.get();
}

void FakeRemoteGattDescriptor::ReadRemoteDescriptor(ValueCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeRemoteGattDescriptor::DispatchReadResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FakeRemoteGattDescriptor::WriteRemoteDescriptor(
    base::span<const uint8_t> value,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeRemoteGattDescriptor::DispatchWriteResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(error_callback), base::ToVector(value)));
}

void FakeRemoteGattDescriptor::DispatchReadResponse(ValueCallback callback) {
  DispatchDescriptorOperationEvent(
      bluetooth::mojom::DescriptorOperationType::kRead,
      /*data=*/std::nullopt);
  read_callbacks_.push_back(std::move(callback));
  if (!next_read_response_) {
    return;
  }
  uint16_t gatt_code = next_read_response_->gatt_code();
  std::optional<std::vector<uint8_t>> value = next_read_response_->value();
  next_read_response_.reset();
  SimulateReadResponse(gatt_code, value);
}

void FakeRemoteGattDescriptor::DispatchWriteResponse(
    base::OnceClosure callback,
    ErrorCallback error_callback,
    std::vector<uint8_t> value) {
  DispatchDescriptorOperationEvent(
      bluetooth::mojom::DescriptorOperationType::kWrite, value);
  write_callbacks_.push_back(
      CreateResponseCallback(std::move(callback), std::move(error_callback)));
  if (!next_write_response_) {
    return;
  }

  uint16_t gatt_code = next_write_response_.value();
  next_write_response_.reset();
  if (gatt_code == mojom::kGATTSuccess) {
    last_written_value_ = value;
  }
  SimulateWriteResponse(gatt_code);
}

void FakeRemoteGattDescriptor::DispatchDescriptorOperationEvent(
    mojom::DescriptorOperationType type,
    const std::optional<std::vector<uint8_t>>& data) {
  fake_characteristic_->fake_service()
      .fake_peripheral()
      .fake_central()
      .DispatchDescriptorOperationEvent(type, data, descriptor_id_);
}

}  // namespace bluetooth
