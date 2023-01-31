// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/bluetooth/floss/bluetooth_remote_gatt_characteristic_floss.h"

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/floss/bluetooth_adapter_floss.h"
#include "device/bluetooth/floss/bluetooth_gatt_service_floss.h"
#include "device/bluetooth/floss/bluetooth_remote_gatt_descriptor_floss.h"
#include "device/bluetooth/floss/bluetooth_remote_gatt_service_floss.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_gatt_manager_client.h"

namespace floss {

// static
std::unique_ptr<BluetoothRemoteGattCharacteristicFloss>
BluetoothRemoteGattCharacteristicFloss::Create(
    BluetoothRemoteGattServiceFloss* service,
    GattCharacteristic* characteristic) {
  return base::WrapUnique(
      new BluetoothRemoteGattCharacteristicFloss(service, characteristic));
}

BluetoothRemoteGattCharacteristicFloss::BluetoothRemoteGattCharacteristicFloss(
    BluetoothRemoteGattServiceFloss* service,
    GattCharacteristic* characteristic)
    : characteristic_(characteristic), service_(service) {
  DCHECK(service);
  DCHECK(characteristic);

  service_->AddObserverForHandle(characteristic_->instance_id, this);

  for (GattDescriptor& d : characteristic_->descriptors) {
    AddDescriptor(
        BluetoothRemoteGattDescriptorFloss::Create(service_, this, &d));
  }
}

BluetoothRemoteGattCharacteristicFloss::
    ~BluetoothRemoteGattCharacteristicFloss() {
  descriptors_.clear();
  service_->RemoveObserverForHandle(characteristic_->instance_id);
}

std::string BluetoothRemoteGattCharacteristicFloss::GetIdentifier() const {
  return base::StringPrintf("%s/%d", service_->GetIdentifier().c_str(),
                            characteristic_->instance_id);
}

device::BluetoothUUID BluetoothRemoteGattCharacteristicFloss::GetUUID() const {
  return characteristic_->uuid;
}

BluetoothRemoteGattCharacteristicFloss::Properties
BluetoothRemoteGattCharacteristicFloss::GetProperties() const {
  return characteristic_->properties;
}

BluetoothRemoteGattCharacteristicFloss::Permissions
BluetoothRemoteGattCharacteristicFloss::GetPermissions() const {
  return characteristic_->permissions;
}

const std::vector<uint8_t>& BluetoothRemoteGattCharacteristicFloss::GetValue()
    const {
  return cached_data_;
}

device::BluetoothRemoteGattService*
BluetoothRemoteGattCharacteristicFloss::GetService() const {
  return static_cast<device::BluetoothRemoteGattService*>(service_.get());
}

void BluetoothRemoteGattCharacteristicFloss::ReadRemoteCharacteristic(
    ValueCallback callback) {
  DCHECK_GE(num_of_reads_in_progress_, 0);
  ++num_of_reads_in_progress_;

  AuthRequired auth = GetAuthForRead();

  FlossDBusManager::Get()->GetGattManagerClient()->ReadCharacteristic(
      base::BindOnce(
          &BluetoothRemoteGattCharacteristicFloss::OnReadCharacteristic,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      service_->GetDevice()->GetAddress(), characteristic_->instance_id, auth);
}

void BluetoothRemoteGattCharacteristicFloss::WriteRemoteCharacteristic(
    const std::vector<uint8_t>& value,
    device::BluetoothRemoteGattCharacteristic::WriteType write_type,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  floss::WriteType gatt_write_type = floss::WriteType::kWriteNoResponse;
  if (write_type ==
      device::BluetoothRemoteGattCharacteristic::WriteType::kWithResponse) {
    gatt_write_type = floss::WriteType::kWrite;
  }

  WriteRemoteCharacteristicImpl(value, gatt_write_type, std::move(callback),
                                std::move(error_callback));
}

void BluetoothRemoteGattCharacteristicFloss::
    DeprecatedWriteRemoteCharacteristic(const std::vector<uint8_t>& value,
                                        base::OnceClosure callback,
                                        ErrorCallback error_callback) {
  WriteRemoteCharacteristicImpl(value, floss::WriteType::kWriteNoResponse,
                                std::move(callback), std::move(error_callback));
}

#if BUILDFLAG(IS_CHROMEOS)
void BluetoothRemoteGattCharacteristicFloss::PrepareWriteRemoteCharacteristic(
    const std::vector<uint8_t>& value,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  WriteRemoteCharacteristicImpl(value, floss::WriteType::kWritePrepare,
                                std::move(callback), std::move(error_callback));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void BluetoothRemoteGattCharacteristicFloss::WriteRemoteCharacteristicImpl(
    const std::vector<uint8_t>& value,
    floss::WriteType write_type,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  AuthRequired auth = GetAuthForWrite();

  FlossDBusManager::Get()->GetGattManagerClient()->WriteCharacteristic(
      base::BindOnce(
          &BluetoothRemoteGattCharacteristicFloss::OnWriteCharacteristic,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback),
          std::move(error_callback), value),
      service_->GetDevice()->GetAddress(), characteristic_->instance_id,
      write_type, auth, value);
}

void BluetoothRemoteGattCharacteristicFloss::GattCharacteristicRead(
    std::string address,
    GattStatus status,
    int32_t handle,
    const std::vector<uint8_t>& data) {
  if (handle != characteristic_->instance_id ||
      address != service_->GetDevice()->GetAddress()) {
    return;
  }

  --num_of_reads_in_progress_;
  DCHECK_GE(num_of_reads_in_progress_, 0);

  if (status == GattStatus::kSuccess) {
    cached_data_ = data;

    std::move(pending_read_callback_)
        .Run(/*error_code=*/absl::nullopt, cached_data_);
    NotifyValueChanged();
  } else {
    std::move(pending_read_callback_)
        .Run(BluetoothGattServiceFloss::GattStatusToServiceError(status), {});
  }
}

void BluetoothRemoteGattCharacteristicFloss::GattCharacteristicWrite(
    std::string address,
    GattStatus status,
    int32_t handle) {
  if (handle != characteristic_->instance_id ||
      address != service_->GetDevice()->GetAddress()) {
    return;
  }

  auto [callback, error_callback, data] = std::move(pending_write_callbacks_);

  if (status == GattStatus::kSuccess) {
    cached_data_ = data;

    std::move(callback).Run();
    NotifyValueChanged();
  } else {
    std::move(error_callback)
        .Run(BluetoothGattServiceFloss::GattStatusToServiceError(status));
  }
}

void BluetoothRemoteGattCharacteristicFloss::GattNotify(
    std::string address,
    int32_t handle,
    const std::vector<uint8_t>& data) {
  if (handle != characteristic_->instance_id ||
      address != service_->GetDevice()->GetAddress()) {
    return;
  }

  cached_data_ = data;
  NotifyValueChanged();
}

void BluetoothRemoteGattCharacteristicFloss::OnReadCharacteristic(
    ValueCallback callback,
    DBusResult<Void> result) {
  if (!result.has_value()) {
    --num_of_reads_in_progress_;
    DCHECK_GE(num_of_reads_in_progress_, 0);

    std::move(callback).Run(
        /*error_code=*/BluetoothGattServiceFloss::GattErrorCode::kFailed, {});
    return;
  }

  pending_read_callback_ = std::move(callback);
}

void BluetoothRemoteGattCharacteristicFloss::OnWriteCharacteristic(
    base::OnceClosure callback,
    ErrorCallback error_callback,
    std::vector<uint8_t> data,
    DBusResult<Void> result) {
  if (!result.has_value()) {
    std::move(error_callback)
        .Run(/*error_code=*/BluetoothGattServiceFloss::GattErrorCode::kFailed);
    return;
  }

  pending_write_callbacks_ = std::make_tuple(
      std::move(callback), std::move(error_callback), std::move(data));
}

void BluetoothRemoteGattCharacteristicFloss::SubscribeToNotifications(
    device::BluetoothRemoteGattDescriptor* ccc_descriptor,
#if BUILDFLAG(IS_CHROMEOS)
    NotificationType notification_type,
#endif  // BUILDFLAG(IS_CHROMEOS)
    base::OnceClosure callback,
    ErrorCallback error_callback) {
#if !BUILDFLAG(IS_CHROMEOS)
  NotificationType notification_type = NotificationType::kNotification;
#endif

  // Set CCCD value to notification type
  std::vector<uint8_t> value = {static_cast<uint8_t>(notification_type), 0};

  // Register this characteristic for notifications
  FlossDBusManager::Get()->GetGattManagerClient()->RegisterForNotification(
      base::BindOnce(
          &BluetoothRemoteGattCharacteristicFloss::OnRegisterForNotification,
          weak_ptr_factory_.GetWeakPtr(), ccc_descriptor, value,
          std::move(callback), std::move(error_callback)),
      service_->GetDevice()->GetAddress(), characteristic_->instance_id);
}

void BluetoothRemoteGattCharacteristicFloss::UnsubscribeFromNotifications(
    device::BluetoothRemoteGattDescriptor* ccc_descriptor,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  // Set CCCD value back to default
  std::vector<uint8_t> value = {0, 0};

  // Unregister this characteristic for notifications
  FlossDBusManager::Get()->GetGattManagerClient()->UnregisterNotification(
      base::BindOnce(
          &BluetoothRemoteGattCharacteristicFloss::OnRegisterForNotification,
          weak_ptr_factory_.GetWeakPtr(), ccc_descriptor, value,
          std::move(callback), std::move(error_callback)),
      service_->GetDevice()->GetAddress(), characteristic_->instance_id);
}

void BluetoothRemoteGattCharacteristicFloss::OnRegisterForNotification(
    device::BluetoothRemoteGattDescriptor* ccc_descriptor,
    const std::vector<uint8_t>& value,
    base::OnceClosure callback,
    ErrorCallback error_callback,
    DBusResult<GattStatus> result) {
  if (!result.has_value() || result.value() != GattStatus::kSuccess) {
    std::move(error_callback)
        .Run(/*error_code=*/BluetoothGattServiceFloss::GattStatusToServiceError(
            result.value()));

    return;
  }

  BluetoothRemoteGattDescriptorFloss* descriptor =
      static_cast<BluetoothRemoteGattDescriptorFloss*>(ccc_descriptor);

  DCHECK(descriptor);

  // Write to the Client Characteristic Configuration descriptor.
  descriptor->WriteRemoteDescriptor(value, std::move(callback),
                                    std::move(error_callback));
}

AuthRequired BluetoothRemoteGattCharacteristicFloss::GetAuthForRead() const {
  AuthRequired auth = AuthRequired::kNoAuth;
  Properties props = GetProperties();

  if (props & PROPERTY_READ_ENCRYPTED_AUTHENTICATED) {
    auth = AuthRequired::kReqMitm;
  } else if (props & PROPERTY_READ_ENCRYPTED) {
    auth = AuthRequired::kNoMitm;
  }

  return auth;
}

AuthRequired BluetoothRemoteGattCharacteristicFloss::GetAuthForWrite() const {
  AuthRequired auth = AuthRequired::kNoAuth;
  Properties props = GetProperties();

  if (props & PROPERTY_WRITE_ENCRYPTED_AUTHENTICATED) {
    auth = AuthRequired::kReqMitm;
    if (props & PROPERTY_AUTHENTICATED_SIGNED_WRITES) {
      auth = AuthRequired::kSignedReqMitm;
    }
  } else if (props & PROPERTY_WRITE_ENCRYPTED) {
    auth = AuthRequired::kNoMitm;
    if (props & PROPERTY_AUTHENTICATED_SIGNED_WRITES) {
      auth = AuthRequired::kSignedNoMitm;
    }
  }

  return auth;
}

void BluetoothRemoteGattCharacteristicFloss::NotifyValueChanged() {
  DCHECK(service_->GetAdapter());

  service_->GetAdapter()->NotifyGattCharacteristicValueChanged(this,
                                                               cached_data_);
}

}  // namespace floss
