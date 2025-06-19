// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/emulation/fake_remote_gatt_characteristic.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "device/bluetooth/emulation/fake_central.h"
#include "device/bluetooth/emulation/fake_peripheral.h"
#include "device/bluetooth/emulation/fake_read_response.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace bluetooth {

namespace {

FakeRemoteGattCharacteristic::ResponseCallback CreateResponseCallback(
    base::OnceClosure callback,
    device::BluetoothGattCharacteristic::ErrorCallback error_callback) {
  return base::BindOnce(
      [](base::OnceClosure callback,
         device::BluetoothGattCharacteristic::ErrorCallback error_callback,
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

void RunCallbacksWithResponseCode(
    uint16_t gatt_code,
    std::vector<FakeRemoteGattCharacteristic::ResponseCallback> callbacks) {
  for (auto& callback : callbacks) {
    std::move(callback).Run(gatt_code);
  }
}

}  // namespace

FakeRemoteGattCharacteristic::FakeRemoteGattCharacteristic(
    const std::string& characteristic_id,
    const device::BluetoothUUID& characteristic_uuid,
    mojom::CharacteristicPropertiesPtr properties,
    FakeRemoteGattService* service)
    : characteristic_id_(characteristic_id),
      characteristic_uuid_(characteristic_uuid),
      fake_service_(*service) {
  properties_ = PROPERTY_NONE;
  if (properties->broadcast) {
    properties_ |= PROPERTY_BROADCAST;
  }
  if (properties->read) {
    properties_ |= PROPERTY_READ;
  }
  if (properties->write_without_response) {
    properties_ |= PROPERTY_WRITE_WITHOUT_RESPONSE;
  }
  if (properties->write) {
    properties_ |= PROPERTY_WRITE;
  }
  if (properties->notify) {
    properties_ |= PROPERTY_NOTIFY;
  }
  if (properties->indicate) {
    properties_ |= PROPERTY_INDICATE;
  }
  if (properties->authenticated_signed_writes) {
    properties_ |= PROPERTY_AUTHENTICATED_SIGNED_WRITES;
  }
  if (properties->extended_properties) {
    properties_ |= PROPERTY_EXTENDED_PROPERTIES;
  }
}

FakeRemoteGattCharacteristic::~FakeRemoteGattCharacteristic() = default;

std::string FakeRemoteGattCharacteristic::AddFakeDescriptor(
    const device::BluetoothUUID& descriptor_uuid) {
  // Attribute instance Ids need to be unique.
  std::string new_descriptor_id = base::StringPrintf(
      "%s_%zu", GetIdentifier().c_str(), ++last_descriptor_id_);

  bool result = AddDescriptor(std::make_unique<FakeRemoteGattDescriptor>(
      new_descriptor_id, descriptor_uuid, this));

  DCHECK(result);
  return new_descriptor_id;
}

bool FakeRemoteGattCharacteristic::RemoveFakeDescriptor(
    const std::string& identifier) {
  return descriptors_.erase(identifier) != 0u;
}

void FakeRemoteGattCharacteristic::SetNextReadResponse(
    uint16_t gatt_code,
    const std::optional<std::vector<uint8_t>>& value) {
  DCHECK(!next_read_response_);
  next_read_response_.emplace(gatt_code, value);
}

void FakeRemoteGattCharacteristic::SetNextWriteResponse(uint16_t gatt_code) {
  DCHECK(!next_write_response_);
  next_write_response_.emplace(gatt_code);
}

void FakeRemoteGattCharacteristic::SetNextSubscribeToNotificationsResponse(
    uint16_t gatt_code) {
  DCHECK(!next_subscribe_to_notifications_response_);
  next_subscribe_to_notifications_response_.emplace(gatt_code);
}

void FakeRemoteGattCharacteristic::SetNextUnsubscribeFromNotificationsResponse(
    uint16_t gatt_code) {
  DCHECK(!next_unsubscribe_from_notifications_response_);
  next_unsubscribe_from_notifications_response_.emplace(gatt_code);
}

void FakeRemoteGattCharacteristic::SimulateReadResponse(
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

void FakeRemoteGattCharacteristic::SimulateWriteResponse(uint16_t gatt_code) {
  RunCallbacksWithResponseCode(gatt_code, std::move(write_callbacks_));
}

void FakeRemoteGattCharacteristic::SimulateSubscribeToNotificationsResponse(
    uint16_t gatt_code) {
  RunCallbacksWithResponseCode(
      gatt_code, std::move(subscribe_to_notifications_callbacks_));
}

void FakeRemoteGattCharacteristic::SimulateUnsubscribeFromNotificationsResponse(
    uint16_t gatt_code) {
  RunCallbacksWithResponseCode(
      gatt_code, std::move(unsubscribe_from_notifications_callbacks_));
}

bool FakeRemoteGattCharacteristic::AllResponsesConsumed() {
  // TODO(crbug.com/40083385): Update this when
  // SetNextUnsubscribeFromNotificationsResponse is implemented.
  return !next_read_response_ && !next_write_response_ &&
         !next_subscribe_to_notifications_response_ &&
         std::ranges::all_of(descriptors_, [](const auto& e) {
           return static_cast<FakeRemoteGattDescriptor*>(e.second.get())
               ->AllResponsesConsumed();
         });
}

std::string FakeRemoteGattCharacteristic::GetIdentifier() const {
  return characteristic_id_;
}

device::BluetoothUUID FakeRemoteGattCharacteristic::GetUUID() const {
  return characteristic_uuid_;
}

FakeRemoteGattCharacteristic::Properties
FakeRemoteGattCharacteristic::GetProperties() const {
  return properties_;
}

FakeRemoteGattCharacteristic::Permissions
FakeRemoteGattCharacteristic::GetPermissions() const {
  NOTREACHED();
}

const std::vector<uint8_t>& FakeRemoteGattCharacteristic::GetValue() const {
  NOTREACHED();
}

device::BluetoothRemoteGattService* FakeRemoteGattCharacteristic::GetService()
    const {
  return &fake_service_.get();
}

void FakeRemoteGattCharacteristic::ReadRemoteCharacteristic(
    ValueCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeRemoteGattCharacteristic::DispatchReadResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FakeRemoteGattCharacteristic::WriteRemoteCharacteristic(
    base::span<const uint8_t> value,
    WriteType write_type,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  mojom::WriteType mojom_write_type;
  switch (write_type) {
    case WriteType::kWithResponse:
      mojom_write_type = mojom::WriteType::kWriteWithResponse;
      break;
    case WriteType::kWithoutResponse:
      mojom_write_type = mojom::WriteType::kWriteWithoutResponse;
      break;
  }

  // It doesn't make sense to dispatch a custom write response if the
  // characteristic only supports write without response but we still need to
  // run the callback because that's the guarantee the API makes.
  if (write_type == WriteType::kWithoutResponse) {
    last_written_value_ = base::ToVector(value);
    last_write_type_ = mojom_write_type;
    DispatchCharacteristicOperationEvent(
        bluetooth::mojom::CharacteristicOperationType::kWrite,
        last_written_value_, last_write_type_);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeRemoteGattCharacteristic::DispatchWriteResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(error_callback), base::ToVector(value),
                     mojom_write_type));
}

void FakeRemoteGattCharacteristic::DeprecatedWriteRemoteCharacteristic(
    base::span<const uint8_t> value,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  const mojom::WriteType write_type = mojom::WriteType::kWriteDefaultDeprecated;
  // It doesn't make sense to dispatch a custom write response if the
  // characteristic only supports write without response but we still need to
  // run the callback because that's the guarantee the API makes.
  if (properties_ & PROPERTY_WRITE_WITHOUT_RESPONSE) {
    last_written_value_ = base::ToVector(value);
    last_write_type_ = write_type;
    DispatchCharacteristicOperationEvent(
        bluetooth::mojom::CharacteristicOperationType::kWrite,
        last_written_value_, last_write_type_);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeRemoteGattCharacteristic::DispatchWriteResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(error_callback), base::ToVector(value),
                     write_type));
}

#if BUILDFLAG(IS_CHROMEOS)
void FakeRemoteGattCharacteristic::PrepareWriteRemoteCharacteristic(
    base::span<const uint8_t> value,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void FakeRemoteGattCharacteristic::SubscribeToNotifications(
    device::BluetoothRemoteGattDescriptor* ccc_descriptor,
#if BUILDFLAG(IS_CHROMEOS)
    NotificationType notification_type,
#endif  // BUILDFLAG(IS_CHROMEOS)
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeRemoteGattCharacteristic::
                         DispatchSubscribeToNotificationsResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(error_callback)));
}

void FakeRemoteGattCharacteristic::UnsubscribeFromNotifications(
    device::BluetoothRemoteGattDescriptor* ccc_descriptor,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeRemoteGattCharacteristic::
                         DispatchUnsubscribeFromNotificationsResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(error_callback)));
}

void FakeRemoteGattCharacteristic::DispatchReadResponse(
    ValueCallback callback) {
  DispatchCharacteristicOperationEvent(
      bluetooth::mojom::CharacteristicOperationType::kRead,
      /*data=*/std::nullopt,
      /*write_type=*/std::nullopt);
  read_callbacks_.push_back(std::move(callback));
  if (!next_read_response_) {
    return;
  }

  uint16_t gatt_code = next_read_response_->gatt_code();
  std::optional<std::vector<uint8_t>> value = next_read_response_->value();
  next_read_response_.reset();
  SimulateReadResponse(gatt_code, value);
}

void FakeRemoteGattCharacteristic::DispatchWriteResponse(
    base::OnceClosure callback,
    ErrorCallback error_callback,
    const std::vector<uint8_t>& value,
    mojom::WriteType write_type) {
  DispatchCharacteristicOperationEvent(
      bluetooth::mojom::CharacteristicOperationType::kWrite, value, write_type);
  write_callbacks_.push_back(
      CreateResponseCallback(std::move(callback), std::move(error_callback)));
  if (!next_write_response_) {
    return;
  }

  uint16_t gatt_code = next_write_response_.value();
  next_write_response_.reset();
  if (gatt_code == mojom::kGATTSuccess) {
    last_written_value_ = value;
    last_write_type_ = write_type;
  }
  SimulateWriteResponse(gatt_code);
}

void FakeRemoteGattCharacteristic::DispatchSubscribeToNotificationsResponse(
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  DispatchCharacteristicOperationEvent(
      bluetooth::mojom::CharacteristicOperationType::kSubscribeToNotifications,
      /*data=*/std::nullopt, /*write_type=*/std::nullopt);
  subscribe_to_notifications_callbacks_.push_back(
      CreateResponseCallback(std::move(callback), std::move(error_callback)));
  if (!next_subscribe_to_notifications_response_) {
    return;
  }

  uint16_t gatt_code = next_subscribe_to_notifications_response_.value();
  next_subscribe_to_notifications_response_.reset();
  SimulateSubscribeToNotificationsResponse(gatt_code);
}

void FakeRemoteGattCharacteristic::DispatchUnsubscribeFromNotificationsResponse(
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  DispatchCharacteristicOperationEvent(
      bluetooth::mojom::CharacteristicOperationType::
          kUnsubscribeFromNotifications,
      /*data=*/std::nullopt, /*write_type=*/std::nullopt);
  unsubscribe_from_notifications_callbacks_.push_back(
      CreateResponseCallback(std::move(callback), std::move(error_callback)));
  if (!next_unsubscribe_from_notifications_response_) {
    return;
  }

  uint16_t gatt_code = next_unsubscribe_from_notifications_response_.value();
  next_unsubscribe_from_notifications_response_.reset();
  SimulateUnsubscribeFromNotificationsResponse(gatt_code);
}

void FakeRemoteGattCharacteristic::DispatchCharacteristicOperationEvent(
    mojom::CharacteristicOperationType type,
    const std::optional<std::vector<uint8_t>>& data,
    const std::optional<mojom::WriteType> write_type) {
  fake_service_->fake_peripheral()
      .fake_central()
      .DispatchCharacteristicOperationEvent(type, data, write_type,
                                            characteristic_id_);
}

}  // namespace bluetooth
