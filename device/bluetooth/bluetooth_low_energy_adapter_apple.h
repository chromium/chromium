// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_ADAPTER_APPLE_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_ADAPTER_APPLE_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_low_energy_advertisement_manager_mac.h"
#include "device/bluetooth/bluetooth_low_energy_device_mac.h"
#include "device/bluetooth/bluetooth_low_energy_device_watcher_mac.h"
#include "device/bluetooth/bluetooth_low_energy_discovery_manager_mac.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

@class CBUUID;

@class BluetoothLowEnergyCentralManagerDelegate;
@class BluetoothLowEnergyPeripheralManagerDelegate;

namespace device {

// BluetoothLowEnergyAdapterApple implements BluetoothAdapter supported with
// CoreBluetooth on Apple devices.
class DEVICE_BLUETOOTH_EXPORT BluetoothLowEnergyAdapterApple
    : public BluetoothAdapter,
      public BluetoothLowEnergyDiscoveryManagerMac::Observer {
 public:
  using DevicesInfo = std::map<std::string, std::string>;
  using GetDevicePairedStatusCallback =
      base::RepeatingCallback<bool(const std::string& address)>;

  BluetoothLowEnergyAdapterApple(const BluetoothLowEnergyAdapterApple&) =
      delete;
  BluetoothLowEnergyAdapterApple& operator=(
      const BluetoothLowEnergyAdapterApple&) = delete;

  // Converts CBUUID into BluetoothUUID
  static BluetoothUUID BluetoothUUIDWithCBUUID(CBUUID* UUID);

  // Converts NSError to string for logging.
  static std::string String(NSError* error);

  // BluetoothAdapter overrides:
  std::string GetAddress() const override;
  std::string GetName() const override;
  void SetName(const std::string& name,
               base::OnceClosure callback,
               ErrorCallback error_callback) override;
  bool IsInitialized() const override;
  bool IsPresent() const override;
  bool IsPowered() const override;
  PermissionStatus GetOsPermissionStatus() const override;
  void RequestSystemPermission(RequestSystemPermissionCallback) override;
  bool IsDiscoverable() const override;
  void SetDiscoverable(bool discoverable,
                       base::OnceClosure callback,
                       ErrorCallback error_callback) override;
  bool IsDiscovering() const override;
  void CreateRfcommService(const BluetoothUUID& uuid,
                           const ServiceOptions& options,
                           CreateServiceCallback callback,
                           CreateServiceErrorCallback error_callback) override;
  void CreateL2capService(const BluetoothUUID& uuid,
                          const ServiceOptions& options,
                          CreateServiceCallback callback,
                          CreateServiceErrorCallback error_callback) override;
  std::unordered_map<BluetoothDevice*, BluetoothDevice::UUIDSet>
  RetrieveGattConnectedDevicesWithDiscoveryFilter(
      const BluetoothDiscoveryFilter& discovery_filter) override;
  UUIDList GetUUIDs() const override;
  void RegisterAdvertisement(
      std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
      CreateAdvertisementCallback callback,
      AdvertisementErrorCallback error_callback) override;
  DeviceList GetDevices() override;
  ConstDeviceList GetDevices() const override;
  BluetoothLocalGattService* GetGattService(
      const std::string& identifier) const override;

  // Creates a GATT connection by calling CoreBluetooth APIs.
  void CreateGattConnection(BluetoothLowEnergyDeviceMac* device_mac);

  // Closes the GATT connection by calling CoreBluetooth APIs.
  void DisconnectGatt(BluetoothLowEnergyDeviceMac* device_mac);

  // Methods called from CBCentralManager delegate.
  void DidConnectPeripheral(CBPeripheral* peripheral);
  void DidFailToConnectPeripheral(CBPeripheral* peripheral, NSError* error);
  void DidDisconnectPeripheral(CBPeripheral* peripheral, NSError* error);

  void UpdateKnownLowEnergyDevices(DevicesInfo updated_low_energy_device_info);

  // Returns true when `device_identifier` is found in
  // `low_energy_devices_info_`. If the framework supports the paired status, it
  // calls GetDevicePairedStatusCallback to check the status of the device.
  bool IsBluetoothLowEnergyDeviceSystemPaired(
      std::string_view device_identifier) const;

 protected:
  BluetoothLowEnergyAdapterApple();
  ~BluetoothLowEnergyAdapterApple() override;

  virtual void LazyInitialize();
  virtual void InitForTest(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  virtual GetDevicePairedStatusCallback GetDevicePairedStatus() const;
  virtual base::WeakPtr<BluetoothLowEnergyAdapterApple>
  GetLowEnergyWeakPtr() = 0;
  virtual void TriggerSystemPermissionPrompt() = 0;

  // BluetoothAdapter override:
  bool SetPoweredImpl(bool powered) override;
  void RemovePairingDelegateInternal(
      device::BluetoothDevice::PairingDelegate* pairing_delegate) override;

  // Returns the CBCentralManager instance.
  CBCentralManager* GetCentralManager();

  // Returns the CBPeripheralManager instance.
  CBPeripheralManager* GetPeripheralManager();

  // Checks if the low energy central manager is powered on. Returns false if
  // BLE is not available.
  bool IsLowEnergyPowered() const;

  // Starts a low energy discovery session or update it if one is already
  // running.
  void StartScanLowEnergy();

  // Stops discovery and clears all advertisement data.
  void StopScanLowEnergy();

  // The Initialize() method intentionally does not initialize
  // |low_energy_central_manager_| or |low_energy_peripheral_manager_| because
  // Chromium might not have permission to access the Bluetooth adapter.
  // Methods which require these to be initialized must call LazyInitialize()
  // first.
  bool lazy_initialized_ = false;

 private:
  friend class BluetoothTestMac;
  friend class BluetoothLowEnergyAdapterAppleTest;
  friend class BluetoothLowEnergyCentralManagerBridge;
  friend class BluetoothLowEnergyPeripheralManagerBridge;

  // BluetoothAdapter overrides:
  void StartScanWithFilter(
      std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override;
  void UpdateFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override;
  void StopScan(DiscoverySessionResultCallback callback) override;
  void Initialize(base::OnceClosure callback) override;

  // BluetoothLowEnergyDiscoveryManagerMac::Observer override:
  void LowEnergyDeviceUpdated(CBPeripheral* peripheral,
                              NSDictionary* advertisement_data,
                              int rssi) override;

  // Updates |devices_| when there is a change to the CBCentralManager's state.
  void LowEnergyCentralManagerUpdatedState();

  // Resets |low_energy_central_manager_| to |central_manager| and sets
  // |low_energy_central_manager_delegate_| as the manager's delegate. Should
  // be called only when |IsLowEnergyAvailable()|.
  void SetCentralManagerForTesting(CBCentralManager* central_manager);

  // Allow the mocking out of BluetoothLowEnergyDeviceWatcher for testing.
  void SetLowEnergyDeviceWatcherForTesting(
      scoped_refptr<BluetoothLowEnergyDeviceWatcherMac> watcher);

  // Returns the list of devices that are connected by other applications than
  // Chromium, based on a service UUID. If no uuid is given, generic access
  // service (1800) is used (since CoreBluetooth requires to use a service).
  std::vector<BluetoothDevice*> RetrieveGattConnectedDevicesWithService(
      const BluetoothUUID* uuid);

  // Returns the BLE device associated with the CoreBluetooth peripheral.
  BluetoothLowEnergyDeviceMac* GetBluetoothLowEnergyDeviceMac(
      CBPeripheral* peripheral);

  // Returns true if a new device collides with an existing device.
  bool DoesCollideWithKnownDevice(CBPeripheral* peripheral,
                                  BluetoothLowEnergyDeviceMac* device_mac);

  void FlushRequestSystemPermissionCallbacks();

  // Discovery manager for Bluetooth Low Energy.
  std::unique_ptr<BluetoothLowEnergyDiscoveryManagerMac>
      low_energy_discovery_manager_;

  // Advertisement manager for Bluetooth Low Energy.
  std::unique_ptr<BluetoothLowEnergyAdvertisementManagerMac>
      low_energy_advertisement_manager_;

  // Underlying CoreBluetooth CBCentralManager and its delegate.
  CBCentralManager* __strong low_energy_central_manager_;
  BluetoothLowEnergyCentralManagerDelegate* __strong
      low_energy_central_manager_delegate_;

  // Underlying CoreBluetooth CBPeripheralManager and its delegate.
  CBPeripheralManager* __strong low_energy_peripheral_manager_;
  BluetoothLowEnergyPeripheralManagerDelegate* __strong
      low_energy_peripheral_manager_delegate_;

  // Watches system file /Library/Preferences/com.apple.Bluetooth.plist to
  // obtain information about system paired bluetooth devices.
  scoped_refptr<BluetoothLowEnergyDeviceWatcherMac>
      bluetooth_low_energy_device_watcher_;

  // Map of UUID formatted device identifiers of paired Bluetooth devices and
  // corresponding device address.
  DevicesInfo low_energy_devices_info_;

  // Callbacks from `RequestSystemPermission` will be called once the Bluetooth
  // system permission has settled.
  std::vector<BluetoothAdapter::RequestSystemPermissionCallback>
      request_system_permission_callbacks_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_ADAPTER_APPLE_H_
