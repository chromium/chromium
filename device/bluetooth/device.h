// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DEVICE_H_
#define DEVICE_BLUETOOTH_DEVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/public/mojom/device.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace bluetooth {

// Implementation of Mojo Device located in
// device/bluetooth/public/mojom/device.mojom.
// It handles requests to interact with Bluetooth Device.
// Uses the platform abstraction of device/bluetooth.
// An instance of this class is constructed by Adapter and strongly bound
// to its MessagePipe. In the case where the BluetoothGattConnection dies, the
// instance closes the binding which causes the instance to be deleted.
class Device : public mojom::Device, public device::BluetoothAdapter::Observer {
 public:
  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;

  ~Device() override;

  static void Create(
      scoped_refptr<device::BluetoothAdapter> adapter,
      std::unique_ptr<device::BluetoothGattConnection> connection,
      mojo::PendingReceiver<mojom::Device> receiver);

  // Creates a mojom::DeviceInfo using info from the given |device|.
  static mojom::DeviceInfoPtr ConstructDeviceInfoStruct(
      const device::BluetoothDevice* device);

  // BluetoothAdapter::Observer overrides:
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void GattServicesDiscovered(device::BluetoothAdapter* adapter,
                              device::BluetoothDevice* device) override;

  // mojom::Device overrides:
  void Disconnect() override;
  void GetInfo(GetInfoCallback callback) override;
  void GetServices(GetServicesCallback callback) override;
  void GetCharacteristics(const std::string& service_id,
                          GetCharacteristicsCallback callback) override;
  void ReadValueForCharacteristic(
      const std::string& service_id,
      const std::string& characteristic_id,
      ReadValueForCharacteristicCallback callback) override;
  void WriteValueForCharacteristic(
      const std::string& service_id,
      const std::string& characteristic_id,
      const std::vector<uint8_t>& value,
      WriteValueForCharacteristicCallback callback) override;
  void GetDescriptors(const std::string& service_id,
                      const std::string& characteristic_id,
                      GetDescriptorsCallback callback) override;
  void ReadValueForDescriptor(const std::string& service_id,
                              const std::string& characteristic_id,
                              const std::string& descriptor_id,
                              ReadValueForDescriptorCallback callback) override;
  void WriteValueForDescriptor(
      const std::string& service_id,
      const std::string& characteristic_id,
      const std::string& descriptor_id,
      const std::vector<uint8_t>& value,
      WriteValueForDescriptorCallback callback) override;

 private:
  Device(scoped_refptr<device::BluetoothAdapter> adapter,
         std::unique_ptr<device::BluetoothGattConnection> connection);

  void GetServicesImpl(GetServicesCallback callback);

  mojom::ServiceInfoPtr ConstructServiceInfoStruct(
      const device::BluetoothRemoteGattService& service);

  void OnReadRemoteCharacteristic(
      ReadValueForCharacteristicCallback callback,
      std::optional<device::BluetoothGattService::GattErrorCode> error_code,
      const std::vector<uint8_t>& value);

  void OnWriteRemoteCharacteristic(
      WriteValueForCharacteristicCallback callback);

  void OnWriteRemoteCharacteristicError(
      WriteValueForCharacteristicCallback callback,
      device::BluetoothGattService::GattErrorCode error_code);

  void OnReadRemoteDescriptor(
      ReadValueForDescriptorCallback callback,
      std::optional<device::BluetoothGattService::GattErrorCode> error_code,
      const std::vector<uint8_t>& value);

  void OnWriteRemoteDescriptor(WriteValueForDescriptorCallback callback);

  void OnWriteRemoteDescriptorError(
      WriteValueForDescriptorCallback callback,
      device::BluetoothGattService::GattErrorCode error_code);

  const std::string& GetAddress();

  // The current BluetoothAdapter.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // The GATT connection to this device.
  std::unique_ptr<device::BluetoothGattConnection> connection_;

  mojo::SelfOwnedReceiverRef<mojom::Device> receiver_;

  // The services request queue which holds callbacks that are waiting for
  // services to be discovered for this device.
  std::vector<base::OnceClosure> pending_services_requests_;

  base::WeakPtrFactory<Device> weak_ptr_factory_{this};
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_DEVICE_H_
