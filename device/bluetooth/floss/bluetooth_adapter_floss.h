// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_ADAPTER_FLOSS_H_
#define DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_ADAPTER_FLOSS_H_

#include <cstdint>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "build/chromeos_buildflags.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_gatt_service.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"
#include "device/bluetooth/bluetooth_low_energy_scan_session.h"
#endif

namespace floss {

// The BluetoothAdapterFloss class implements BluetoothAdapter for platforms
// that use Floss, a dbus front-end for the Fluoride Bluetooth stack.
//
// Floss separates the "Powered" management of adapters in a separate manager
// interface. This class will first initialize the manager interface before
// initializing any clients that depend on a specific adapter being targeted.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterFloss final
    : public device::BluetoothAdapter {
 public:
  BluetoothAdapterFloss(const BluetoothAdapterFloss&) = delete;
  BluetoothAdapterFloss& operator=(const BluetoothAdapterFloss&) = delete;

  static scoped_refptr<BluetoothAdapterFloss> CreateAdapter();

  // BluetoothAdapter:
  void Initialize(base::OnceClosure callback) override;
  void Shutdown() override;

  UUIDList GetUUIDs() const override;

  std::string GetAddress() const override;
  std::string GetName() const override;
  std::string GetSystemName() const override;
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
  bool IsDiscovering() const override;

  std::unordered_map<device::BluetoothDevice*, device::BluetoothDevice::UUIDSet>
  RetrieveGattConnectedDevicesWithDiscoveryFilter(
      const device::BluetoothDiscoveryFilter& discovery_filter) override;
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

  void SetAdvertisingInterval(
      const base::TimeDelta& min,
      const base::TimeDelta& max,
      base::OnceClosure callback,
      AdvertisementErrorCallback error_callback) override;

  void ResetAdvertising(base::OnceClosure callback,
                        AdvertisementErrorCallback error_callback) override;

  void ConnectDevice(
      const std::string& address,
      const absl::optional<device::BluetoothDevice::AddressType>& address_type,
      ConnectDeviceCallback callback,
      ErrorCallback error_callback) override;

  device::BluetoothLocalGattService* GetGattService(
      const std::string& identifier) const override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetServiceAllowList(const UUIDList& uuids,
                           base::OnceClosure callback,
                           ErrorCallback error_callback) override;

  std::unique_ptr<device::BluetoothLowEnergyScanSession>
  StartLowEnergyScanSession(
      std::unique_ptr<device::BluetoothLowEnergyScanFilter> filter,
      base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate> delegate)
      override;
#endif

 protected:
  // BluetoothAdapter:
  void RemovePairingDelegateInternal(
      device::BluetoothDevice::PairingDelegate* pairing_delegate) override;

 private:
  BluetoothAdapterFloss();
  ~BluetoothAdapterFloss() override;

  // BluetoothAdapter:
  base::WeakPtr<BluetoothAdapter> GetWeakPtr() override;
  bool SetPoweredImpl(bool powered) override;
  void StartScanWithFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override;
  void UpdateFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override;
  void StopScan(DiscoverySessionResultCallback callback) override;

  base::WeakPtrFactory<BluetoothAdapterFloss> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_ADAPTER_FLOSS_H_
