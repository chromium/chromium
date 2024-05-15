// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_GATT_SERVICE_H_
#define DEVICE_BLUETOOTH_GATT_SERVICE_H_

#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace device {
class BluetoothLocalGattService;
class BluetoothLocalGattCharacteristic;
class BluetoothAdapter;
}  // namespace device

namespace bluetooth {

// Implementation of Mojo GattService in
// device/bluetooth/public/mojom/adapter.mojom.
// Uses the platform abstraction of //device/bluetooth.
//
// TODO(b/322381543): Re-evaluate the directory of `GattService`.
class GattService : public mojom::GattService,
                    public device::BluetoothLocalGattService::Delegate {
 public:
  // When owners are notified the `GattService` is invalidated via
  // `on_gatt_service_invalidated`, they are expected to destroy their
  // `GattService` instance.
  GattService(
      mojo::PendingReceiver<mojom::GattService> pending_gatt_service_receiver,
      mojo::PendingRemote<mojom::GattServiceObserver> pending_observer_remote,
      const device::BluetoothUUID& service_id,
      scoped_refptr<device::BluetoothAdapter> adapter,
      base::OnceCallback<void(device::BluetoothUUID)>
          on_gatt_service_invalidated);
  ~GattService() override;
  GattService(const GattService&) = delete;
  GattService& operator=(const GattService&) = delete;

 private:
  // mojom::GattService:
  void CreateCharacteristic(
      const device::BluetoothUUID& characteristic_uuid,
      const device::BluetoothGattCharacteristic::Permissions& permission,
      const device::BluetoothGattCharacteristic::Properties& property,
      CreateCharacteristicCallback callback) override;
  void Register(RegisterCallback callback) override;

  // device::BluetoothLocalGattService::Delegate:
  void OnCharacteristicReadRequest(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattCharacteristic* characteristic,
      int offset,
      ValueCallback callback) override;
  void OnCharacteristicWriteRequest(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value,
      int offset,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;
  void OnCharacteristicPrepareWriteRequest(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value,
      int offset,
      bool has_subsequent_request,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;
  void OnDescriptorReadRequest(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattDescriptor* descriptor,
      int offset,
      ValueCallback callback) override;
  void OnDescriptorWriteRequest(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattDescriptor* descriptor,
      const std::vector<uint8_t>& value,
      int offset,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;
  void OnNotificationsStart(
      const device::BluetoothDevice* device,
      device::BluetoothGattCharacteristic::NotificationType notification_type,
      const device::BluetoothLocalGattCharacteristic* characteristic) override;
  void OnNotificationsStop(
      const device::BluetoothDevice* device,
      const device::BluetoothLocalGattCharacteristic* characteristic) override;

  void OnLocalCharacteristicReadResponse(
      ValueCallback callback,
      mojom::LocalCharacteristicReadResultPtr read_result);

  void OnMojoDisconnect();

  void OnRegisterSuccess(RegisterCallback callback);
  void OnRegisterFailure(
      RegisterCallback callback,
      device::BluetoothGattService::GattErrorCode error_code);

  base::OnceCallback<void(device::BluetoothUUID)> on_gatt_service_invalidated_;
  const device::BluetoothUUID service_id_;
  std::string gatt_service_identifier_;
  std::set<device::BluetoothUUID> characteristic_uuids_;
  mojo::Remote<mojom::GattServiceObserver> observer_remote_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  mojo::Receiver<mojom::GattService> receiver_{this};
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_GATT_SERVICE_H_
