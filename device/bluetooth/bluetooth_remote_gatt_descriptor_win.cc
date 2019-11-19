// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_descriptor_win.h"

#include "base/bind.h"
#include "device/bluetooth/bluetooth_adapter_win.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic_win.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_win.h"

namespace device {

BluetoothRemoteGattDescriptorWin::BluetoothRemoteGattDescriptorWin(
    BluetoothRemoteGattCharacteristicWin* parent_characteristic,
    BTH_LE_GATT_DESCRIPTOR* descriptor_info,
    scoped_refptr<base::SequencedTaskRunner>& ui_task_runner)
    : parent_characteristic_(parent_characteristic),
      descriptor_info_(descriptor_info),
      ui_task_runner_(ui_task_runner) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(parent_characteristic_);
  DCHECK(descriptor_info_.get());

  task_manager_ = parent_characteristic_->GetWinService()
                      ->GetWinAdapter()
                      ->GetWinBluetoothTaskManager();
  DCHECK(task_manager_);
  service_path_ = parent_characteristic_->GetWinService()->GetServicePath();
  DCHECK(!service_path_.empty());
  descriptor_uuid_ =
      BluetoothTaskManagerWin::BluetoothLowEnergyUuidToBluetoothUuid(
          descriptor_info_.get()->DescriptorUuid);
  DCHECK(descriptor_uuid_.IsValid());
  descriptor_identifier_ = parent_characteristic_->GetIdentifier() + "_" +
                           std::to_string(descriptor_info_->AttributeHandle);
}

BluetoothRemoteGattDescriptorWin::~BluetoothRemoteGattDescriptorWin() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  parent_characteristic_->GetWinService()
      ->GetWinAdapter()
      ->NotifyGattDescriptorRemoved(this);
}

std::string BluetoothRemoteGattDescriptorWin::GetIdentifier() const {
  return descriptor_identifier_;
}

BluetoothUUID BluetoothRemoteGattDescriptorWin::GetUUID() const {
  return descriptor_uuid_;
}

std::vector<uint8_t>& BluetoothRemoteGattDescriptorWin::GetValue() const {
  NOTIMPLEMENTED();
  return const_cast<std::vector<uint8_t>&>(descriptor_value_);
}

BluetoothRemoteGattCharacteristic*
BluetoothRemoteGattDescriptorWin::GetCharacteristic() const {
  return parent_characteristic_;
}

BluetoothRemoteGattCharacteristic::Permissions
BluetoothRemoteGattDescriptorWin::GetPermissions() const {
  NOTIMPLEMENTED();
  return descriptor_permissions_;
}

void BluetoothRemoteGattDescriptorWin::ReadRemoteDescriptor(
    ValueCallback callback,
    ErrorCallback error_callback) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  NOTIMPLEMENTED();
  std::move(error_callback)
      .Run(BluetoothRemoteGattService::GATT_ERROR_NOT_SUPPORTED);
}

void BluetoothRemoteGattDescriptorWin::WriteRemoteDescriptor(
    const std::vector<uint8_t>& new_value,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  NOTIMPLEMENTED();
  std::move(error_callback)
      .Run(BluetoothRemoteGattService::GATT_ERROR_NOT_SUPPORTED);
}

uint16_t BluetoothRemoteGattDescriptorWin::GetAttributeHandle() const {
  return descriptor_info_->AttributeHandle;
}

}  // namespace device.
