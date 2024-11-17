// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/emulation/fake_central.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_filter.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"
#include "device/bluetooth/emulation/fake_peripheral.h"
#include "device/bluetooth/emulation/fake_remote_gatt_characteristic.h"
#include "device/bluetooth/emulation/fake_remote_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/public/mojom/emulation/fake_bluetooth.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace bluetooth {

namespace {

template <typename Optional, typename T = typename Optional::value_type>
T ValueOrDefault(Optional&& opt) {
  return std::forward<Optional>(opt).value_or(T{});
}

device::BluetoothDevice::ManufacturerDataMap ToManufacturerDataMap(
    base::flat_map<uint16_t, std::vector<uint8_t>>&& map) {
  return device::BluetoothDevice::ManufacturerDataMap(
      std::make_move_iterator(map.begin()), std::make_move_iterator(map.end()));
}

}  // namespace

FakeCentral::FakeCentral(mojom::CentralState state,
                         mojo::PendingReceiver<mojom::FakeCentral> receiver)
    : state_(state), receiver_(this, std::move(receiver)) {}

void FakeCentral::SimulatePreconnectedPeripheral(
    const std::string& address,
    const std::string& name,
    const base::flat_map<uint16_t, std::vector<uint8_t>>& manufacturer_data,
    const std::vector<device::BluetoothUUID>& known_service_uuids,
    SimulatePreconnectedPeripheralCallback callback) {
  FakePeripheral* fake_peripheral = GetFakePeripheral(address);
  if (fake_peripheral == nullptr) {
    auto fake_peripheral_ptr = std::make_unique<FakePeripheral>(this, address);
    fake_peripheral = fake_peripheral_ptr.get();
    auto pair = devices_.emplace(address, std::move(fake_peripheral_ptr));
    DCHECK(pair.second);
  }

  fake_peripheral->SetName(name);
  fake_peripheral->SetSystemConnected(true);
  fake_peripheral->SetManufacturerData(
      device::BluetoothDevice::ManufacturerDataMap(manufacturer_data.begin(),
                                                   manufacturer_data.end()));
  fake_peripheral->SetServiceUUIDs(device::BluetoothDevice::UUIDSet(
      known_service_uuids.begin(), known_service_uuids.end()));

  std::move(callback).Run();
}

void FakeCentral::SimulateAdvertisementReceived(
    mojom::ScanResultPtr scan_result_ptr,
    SimulateAdvertisementReceivedCallback callback) {
  if (NumDiscoverySessions() == 0) {
    std::move(callback).Run();
    return;
  }
  auto* fake_peripheral = GetFakePeripheral(scan_result_ptr->device_address);
  const bool is_new_device = fake_peripheral == nullptr;
  if (is_new_device) {
    auto fake_peripheral_ptr =
        std::make_unique<FakePeripheral>(this, scan_result_ptr->device_address);
    fake_peripheral = fake_peripheral_ptr.get();
    auto pair = devices_.emplace(scan_result_ptr->device_address,
                                 std::move(fake_peripheral_ptr));
    DCHECK(pair.second);
  }

  auto& scan_record = scan_result_ptr->scan_record;
  auto uuids = ValueOrDefault(std::move(scan_record->uuids));
  auto service_data = ValueOrDefault(std::move(scan_record->service_data));
  auto manufacturer_data = ToManufacturerDataMap(
      ValueOrDefault(std::move(scan_record->manufacturer_data)));

  for (auto& observer : observers_) {
    observer.DeviceAdvertisementReceived(
        scan_result_ptr->device_address, scan_record->name, scan_record->name,
        scan_result_ptr->rssi, scan_record->tx_power->value,
        std::nullopt, /* TODO(crbug.com/41240161) Implement appearance */
        uuids, service_data, manufacturer_data);
  }

  fake_peripheral->SetName(std::move(scan_record->name));
  fake_peripheral->UpdateAdvertisementData(
      scan_result_ptr->rssi, std::nullopt /* flags */, uuids,
      scan_record->tx_power->has_value
          ? std::make_optional(scan_record->tx_power->value)
          : std::nullopt,
      service_data, manufacturer_data);

  if (is_new_device) {
    // Call DeviceAdded on observers because it is a newly detected peripheral.
    for (auto& observer : observers_) {
      observer.DeviceAdded(this, fake_peripheral);
    }
  } else {
    // Call DeviceChanged on observers because it is a device that was detected
    // before.
    for (auto& observer : observers_) {
      observer.DeviceChanged(this, fake_peripheral);
    }
  }

  std::move(callback).Run();
}

void FakeCentral::SetState(mojom::CentralState new_state,
                           SetStateCallback callback) {
  // In real devices, when a powered on adapter is added, we notify that it was
  // added and then that it was powered on. When an adapter is removed, we
  // notify that it was powered off and then that it was removed. The following
  // logic simulates this behavior.
  if (new_state == state_) {
    std::move(callback).Run();
    return;
  }

  const mojom::CentralState old_state = state_;
  state_ = new_state;
  auto notify_present_changed = [this]() {
    NotifyAdapterPresentChanged(IsPresent());
  };
  auto notify_powered_changed = [this]() {
    NotifyAdapterPoweredChanged(IsPowered());
  };

  switch (old_state) {
    case mojom::CentralState::ABSENT:
      notify_present_changed();
      if (new_state == mojom::CentralState::POWERED_ON) {
        notify_powered_changed();
      }
      break;
    case mojom::CentralState::POWERED_OFF:
      if (new_state == mojom::CentralState::ABSENT) {
        notify_present_changed();
      } else {
        notify_powered_changed();
      }
      break;
    case mojom::CentralState::POWERED_ON:
      notify_powered_changed();
      if (new_state == mojom::CentralState::ABSENT) {
        notify_present_changed();
      }
      break;
  }
  std::move(callback).Run();
}

void FakeCentral::SetNextGATTConnectionResponse(
    const std::string& address,
    uint16_t code,
    SetNextGATTConnectionResponseCallback callback) {
  FakePeripheral* fake_peripheral = GetFakePeripheral(address);
  if (fake_peripheral == nullptr) {
    std::move(callback).Run(false);
    return;
  }

  fake_peripheral->SetNextGATTConnectionResponse(code);
  std::move(callback).Run(true);
}

void FakeCentral::SetNextGATTDiscoveryResponse(
    const std::string& address,
    uint16_t code,
    SetNextGATTDiscoveryResponseCallback callback) {
  FakePeripheral* fake_peripheral = GetFakePeripheral(address);
  if (fake_peripheral == nullptr) {
    std::move(callback).Run(false);
    return;
  }

  fake_peripheral->SetNextGATTDiscoveryResponse(code);
  std::move(callback).Run(true);
}

bool FakeCentral::AllResponsesConsumed() {
  return base::ranges::all_of(devices_, [](const auto& e) {
    // static_cast is safe because the parent class's devices_ is only
    // populated via this FakeCentral, and only with FakePeripherals.
    return static_cast<FakePeripheral*>(e.second.get())->AllResponsesConsumed();
  });
}

void FakeCentral::SimulateGATTDisconnection(
    const std::string& address,
    SimulateGATTDisconnectionCallback callback) {
  FakePeripheral* fake_peripheral = GetFakePeripheral(address);
  if (fake_peripheral == nullptr) {
    std::move(callback).Run(false);
    return;
  }

  fake_peripheral->SimulateGATTDisconnection();
  std::move(callback).Run(true);
}

void FakeCentral::SimulateGATTServicesChanged(
    const std::string& address,
    SimulateGATTServicesChangedCallback callback) {
  FakePeripheral* fake_peripheral = GetFakePeripheral(address);
  if (fake_peripheral == nullptr) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

void FakeCentral::AddFakeService(const std::string& peripheral_address,
                                 const device::BluetoothUUID& service_uuid,
                                 AddFakeServiceCallback callback) {
  FakePeripheral* fake_peripheral = GetFakePeripheral(peripheral_address);
  if (fake_peripheral == nullptr) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(fake_peripheral->AddFakeService(service_uuid));
}

void FakeCentral::RemoveFakeService(const std::string& identifier,
                                    const std::string& peripheral_address,
                                    RemoveFakeServiceCallback callback) {
  FakePeripheral* fake_peripheral = GetFakePeripheral(peripheral_address);
  if (!fake_peripheral) {
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(fake_peripheral->RemoveFakeService(identifier));
}

void FakeCentral::AddFakeCharacteristic(
    const device::BluetoothUUID& characteristic_uuid,
    mojom::CharacteristicPropertiesPtr properties,
    const std::string& service_id,
    const std::string& peripheral_address,
    AddFakeCharacteristicCallback callback) {
  FakeRemoteGattService* fake_remote_gatt_service =
      GetFakeRemoteGattService(peripheral_address, service_id);
  if (fake_remote_gatt_service == nullptr) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(fake_remote_gatt_service->AddFakeCharacteristic(
      characteristic_uuid, std::move(properties)));
}

void FakeCentral::RemoveFakeCharacteristic(
    const std::string& identifier,
    const std::string& service_id,
    const std::string& peripheral_address,
    RemoveFakeCharacteristicCallback callback) {
  FakeRemoteGattService* fake_remote_gatt_service =
      GetFakeRemoteGattService(peripheral_address, service_id);
  if (fake_remote_gatt_service == nullptr) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(
      fake_remote_gatt_service->RemoveFakeCharacteristic(identifier));
}

void FakeCentral::AddFakeDescriptor(
    const device::BluetoothUUID& descriptor_uuid,
    const std::string& characteristic_id,
    const std::string& service_id,
    const std::string& peripheral_address,
    AddFakeDescriptorCallback callback) {
  FakeRemoteGattCharacteristic* fake_remote_gatt_characteristic =
      GetFakeRemoteGattCharacteristic(peripheral_address, service_id,
                                      characteristic_id);
  if (fake_remote_gatt_characteristic == nullptr) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(
      fake_remote_gatt_characteristic->AddFakeDescriptor(descriptor_uuid));
}

void FakeCentral::RemoveFakeDescriptor(const std::string& descriptor_id,
                                       const std::string& characteristic_id,
                                       const std::string& service_id,
                                       const std::string& peripheral_address,
                                       RemoveFakeDescriptorCallback callback) {
  FakeRemoteGattCharacteristic* fake_remote_gatt_characteristic =
      GetFakeRemoteGattCharacteristic(peripheral_address, service_id,
                                      characteristic_id);
  if (!fake_remote_gatt_characteristic) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(
      fake_remote_gatt_characteristic->RemoveFakeDescriptor(descriptor_id));
}

void FakeCentral::SetNextReadCharacteristicResponse(
    uint16_t gatt_code,
    const std::optional<std::vector<uint8_t>>& value,
    const std::string& characteristic_id,
    const std::string& service_id,
    const std::string& peripheral_address,
    SetNextReadCharacteristicResponseCallback callback) {
  FakeRemoteGattCharacteristic* fake_remote_gatt_characteristic =
      GetFakeRemoteGattCharacteristic(peripheral_address, service_id,
                                      characteristic_id);
  if (fake_remote_gatt_characteristic == nullptr) {
    std::move(callback).Run(false);
    return;
  }

  fake_remote_gatt_characteristic->SetNextReadResponse(gatt_code, value);
  std::move(callback).Run(true);
}

void FakeCentral::SetNextWriteCharacteristicResponse(
    uint16_t gatt_code,
    const std::string& characteristic_id,
    const std::string& service_id,
    const std::string& peripheral_address,
    SetNextWriteCharacteristicResponseCallback callback) {
  FakeRemoteGattCharacteristic* fake_remote_gatt_characteristic =
      GetFakeRemoteGattCharacteristic(peripheral_address, service_id,
                                      characteristic_id);
  if (fake_remote_gatt_characteristic == nullptr) {
    std::move(callback).Run(false);
    return;
  }

  fake_remote_gatt_characteristic->SetNextWriteResponse(gatt_code);
  std::move(callback).Run(true);
}

void FakeCentral::SetNextSubscribeToNotificationsResponse(
    uint16_t gatt_code,
    const std::string& characteristic_id,
    const std::string& service_id,
    const std::string& peripheral_address,
    SetNextSubscribeToNotificationsResponseCallback callback) {
  FakeRemoteGattCharacteristic* fake_remote_gatt_characteristic =
      GetFakeRemoteGattCharacteristic(peripheral_address, service_id,
                                      characteristic_id);
  if (fake_remote_gatt_characteristic == nullptr) {
    std::move(callback).Run(false);
    return;
  }

  fake_remote_gatt_characteristic->SetNextSubscribeToNotificationsResponse(
      gatt_code);
  std::move(callback).Run(true);
}

void FakeCentral::SetNextUnsubscribeFromNotificationsResponse(
    uint16_t gatt_code,
    const std::string& characteristic_id,
    const std::string& service_id,
    const std::string& peripheral_address,
    SetNextUnsubscribeFromNotificationsResponseCallback callback) {
  FakeRemoteGattCharacteristic* fake_remote_gatt_characteristic =
      GetFakeRemoteGattCharacteristic(peripheral_address, service_id,
                                      characteristic_id);
  if (fake_remote_gatt_characteristic == nullptr) {
    std::move(callback).Run(false);
    return;
  }

  fake_remote_gatt_characteristic->SetNextUnsubscribeFromNotificationsResponse(
      gatt_code);
  std::move(callback).Run(true);
}

void FakeCentral::IsNotifying(const std::string& characteristic_id,
                              const std::string& service_id,
                              const std::string& peripheral_address,
                              IsNotifyingCallback callback) {
  FakeRemoteGattCharacteristic* fake_remote_gatt_characteristic =
      GetFakeRemoteGattCharacteristic(peripheral_address, service_id,
                                      characteristic_id);
  if (!fake_remote_gatt_characteristic) {
    std::move(callback).Run(false, false);
    return;
  }

  std::move(callback).Run(true, fake_remote_gatt_characteristic->IsNotifying());
}

void FakeCentral::GetLastWrittenCharacteristicValue(
    const std::string& characteristic_id,
    const std::string& service_id,
    const std::string& peripheral_address,
    GetLastWrittenCharacteristicValueCallback callback) {
  FakeRemoteGattCharacteristic* fake_remote_gatt_characteristic =
      GetFakeRemoteGattCharacteristic(peripheral_address, service_id,
                                      characteristic_id);
  if (fake_remote_gatt_characteristic == nullptr) {
    std::move(callback).Run(false, std::nullopt, mojom::WriteType::kNone);
    return;
  }

  std::move(callback).Run(true,
                          fake_remote_gatt_characteristic->last_written_value(),
                          fake_remote_gatt_characteristic->last_write_type());
}

void FakeCentral::SetNextReadDescriptorResponse(
    uint16_t gatt_code,
    const std::optional<std::vector<uint8_t>>& value,
    const std::string& descriptor_id,
    const std::string& characteristic_id,
    const std::string& service_id,
    const std::string& peripheral_address,
    SetNextReadDescriptorResponseCallback callback) {
  FakeRemoteGattDescriptor* fake_remote_gatt_descriptor =
      GetFakeRemoteGattDescriptor(peripheral_address, service_id,
                                  characteristic_id, descriptor_id);
  if (fake_remote_gatt_descriptor == nullptr) {
    std::move(callback).Run(false);
    return;
  }

  fake_remote_gatt_descriptor->SetNextReadResponse(gatt_code, value);
  std::move(callback).Run(true);
}

void FakeCentral::SetNextWriteDescriptorResponse(
    uint16_t gatt_code,
    const std::string& descriptor_id,
    const std::string& characteristic_id,
    const std::string& service_id,
    const std::string& peripheral_address,
    SetNextWriteDescriptorResponseCallback callback) {
  FakeRemoteGattDescriptor* fake_remote_gatt_descriptor =
      GetFakeRemoteGattDescriptor(peripheral_address, service_id,
                                  characteristic_id, descriptor_id);
  if (!fake_remote_gatt_descriptor) {
    std::move(callback).Run(false);
    return;
  }

  fake_remote_gatt_descriptor->SetNextWriteResponse(gatt_code);
  std::move(callback).Run(true);
}

void FakeCentral::GetLastWrittenDescriptorValue(
    const std::string& descriptor_id,
    const std::string& characteristic_id,
    const std::string& service_id,
    const std::string& peripheral_address,
    GetLastWrittenDescriptorValueCallback callback) {
  FakeRemoteGattDescriptor* fake_remote_gatt_descriptor =
      GetFakeRemoteGattDescriptor(peripheral_address, service_id,
                                  characteristic_id, descriptor_id);
  if (!fake_remote_gatt_descriptor) {
    std::move(callback).Run(false, std::nullopt);
    return;
  }

  std::move(callback).Run(true,
                          fake_remote_gatt_descriptor->last_written_value());
}

void FakeCentral::Initialize(base::OnceClosure callback) {
  std::move(callback).Run();
}

std::string FakeCentral::GetAddress() const {
  NOTREACHED();
}

std::string FakeCentral::GetName() const {
  NOTREACHED();
}

void FakeCentral::SetName(const std::string& name,
                          base::OnceClosure callback,
                          ErrorCallback error_callback) {
  NOTREACHED();
}

bool FakeCentral::IsInitialized() const {
  return true;
}

bool FakeCentral::IsPresent() const {
  switch (state_) {
    case mojom::CentralState::ABSENT:
      return false;
    case mojom::CentralState::POWERED_OFF:
    case mojom::CentralState::POWERED_ON:
      return true;
  }
  NOTREACHED();
}

bool FakeCentral::IsPowered() const {
  switch (state_) {
    case mojom::CentralState::ABSENT:
    // SetState() calls IsPowered() to notify observers properly when an adapter
    // being removed is simulated, so it should return false.
    case mojom::CentralState::POWERED_OFF:
      return false;
    case mojom::CentralState::POWERED_ON:
      return true;
  }
  NOTREACHED();
}

void FakeCentral::SetPowered(bool powered,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) {
  NOTREACHED();
}

bool FakeCentral::IsDiscoverable() const {
  NOTREACHED();
}

void FakeCentral::SetDiscoverable(bool discoverable,
                                  base::OnceClosure callback,
                                  ErrorCallback error_callback) {
  NOTREACHED();
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
base::TimeDelta FakeCentral::GetDiscoverableTimeout() const {
  NOTREACHED();
}
#endif

bool FakeCentral::IsDiscovering() const {
  NOTREACHED();
}

FakeCentral::UUIDList FakeCentral::GetUUIDs() const {
  NOTREACHED();
}

void FakeCentral::CreateRfcommService(
    const device::BluetoothUUID& uuid,
    const ServiceOptions& options,
    CreateServiceCallback callback,
    CreateServiceErrorCallback error_callback) {
  NOTREACHED();
}

void FakeCentral::CreateL2capService(
    const device::BluetoothUUID& uuid,
    const ServiceOptions& options,
    CreateServiceCallback callback,
    CreateServiceErrorCallback error_callback) {
  NOTREACHED();
}

void FakeCentral::RegisterAdvertisement(
    std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
    CreateAdvertisementCallback callback,
    AdvertisementErrorCallback error_callback) {
  NOTREACHED();
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
void FakeCentral::SetAdvertisingInterval(
    const base::TimeDelta& min,
    const base::TimeDelta& max,
    base::OnceClosure callback,
    AdvertisementErrorCallback error_callback) {
  NOTREACHED();
}
void FakeCentral::ResetAdvertising(base::OnceClosure callback,
                                   AdvertisementErrorCallback error_callback) {
  NOTREACHED();
}
void FakeCentral::ConnectDevice(
    const std::string& address,
    const std::optional<device::BluetoothDevice::AddressType>& address_type,
    ConnectDeviceCallback callback,
    ConnectDeviceErrorCallback error_callback) {
  NOTREACHED();
}
#endif

device::BluetoothLocalGattService* FakeCentral::GetGattService(
    const std::string& identifier) const {
  NOTREACHED();
}

#if BUILDFLAG(IS_CHROMEOS)
void FakeCentral::SetServiceAllowList(const UUIDList& uuids,
                                      base::OnceClosure callback,
                                      ErrorCallback error_callback) {
  NOTREACHED();
}

std::unique_ptr<device::BluetoothLowEnergyScanSession>
FakeCentral::StartLowEnergyScanSession(
    std::unique_ptr<device::BluetoothLowEnergyScanFilter> filter,
    base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate> delegate) {
  NOTREACHED();
}

device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
FakeCentral::GetLowEnergyScanSessionHardwareOffloadingStatus() {
  return LowEnergyScanSessionHardwareOffloadingStatus::kNotSupported;
}

std::vector<device::BluetoothAdapter::BluetoothRole>
FakeCentral::GetSupportedRoles() {
  NOTREACHED();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
void FakeCentral::SetStandardChromeOSAdapterName() {
  NOTREACHED();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

base::WeakPtr<device::BluetoothAdapter> FakeCentral::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool FakeCentral::SetPoweredImpl(bool powered) {
  NOTREACHED();
}

void FakeCentral::UpdateFilter(
    std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  if (!IsPresent()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback), /*is_error=*/true,
            device::UMABluetoothDiscoverySessionOutcome::ADAPTER_NOT_PRESENT));
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), /*is_error=*/false,
                     device::UMABluetoothDiscoverySessionOutcome::SUCCESS));
}

void FakeCentral::StartScanWithFilter(
    std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  if (!IsPresent()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback), /*is_error=*/true,
            device::UMABluetoothDiscoverySessionOutcome::ADAPTER_NOT_PRESENT));
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), /*is_error=*/false,
                     device::UMABluetoothDiscoverySessionOutcome::SUCCESS));
}

void FakeCentral::StopScan(DiscoverySessionResultCallback callback) {
  if (!IsPresent()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback), /*is_error=*/false,
            device::UMABluetoothDiscoverySessionOutcome::ADAPTER_NOT_PRESENT));
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(callback), /*is_error=*/false,
          device::UMABluetoothDiscoverySessionOutcome::ADAPTER_NOT_PRESENT));
}

void FakeCentral::RemovePairingDelegateInternal(
    device::BluetoothDevice::PairingDelegate* pairing_delegate) {
  NOTREACHED();
}

FakeCentral::~FakeCentral() = default;

FakePeripheral* FakeCentral::GetFakePeripheral(
    const std::string& peripheral_address) const {
  auto device_iter = devices_.find(peripheral_address);
  if (device_iter == devices_.end()) {
    return nullptr;
  }

  // static_cast is safe because the parent class's devices_ is only
  // populated via this FakeCentral, and only with FakePeripherals.
  return static_cast<FakePeripheral*>(device_iter->second.get());
}

FakeRemoteGattService* FakeCentral::GetFakeRemoteGattService(
    const std::string& peripheral_address,
    const std::string& service_id) const {
  FakePeripheral* fake_peripheral = GetFakePeripheral(peripheral_address);
  if (fake_peripheral == nullptr) {
    return nullptr;
  }

  // static_cast is safe because FakePeripheral is only populated with
  // FakeRemoteGattServices.
  return static_cast<FakeRemoteGattService*>(
      fake_peripheral->GetGattService(service_id));
}

FakeRemoteGattCharacteristic* FakeCentral::GetFakeRemoteGattCharacteristic(
    const std::string& peripheral_address,
    const std::string& service_id,
    const std::string& characteristic_id) const {
  FakeRemoteGattService* fake_remote_gatt_service =
      GetFakeRemoteGattService(peripheral_address, service_id);
  if (fake_remote_gatt_service == nullptr) {
    return nullptr;
  }

  // static_cast is safe because FakeRemoteGattService is only populated with
  // FakeRemoteGattCharacteristics.
  return static_cast<FakeRemoteGattCharacteristic*>(
      fake_remote_gatt_service->GetCharacteristic(characteristic_id));
}

FakeRemoteGattDescriptor* FakeCentral::GetFakeRemoteGattDescriptor(
    const std::string& peripheral_address,
    const std::string& service_id,
    const std::string& characteristic_id,
    const std::string& descriptor_id) const {
  FakeRemoteGattCharacteristic* fake_remote_gatt_characteristic =
      GetFakeRemoteGattCharacteristic(peripheral_address, service_id,
                                      characteristic_id);
  if (fake_remote_gatt_characteristic == nullptr) {
    return nullptr;
  }

  // static_cast is safe because FakeRemoteGattCharacteristic is only populated
  // with FakeRemoteGattDescriptors.
  return static_cast<FakeRemoteGattDescriptor*>(
      fake_remote_gatt_characteristic->GetDescriptor(descriptor_id));
}

}  // namespace bluetooth
