// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_remote_gatt_characteristic.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/public/mojom/test/fake_bluetooth.mojom.h"
#include "device/bluetooth/test/fake_read_response.h"

namespace bluetooth {

FakeRemoteGattCharacteristic::FakeRemoteGattCharacteristic(
    const std::string& characteristic_id,
    const device::BluetoothUUID& characteristic_uuid,
    mojom::CharacteristicPropertiesPtr properties,
    device::BluetoothRemoteGattService* service)
    : characteristic_id_(characteristic_id),
      characteristic_uuid_(characteristic_uuid),
      service_(service) {
  properties_ = PROPERTY_NONE;
  if (properties->broadcast)
    properties_ |= PROPERTY_BROADCAST;
  if (properties->read)
    properties_ |= PROPERTY_READ;
  if (properties->write_without_response)
    properties_ |= PROPERTY_WRITE_WITHOUT_RESPONSE;
  if (properties->write)
    properties_ |= PROPERTY_WRITE;
  if (properties->notify)
    properties_ |= PROPERTY_NOTIFY;
  if (properties->indicate)
    properties_ |= PROPERTY_INDICATE;
  if (properties->authenticated_signed_writes)
    properties_ |= PROPERTY_AUTHENTICATED_SIGNED_WRITES;
  if (properties->extended_properties)
    properties_ |= PROPERTY_EXTENDED_PROPERTIES;
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
    const base::Optional<std::vector<uint8_t>>& value) {
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

bool FakeRemoteGattCharacteristic::AllResponsesConsumed() {
  // TODO(crbug.com/569709): Update this when
  // SetNextUnsubscribeFromNotificationsResponse is implemented.
  return !next_read_response_ && !next_write_response_ &&
         !next_subscribe_to_notifications_response_ &&
         std::all_of(
             descriptors_.begin(), descriptors_.end(), [](const auto& e) {
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
  return PERMISSION_NONE;
}

const std::vector<uint8_t>& FakeRemoteGattCharacteristic::GetValue() const {
  NOTREACHED();
  return value_;
}

device::BluetoothRemoteGattService* FakeRemoteGattCharacteristic::GetService()
    const {
  return service_;
}

void FakeRemoteGattCharacteristic::ReadRemoteCharacteristic(
    ValueCallback callback,
    ErrorCallback error_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeRemoteGattCharacteristic::DispatchReadResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(error_callback)));
}

void FakeRemoteGattCharacteristic::WriteRemoteCharacteristic(
    const std::vector<uint8_t>& value,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  // It doesn't make sense to dispatch a custom write response if the
  // characteristic only supports write without response but we still need to
  // run the callback because that's the guarantee the API makes.
  if (properties_ & PROPERTY_WRITE_WITHOUT_RESPONSE) {
    last_written_value_ = value;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeRemoteGattCharacteristic::DispatchWriteResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(error_callback), value));
}

#if defined(OS_CHROMEOS)
void FakeRemoteGattCharacteristic::PrepareWriteRemoteCharacteristic(
    const std::vector<uint8_t>& value,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}
#endif

bool FakeRemoteGattCharacteristic::WriteWithoutResponse(
    base::span<const uint8_t> value) {
  if (properties_ & PROPERTY_WRITE_WITHOUT_RESPONSE) {
    last_written_value_.emplace(value.begin(), value.end());
    return true;
  }

  return false;
}

void FakeRemoteGattCharacteristic::SubscribeToNotifications(
    device::BluetoothRemoteGattDescriptor* ccc_descriptor,
#if defined(OS_CHROMEOS)
    NotificationType notification_type,
#endif
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeRemoteGattCharacteristic::
                         DispatchUnsubscribeFromNotificationsResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(error_callback)));
}

void FakeRemoteGattCharacteristic::DispatchReadResponse(
    ValueCallback callback,
    ErrorCallback error_callback) {
  DCHECK(next_read_response_);
  uint16_t gatt_code = next_read_response_->gatt_code();
  base::Optional<std::vector<uint8_t>> value = next_read_response_->value();
  next_read_response_.reset();

  switch (gatt_code) {
    case mojom::kGATTSuccess:
      DCHECK(value);
      value_ = std::move(value.value());
      std::move(callback).Run(value_);
      break;
    case mojom::kGATTInvalidHandle:
      DCHECK(!value);
      std::move(error_callback)
          .Run(device::BluetoothGattService::GATT_ERROR_FAILED);
      break;
    default:
      NOTREACHED();
  }
}

void FakeRemoteGattCharacteristic::DispatchWriteResponse(
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

void FakeRemoteGattCharacteristic::DispatchSubscribeToNotificationsResponse(
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  DCHECK(next_subscribe_to_notifications_response_);
  uint16_t gatt_code = next_subscribe_to_notifications_response_.value();
  next_subscribe_to_notifications_response_.reset();

  switch (gatt_code) {
    case mojom::kGATTSuccess:
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

void FakeRemoteGattCharacteristic::DispatchUnsubscribeFromNotificationsResponse(
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  DCHECK(next_unsubscribe_from_notifications_response_);
  uint16_t gatt_code = next_unsubscribe_from_notifications_response_.value();
  next_unsubscribe_from_notifications_response_.reset();

  switch (gatt_code) {
    case mojom::kGATTSuccess:
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
