// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_EMULATION_FAKE_CENTRAL_H_
#define DEVICE_BLUETOOTH_EMULATION_FAKE_CENTRAL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/public/mojom/emulation/fake_bluetooth.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"
#include "device/bluetooth/bluetooth_low_energy_scan_session.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace bluetooth {

class FakePeripheral;
class FakeRemoteGattCharacteristic;
class FakeRemoteGattDescriptor;
class FakeRemoteGattService;

// Implementation of FakeCentral in
// src/device/bluetooth/public/mojom/emulation/fake_bluetooth.mojom.
// Implemented on top of the C++ device/bluetooth API, mainly
// device/bluetooth/bluetooth_adapter.h.
//
// Not intended for direct use by clients.  See README.md.
class FakeCentral final : public mojom::FakeCentral,
                          public device::BluetoothAdapter {
 public:
  FakeCentral(mojom::CentralState state,
              mojo::PendingReceiver<mojom::FakeCentral> receiver);

  // FakeCentral overrides:
  void SimulatePreconnectedPeripheral(
      const std::string& address,
      const std::string& name,
      const base::flat_map<uint16_t, std::vector<uint8_t>>& manufacturer_data,
      const std::vector<device::BluetoothUUID>& known_service_uuids,
      SimulatePreconnectedPeripheralCallback callback) override;
  void SimulateAdvertisementReceived(
      mojom::ScanResultPtr scan_result_ptr,
      SimulateAdvertisementReceivedCallback callback) override;
  void SetState(mojom::CentralState state, SetStateCallback callback) override;
  void SetNextGATTConnectionResponse(
      const std::string& address,
      uint16_t code,
      SetNextGATTConnectionResponseCallback) override;
  void SetNextGATTDiscoveryResponse(
      const std::string& address,
      uint16_t code,
      SetNextGATTDiscoveryResponseCallback callback) override;
  bool AllResponsesConsumed();
  void SimulateGATTDisconnection(
      const std::string& address,
      SimulateGATTDisconnectionCallback callback) override;
  void SimulateGATTServicesChanged(
      const std::string& address,
      SimulateGATTServicesChangedCallback callback) override;
  void AddFakeService(const std::string& peripheral_address,
                      const device::BluetoothUUID& service_uuid,
                      AddFakeServiceCallback callback) override;
  void RemoveFakeService(const std::string& identifier,
                         const std::string& peripheral_address,
                         RemoveFakeServiceCallback callback) override;
  void AddFakeCharacteristic(const device::BluetoothUUID& characteristic_uuid,
                             mojom::CharacteristicPropertiesPtr properties,
                             const std::string& service_id,
                             const std::string& peripheral_address,
                             AddFakeCharacteristicCallback callback) override;
  void RemoveFakeCharacteristic(
      const std::string& identifier,
      const std::string& service_id,
      const std::string& peripheral_address,
      RemoveFakeCharacteristicCallback callback) override;
  void AddFakeDescriptor(const device::BluetoothUUID& characteristic_uuid,
                         const std::string& characteristic_id,
                         const std::string& service_id,
                         const std::string& peripheral_address,
                         AddFakeDescriptorCallback callback) override;
  void RemoveFakeDescriptor(const std::string& descriptor_id,
                            const std::string& characteristic_id,
                            const std::string& service_id,
                            const std::string& peripheral_address,
                            RemoveFakeDescriptorCallback callback) override;
  void SetNextReadCharacteristicResponse(
      uint16_t gatt_code,
      const std::optional<std::vector<uint8_t>>& value,
      const std::string& characteristic_id,
      const std::string& service_id,
      const std::string& peripheral_address,
      SetNextReadCharacteristicResponseCallback callback) override;
  void SetNextWriteCharacteristicResponse(
      uint16_t gatt_code,
      const std::string& characteristic_id,
      const std::string& service_id,
      const std::string& peripheral_address,
      SetNextWriteCharacteristicResponseCallback callback) override;
  void SetNextSubscribeToNotificationsResponse(
      uint16_t gatt_code,
      const std::string& characteristic_id,
      const std::string& service_id,
      const std::string& peripheral_address,
      SetNextSubscribeToNotificationsResponseCallback callback) override;
  void SetNextUnsubscribeFromNotificationsResponse(
      uint16_t gatt_code,
      const std::string& characteristic_id,
      const std::string& service_id,
      const std::string& peripheral_address,
      SetNextUnsubscribeFromNotificationsResponseCallback callback) override;
  void IsNotifying(const std::string& characteristic_id,
                   const std::string& service_id,
                   const std::string& peripheral_address,
                   IsNotifyingCallback callback) override;
  void GetLastWrittenCharacteristicValue(
      const std::string& characteristic_id,
      const std::string& service_id,
      const std::string& peripheral_address,
      GetLastWrittenCharacteristicValueCallback callback) override;
  void SetNextReadDescriptorResponse(
      uint16_t gatt_code,
      const std::optional<std::vector<uint8_t>>& value,
      const std::string& descriptor_id,
      const std::string& characteristic_id,
      const std::string& service_id,
      const std::string& peripheral_address,
      SetNextReadDescriptorResponseCallback callback) override;
  void SetNextWriteDescriptorResponse(
      uint16_t gatt_code,
      const std::string& descriptor_id,
      const std::string& characteristic_id,
      const std::string& service_id,
      const std::string& peripheral_address,
      SetNextWriteDescriptorResponseCallback callback) override;
  void GetLastWrittenDescriptorValue(
      const std::string& descriptor_id,
      const std::string& characteristic_id,
      const std::string& service_id,
      const std::string& peripheral_address,
      GetLastWrittenDescriptorValueCallback callback) override;

  // BluetoothAdapter overrides:
  void Initialize(base::OnceClosure callback) override;
  std::string GetAddress() const override;
  std::string GetName() const override;
  void SetName(const std::string& name,
               base::OnceClosure callback,
               ErrorCallback error_callback) override;
  bool IsInitialized() const override;
  bool IsPresent() const override;
  bool IsPowered() const override;
  void SetPowered(bool powered,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override;
  bool IsDiscoverable() const override;
  void SetDiscoverable(bool discoverable,
                       base::OnceClosure callback,
                       ErrorCallback error_callback) override;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  base::TimeDelta GetDiscoverableTimeout() const override;
#endif
  bool IsDiscovering() const override;
  UUIDList GetUUIDs() const override;
  void CreateRfcommService(const device::BluetoothUUID& uuid,
                           const ServiceOptions& options,
                           CreateServiceCallback callback,
                           CreateServiceErrorCallback error_callback) override;
  void CreateL2capService(const device::BluetoothUUID& uuid,
                          const ServiceOptions& options,
                          CreateServiceCallback callback,
                          CreateServiceErrorCallback error_callback) override;
  void RegisterAdvertisement(
      std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
      CreateAdvertisementCallback callback,
      AdvertisementErrorCallback error_callback) override;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  void SetAdvertisingInterval(
      const base::TimeDelta& min,
      const base::TimeDelta& max,
      base::OnceClosure callback,
      AdvertisementErrorCallback error_callback) override;
  void ResetAdvertising(base::OnceClosure callback,
                        AdvertisementErrorCallback error_callback) override;
  void ConnectDevice(
      const std::string& address,
      const std::optional<device::BluetoothDevice::AddressType>& address_type,
      ConnectDeviceCallback callback,
      ConnectDeviceErrorCallback error_callback) override;
#endif
  device::BluetoothLocalGattService* GetGattService(
      const std::string& identifier) const override;
#if BUILDFLAG(IS_CHROMEOS)
  void SetServiceAllowList(const UUIDList& uuids,
                           base::OnceClosure callback,
                           ErrorCallback error_callback) override;
  LowEnergyScanSessionHardwareOffloadingStatus
  GetLowEnergyScanSessionHardwareOffloadingStatus() override;
  std::unique_ptr<device::BluetoothLowEnergyScanSession>
  StartLowEnergyScanSession(
      std::unique_ptr<device::BluetoothLowEnergyScanFilter> filter,
      base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate> delegate)
      override;
  std::vector<BluetoothRole> GetSupportedRoles() override;
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetStandardChromeOSAdapterName() override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  base::WeakPtr<BluetoothAdapter> GetWeakPtr() override;
  bool SetPoweredImpl(bool powered) override;
  void UpdateFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override;
  void StartScanWithFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override;
  void StopScan(DiscoverySessionResultCallback callback) override;
  void RemovePairingDelegateInternal(
      device::BluetoothDevice::PairingDelegate* pairing_delegate) override;

 private:
  ~FakeCentral() override;

  FakePeripheral* GetFakePeripheral(
      const std::string& peripheral_address) const;
  FakeRemoteGattService* GetFakeRemoteGattService(
      const std::string& peripheral_address,
      const std::string& service_id) const;
  FakeRemoteGattCharacteristic* GetFakeRemoteGattCharacteristic(
      const std::string& peripheral_address,
      const std::string& service_id,
      const std::string& characteristic_id) const;
  FakeRemoteGattDescriptor* GetFakeRemoteGattDescriptor(
      const std::string& peripheral_address,
      const std::string& service_id,
      const std::string& characteristic_id,
      const std::string& descriptor_id) const;

  mojom::CentralState state_;
  mojo::Receiver<mojom::FakeCentral> receiver_;
  base::WeakPtrFactory<FakeCentral> weak_ptr_factory_{this};
};

}  // namespace bluetooth

#endif  // DEVICE_BLUETOOTH_EMULATION_FAKE_CENTRAL_H_
