// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_local_gatt_service_floss.h"

#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/object_path.h"
#include "device/bluetooth/floss/bluetooth_adapter_floss.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"

namespace floss {

// staic
base::WeakPtr<BluetoothLocalGattServiceFloss>
BluetoothLocalGattServiceFloss::Create(BluetoothAdapterFloss* adapter,
                                       const device::BluetoothUUID& uuid,
                                       bool is_primary) {
  auto* service = new BluetoothLocalGattServiceFloss(adapter, uuid, is_primary);
  auto weak_ptr = service->weak_ptr_factory_.GetWeakPtr();
  adapter->AddLocalGattService(base::WrapUnique(service));
  return weak_ptr;
}

BluetoothLocalGattServiceFloss::BluetoothLocalGattServiceFloss(
    BluetoothAdapterFloss* adapter,
    const device::BluetoothUUID& uuid,
    bool is_primary)
    : BluetoothGattServiceFloss(adapter), is_primary_(is_primary) {
  local_service_.uuid = uuid;
  // TODO: Redesign after the GATT server registration wiring is finished.
  // Temporarily use a random number to prefill the instance_id, as the
  // application may want to access the object before GATT service registration
  // when an instance_id is provided by the daemon through DBUS callback.
  local_service_.instance_id = static_cast<int32_t>(base::RandUint64());
}

BluetoothLocalGattServiceFloss::~BluetoothLocalGattServiceFloss() = default;

std::string BluetoothLocalGattServiceFloss::GetIdentifier() const {
  return base::StringPrintf("%s/%d", GetAdapter()->GetAddress().c_str(),
                            local_service_.instance_id);
}

device::BluetoothUUID BluetoothLocalGattServiceFloss::GetUUID() const {
  return local_service_.uuid;
}

bool BluetoothLocalGattServiceFloss::IsPrimary() const {
  return is_primary_;
}

void BluetoothLocalGattServiceFloss::Register(base::OnceClosure callback,
                                              ErrorCallback error_callback) {
  if (is_registered_) {
    BLUETOOTH_LOG(ERROR)
        << "Re-registering a service that is already registered!";
    std::move(error_callback)
        .Run(device::BluetoothGattService::GattErrorCode::kFailed);
    return;
  }
  DCHECK(GetAdapter());
  GetAdapter()->RegisterGattService(this, std::move(callback),
                                    std::move(error_callback));
}

void BluetoothLocalGattServiceFloss::Unregister(base::OnceClosure callback,
                                                ErrorCallback error_callback) {
  if (!is_registered_) {
    BLUETOOTH_LOG(ERROR)
        << "Unregistering a service that isn't registered! Identifier: "
        << GetIdentifier();
    std::move(error_callback)
        .Run(device::BluetoothGattService::GattErrorCode::kFailed);
    return;
  }
  DCHECK(GetAdapter());
  GetAdapter()->UnregisterGattService(this, std::move(callback),
                                      std::move(error_callback));
}

bool BluetoothLocalGattServiceFloss::IsRegistered() {
  return is_registered_;
}

void BluetoothLocalGattServiceFloss::SetRegistered(bool is_registered) {
  is_registered_ = is_registered;
}

void BluetoothLocalGattServiceFloss::Delete() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  GetAdapter()->RemoveLocalGattService(this);
}

device::BluetoothLocalGattCharacteristic*
BluetoothLocalGattServiceFloss::GetCharacteristic(
    const std::string& identifier) {
  const auto& service = characteristics_.find(identifier);
  return service == characteristics_.end() ? nullptr : service->second.get();
}

void BluetoothLocalGattServiceFloss::AddCharacteristic(
    std::unique_ptr<BluetoothLocalGattCharacteristicFloss> characteristic) {
  DCHECK(!base::Contains(characteristics_, characteristic->GetIdentifier()));
  characteristics_[characteristic->GetIdentifier()] = std::move(characteristic);
}

}  // namespace floss
