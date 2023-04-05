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

// static
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
    : BluetoothGattServiceFloss(adapter),
      is_primary_(is_primary),
      uuid_(uuid),
      client_instance_id_(NewInstanceId()) {}

BluetoothLocalGattServiceFloss::~BluetoothLocalGattServiceFloss() = default;

std::string BluetoothLocalGattServiceFloss::GetIdentifier() const {
  return base::StringPrintf("%s-%s/%04x", GetAdapter()->GetAddress().c_str(),
                            GetUUID().value().c_str(), client_instance_id_);
}

device::BluetoothUUID BluetoothLocalGattServiceFloss::GetUUID() const {
  return uuid_;
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

  if (register_callbacks_.first || register_callbacks_.second) {
    std::move(error_callback)
        .Run(device::BluetoothGattService::GattErrorCode::kInProgress);
    return;
  }
  register_callbacks_ =
      std::make_pair(std::move(callback), std::move(error_callback));

  GetAdapter()->RegisterGattService(this);
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

  if (unregister_callbacks_.first || unregister_callbacks_.second) {
    std::move(error_callback)
        .Run(device::BluetoothGattService::GattErrorCode::kInProgress);
    return;
  }
  unregister_callbacks_ =
      std::make_pair(std::move(callback), std::move(error_callback));

  GetAdapter()->UnregisterGattService(this);
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
  for (auto& characteristic : characteristics_) {
    if (characteristic->GetIdentifier() == identifier) {
      return characteristic.get();
    }
  }
  return nullptr;
}

int32_t BluetoothLocalGattServiceFloss::AddCharacteristic(
    std::unique_ptr<BluetoothLocalGattCharacteristicFloss> characteristic) {
  characteristics_.push_back(std::move(characteristic));
  return characteristics_.size() - 1;
}

GattService BluetoothLocalGattServiceFloss::ToGattService() {
  GattService service;
  service.uuid = uuid_;
  service.instance_id = floss_instance_id_;
  service.service_type = is_primary_ ? GattService::GATT_SERVICE_TYPE_PRIMARY
                                     : GattService::GATT_SERVICE_TYPE_SECONDARY;
  for (auto& included_service : included_services_) {
    service.included_services.push_back(included_service->ToGattService());
  }
  for (auto& characteristic : characteristics_) {
    service.characteristics.push_back(characteristic->ToGattCharacteristic());
  }
  return service;
}

void BluetoothLocalGattServiceFloss::ResolveInstanceId(
    const GattService& service) {
  floss_instance_id_ = service.instance_id;
}

void BluetoothLocalGattServiceFloss::GattServerServiceAdded(
    GattStatus status,
    GattService service) {
  if (service.uuid != GetUUID()) {
    return;
  }
  if (!is_included_service_ &&
      (!register_callbacks_.first || !register_callbacks_.second)) {
    // If register callbacks are not set, we are not meant to handle this.
    return;
  }
  if (status != GattStatus::kSuccess) {
    std::move(register_callbacks_).second.Run(GattStatusToServiceError(status));
    DCHECK(!register_callbacks_.second);
    return;
  }

  // Resolve instance ids of included services and their sub-attributes.
  DCHECK(included_services_.size() == service.included_services.size());
  int32_t service_idx = 0;
  for (auto& included_service : included_services_) {
    included_service->GattServerServiceAdded(
        GattStatus::kSuccess, service.included_services[service_idx++]);
  }

  for (auto& characteristic : characteristics_) {
    characteristic->ResolveInstanceId(service);
    GattCharacteristic local_characteristic =
        characteristic->ToGattCharacteristic();
    for (auto& descriptor : characteristic->descriptors_) {
      descriptor->ResolveInstanceId(local_characteristic);
    }
  }

  this->ResolveInstanceId(service);
  if (is_included_service_) {
    return;
  }
  SetRegistered(true);
  std::move(register_callbacks_).first.Run();
  DCHECK(!register_callbacks_.first);
}

void BluetoothLocalGattServiceFloss::GattServerServiceRemoved(GattStatus status,
                                                              int32_t handle) {
  if (handle != floss_instance_id_) {
    return;
  }
  if (!unregister_callbacks_.first || !unregister_callbacks_.second) {
    return;
  }

  if (status != GattStatus::kSuccess) {
    std::move(unregister_callbacks_)
        .second.Run(GattStatusToServiceError(status));
    DCHECK(!unregister_callbacks_.second);
    return;
  }

  SetRegistered(false);
  std::move(unregister_callbacks_).first.Run();
  DCHECK(!unregister_callbacks_.first);
}

// static
uint32_t BluetoothLocalGattServiceFloss::instance_id_tracker_ = 1000;

// static
uint32_t BluetoothLocalGattServiceFloss::NewInstanceId() {
  return instance_id_tracker_++;
}

}  // namespace floss
