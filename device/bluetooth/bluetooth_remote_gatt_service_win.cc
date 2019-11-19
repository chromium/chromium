// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_service_win.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "device/bluetooth/bluetooth_adapter_win.h"
#include "device/bluetooth/bluetooth_device_win.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic_win.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"

namespace device {

BluetoothRemoteGattServiceWin::BluetoothRemoteGattServiceWin(
    BluetoothDeviceWin* device,
    base::FilePath service_path,
    BluetoothUUID service_uuid,
    uint16_t service_attribute_handle,
    bool is_primary,
    BluetoothRemoteGattServiceWin* parent_service,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : device_(device),
      service_path_(service_path),
      service_uuid_(service_uuid),
      service_attribute_handle_(service_attribute_handle),
      is_primary_(is_primary),
      parent_service_(parent_service),
      ui_task_runner_(std::move(ui_task_runner)) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!service_path_.empty());
  DCHECK(service_uuid_.IsValid());
  DCHECK(service_attribute_handle_);
  DCHECK(device_);
  if (!is_primary_)
    DCHECK(parent_service_);

  adapter_ = static_cast<BluetoothAdapterWin*>(device_->GetAdapter());
  DCHECK(adapter_);
  task_manager_ = adapter_->GetWinBluetoothTaskManager();
  DCHECK(task_manager_);
  service_identifier_ = device_->GetIdentifier() + "/" + service_uuid_.value() +
                        "_" + std::to_string(service_attribute_handle_);
  Update();
}

BluetoothRemoteGattServiceWin::~BluetoothRemoteGattServiceWin() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  ClearIncludedCharacteristics();

  adapter_->NotifyGattServiceRemoved(this);
}

std::string BluetoothRemoteGattServiceWin::GetIdentifier() const {
  return service_identifier_;
}

BluetoothUUID BluetoothRemoteGattServiceWin::GetUUID() const {
  return const_cast<BluetoothUUID&>(service_uuid_);
}

bool BluetoothRemoteGattServiceWin::IsPrimary() const {
  return is_primary_;
}

BluetoothDevice* BluetoothRemoteGattServiceWin::GetDevice() const {
  return device_;
}

std::vector<BluetoothRemoteGattService*>
BluetoothRemoteGattServiceWin::GetIncludedServices() const {
  NOTIMPLEMENTED();
  // TODO(crbug.com/590008): Needs implementation.
  return std::vector<BluetoothRemoteGattService*>();
}

void BluetoothRemoteGattServiceWin::GattCharacteristicDiscoveryComplete(
    BluetoothRemoteGattCharacteristicWin* characteristic) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(base::Contains(characteristics_, characteristic->GetIdentifier()));

  discovery_completed_included_characteristics_.insert(
      characteristic->GetIdentifier());
  SetDiscoveryComplete(characteristics_.size() ==
                       discovery_completed_included_characteristics_.size());
  adapter_->NotifyGattCharacteristicAdded(characteristic);
  NotifyGattServiceDiscoveryCompleteIfNecessary();
}

void BluetoothRemoteGattServiceWin::Update() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  ++discovery_pending_count_;
  task_manager_->PostGetGattIncludedCharacteristics(
      service_path_, service_uuid_, service_attribute_handle_,
      base::Bind(&BluetoothRemoteGattServiceWin::OnGetIncludedCharacteristics,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothRemoteGattServiceWin::OnGetIncludedCharacteristics(
    std::unique_ptr<BTH_LE_GATT_CHARACTERISTIC> characteristics,
    uint16_t num,
    HRESULT hr) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  if (--discovery_pending_count_ != 0)
    return;

  UpdateIncludedCharacteristics(characteristics.get(), num);
  SetDiscoveryComplete(characteristics_.size() ==
                       discovery_completed_included_characteristics_.size());

  // In case there are new included characterisitics that haven't been
  // discovered yet, observers should be notified once that the discovery of
  // these characteristics is complete. Hence the discovery complete flag is
  // reset.
  if (!IsDiscoveryComplete()) {
    discovery_complete_notified_ = false;
    return;
  }

  adapter_->NotifyGattServiceChanged(this);
  NotifyGattServiceDiscoveryCompleteIfNecessary();
}

void BluetoothRemoteGattServiceWin::UpdateIncludedCharacteristics(
    PBTH_LE_GATT_CHARACTERISTIC characteristics,
    uint16_t num) {
  if (num == 0) {
    if (!characteristics_.empty()) {
      ClearIncludedCharacteristics();
      adapter_->NotifyGattServiceChanged(this);
    }
    return;
  }

  // First, remove characteristics that no longer exist.
  std::vector<std::string> to_be_removed;
  for (const auto& c : characteristics_) {
    if (!DoesCharacteristicExist(
            characteristics, num,
            static_cast<BluetoothRemoteGattCharacteristicWin*>(
                c.second.get()))) {
      to_be_removed.push_back(c.second->GetIdentifier());
    }
  }
  for (const auto& id : to_be_removed) {
    RemoveIncludedCharacteristic(id);
  }

  // Update previously known characteristics.
  for (auto& c : characteristics_) {
    static_cast<BluetoothRemoteGattCharacteristicWin*>(c.second.get())
        ->Update();
  }

  // Return if no new characteristics have been added.
  if (characteristics_.size() == num)
    return;

  // Add new characteristics.
  for (uint16_t i = 0; i < num; i++) {
    if (!IsCharacteristicDiscovered(characteristics[i].CharacteristicUuid,
                                    characteristics[i].AttributeHandle)) {
      PBTH_LE_GATT_CHARACTERISTIC info = new BTH_LE_GATT_CHARACTERISTIC();
      *info = characteristics[i];
      AddCharacteristic(std::make_unique<BluetoothRemoteGattCharacteristicWin>(
          this, info, ui_task_runner_));
    }
  }
}

void BluetoothRemoteGattServiceWin::
    NotifyGattServiceDiscoveryCompleteIfNecessary() {
  if (IsDiscoveryComplete() && !discovery_complete_notified_) {
    discovery_complete_notified_ = true;
    device_->GattServiceDiscoveryComplete(this);
  }
}

bool BluetoothRemoteGattServiceWin::IsCharacteristicDiscovered(
    const BTH_LE_UUID& uuid,
    uint16_t attribute_handle) {
  BluetoothUUID bt_uuid =
      BluetoothTaskManagerWin::BluetoothLowEnergyUuidToBluetoothUuid(uuid);
  for (const auto& c : characteristics_) {
    if (bt_uuid == c.second->GetUUID() &&
        attribute_handle ==
            static_cast<BluetoothRemoteGattCharacteristicWin*>(c.second.get())
                ->GetAttributeHandle()) {
      return true;
    }
  }
  return false;
}

bool BluetoothRemoteGattServiceWin::DoesCharacteristicExist(
    PBTH_LE_GATT_CHARACTERISTIC characteristics,
    uint16_t num,
    BluetoothRemoteGattCharacteristicWin* characteristic) {
  for (uint16_t i = 0; i < num; i++) {
    BluetoothUUID uuid =
        BluetoothTaskManagerWin::BluetoothLowEnergyUuidToBluetoothUuid(
            characteristics[i].CharacteristicUuid);
    if (characteristic->GetUUID() == uuid &&
        characteristic->GetAttributeHandle() ==
            characteristics[i].AttributeHandle) {
      return true;
    }
  }
  return false;
}

void BluetoothRemoteGattServiceWin::RemoveIncludedCharacteristic(
    std::string identifier) {
  discovery_completed_included_characteristics_.erase(identifier);

  // Explicitly moving the to be deleted characteristic into a local variable,
  // so that we can erase the entry from |characteristics_| before calling the
  // characteristic's destructor. This will ensure that any call to
  // GetCharacteristics() won't contain an entry corresponding to |identifier|.
  // Note: `characteristics_.erase(identifier);` would not guarantee this.
  DCHECK(base::Contains(characteristics_, identifier));
  auto iter = characteristics_.find(identifier);
  auto pair = std::move(*iter);
  characteristics_.erase(iter);
}

void BluetoothRemoteGattServiceWin::ClearIncludedCharacteristics() {
  discovery_completed_included_characteristics_.clear();
  // Explicitly reset to null to ensure that calling GetCharacteristics() in
  // GattCharacteristicRemoved() will return an empty collection.
  // Note: `characteristics_.clear();` would not guarantee this.
  std::exchange(characteristics_, {});
}

}  // namespace device.
