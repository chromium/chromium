// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_remote_gatt_descriptor_floss.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/floss/bluetooth_adapter_floss.h"
#include "device/bluetooth/floss/bluetooth_remote_gatt_characteristic_floss.h"
#include "device/bluetooth/floss/bluetooth_remote_gatt_service_floss.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_gatt_manager_client.h"

namespace floss {

const int kGattTimeoutMs = 2000;

// static
std::unique_ptr<BluetoothRemoteGattDescriptorFloss>
BluetoothRemoteGattDescriptorFloss::Create(
    BluetoothRemoteGattServiceFloss* service,
    BluetoothRemoteGattCharacteristicFloss* characteristic,
    GattDescriptor* descriptor) {
  return base::WrapUnique(new BluetoothRemoteGattDescriptorFloss(
      service, characteristic, descriptor));
}

BluetoothRemoteGattDescriptorFloss::BluetoothRemoteGattDescriptorFloss(
    BluetoothRemoteGattServiceFloss* service,
    BluetoothRemoteGattCharacteristicFloss* characteristic,
    GattDescriptor* descriptor)
    : characteristic_(characteristic),
      descriptor_(descriptor),
      service_(service) {
  DCHECK(service);
  DCHECK(characteristic);
  DCHECK(descriptor);

  service_->AddObserverForHandle(descriptor_->instance_id, this);
}

BluetoothRemoteGattDescriptorFloss::~BluetoothRemoteGattDescriptorFloss() {
  service_->RemoveObserverForHandle(descriptor_->instance_id);
}

std::string BluetoothRemoteGattDescriptorFloss::GetIdentifier() const {
  return base::StringPrintf("%s/%04x", characteristic_->GetIdentifier().c_str(),
                            descriptor_->instance_id);
}

device::BluetoothUUID BluetoothRemoteGattDescriptorFloss::GetUUID() const {
  return descriptor_->uuid;
}

const std::vector<uint8_t>& BluetoothRemoteGattDescriptorFloss::GetValue()
    const {
  return cached_data_;
}

device::BluetoothRemoteGattCharacteristic*
BluetoothRemoteGattDescriptorFloss::GetCharacteristic() const {
  return static_cast<device::BluetoothRemoteGattCharacteristic*>(
      characteristic_.get());
}

device::BluetoothRemoteGattCharacteristic::Permissions
BluetoothRemoteGattDescriptorFloss::GetPermissions() const {
  const auto& [props, perms] =
      BluetoothGattCharacteristicFloss::ConvertPropsAndPermsFromFloss(
          /*properties=*/0, descriptor_->permissions);
  return perms;
}

void BluetoothRemoteGattDescriptorFloss::ReadRemoteDescriptor(
    ValueCallback callback) {
  DCHECK_GE(num_of_reads_in_progress_, 0);
  ++num_of_reads_in_progress_;

  AuthRequired auth = characteristic_->GetAuthForRead();

  FlossDBusManager::Get()->GetGattManagerClient()->ReadDescriptor(
      base::BindOnce(&BluetoothRemoteGattDescriptorFloss::OnReadDescriptor,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      service_->GetDevice()->GetAddress(), descriptor_->instance_id, auth);
}

void BluetoothRemoteGattDescriptorFloss::WriteRemoteDescriptor(
    const std::vector<uint8_t>& new_value,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  AuthRequired auth = characteristic_->GetAuthForWrite();

  FlossDBusManager::Get()->GetGattManagerClient()->WriteDescriptor(
      base::BindOnce(&BluetoothRemoteGattDescriptorFloss::OnWriteDescriptor,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(error_callback), new_value),
      service_->GetDevice()->GetAddress(), descriptor_->instance_id, auth,
      new_value);
}

void BluetoothRemoteGattDescriptorFloss::GattDescriptorRead(
    std::string address,
    GattStatus status,
    int32_t handle,
    const std::vector<uint8_t>& data) {
  if (handle != descriptor_->instance_id ||
      address != service_->GetDevice()->GetAddress()) {
    return;
  }

  --num_of_reads_in_progress_;
  DCHECK_GE(num_of_reads_in_progress_, 0);

  if (status == GattStatus::kSuccess) {
    cached_data_ = data;

    std::move(pending_read_callback_)
        .Run(/*error_code=*/std::nullopt, cached_data_);
  } else {
    std::move(pending_read_callback_)
        .Run(BluetoothGattServiceFloss::GattStatusToServiceError(status), {});
  }
}

void BluetoothRemoteGattDescriptorFloss::GattDescriptorWrite(
    std::string address,
    GattStatus status,
    int32_t handle) {
  if (handle != descriptor_->instance_id ||
      address != service_->GetDevice()->GetAddress()) {
    return;
  }

  // Only handle if there is a write callback pending.
  auto [callback, error_callback, data] = std::move(pending_write_callbacks_);
  if (!callback) {
    return;
  }

  if (status == GattStatus::kSuccess) {
    cached_data_ = data;

    std::move(callback).Run();
  } else {
    std::move(error_callback)
        .Run(BluetoothGattServiceFloss::GattStatusToServiceError(status));
  }
}

void BluetoothRemoteGattDescriptorFloss::GattNotify(
    std::string address,
    int32_t handle,
    const std::vector<uint8_t>& data) {
  if (handle != descriptor_->instance_id ||
      address != service_->GetDevice()->GetAddress()) {
    return;
  }

  cached_data_ = data;
  NotifyValueChanged();
}

void BluetoothRemoteGattDescriptorFloss::OnReadDescriptor(
    ValueCallback callback,
    DBusResult<Void> result) {
  if (!result.has_value()) {
    --num_of_reads_in_progress_;
    DCHECK_GE(num_of_reads_in_progress_, 0);

    std::move(callback).Run(BluetoothGattServiceFloss::GattErrorCode::kFailed,
                            {});
    return;
  }

  pending_read_callback_ = std::move(callback);
}

void BluetoothRemoteGattDescriptorFloss::OnWriteDescriptor(
    base::OnceClosure callback,
    ErrorCallback error_callback,
    std::vector<uint8_t> data,
    DBusResult<Void> result) {
  if (!result.has_value()) {
    std::move(error_callback)
        .Run(BluetoothGattServiceFloss::GattErrorCode::kFailed);
    return;
  }

  pending_write_callbacks_ = std::make_tuple(
      std::move(callback), std::move(error_callback), std::move(data));

  // Ensure callbacks don't get dropped if no |GattDescriptorWrite| received.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BluetoothRemoteGattDescriptorFloss::OnWriteTimeout,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Milliseconds(kGattTimeoutMs));
}

void BluetoothRemoteGattDescriptorFloss::OnWriteTimeout() {
  if (std::get<0>(pending_write_callbacks_)) {
    BLUETOOTH_LOG(ERROR)
        << "Timeout waiting for GattDescriptorWrite for Device "
        << service_->GetDevice()->GetAddress();
    GattDescriptorWrite(service_->GetDevice()->GetAddress(), GattStatus::kError,
                        descriptor_->instance_id);
  }
}

void BluetoothRemoteGattDescriptorFloss::NotifyValueChanged() {
  DCHECK(service_->GetAdapter());

  service_->GetAdapter()->NotifyGattDescriptorValueChanged(this, cached_data_);
}

}  // namespace floss
