// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_local_gatt_descriptor_floss.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/types/cxx23_to_underlying.h"
#include "device/bluetooth/floss/bluetooth_gatt_characteristic_floss.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"

namespace floss {

// static
base::WeakPtr<BluetoothLocalGattDescriptorFloss>
BluetoothLocalGattDescriptorFloss::Create(
    const device::BluetoothUUID& uuid,
    device::BluetoothGattCharacteristic::Permissions permissions,
    BluetoothLocalGattCharacteristicFloss* characteristic) {
  const auto& [_, floss_permissions] =
      BluetoothGattCharacteristicFloss::ConvertPropsAndPermsToFloss(
          /*properties=*/0, static_cast<uint16_t>(permissions));
  auto* descriptor = new BluetoothLocalGattDescriptorFloss(
      uuid, floss_permissions, characteristic);
  auto weak_ptr = descriptor->weak_ptr_factory_.GetWeakPtr();
  weak_ptr->index_ =
      characteristic->AddDescriptor(base::WrapUnique(descriptor));
  return weak_ptr;
}

BluetoothLocalGattDescriptorFloss::BluetoothLocalGattDescriptorFloss(
    const device::BluetoothUUID& uuid,
    device::BluetoothGattCharacteristic::Permissions permissions,
    BluetoothLocalGattCharacteristicFloss* characteristic)
    : uuid_(uuid),
      permissions_(permissions),
      characteristic_(raw_ref<BluetoothLocalGattCharacteristicFloss>::from_ptr(
          characteristic)),
      client_instance_id_(characteristic_->service_->NewInstanceId()) {}

BluetoothLocalGattDescriptorFloss::~BluetoothLocalGattDescriptorFloss() {
  characteristic_->service_->RemoveServerObserverForHandle(floss_instance_id_);
}

std::string BluetoothLocalGattDescriptorFloss::GetIdentifier() const {
  return base::StringPrintf(
      "%s-%s/%04x",
      characteristic_->service_->GetAdapter()->GetAddress().c_str(),
      GetUUID().value().c_str(), client_instance_id_);
}

device::BluetoothUUID BluetoothLocalGattDescriptorFloss::GetUUID() const {
  return uuid_;
}

device::BluetoothGattCharacteristic::Permissions
BluetoothLocalGattDescriptorFloss::GetPermissions() const {
  const auto& [_, perms] =
      BluetoothGattCharacteristicFloss::ConvertPropsAndPermsFromFloss(
          /*properties=*/0, permissions_);
  return perms;
}

device::BluetoothLocalGattCharacteristic*
BluetoothLocalGattDescriptorFloss::GetCharacteristic() const {
  return &*characteristic_;
}

GattDescriptor BluetoothLocalGattDescriptorFloss::ToGattDescriptor() {
  GattDescriptor descriptor;
  descriptor.uuid = uuid_;
  descriptor.instance_id = floss_instance_id_;
  descriptor.permissions = permissions_;
  return descriptor;
}

void BluetoothLocalGattDescriptorFloss::ResolveInstanceId(
    const GattCharacteristic& characteristic) {
  DCHECK(characteristic.descriptors[index_].uuid == GetUUID());
  floss_instance_id_ = characteristic.descriptors[index_].instance_id;
  characteristic_->service_->AddServerObserverForHandle(floss_instance_id_,
                                                        this);
}

void BluetoothLocalGattDescriptorFloss::GattServerDescriptorReadRequest(
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

  device::BluetoothLocalGattService::Delegate* delegate =
      characteristic_->service_->delegate_;
  if (!delegate) {
    LOG(ERROR) << __func__ << ": No delegate for local GATT service";
    FlossDBusManager::Get()->GetGattManagerClient()->SendResponse(
        base::DoNothing(), address, request_id, GattStatus::kError, offset,
        std::vector<uint8_t>());
    return;
  }

  pending_request_.emplace(GattRequest{address, request_id, offset});
  auto* device = characteristic_->service_->GetAdapter()->GetDevice(address);
  BluetoothLocalGattDescriptor* descriptor =
      static_cast<BluetoothLocalGattDescriptor*>(this);

  // This callback is expected to run, so run it if the client has not done so
  // within the next second.
  response_timer_.Start(
      FROM_HERE, kResponseTimeout,
      base::BindOnce(&BluetoothLocalGattDescriptorFloss::OnReadRequestCallback,
                     weak_ptr_factory_.GetWeakPtr(), request_id,
                     BluetoothGattServiceFloss::GattErrorCode::kFailed,
                     base::OwnedRef(std::vector<uint8_t>())));

  delegate->OnDescriptorReadRequest(
      device, descriptor, offset,
      base::BindOnce(&BluetoothLocalGattDescriptorFloss::OnReadRequestCallback,
                     weak_ptr_factory_.GetWeakPtr(), request_id));
}

void BluetoothLocalGattDescriptorFloss::OnReadRequestCallback(
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

void BluetoothLocalGattDescriptorFloss::GattServerDescriptorWriteRequest(
    std::string address,
    int32_t request_id,
    int32_t offset,
    int32_t length,
    bool is_prepared_write,
    bool needs_response,
    int32_t handle,
    std::vector<uint8_t> value) {
  DCHECK(handle == floss_instance_id_);

  if (is_prepared_write) {
    // TODO(b/329709266) - Support prepare write requests for descriptors
    LOG(ERROR) << __func__ << ": Prepared write request not supported.";
    if (needs_response) {
      FlossDBusManager::Get()->GetGattManagerClient()->SendResponse(
          base::DoNothing(), address, request_id, GattStatus::kReqNotSupported,
          offset, value);
    }
    return;
  }

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

  device::BluetoothLocalGattService::Delegate* delegate =
      characteristic_->service_->delegate_;
  if (!delegate) {
    LOG(ERROR) << __func__ << ": No delegate for local GATT service";
    if (needs_response) {
      FlossDBusManager::Get()->GetGattManagerClient()->SendResponse(
          base::DoNothing(), address, request_id, GattStatus::kError, offset,
          value);
    }
    return;
  }

  if (GetUUID() ==
      BluetoothGattDescriptor::ClientCharacteristicConfigurationUuid()) {
    auto status = HandleCccDescriptor(address, value);
    if (needs_response) {
      FlossDBusManager::Get()->GetGattManagerClient()->SendResponse(
          base::DoNothing(), address, request_id, status, offset, value);
    }
    return;
  }

  pending_request_.emplace(GattRequest{address, request_id, offset});
  auto* device = characteristic_->service_->GetAdapter()->GetDevice(address);
  BluetoothLocalGattDescriptor* descriptor =
      static_cast<BluetoothLocalGattDescriptor*>(this);

  // This callback is expected to run, so run it if the client has not done so
  // within the next second.
  response_timer_.Start(
      FROM_HERE, kResponseTimeout,
      base::BindOnce(&BluetoothLocalGattDescriptorFloss::OnWriteRequestCallback,
                     weak_ptr_factory_.GetWeakPtr(), request_id,
                     std::ref(value), needs_response, /*success=*/false));

  delegate->OnDescriptorWriteRequest(
      device, descriptor, value, offset,
      base::BindOnce(&BluetoothLocalGattDescriptorFloss::OnWriteRequestCallback,
                     weak_ptr_factory_.GetWeakPtr(), request_id,
                     std::ref(value), needs_response,
                     /*success=*/true),
      base::BindOnce(&BluetoothLocalGattDescriptorFloss::OnWriteRequestCallback,
                     weak_ptr_factory_.GetWeakPtr(), request_id,
                     std::ref(value), needs_response,
                     /*success=*/false));
}

void BluetoothLocalGattDescriptorFloss::OnWriteRequestCallback(
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

GattStatus BluetoothLocalGattDescriptorFloss::HandleCccDescriptor(
    std::string address,
    std::vector<uint8_t>& value) {
  device::BluetoothLocalGattService::Delegate* delegate =
      characteristic_->service_->delegate_;
  auto* device = characteristic_->service_->GetAdapter()->GetDevice(address);
  device::BluetoothLocalGattCharacteristic* characteristic =
      static_cast<device::BluetoothLocalGattCharacteristic*>(
          &characteristic_.get());

  if (value.size() != 2) {
    LOG(ERROR) << __func__ << ": Value is not a valid CccdValueType";
    return GattStatus::kCccCfgErr;
  }
  uint16_t notification_type = (value[1] << 8) + value[0];

  auto properties = characteristic_->GetProperties();
  switch (notification_type) {
    case base::to_underlying(
        device::BluetoothGattCharacteristic::NotificationType::kNone):
      cccd_type_ = device::BluetoothGattCharacteristic::NotificationType::kNone;
      delegate->OnNotificationsStop(device, characteristic);
      break;
    case base::to_underlying(
        device::BluetoothGattCharacteristic::NotificationType::kNotification):
      if (!(properties &
            device::BluetoothGattCharacteristic::PROPERTY_NOTIFY)) {
        LOG(WARNING) << __func__ << ": Parent characteristic (uuid: "
                     << characteristic_->GetUUID()
                     << ") does not have the necessary properties to notify "
                        "(properties: "
                     << properties << ")";
        return GattStatus::kCccCfgErr;
      }
      cccd_type_ =
          device::BluetoothGattCharacteristic::NotificationType::kNotification;
      delegate->OnNotificationsStart(device, cccd_type_, characteristic);
      break;
    case base::to_underlying(
        device::BluetoothGattCharacteristic::NotificationType::kIndication):
      if (!(properties &
            device::BluetoothGattCharacteristic::PROPERTY_INDICATE)) {
        LOG(WARNING) << __func__ << ": Parent characteristic (uuid: "
                     << characteristic_->GetUUID()
                     << ") does not have the necessary properties to indicate "
                        "(properties: "
                     << properties << ")";
        return GattStatus::kCccCfgErr;
      }
      cccd_type_ =
          device::BluetoothGattCharacteristic::NotificationType::kIndication;
      delegate->OnNotificationsStart(device, cccd_type_, characteristic);
      break;
    default:
      LOG(WARNING) << __func__ << ": Value '" << notification_type
                   << "' is not a valid CccdValueType";
      return GattStatus::kCccCfgErr;
  }
  return GattStatus::kSuccess;
}

}  // namespace floss
