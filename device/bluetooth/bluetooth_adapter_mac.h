// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_MAC_H_

#include <IOKit/IOReturn.h>

#include <memory>
#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_discovery_manager_mac.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_low_energy_adapter_apple.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

@class BluetoothDevicesConnectListener;
@class IOBluetoothDevice;

namespace device {

class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterMac
    : public BluetoothLowEnergyAdapterApple,
      public BluetoothDiscoveryManagerMac::Observer {
 public:
  static scoped_refptr<BluetoothAdapterMac> CreateAdapter();
  static scoped_refptr<BluetoothAdapterMac> CreateAdapterForTest(
      std::string name,
      std::string address,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  BluetoothAdapterMac(const BluetoothAdapterMac&) = delete;
  BluetoothAdapterMac& operator=(const BluetoothAdapterMac&) = delete;

  // BluetoothAdapter overrides:
  std::string GetAddress() const override;
  std::string GetName() const override;
  void SetName(const std::string& name,
               base::OnceClosure callback,
               ErrorCallback error_callback) override;
  bool IsPresent() const override;
  bool IsPowered() const override;
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

  // BluetoothDiscoveryManagerMac::Observer overrides:
  void ClassicDeviceFound(IOBluetoothDevice* device) override;
  void ClassicDiscoveryStopped(bool unexpected) override;

  // Used for delivering device connect notification from MacOS IOBluetooth
  // framework to this adapter object.
  void OnConnectNotification(IOBluetoothDevice* device);

  // Registers that a new |device| has connected to the local host.
  void DeviceConnected(std::unique_ptr<BluetoothDevice> device);

 protected:
  // BluetoothAdapter override:
  base::WeakPtr<BluetoothAdapter> GetWeakPtr() override;
  bool SetPoweredImpl(bool powered) override;

  // BluetoothLowEnergyAdapterApple override:
  base::WeakPtr<BluetoothLowEnergyAdapterApple> GetLowEnergyWeakPtr() override;
  void TriggerSystemPermissionPrompt() override;

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

  // BluetoothLowEnergyAdapterApple:
  void LazyInitialize() override;
  void InitForTest(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) override;
  BluetoothLowEnergyAdapterApple::GetDevicePairedStatusCallback
  GetDevicePairedStatus() const override;

  // Queries the state of the IOBluetoothHostController.
  HostControllerState GetHostControllerState();

  // Allows configuring whether the adapter is present when running in a test
  // configuration.
  void SetPresentForTesting(bool present);

  // Allow the mocking out of getting the HostController state for testing.
  void SetHostControllerStateFunctionForTesting(
      HostControllerStateFunction controller_state_function);

  // Allow the mocking out of setting the controller power state for testing.
  void SetPowerStateFunctionForTesting(
      SetControllerPowerStateFunction power_state_function);

  // Allow the mocking of out GetDevicePairedStatusCallback for testing.
  void SetGetDevicePairedStatusCallbackForTesting(
      BluetoothLowEnergyAdapterApple::GetDevicePairedStatusCallback callback);

  // The length of time that must elapse since the last Inquiry response (on
  // Classic devices) or call to BluetoothLowEnergyDevice::Update() (on Low
  // Energy) before a discovered device is considered to be no longer available.
  const static NSTimeInterval kDiscoveryTimeoutSec;

  friend class BluetoothTestMac;
  friend class BluetoothAdapterMacTest;
  friend class BluetoothLowEnergyAdapterAppleTest;

  BluetoothAdapterMac();
  ~BluetoothAdapterMac() override;

  // BluetoothAdapter overrides:
  void StartScanWithFilter(
      std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override;
  void StopScan(DiscoverySessionResultCallback callback) override;

  void PollAdapter();

  // Registers that a new |device| has replied to an Inquiry, is paired, or has
  // connected to the local host.
  void ClassicDeviceAdded(std::unique_ptr<BluetoothDevice> device);

  // Updates |devices_| to include the currently paired devices and notifies
  // observers.
  void AddPairedDevices();

  std::string address_;
  bool classic_powered_ = false;
  std::optional<bool> is_present_for_testing_;

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
  mutable bool should_update_name_ = true;

  // Discovery manager for Bluetooth Classic.
  std::unique_ptr<BluetoothDiscoveryManagerMac> classic_discovery_manager_;

  BluetoothLowEnergyAdapterApple::GetDevicePairedStatusCallback
      device_paired_status_callback_;

  // The paired device count the last time the adapter was polled, or nullopt if
  // the adapter has not been polled.
  std::optional<uint32_t> paired_count_;

  BluetoothDevicesConnectListener* __strong connect_listener_;

  base::WeakPtrFactory<BluetoothAdapterMac> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_MAC_H_
