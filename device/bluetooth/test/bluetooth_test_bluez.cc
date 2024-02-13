// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/bluetooth_test_bluez.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/bluez/bluetooth_device_bluez.h"
#include "device/bluetooth/bluez/bluetooth_gatt_characteristic_bluez.h"
#include "device/bluetooth/bluez/bluetooth_gatt_descriptor_bluez.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_characteristic_bluez.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_descriptor_bluez.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_service_bluez.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_characteristic_service_provider.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_descriptor_service_provider.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_manager_client.h"
#include "device/bluetooth/test/test_bluetooth_local_gatt_service_delegate.h"

namespace device {

namespace {

void GetValueCallback(
    base::OnceClosure quit_closure,
    BluetoothLocalGattService::Delegate::ValueCallback value_callback,
    std::optional<BluetoothGattService::GattErrorCode> error_code,
    const std::vector<uint8_t>& value) {
  std::move(value_callback).Run(error_code, value);
  std::move(quit_closure).Run();
}

void ClosureCallback(base::OnceClosure quit_closure,
                     base::OnceClosure callback) {
  std::move(callback).Run();
  std::move(quit_closure).Run();
}

dbus::ObjectPath GetDevicePath(BluetoothDevice* device) {
  bluez::BluetoothDeviceBlueZ* device_bluez =
      static_cast<bluez::BluetoothDeviceBlueZ*>(device);
  return device_bluez->object_path();
}

}  // namespace

BluetoothTestBlueZ::BluetoothTestBlueZ()
    : fake_bluetooth_device_client_(nullptr) {}

BluetoothTestBlueZ::~BluetoothTestBlueZ() = default;

void BluetoothTestBlueZ::SetUp() {
  BluetoothTestBase::SetUp();
  std::unique_ptr<bluez::BluezDBusManagerSetter> dbus_setter =
      bluez::BluezDBusManager::GetSetterForTesting();

  auto adapter_client = std::make_unique<bluez::FakeBluetoothAdapterClient>();
  fake_bluetooth_adapter_client_ = adapter_client.get();
  dbus_setter->SetBluetoothAdapterClient(std::move(adapter_client));

  auto device_client = std::make_unique<bluez::FakeBluetoothDeviceClient>();
  fake_bluetooth_device_client_ = device_client.get();
  dbus_setter->SetBluetoothDeviceClient(std::move(device_client));

  // Make the fake adapter post tasks without delay in order to avoid timing
  // issues.
  fake_bluetooth_adapter_client_->SetSimulationIntervalMs(0);
}

void BluetoothTestBlueZ::TearDown() {
  for (const auto& connection : gatt_connections_) {
    if (connection->IsConnected())
      connection->Disconnect();
  }
  gatt_connections_.clear();

  for (const auto& session : discovery_sessions_) {
    if (session->IsActive())
      session->Stop(base::DoNothing(), base::DoNothing());
  }
  discovery_sessions_.clear();

  adapter_ = nullptr;
  fake_bluetooth_adapter_client_ = nullptr;
  fake_bluetooth_device_client_ = nullptr;
  bluez::BluezDBusManager::Shutdown();
  BluetoothTestBase::TearDown();
}

bool BluetoothTestBlueZ::PlatformSupportsLowEnergy() {
  return true;
}

void BluetoothTestBlueZ::InitWithFakeAdapter() {
  base::RunLoop run_loop;
  adapter_ = bluez::BluetoothAdapterBlueZ::CreateAdapter();
  adapter_->Initialize(run_loop.QuitClosure());
  run_loop.Run();
  adapter_->SetPowered(true, base::DoNothing(), base::DoNothing());
}

BluetoothDevice* BluetoothTestBlueZ::SimulateLowEnergyDevice(
    int device_ordinal) {
  LowEnergyDeviceData data = GetLowEnergyDeviceData(device_ordinal);

  std::vector<std::string> service_uuids;
  for (const auto& uuid : data.advertised_uuids)
    service_uuids.push_back(uuid.canonical_value());

  std::map<std::string, std::vector<uint8_t>> service_data;
  for (const auto& service : data.service_data)
    service_data.emplace(service.first.canonical_value(), service.second);

  std::map<uint16_t, std::vector<uint8_t>> manufacturer_data(
      data.manufacturer_data.begin(), data.manufacturer_data.end());

  BluetoothDevice* device = adapter_->GetDevice(data.address);
  if (device) {
    fake_bluetooth_device_client_->UpdateServiceAndManufacturerData(
        GetDevicePath(device), service_uuids, service_data, manufacturer_data);
    return device;
  }

  fake_bluetooth_device_client_->CreateTestDevice(
      dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
      /* name */ data.name,
      /* alias */ data.name.value_or("") + "(alias)", data.address,
      service_uuids, data.transport, service_data, manufacturer_data);

  return adapter_->GetDevice(data.address);
}

BluetoothDevice* BluetoothTestBlueZ::SimulateClassicDevice() {
  std::string device_name = kTestDeviceName;
  std::string device_address = kTestDeviceAddress3;
  std::vector<std::string> service_uuids;
  std::map<std::string, std::vector<uint8_t>> service_data;
  std::map<uint16_t, std::vector<uint8_t>> manufacturer_data;

  if (!adapter_->GetDevice(device_address)) {
    fake_bluetooth_device_client_->CreateTestDevice(
        dbus::ObjectPath(bluez::FakeBluetoothAdapterClient::kAdapterPath),
        device_name /* name */, device_name /* alias */, device_address,
        service_uuids, BLUETOOTH_TRANSPORT_CLASSIC, service_data,
        manufacturer_data);
  }
  return adapter_->GetDevice(device_address);
}

void BluetoothTestBlueZ::SimulateLocalGattCharacteristicValueReadRequest(
    BluetoothDevice* from_device,
    BluetoothLocalGattCharacteristic* characteristic,
    BluetoothLocalGattService::Delegate::ValueCallback value_callback) {
  bluez::BluetoothLocalGattCharacteristicBlueZ* characteristic_bluez =
      static_cast<bluez::BluetoothLocalGattCharacteristicBlueZ*>(
          characteristic);
  bluez::FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<bluez::FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  bluez::FakeBluetoothGattCharacteristicServiceProvider*
      characteristic_provider =
          fake_bluetooth_gatt_manager_client->GetCharacteristicServiceProvider(
              characteristic_bluez->object_path());

  bluez::BluetoothLocalGattServiceBlueZ* service_bluez =
      static_cast<bluez::BluetoothLocalGattServiceBlueZ*>(
          characteristic->GetService());
  static_cast<TestBluetoothLocalGattServiceDelegate*>(
      service_bluez->GetDelegate())
      ->set_expected_characteristic(characteristic);

  base::RunLoop run_loop;
  characteristic_provider->GetValue(
      GetDevicePath(from_device),
      base::BindOnce(&GetValueCallback, run_loop.QuitClosure(),
                     std::move(value_callback)));
  run_loop.Run();
}

void BluetoothTestBlueZ::SimulateLocalGattCharacteristicValueWriteRequest(
    BluetoothDevice* from_device,
    BluetoothLocalGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value_to_write,
    base::OnceClosure success_callback,
    base::OnceClosure error_callback) {
  bluez::BluetoothLocalGattCharacteristicBlueZ* characteristic_bluez =
      static_cast<bluez::BluetoothLocalGattCharacteristicBlueZ*>(
          characteristic);
  bluez::FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<bluez::FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  bluez::FakeBluetoothGattCharacteristicServiceProvider*
      characteristic_provider =
          fake_bluetooth_gatt_manager_client->GetCharacteristicServiceProvider(
              characteristic_bluez->object_path());

  bluez::BluetoothLocalGattServiceBlueZ* service_bluez =
      static_cast<bluez::BluetoothLocalGattServiceBlueZ*>(
          characteristic->GetService());
  static_cast<TestBluetoothLocalGattServiceDelegate*>(
      service_bluez->GetDelegate())
      ->set_expected_characteristic(characteristic);

  base::RunLoop run_loop;
  characteristic_provider->SetValue(
      GetDevicePath(from_device), value_to_write,
      base::BindOnce(&ClosureCallback, run_loop.QuitClosure(),
                     std::move(success_callback)),
      base::BindOnce(&ClosureCallback, run_loop.QuitClosure(),
                     std::move(error_callback)));
  run_loop.Run();
}

void BluetoothTestBlueZ::
    SimulateLocalGattCharacteristicValuePrepareWriteRequest(
        BluetoothDevice* from_device,
        BluetoothLocalGattCharacteristic* characteristic,
        const std::vector<uint8_t>& value_to_write,
        int offset,
        bool has_subsequent_write,
        base::OnceClosure success_callback,
        base::OnceClosure error_callback) {
  bluez::BluetoothLocalGattCharacteristicBlueZ* characteristic_bluez =
      static_cast<bluez::BluetoothLocalGattCharacteristicBlueZ*>(
          characteristic);
  bluez::FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<bluez::FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  bluez::FakeBluetoothGattCharacteristicServiceProvider*
      characteristic_provider =
          fake_bluetooth_gatt_manager_client->GetCharacteristicServiceProvider(
              characteristic_bluez->object_path());

  bluez::BluetoothLocalGattServiceBlueZ* service_bluez =
      static_cast<bluez::BluetoothLocalGattServiceBlueZ*>(
          characteristic->GetService());
  static_cast<TestBluetoothLocalGattServiceDelegate*>(
      service_bluez->GetDelegate())
      ->set_expected_characteristic(characteristic);

  base::RunLoop run_loop;
  characteristic_provider->PrepareSetValue(
      GetDevicePath(from_device), value_to_write, offset, has_subsequent_write,
      base::BindOnce(&ClosureCallback, run_loop.QuitClosure(),
                     std::move(success_callback)),
      base::BindOnce(&ClosureCallback, run_loop.QuitClosure(),
                     std::move(error_callback)));
  run_loop.Run();
}

void BluetoothTestBlueZ::SimulateLocalGattDescriptorValueReadRequest(
    BluetoothDevice* from_device,
    BluetoothLocalGattDescriptor* descriptor,
    BluetoothLocalGattService::Delegate::ValueCallback value_callback) {
  bluez::BluetoothLocalGattDescriptorBlueZ* descriptor_bluez =
      static_cast<bluez::BluetoothLocalGattDescriptorBlueZ*>(descriptor);
  bluez::FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<bluez::FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  bluez::FakeBluetoothGattDescriptorServiceProvider* descriptor_provider =
      fake_bluetooth_gatt_manager_client->GetDescriptorServiceProvider(
          descriptor_bluez->object_path());

  bluez::BluetoothLocalGattServiceBlueZ* service_bluez =
      static_cast<bluez::BluetoothLocalGattServiceBlueZ*>(
          descriptor->GetCharacteristic()->GetService());
  static_cast<TestBluetoothLocalGattServiceDelegate*>(
      service_bluez->GetDelegate())
      ->set_expected_descriptor(descriptor);

  base::RunLoop run_loop;
  descriptor_provider->GetValue(
      GetDevicePath(from_device),
      base::BindOnce(&GetValueCallback, run_loop.QuitClosure(),
                     std::move(value_callback)));
  run_loop.Run();
}

void BluetoothTestBlueZ::SimulateLocalGattDescriptorValueWriteRequest(
    BluetoothDevice* from_device,
    BluetoothLocalGattDescriptor* descriptor,
    const std::vector<uint8_t>& value_to_write,
    base::OnceClosure success_callback,
    base::OnceClosure error_callback) {
  bluez::BluetoothLocalGattDescriptorBlueZ* descriptor_bluez =
      static_cast<bluez::BluetoothLocalGattDescriptorBlueZ*>(descriptor);
  bluez::FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<bluez::FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  bluez::FakeBluetoothGattDescriptorServiceProvider* descriptor_provider =
      fake_bluetooth_gatt_manager_client->GetDescriptorServiceProvider(
          descriptor_bluez->object_path());

  bluez::BluetoothLocalGattServiceBlueZ* service_bluez =
      static_cast<bluez::BluetoothLocalGattServiceBlueZ*>(
          descriptor->GetCharacteristic()->GetService());
  static_cast<TestBluetoothLocalGattServiceDelegate*>(
      service_bluez->GetDelegate())
      ->set_expected_descriptor(descriptor);

  base::RunLoop run_loop;
  descriptor_provider->SetValue(
      GetDevicePath(from_device), value_to_write,
      base::BindOnce(&ClosureCallback, run_loop.QuitClosure(),
                     std::move(success_callback)),
      base::BindOnce(&ClosureCallback, run_loop.QuitClosure(),
                     std::move(error_callback)));
  run_loop.Run();
}

bool BluetoothTestBlueZ::SimulateLocalGattCharacteristicNotificationsRequest(
    BluetoothDevice* from_device,
    BluetoothLocalGattCharacteristic* characteristic,
    bool start) {
  bluez::BluetoothLocalGattCharacteristicBlueZ* characteristic_bluez =
      static_cast<bluez::BluetoothLocalGattCharacteristicBlueZ*>(
          characteristic);
  bluez::FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<bluez::FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  bluez::FakeBluetoothGattCharacteristicServiceProvider*
      characteristic_provider =
          fake_bluetooth_gatt_manager_client->GetCharacteristicServiceProvider(
              characteristic_bluez->object_path());

  bluez::BluetoothLocalGattServiceBlueZ* service_bluez =
      static_cast<bluez::BluetoothLocalGattServiceBlueZ*>(
          characteristic->GetService());
  static_cast<TestBluetoothLocalGattServiceDelegate*>(
      service_bluez->GetDelegate())
      ->set_expected_characteristic(characteristic);

  return characteristic_provider->NotificationsChange(
      GetDevicePath(from_device), start);
}

std::vector<uint8_t> BluetoothTestBlueZ::LastNotifactionValueForCharacteristic(
    BluetoothLocalGattCharacteristic* characteristic) {
  bluez::BluetoothLocalGattCharacteristicBlueZ* characteristic_bluez =
      static_cast<bluez::BluetoothLocalGattCharacteristicBlueZ*>(
          characteristic);
  bluez::FakeBluetoothGattManagerClient* fake_bluetooth_gatt_manager_client =
      static_cast<bluez::FakeBluetoothGattManagerClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothGattManagerClient());
  bluez::FakeBluetoothGattCharacteristicServiceProvider*
      characteristic_provider =
          fake_bluetooth_gatt_manager_client->GetCharacteristicServiceProvider(
              characteristic_bluez->object_path());

  return characteristic_provider ? characteristic_provider->sent_value()
                                 : std::vector<uint8_t>();
}

std::vector<BluetoothLocalGattService*>
BluetoothTestBlueZ::RegisteredGattServices() {
  std::vector<BluetoothLocalGattService*> services;
  bluez::BluetoothAdapterBlueZ* adapter_bluez =
      static_cast<bluez::BluetoothAdapterBlueZ*>(adapter_.get());

  for (const auto& iter : adapter_bluez->registered_gatt_services_)
    services.push_back(iter.second);
  return services;
}

}  // namespace device
