// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_MAC_H_

#include <IOKit/IOReturn.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_discovery_manager_mac.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_low_energy_advertisement_manager_mac.h"
#include "device/bluetooth/bluetooth_low_energy_device_mac.h"
#include "device/bluetooth/bluetooth_low_energy_device_watcher_mac.h"
#include "device/bluetooth/bluetooth_low_energy_discovery_manager_mac.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

@class CBUUID;
@class IOBluetoothDevice;
@class NSArray;
@class NSDate;

@class BluetoothAdvertisementMac;
@class BluetoothLowEnergyCentralManagerDelegate;
@class BluetoothLowEnergyPeripheralManagerDelegate;

namespace device {

// The 10.13 SDK deprecates the CBCentralManagerState enum, but marks the
// replacement enum with limited availability, making it unusable. API methods
// now return the new enum, so to compare enum values the new enum must be cast.
// Wrap this in a function to obtain the state via a call to [manager state] to
// avoid code that would use the replacement enum and trigger warnings.
CBCentralManagerState GetCBManagerState(CBCentralManager* manager);

class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterMac
    : public BluetoothAdapter,
      public BluetoothDiscoveryManagerMac::Observer,
      public BluetoothLowEnergyDiscoveryManagerMac::Observer {
 public:
  static base::WeakPtr<BluetoothAdapterMac> CreateAdapter();
  static base::WeakPtr<BluetoothAdapterMac> CreateAdapterForTest(
      std::string name,
      std::string address,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  // Converts CBUUID into BluetoothUUID
  static BluetoothUUID BluetoothUUIDWithCBUUID(CBUUID* UUID);

  // Converts NSError to string for logging.
  static std::string String(NSError* error);

  // BluetoothAdapter overrides:
  std::string GetAddress() const override;
  std::string GetName() const override;
  void SetName(const std::string& name,
               const base::Closure& callback,
               const ErrorCallback& error_callback) override;
  bool IsInitialized() const override;
  bool IsPresent() const override;
  bool IsPowered() const override;
  bool IsDiscoverable() const override;
  void SetDiscoverable(bool discoverable,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) override;
  bool IsDiscovering() const override;
  std::unordered_map<BluetoothDevice*, BluetoothDevice::UUIDSet>
  RetrieveGattConnectedDevicesWithDiscoveryFilter(
      const BluetoothDiscoveryFilter& discovery_filter) override;
  UUIDList GetUUIDs() const override;
  void CreateRfcommService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override;
  void CreateL2capService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override;
  void RegisterAdvertisement(
      std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
      const CreateAdvertisementCallback& callback,
      const AdvertisementErrorCallback& error_callback) override;
  BluetoothLocalGattService* GetGattService(
      const std::string& identifier) const override;

  // BluetoothDiscoveryManagerMac::Observer overrides:
  void ClassicDeviceFound(IOBluetoothDevice* device) override;
  void ClassicDiscoveryStopped(bool unexpected) override;

  // Registers that a new |device| has connected to the local host.
  void DeviceConnected(IOBluetoothDevice* device);

  // Creates a GATT connection by calling CoreBluetooth APIs.
  void CreateGattConnection(BluetoothLowEnergyDeviceMac* device_mac);

  // Closes the GATT connection by calling CoreBluetooth APIs.
  void DisconnectGatt(BluetoothLowEnergyDeviceMac* device_mac);

  // Methods called from CBCentralManager delegate.
  void DidConnectPeripheral(CBPeripheral* peripheral);
  void DidFailToConnectPeripheral(CBPeripheral* peripheral, NSError* error);
  void DidDisconnectPeripheral(CBPeripheral* peripheral, NSError* error);

  bool IsBluetoothLowEnergyDeviceSystemPaired(
      base::StringPiece device_identifier) const;

 protected:
  using GetDevicePairedStatusCallback =
      base::RepeatingCallback<bool(const std::string& address)>;

  // BluetoothAdapter override:
  base::WeakPtr<BluetoothAdapter> GetWeakPtr() override;
  bool SetPoweredImpl(bool powered) override;
  void RemovePairingDelegateInternal(
      device::BluetoothDevice::PairingDelegate* pairing_delegate) override;

  void UpdateKnownLowEnergyDevices(
      std::map<std::string, std::string> updated_low_energy_device_info);

 private:
  // Struct bundling information about the state of the HostController.
  struct HostControllerState {
    bool is_present = false;
    bool classic_powered = false;
    std::string address;
  };

  // Typedef for function returning the state of the HostController.
  using HostControllerStateFunction =
      base::RepeatingCallback<HostControllerState()>;

  // Type of the underlying implementation of SetPowered(). It takes an int
  // instead of a bool, since the production code calls into a C API that does
  // not know about bool.
  using SetControllerPowerStateFunction = base::RepeatingCallback<void(int)>;

  // Queries the state of the IOBluetoothHostController.
  HostControllerState GetHostControllerState();

  // Resets |low_energy_central_manager_| to |central_manager| and sets
  // |low_energy_central_manager_delegate_| as the manager's delegate. Should
  // be called only when |IsLowEnergyAvailable()|.
  void SetCentralManagerForTesting(CBCentralManager* central_manager);

  // Returns the CBCentralManager instance.
  CBCentralManager* GetCentralManager();

  // Returns the CBPeripheralManager instance.
  CBPeripheralManager* GetPeripheralManager();

  // Allow the mocking out of getting the HostController state for testing.
  void SetHostControllerStateFunctionForTesting(
      HostControllerStateFunction controller_state_function);

  // Allow the mocking out of setting the controller power state for testing.
  void SetPowerStateFunctionForTesting(
      SetControllerPowerStateFunction power_state_function);

  // Allow the mocking out of BluetoothLowEnergyDeviceWatcher for testing.
  void SetLowEnergyDeviceWatcherForTesting(
      scoped_refptr<BluetoothLowEnergyDeviceWatcherMac> watcher);

  // Allow the mocking of out GetDevicePairedStatusCallback for testing.
  void SetGetDevicePairedStatusCallbackForTesting(
      GetDevicePairedStatusCallback callback);

  // The length of time that must elapse since the last Inquiry response (on
  // Classic devices) or call to BluetoothLowEnergyDevice::Update() (on Low
  // Energy) before a discovered device is considered to be no longer available.
  const static NSTimeInterval kDiscoveryTimeoutSec;

  friend class BluetoothTestMac;
  friend class BluetoothAdapterMacTest;
  friend class BluetoothLowEnergyCentralManagerBridge;
  friend class BluetoothLowEnergyPeripheralManagerBridge;

  BluetoothAdapterMac();
  ~BluetoothAdapterMac() override;

  // BluetoothAdapter overrides:
  void StartScanWithFilter(
      std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override;
  void UpdateFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override;
  void StopScan(DiscoverySessionResultCallback callback) override;

  // Start classic and/or low energy discovery sessions, according to the
  // filter.  If a discovery session is already running the filter is updated.
  bool StartDiscovery(BluetoothDiscoveryFilter* discovery_filter);

  void Init();
  void InitForTest(scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  void PollAdapter();

  // Registers that a new |device| has replied to an Inquiry, is paired, or has
  // connected to the local host.
  void ClassicDeviceAdded(IOBluetoothDevice* device);

  // Checks if the low energy central manager is powered on. Returns false if
  // BLE is not available.
  bool IsLowEnergyPowered() const;

  // BluetoothLowEnergyDiscoveryManagerMac::Observer override:
  void LowEnergyDeviceUpdated(CBPeripheral* peripheral,
                              NSDictionary* advertisement_data,
                              int rssi) override;

  // Updates |devices_| when there is a change to the CBCentralManager's state.
  void LowEnergyCentralManagerUpdatedState();

  // Updates |advertisements_| when there is a change to the
  // CBPeripheralManager's state.
  void LowEnergyPeripheralManagerUpdatedState();

  // Updates |devices_| to include the currently paired devices and notifies
  // observers.
  void AddPairedDevices();

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

  std::string address_;
  bool classic_powered_;

  // Function returning the state of the HostController. Can be overridden for
  // tests.
  HostControllerStateFunction controller_state_function_;

  // SetPowered() implementation and callbacks.
  SetControllerPowerStateFunction power_state_function_;
  std::unique_ptr<SetPoweredCallbacks> set_powered_callbacks_;

  // Cached name. Updated in GetName if should_update_name_ is true.
  //
  // For performance reasons, cache the adapter's name. It's not uncommon for
  // a call to [controller nameAsString] to take tens of milliseconds. Note
  // that this caching strategy might result in clients receiving a stale
  // name. If this is a significant issue, then some more sophisticated
  // workaround for the performance bottleneck will be needed. For additional
  // context, see http://crbug.com/461181 and http://crbug.com/467316
  mutable std::string name_;
  // True if the name hasn't been acquired yet, the last acquired name is empty
  // or the address has changed indicating the name might have changed.
  mutable bool should_update_name_;

  // Discovery manager for Bluetooth Classic.
  std::unique_ptr<BluetoothDiscoveryManagerMac> classic_discovery_manager_;

  // Discovery manager for Bluetooth Low Energy.
  std::unique_ptr<BluetoothLowEnergyDiscoveryManagerMac>
      low_energy_discovery_manager_;

  // Advertisement manager for Bluetooth Low Energy.
  std::unique_ptr<BluetoothLowEnergyAdvertisementManagerMac>
      low_energy_advertisement_manager_;

  // Underlying CoreBluetooth CBCentralManager and its delegate.
  base::scoped_nsobject<CBCentralManager> low_energy_central_manager_;
  base::scoped_nsobject<BluetoothLowEnergyCentralManagerDelegate>
      low_energy_central_manager_delegate_;

  // Underlying CoreBluetooth CBPeripheralManager and its delegate.
  base::scoped_nsobject<CBPeripheralManager> low_energy_peripheral_manager_;
  base::scoped_nsobject<BluetoothLowEnergyPeripheralManagerDelegate>
      low_energy_peripheral_manager_delegate_;

  GetDevicePairedStatusCallback device_paired_status_callback_;

  // Watches system file /Library/Preferences/com.apple.Bluetooth.plist to
  // obtain information about system paired bluetooth devices.
  scoped_refptr<BluetoothLowEnergyDeviceWatcherMac>
      bluetooth_low_energy_device_watcher_;

  // Map of UUID formatted device identifiers of paired Bluetooth devices and
  // corresponding device address.
  std::map<std::string, std::string> low_energy_devices_info_;

  base::WeakPtrFactory<BluetoothAdapterMac> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterMac);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_MAC_H_
