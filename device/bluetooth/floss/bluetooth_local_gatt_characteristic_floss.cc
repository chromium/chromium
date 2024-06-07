// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_local_gatt_characteristic_floss.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/floss/bluetooth_adapter_floss.h"
#include "device/bluetooth/floss/bluetooth_gatt_characteristic_floss.h"
#include "device/bluetooth/floss/bluetooth_gatt_service_floss.h"
#include "device/bluetooth/floss/bluetooth_local_gatt_service_floss.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"

namespace floss {

// static
base::WeakPtr<BluetoothLocalGattCharacteristicFloss>
BluetoothLocalGattCharacteristicFloss::Create(
    const device::BluetoothUUID& uuid,
    Properties properties,
    Permissions permissions,
    BluetoothLocalGattServiceFloss* service) {
  const auto& [floss_properties, floss_permissions] =
      BluetoothGattCharacteristicFloss::ConvertPropsAndPermsToFloss(
          static_cast<uint8_t>(properties), static_cast<uint16_t>(permissions));
  auto* characteristic = new BluetoothLocalGattCharacteristicFloss(
      uuid, floss_properties, floss_permissions, service);

  if ((properties & device::BluetoothGattCharacteristic::PROPERTY_NOTIFY) ||
      (properties & device::BluetoothGattCharacteristic::PROPERTY_INDICATE)) {
    BluetoothLocalGattDescriptorFloss::Create(
        device::BluetoothGattDescriptor::
            ClientCharacteristicConfigurationUuid(),
        device::BluetoothGattCharacteristic::PERMISSION_READ +
            device::BluetoothGattCharacteristic::PERMISSION_READ,
        characteristic);
  }

  auto weak_ptr = characteristic->weak_ptr_factory_.GetWeakPtr();
  weak_ptr->index_ =
      service->AddCharacteristic(base::WrapUnique(characteristic));
  return weak_ptr;
}

BluetoothLocalGattCharacteristicFloss::BluetoothLocalGattCharacteristicFloss(
    const device::BluetoothUUID& uuid,
    Properties properties,
    Permissions permissions,
    BluetoothLocalGattServiceFloss* service)
    : uuid_(uuid),
      properties_(properties),
      permissions_(permissions),
      service_(raw_ref<BluetoothLocalGattServiceFloss>::from_ptr(service)),
      client_instance_id_(service_->NewInstanceId()) {}

BluetoothLocalGattCharacteristicFloss::
    ~BluetoothLocalGattCharacteristicFloss() {
  service_->RemoveServerObserverForHandle(floss_instance_id_);
}

std::string BluetoothLocalGattCharacteristicFloss::GetIdentifier() const {
  return base::StringPrintf("%s-%s/%04x",
                            service_->GetAdapter()->GetAddress().c_str(),
                            GetUUID().value().c_str(), client_instance_id_);
}

device::BluetoothUUID BluetoothLocalGattCharacteristicFloss::GetUUID() const {
  return uuid_;
}

device::BluetoothGattCharacteristic::Properties
BluetoothLocalGattCharacteristicFloss::GetProperties() const {
  const auto& [props, _] =
      BluetoothGattCharacteristicFloss::ConvertPropsAndPermsFromFloss(
          properties_, permissions_);
  return props;
}

device::BluetoothGattCharacteristic::Permissions
BluetoothLocalGattCharacteristicFloss::GetPermissions() const {
  const auto& [_, perms] =
      BluetoothGattCharacteristicFloss::ConvertPropsAndPermsFromFloss(
          properties_, permissions_);
  return perms;
}

device::BluetoothLocalGattCharacteristic::NotificationStatus
BluetoothLocalGattCharacteristicFloss::NotifyValueChanged(
    const device::BluetoothDevice* device,
    const std::vector<uint8_t>& new_value,
    bool indicate) {
  if (indicate &&
      !(properties_ & GattCharacteristic::GATT_CHAR_PROP_BIT_INDICATE)) {
    return INDICATE_PROPERTY_NOT_SET;
  }
  if (!indicate &&
      !(properties_ & GattCharacteristic::GATT_CHAR_PROP_BIT_NOTIFY)) {
    return NOTIFY_PROPERTY_NOT_SET;
  }
  return service_->GetAdapter()->SendValueChanged(this, new_value)
             ? NOTIFICATION_SUCCESS
             : SERVICE_NOT_REGISTERED;
}

device::BluetoothLocalGattService*
BluetoothLocalGattCharacteristicFloss::GetService() const {
  return &*service_;
}

GattCharacteristic
BluetoothLocalGattCharacteristicFloss::ToGattCharacteristic() {
  GattCharacteristic characteristic;
  characteristic.uuid = uuid_;
  characteristic.properties = properties_;
  characteristic.permissions = permissions_;
  for (auto& descriptor : descriptors_) {
    characteristic.descriptors.push_back(descriptor->ToGattDescriptor());
  }
  return characteristic;
}

void BluetoothLocalGattCharacteristicFloss::ResolveInstanceId(
    const GattService& service) {
  DCHECK(service.characteristics[index_].uuid == GetUUID());
  floss_instance_id_ = service.characteristics[index_].instance_id;
  service_->AddServerObserverForHandle(floss_instance_id_, this);
}

void BluetoothLocalGattCharacteristicFloss::GattServerCharacteristicReadRequest(
    std::string address,
    int32_t request_id,
    int32_t offset,
    bool is_long,
    int32_t handle) {
  DCHECK(handle == floss_instance_id_);

  if (pending_request_.has_value()) {
    LOG(ERROR) << __func__ << ": A request for device '"
               << pending_request_.value().address << "' is already pending";
    FlossDBusManager::Get()->GetGattManagerClient()->SendResponse(
        base::DoNothing(), address, request_id, GattStatus::kBusy, offset,
        std::vector<uint8_t>());
    return;
  }

  device::BluetoothLocalGattService::Delegate* delegate = service_->delegate_;
  if (!delegate) {
    LOG(ERROR) << __func__ << ": No delegate for local GATT service";
    FlossDBusManager::Get()->GetGattManagerClient()->SendResponse(
        base::DoNothing(), address, request_id, GattStatus::kError, offset,
        std::vector<uint8_t>());
    return;
  }

  pending_request_.emplace(GattRequest{address, request_id, offset});
  auto* device = service_->GetAdapter()->GetDevice(address);
  BluetoothLocalGattCharacteristic* characteristic =
      static_cast<BluetoothLocalGattCharacteristic*>(this);

  // This callback is expected to run, so run it if the client has not done so
  // within the next second.
  response_timer_.Start(
      FROM_HERE, kResponseTimeout,
      base::BindOnce(
          &BluetoothLocalGattCharacteristicFloss::OnReadRequestCallback,
          weak_ptr_factory_.GetWeakPtr(), request_id,
          BluetoothGattServiceFloss::GattErrorCode::kFailed,
          base::OwnedRef(std::vector<uint8_t>())));

  delegate->OnCharacteristicReadRequest(
      device, characteristic, offset,
      base::BindOnce(
          &BluetoothLocalGattCharacteristicFloss::OnReadRequestCallback,
          weak_ptr_factory_.GetWeakPtr(), request_id));
}

void BluetoothLocalGattCharacteristicFloss::OnReadRequestCallback(
    int32_t request_id,
    std::optional<BluetoothGattServiceFloss::GattErrorCode> error_code,
    const std::vector<uint8_t>& value) {
  if (!pending_request_.has_value()) {
    // If this check trips, we have already handled the request response.
    LOG(ERROR) << __func__ << ": No pending read request for request with id "
               << request_id;
    return;
  }
  auto read_request = pending_request_.value();
  if (read_request.request_id != request_id) {
    // This check may trip due to a stale (timed-out) request being belatedly
    // responded to.
    LOG(ERROR) << __func__ << ": Read request id mismatch. Expected: "
               << read_request.request_id << ", Actual: " << request_id;
    return;
  }
  response_timer_.Stop();

  GattStatus status = error_code.has_value()
                          ? BluetoothGattServiceFloss::GattServiceErrorToStatus(
                                error_code.value())
                          : GattStatus::kSuccess;
  FlossDBusManager::Get()->GetGattManagerClient()->SendResponse(
      base::DoNothing(), read_request.address, request_id, status,
      read_request.offset, value);
  pending_request_.reset();
}

void BluetoothLocalGattCharacteristicFloss::
    GattServerCharacteristicWriteRequest(std::string address,
                                         int32_t request_id,
                                         int32_t offset,
                                         int32_t length,
                                         bool is_prepared_write,
                                         bool needs_response,
                                         int32_t handle,
                                         std::vector<uint8_t> value) {
  DCHECK(handle == floss_instance_id_);

  if (pending_request_.has_value()) {
    LOG(ERROR) << __func__ << ": A request for device '"
               << pending_request_.value().address << "' is already pending";
    if (needs_response) {
      FlossDBusManager::Get()->GetGattManagerClient()->SendResponse(
          base::DoNothing(), address, request_id, GattStatus::kBusy, offset,
          value);
    }
    return;
  }

  device::BluetoothLocalGattService::Delegate* delegate = service_->delegate_;
  if (!delegate) {
    LOG(ERROR) << __func__ << ": No delegate for local GATT service";
    if (needs_response) {
      FlossDBusManager::Get()->GetGattManagerClient()->SendResponse(
          base::DoNothing(), address, request_id, GattStatus::kError, offset,
          value);
    }
    return;
  }

  pending_request_.emplace(GattRequest{address, request_id, offset});
  auto* device = service_->GetAdapter()->GetDevice(address);
  BluetoothLocalGattCharacteristic* characteristic =
      static_cast<BluetoothLocalGattCharacteristic*>(this);

  // This callback is expected to run, so run it if the client has not done so
  // within the next second.
  response_timer_.Start(
      FROM_HERE, kResponseTimeout,
      base::BindOnce(
          &BluetoothLocalGattCharacteristicFloss::OnWriteRequestCallback,
          weak_ptr_factory_.GetWeakPtr(), request_id, std::ref(value),
          needs_response, /*success=*/false));

  if (is_prepared_write) {
    delegate->OnCharacteristicPrepareWriteRequest(
        device, characteristic, value, offset, /*has_subsequent_request=*/true,
        base::BindOnce(
            &BluetoothLocalGattCharacteristicFloss::OnWriteRequestCallback,
            weak_ptr_factory_.GetWeakPtr(), request_id, std::ref(value),
            needs_response,
            /*success=*/true),
        base::BindOnce(
            &BluetoothLocalGattCharacteristicFloss::OnWriteRequestCallback,
            weak_ptr_factory_.GetWeakPtr(), request_id, std::ref(value),
            needs_response,
            /*success=*/false));
  } else {
    delegate->OnCharacteristicWriteRequest(
        device, characteristic, value, offset,
        base::BindOnce(
            &BluetoothLocalGattCharacteristicFloss::OnWriteRequestCallback,
            weak_ptr_factory_.GetWeakPtr(), request_id, std::ref(value),
            needs_response,
            /*success=*/true),
        base::BindOnce(
            &BluetoothLocalGattCharacteristicFloss::OnWriteRequestCallback,
            weak_ptr_factory_.GetWeakPtr(), request_id, std::ref(value),
            needs_response,
            /*success=*/false));
  }
}

void BluetoothLocalGattCharacteristicFloss::OnWriteRequestCallback(
    int32_t request_id,
    std::vector<uint8_t>& value,
    bool needs_response,
    bool success) {
  if (!pending_request_.has_value()) {
    // If this check trips, we have already handled the request response.
    LOG(ERROR) << __func__ << ": No pending write request for request with id "
               << request_id;
    return;
  }
  auto write_request = pending_request_.value();
  if (write_request.request_id != request_id) {
    // This check may trip due to a stale (timed-out) request being belatedly
    // responded to.
    LOG(ERROR) << __func__ << ": Write request id mismatch. Expected: "
               << write_request.request_id << ", Actual: " << request_id;
    return;
  }
  response_timer_.Stop();

  if (!needs_response) {
    pending_request_.reset();
    return;
  }
  GattStatus status = success ? GattStatus::kSuccess : GattStatus::kError;
  FlossDBusManager::Get()->GetGattManagerClient()->SendResponse(
      base::DoNothing(), write_request.address, write_request.request_id,
      status, write_request.offset, value);
  pending_request_.reset();
}

void BluetoothLocalGattCharacteristicFloss::GattServerExecuteWrite(
    std::string address,
    int32_t request_id,
    bool execute_write) {
  if (!execute_write) {
    // TODO(b/329667574) - Support aborted prepared writes
    LOG(ERROR) << __func__ << ": Aborting prepared writes is not supported";
  }

  if (pending_request_.has_value()) {
    LOG(ERROR) << __func__ << ": A request for device '"
               << pending_request_.value().address << "' is already pending";
    FlossDBusManager::Get()->GetGattManagerClient()->SendResponse(
        base::DoNothing(), address, request_id, GattStatus::kBusy, /*offset=*/0,
        std::vector<uint8_t>());
    return;
  }

  device::BluetoothLocalGattService::Delegate* delegate = service_->delegate_;
  if (!delegate) {
    LOG(ERROR) << __func__ << ": No delegate for local GATT service";
    FlossDBusManager::Get()->GetGattManagerClient()->SendResponse(
        base::DoNothing(), address, request_id, GattStatus::kError,
        /*offset=*/0, std::vector<uint8_t>());
    return;
  }

  pending_request_.emplace(GattRequest{address, request_id, /*offset=*/0});
  auto* device = service_->GetAdapter()->GetDevice(address);
  BluetoothLocalGattCharacteristic* characteristic =
      static_cast<BluetoothLocalGattCharacteristic*>(this);

  // This callback is expected to run, so run it if the client has not done so
  // within the next second.
  response_timer_.Start(
      FROM_HERE, kResponseTimeout,
      base::BindOnce(
          &BluetoothLocalGattCharacteristicFloss::OnWriteRequestCallback,
          weak_ptr_factory_.GetWeakPtr(), request_id,
          base::OwnedRef(std::vector<uint8_t>()),
          /*needs_response=*/true, /*success=*/false));

  delegate->OnCharacteristicPrepareWriteRequest(
      device, characteristic, std::vector<uint8_t>(), /*offset=*/0,
      /*has_subsequent_request=*/false,
      base::BindOnce(
          &BluetoothLocalGattCharacteristicFloss::OnWriteRequestCallback,
          weak_ptr_factory_.GetWeakPtr(), request_id,
          base::OwnedRef(std::vector<uint8_t>()),
          /*needs_response=*/true, /*success=*/true),
      base::BindOnce(
          &BluetoothLocalGattCharacteristicFloss::OnWriteRequestCallback,
          weak_ptr_factory_.GetWeakPtr(), request_id,
          base::OwnedRef(std::vector<uint8_t>()),
          /*needs_response=*/true, /*success=*/false));
}

int32_t BluetoothLocalGattCharacteristicFloss::AddDescriptor(
    std::unique_ptr<BluetoothLocalGattDescriptorFloss> descriptor) {
  descriptors_.push_back(std::move(descriptor));
  return descriptors_.size() - 1;
}

std::vector<device::BluetoothLocalGattDescriptor*>
BluetoothLocalGattCharacteristicFloss::GetDescriptors() const {
  std::vector<device::BluetoothLocalGattDescriptor*> descriptors;
  descriptors.reserve(descriptors_.size());
  for (const auto& d : descriptors_) {
    descriptors.push_back(d.get());
  }
  return descriptors;
}

device::BluetoothGattCharacteristic::NotificationType
BluetoothLocalGattCharacteristicFloss::CccdNotificationType() {
  for (auto& descriptor : descriptors_) {
    if (descriptor->GetUUID() == device::BluetoothGattDescriptor::
                                     ClientCharacteristicConfigurationUuid()) {
      return descriptor->CccdNotificationType();
    }
  }
  LOG(WARNING) << __func__ << ": No CCCD found for characteristic with uuid "
               << GetUUID();
  return device::BluetoothGattCharacteristic::NotificationType::kNone;
}

}  // namespace floss
