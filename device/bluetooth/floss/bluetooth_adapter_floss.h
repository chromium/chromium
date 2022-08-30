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
#include "device/bluetooth/floss/floss_adapter_client.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/floss/floss_manager_client.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"
#include "device/bluetooth/bluetooth_low_energy_scan_session.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace floss {

class BluetoothDeviceFloss;

// The BluetoothAdapterFloss class implements BluetoothAdapter for platforms
// that use Floss, a dbus front-end for the Fluoride Bluetooth stack.
//
// Floss separates the "Powered" management of adapters in a separate manager
// interface. This class will first initialize the manager interface before
// initializing any clients that depend on a specific adapter being targeted.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterFloss final
    : public device::BluetoothAdapter,
      public floss::FlossManagerClient::Observer,
      public floss::FlossAdapterClient::Observer {
 public:
  static scoped_refptr<BluetoothAdapterFloss> CreateAdapter();

  BluetoothAdapterFloss(const BluetoothAdapterFloss&) = delete;
  BluetoothAdapterFloss& operator=(const BluetoothAdapterFloss&) = delete;

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
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Set the adapter name to one chosen from the system information. Only Ash
  // needs to do this.
  void SetStandardChromeOSAdapterName() override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 protected:
  // BluetoothAdapter:
  void RemovePairingDelegateInternal(
      device::BluetoothDevice::PairingDelegate* pairing_delegate) override;

 private:
  BluetoothAdapterFloss();
  ~BluetoothAdapterFloss() override;

  // Init will get asynchronouly called once we know if Object Manager is
  // supported.
  void Init();

  // Handle responses to most method calls
  void OnMethodResponse(base::OnceClosure callback,
                        ErrorCallback error_callback,
                        DBusResult<Void> ret);

  // Handle when discovery is automatically repeated based on active sessions.
  void OnRepeatedDiscoverySessionResult(
      bool start_discovery,
      bool is_error,
      device::UMABluetoothDiscoverySessionOutcome outcome);

  // Called on completion of start discovery and stop discovery
  void OnStartDiscovery(DiscoverySessionResultCallback callback,
                        DBusResult<Void> ret);
  void OnStopDiscovery(DiscoverySessionResultCallback callback,
                       DBusResult<Void> ret);
  void OnGetConnectionState(const FlossDeviceId& device_id,
                            DBusResult<uint32_t> ret);
  void OnGetBondState(const FlossDeviceId& device_id, DBusResult<uint32_t> ret);

  // Announce to observers a change in the adapter state.
  void DiscoveringChanged(bool discovering);
  void PresentChanged(bool present);
  void NotifyAdapterPoweredChanged(bool powered);

  // Announce to observers that |device| has changed its connected state.
  void NotifyDeviceConnectedStateChanged(BluetoothDeviceFloss* device,
                                         bool is_now_connected);

  // Observers
  // floss::FlossManagerClient::Observer override.
  void AdapterPresent(int adapter, bool present) override;
  void AdapterEnabledChanged(int adapter, bool enabled) override;

  // Initialize observers for adapter dependent clients
  void AddAdapterObservers();

  // Remove any active adapters.
  void RemoveAdapter();

  void PopulateInitialDevices();
  void ClearAllDevices();

  // floss::FlossAdapterClient::Observer override.
  void DiscoverableChanged(bool discoverable) override;
  void AdapterDiscoveringChanged(bool state) override;
  void AdapterFoundDevice(const FlossDeviceId& device_found) override;
  void AdapterClearedDevice(const FlossDeviceId& device_found) override;
  void AdapterSspRequest(const FlossDeviceId& remote_device,
                         uint32_t cod,
                         FlossAdapterClient::BluetoothSspVariant variant,
                         uint32_t passkey) override;
  void DeviceBondStateChanged(
      const FlossDeviceId& remote_device,
      uint32_t status,
      FlossAdapterClient::BondState bond_state) override;
  void AdapterDeviceConnected(const FlossDeviceId& device_id) override;
  void AdapterDeviceDisconnected(const FlossDeviceId& device_id) override;

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

  base::OnceClosure init_callback_;

  // Keeps track of whether the adapter is fully initialized.
  bool initialized_ = false;

  // Keeps track of whether Shutdown is called (and dbus clients are cleaned
  // up properly).
  bool dbus_is_shutdown_ = false;

  base::WeakPtrFactory<BluetoothAdapterFloss> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_ADAPTER_FLOSS_H_
