// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/device.h"
#include "device/bluetooth/public/mojom/gatt_result_type_converter.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace bluetooth {
Device::~Device() {
  adapter_->RemoveObserver(this);
}

// static
void Device::Create(scoped_refptr<device::BluetoothAdapter> adapter,
                    std::unique_ptr<device::BluetoothGattConnection> connection,
                    mojo::PendingReceiver<mojom::Device> receiver) {
  auto device_impl =
      base::WrapUnique(new Device(adapter, std::move(connection)));
  auto* device_ptr = device_impl.get();
  device_ptr->receiver_ =
      mojo::MakeSelfOwnedReceiver(std::move(device_impl), std::move(receiver));
}

// static
mojom::DeviceInfoPtr Device::ConstructDeviceInfoStruct(
    const device::BluetoothDevice* device) {
  mojom::DeviceInfoPtr device_info = mojom::DeviceInfo::New();

  device_info->name = device->GetName();
  device_info->name_for_display =
      base::UTF16ToUTF8(device->GetNameForDisplay());
  device_info->address = device->GetAddress();
  device_info->is_gatt_connected = device->IsGattConnected();

  if (device->GetInquiryRSSI()) {
    device_info->rssi = mojom::RSSIWrapper::New();
    device_info->rssi->value = device->GetInquiryRSSI().value();
  }

  return device_info;
}

void Device::DeviceChanged(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device) {
  if (device->GetAddress() != GetAddress()) {
    return;
  }

  if (!device->IsGattConnected()) {
    receiver_->Close();
  }
}

void Device::GattServicesDiscovered(device::BluetoothAdapter* adapter,
                                    device::BluetoothDevice* device) {
  if (device->GetAddress() != GetAddress()) {
    return;
  }

  std::vector<base::OnceClosure> requests;
  requests.swap(pending_services_requests_);
  for (base::OnceClosure& request : requests) {
    std::move(request).Run();
  }
}

void Device::Disconnect() {
  receiver_->Close();
}

void Device::GetInfo(GetInfoCallback callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(GetAddress());
  DCHECK(device);

  std::move(callback).Run(ConstructDeviceInfoStruct(device));
}

void Device::GetServices(GetServicesCallback callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(GetAddress());
  DCHECK(device);

  if (device->IsGattServicesDiscoveryComplete()) {
    GetServicesImpl(std::move(callback));
    return;
  }

  // pending_services_requests_ is owned by Device, so base::Unretained is
  // safe.
  pending_services_requests_.push_back(base::BindOnce(&Device::GetServicesImpl,
                                                      base::Unretained(this),
                                                      base::Passed(&callback)));
}

void Device::GetCharacteristics(const std::string& service_id,
                                GetCharacteristicsCallback callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(GetAddress());
  DCHECK(device);

  device::BluetoothRemoteGattService* service =
      device->GetGattService(service_id);
  if (service == nullptr) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  std::vector<mojom::CharacteristicInfoPtr> characteristics;

  for (const auto* characteristic : service->GetCharacteristics()) {
    mojom::CharacteristicInfoPtr characteristic_info =
        mojom::CharacteristicInfo::New();

    characteristic_info->id = characteristic->GetIdentifier();
    characteristic_info->uuid = characteristic->GetUUID();
    characteristic_info->properties = characteristic->GetProperties();

    characteristics.push_back(std::move(characteristic_info));
  }

  std::move(callback).Run(std::move(characteristics));
}

void Device::ReadValueForCharacteristic(
    const std::string& service_id,
    const std::string& characteristic_id,
    ReadValueForCharacteristicCallback callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(GetAddress());
  DCHECK(device);

  device::BluetoothRemoteGattService* service =
      device->GetGattService(service_id);
  if (service == nullptr) {
    std::move(callback).Run(mojom::GattResult::SERVICE_NOT_FOUND,
                            base::nullopt /* value */);
    return;
  }

  device::BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(characteristic_id);
  if (characteristic == nullptr) {
    std::move(callback).Run(mojom::GattResult::CHARACTERISTIC_NOT_FOUND,
                            base::nullopt /* value */);
    return;
  }

  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  characteristic->ReadRemoteCharacteristic(
      base::BindOnce(&Device::OnReadRemoteCharacteristic,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback),
      base::BindOnce(&Device::OnReadRemoteCharacteristicError,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback));
}

void Device::WriteValueForCharacteristic(
    const std::string& service_id,
    const std::string& characteristic_id,
    const std::vector<uint8_t>& value,
    WriteValueForCharacteristicCallback callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(GetAddress());
  DCHECK(device);

  device::BluetoothRemoteGattService* service =
      device->GetGattService(service_id);
  if (service == nullptr) {
    std::move(callback).Run(mojom::GattResult::SERVICE_NOT_FOUND);
    return;
  }

  device::BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(characteristic_id);
  if (characteristic == nullptr) {
    std::move(callback).Run(mojom::GattResult::CHARACTERISTIC_NOT_FOUND);
    return;
  }

  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  characteristic->WriteRemoteCharacteristic(
      value,
      base::BindOnce(&Device::OnWriteRemoteCharacteristic,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback),
      base::BindOnce(&Device::OnWriteRemoteCharacteristicError,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback));
}

void Device::GetDescriptors(const std::string& service_id,
                            const std::string& characteristic_id,
                            GetDescriptorsCallback callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(GetAddress());
  if (!device) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  device::BluetoothRemoteGattService* service =
      device->GetGattService(service_id);
  if (!service) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  device::BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(characteristic_id);
  if (!characteristic) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  std::vector<mojom::DescriptorInfoPtr> descriptors;

  for (const auto* descriptor : characteristic->GetDescriptors()) {
    mojom::DescriptorInfoPtr descriptor_info = mojom::DescriptorInfo::New();

    descriptor_info->id = descriptor->GetIdentifier();
    descriptor_info->uuid = descriptor->GetUUID();
    descriptor_info->last_known_value = descriptor->GetValue();

    descriptors.push_back(std::move(descriptor_info));
  }

  std::move(callback).Run(std::move(descriptors));
}

void Device::ReadValueForDescriptor(const std::string& service_id,
                                    const std::string& characteristic_id,
                                    const std::string& descriptor_id,
                                    ReadValueForDescriptorCallback callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(GetAddress());
  DCHECK(device);

  device::BluetoothRemoteGattService* service =
      device->GetGattService(service_id);
  if (!service) {
    std::move(callback).Run(mojom::GattResult::SERVICE_NOT_FOUND,
                            base::nullopt /* value */);
    return;
  }

  device::BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(characteristic_id);
  if (!characteristic) {
    std::move(callback).Run(mojom::GattResult::CHARACTERISTIC_NOT_FOUND,
                            base::nullopt /* value */);
    return;
  }

  device::BluetoothRemoteGattDescriptor* descriptor =
      characteristic->GetDescriptor(descriptor_id);
  if (!descriptor) {
    std::move(callback).Run(mojom::GattResult::DESCRIPTOR_NOT_FOUND,
                            base::nullopt /* value */);
    return;
  }

  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  descriptor->ReadRemoteDescriptor(
      base::BindOnce(&Device::OnReadRemoteDescriptor,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback),
      base::BindOnce(&Device::OnReadRemoteDescriptorError,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback));
}

void Device::WriteValueForDescriptor(const std::string& service_id,
                                     const std::string& characteristic_id,
                                     const std::string& descriptor_id,
                                     const std::vector<uint8_t>& value,
                                     WriteValueForDescriptorCallback callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(GetAddress());
  DCHECK(device);

  device::BluetoothRemoteGattService* service =
      device->GetGattService(service_id);
  if (!service) {
    std::move(callback).Run(mojom::GattResult::SERVICE_NOT_FOUND);
    return;
  }

  device::BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristic(characteristic_id);
  if (!characteristic) {
    std::move(callback).Run(mojom::GattResult::CHARACTERISTIC_NOT_FOUND);
    return;
  }

  device::BluetoothRemoteGattDescriptor* descriptor =
      characteristic->GetDescriptor(descriptor_id);
  if (!descriptor) {
    std::move(callback).Run(mojom::GattResult::DESCRIPTOR_NOT_FOUND);
    return;
  }

  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  descriptor->WriteRemoteDescriptor(
      value,
      base::BindOnce(&Device::OnWriteRemoteDescriptor,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback),
      base::BindOnce(&Device::OnWriteRemoteDescriptorError,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback));
}

Device::Device(scoped_refptr<device::BluetoothAdapter> adapter,
               std::unique_ptr<device::BluetoothGattConnection> connection)
    : adapter_(std::move(adapter)), connection_(std::move(connection)) {
  adapter_->AddObserver(this);
}

void Device::GetServicesImpl(GetServicesCallback callback) {
  device::BluetoothDevice* device = adapter_->GetDevice(GetAddress());
  DCHECK(device);

  std::vector<mojom::ServiceInfoPtr> services;

  for (const device::BluetoothRemoteGattService* service :
       device->GetGattServices()) {
    services.push_back(ConstructServiceInfoStruct(*service));
  }

  std::move(callback).Run(std::move(services));
}

mojom::ServiceInfoPtr Device::ConstructServiceInfoStruct(
    const device::BluetoothRemoteGattService& service) {
  mojom::ServiceInfoPtr service_info = mojom::ServiceInfo::New();

  service_info->id = service.GetIdentifier();
  service_info->uuid = service.GetUUID();
  service_info->is_primary = service.IsPrimary();

  return service_info;
}

void Device::OnReadRemoteCharacteristic(
    ReadValueForCharacteristicCallback callback,
    const std::vector<uint8_t>& value) {
  std::move(callback).Run(mojom::GattResult::SUCCESS, std::move(value));
}

void Device::OnReadRemoteCharacteristicError(
    ReadValueForCharacteristicCallback callback,
    device::BluetoothGattService::GattErrorCode error_code) {
  std::move(callback).Run(mojo::ConvertTo<mojom::GattResult>(error_code),
                          base::nullopt /* value */);
}

void Device::OnWriteRemoteCharacteristic(
    WriteValueForCharacteristicCallback callback) {
  std::move(callback).Run(mojom::GattResult::SUCCESS);
}

void Device::OnWriteRemoteCharacteristicError(
    WriteValueForCharacteristicCallback callback,
    device::BluetoothGattService::GattErrorCode error_code) {
  std::move(callback).Run(mojo::ConvertTo<mojom::GattResult>(error_code));
}

void Device::OnReadRemoteDescriptor(ReadValueForDescriptorCallback callback,
                                    const std::vector<uint8_t>& value) {
  std::move(callback).Run(mojom::GattResult::SUCCESS, std::move(value));
}

void Device::OnReadRemoteDescriptorError(
    ReadValueForDescriptorCallback callback,
    device::BluetoothGattService::GattErrorCode error_code) {
  std::move(callback).Run(mojo::ConvertTo<mojom::GattResult>(error_code),
                          base::nullopt /* value */);
}

void Device::OnWriteRemoteDescriptor(WriteValueForDescriptorCallback callback) {
  std::move(callback).Run(mojom::GattResult::SUCCESS);
}

void Device::OnWriteRemoteDescriptorError(
    WriteValueForDescriptorCallback callback,
    device::BluetoothGattService::GattErrorCode error_code) {
  std::move(callback).Run(mojo::ConvertTo<mojom::GattResult>(error_code));
}

const std::string& Device::GetAddress() {
  return connection_->GetDeviceAddress();
}

}  // namespace bluetooth
