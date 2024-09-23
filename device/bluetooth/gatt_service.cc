// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/gatt_service.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/device.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace {

std::string_view GattErrorCodeToString(
    device::BluetoothGattService::GattErrorCode error_code) {
  switch (error_code) {
    case device::BluetoothGattService::GattErrorCode::kUnknown:
      return "Unknown";
    case device::BluetoothGattService::GattErrorCode::kFailed:
      return "Failed";
    case device::BluetoothGattService::GattErrorCode::kInProgress:
      return "In Progress";
    case device::BluetoothGattService::GattErrorCode::kInvalidLength:
      return "Invalid Length";
    case device::BluetoothGattService::GattErrorCode::kNotPermitted:
      return "Not Permitted";
    case device::BluetoothGattService::GattErrorCode::kNotAuthorized:
      return "Not Authorized";
    case device::BluetoothGattService::GattErrorCode::kNotPaired:
      return "Not Paired";
    case device::BluetoothGattService::GattErrorCode::kNotSupported:
      return "Not Supported";
  };
}

}  // namespace

namespace bluetooth {

GattService::GattService(
    mojo::PendingReceiver<mojom::GattService> pending_gatt_service_receiver,
    mojo::PendingRemote<mojom::GattServiceObserver> pending_observer_remote,
    const device::BluetoothUUID& service_id,
    scoped_refptr<device::BluetoothAdapter> adapter,
    base::OnceCallback<void(device::BluetoothUUID)> on_gatt_service_invalidated)
    : on_gatt_service_invalidated_(std::move(on_gatt_service_invalidated)),
      service_id_(service_id),
      observer_remote_(std::move(pending_observer_remote)),
      adapter_(std::move(adapter)) {
  receiver_.Bind(std::move(pending_gatt_service_receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&GattService::OnMojoDisconnect, base::Unretained(this)));
  observer_remote_.set_disconnect_handler(
      base::BindOnce(&GattService::OnMojoDisconnect, base::Unretained(this)));

  gatt_service_identifier_ = adapter_
                                 ->CreateLocalGattService(
                                     /*uuid=*/service_id,
                                     /*is_primary=*/true,
                                     /*delegate=*/this)
                                 ->GetIdentifier();
}

GattService::~GattService() = default;

void GattService::CreateCharacteristic(
    const device::BluetoothUUID& characteristic_uuid,
    const device::BluetoothGattCharacteristic::Permissions& permissions,
    const device::BluetoothGattCharacteristic::Properties& properties,
    CreateCharacteristicCallback callback) {
  device::BluetoothLocalGattService* service =
      adapter_->GetGattService(gatt_service_identifier_);
  if (!service) {
    LOG(WARNING) << __func__ << ": expected local GATT service at service id = "
                 << service_id_.canonical_value() << " does not exist.";
    std::move(callback).Run(/*success=*/false);
    return;
  }

  // Otherwise, attempt  to create the characteristic and add it to this GATT
  // server.
  auto created_characteristic = service->CreateCharacteristic(
      /*uuid=*/characteristic_uuid,
      /*properties=*/properties,
      /*permissions=*/permissions);
  if (!created_characteristic) {
    LOG(WARNING) << __func__
                 << ": failure to create a characteristic at uuid = "
                 << characteristic_uuid.canonical_value();
    std::move(callback).Run(/*success=*/false);
    return;
  }

  characteristic_uuids_.insert(characteristic_uuid);
  std::move(callback).Run(/*success=*/true);
}

void GattService::Register(RegisterCallback callback) {
  device::BluetoothLocalGattService* service =
      adapter_->GetGattService(gatt_service_identifier_);
  if (!service) {
    LOG(WARNING) << __func__ << ": local GATT service destroyed.";
    std::move(callback).Run(
        device::BluetoothGattService::GattErrorCode::kFailed);
    return;
  }

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  service->Register(/*callback=*/base::BindOnce(
                        &GattService::OnRegisterSuccess, base::Unretained(this),
                        std::move(split_callback.first)),
                    /*error_callback=*/base::BindOnce(
                        &GattService::OnRegisterFailure, base::Unretained(this),
                        std::move(split_callback.second)));
}

void GattService::OnCharacteristicReadRequest(
    const device::BluetoothDevice* device,
    const device::BluetoothLocalGattCharacteristic* characteristic,
    int offset,
    ValueCallback callback) {
  CHECK(characteristic);
  CHECK(base::Contains(characteristic_uuids_, characteristic->GetUUID()));

  observer_remote_->OnLocalCharacteristicRead(
      /*remote_device=*/Device::ConstructDeviceInfoStruct(device),
      /*characteristic_uuid=*/characteristic->GetUUID(),
      /*service_uuid=*/service_id_,
      /*offset=*/offset,
      /*callback=*/
      base::BindOnce(&GattService::OnLocalCharacteristicReadResponse,
                     base::Unretained(this), std::move(callback)));
}

void GattService::OnCharacteristicWriteRequest(
    const device::BluetoothDevice* device,
    const device::BluetoothLocalGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value,
    int offset,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void GattService::OnCharacteristicPrepareWriteRequest(
    const device::BluetoothDevice* device,
    const device::BluetoothLocalGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value,
    int offset,
    bool has_subsequent_request,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void GattService::OnDescriptorReadRequest(
    const device::BluetoothDevice* device,
    const device::BluetoothLocalGattDescriptor* descriptor,
    int offset,
    ValueCallback callback) {
  NOTIMPLEMENTED();
}

void GattService::OnDescriptorWriteRequest(
    const device::BluetoothDevice* device,
    const device::BluetoothLocalGattDescriptor* descriptor,
    const std::vector<uint8_t>& value,
    int offset,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void GattService::OnNotificationsStart(
    const device::BluetoothDevice* device,
    device::BluetoothGattCharacteristic::NotificationType notification_type,
    const device::BluetoothLocalGattCharacteristic* characteristic) {
  NOTIMPLEMENTED();
}

void GattService::OnNotificationsStop(
    const device::BluetoothDevice* device,
    const device::BluetoothLocalGattCharacteristic* characteristic) {
  NOTIMPLEMENTED();
}

void GattService::OnLocalCharacteristicReadResponse(
    ValueCallback callback,
    mojom::LocalCharacteristicReadResultPtr read_result) {
  if (read_result->is_error_code()) {
    LOG(WARNING) << __func__ << ": failed due to error: "
                 << GattErrorCodeToString(read_result->get_error_code());
    std::move(callback).Run(
        /*error_code=*/read_result->get_error_code(),
        /*data=*/std::vector<uint8_t>());
    return;
  }

  CHECK(read_result->is_data());
  std::move(callback).Run(
      /*error_code=*/std::nullopt,
      /*data=*/read_result->get_data());
}

void GattService::OnMojoDisconnect() {
  device::BluetoothLocalGattService* service =
      adapter_->GetGattService(gatt_service_identifier_);
  if (!service) {
    LOG(WARNING) << __func__ << ": local GATT service does not exist.";
  } else {
    service->Delete();
  }

  receiver_.reset();

  // This call needs to be the last in the `OnMojoDisconnect()` logic because
  // this `GattService` is expected to be destroyed after calling
  // `on_gatt_service_invalidated_`.
  std::move(on_gatt_service_invalidated_).Run(service_id_);
}

void GattService::OnRegisterSuccess(RegisterCallback callback) {
  VLOG(1) << __func__;
  std::move(callback).Run(/*error_code=*/std::nullopt);
}

void GattService::OnRegisterFailure(
    RegisterCallback callback,
    device::BluetoothGattService::GattErrorCode error_code) {
  LOG(WARNING) << __func__ << ": failed due to error: "
               << GattErrorCodeToString(error_code);
  std::move(callback).Run(error_code);
}

}  // namespace bluetooth
