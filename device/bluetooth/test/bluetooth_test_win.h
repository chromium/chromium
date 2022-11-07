// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_WIN_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_WIN_H_

#include "device/bluetooth/test/bluetooth_test.h"

#include <Windows.Devices.Enumeration.h>

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_pending_task.h"
#include "base/test/test_simple_task_runner.h"
#include "base/win/scoped_winrt_initializer.h"
#include "device/bluetooth/bluetooth_classic_win_fake.h"
#include "device/bluetooth/bluetooth_low_energy_win_fake.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

// Windows implementation of BluetoothTestBase.
class BluetoothTestWin : public BluetoothTestBase,
                         public win::BluetoothLowEnergyWrapperFake::Observer {
 public:
  BluetoothTestWin();
  ~BluetoothTestWin() override;

  // BluetoothTestBase overrides
  bool PlatformSupportsLowEnergy() override;
  void InitWithDefaultAdapter() override;
  void InitWithoutDefaultAdapter() override;
  void InitWithFakeAdapter() override;
  bool DenyPermission() override;
  void StartLowEnergyDiscoverySession() override;
  BluetoothDevice* SimulateLowEnergyDevice(int device_ordinal) override;
  absl::optional<BluetoothUUID> GetTargetGattService(
      BluetoothDevice* device) override;
  void SimulateGattConnection(BluetoothDevice* device) override;
  void SimulateStatusChangeToDisconnect(BluetoothDevice* device) override;
  void SimulateGattServicesDiscovered(
      BluetoothDevice* device,
      const std::vector<std::string>& uuids,
      const std::vector<std::string>& blocked_uuids = {}) override;
  void SimulateGattServiceRemoved(BluetoothRemoteGattService* service) override;
  void SimulateGattCharacteristic(BluetoothRemoteGattService* service,
                                  const std::string& uuid,
                                  int properties) override;
  void SimulateGattCharacteristicRemoved(
      BluetoothRemoteGattService* service,
      BluetoothRemoteGattCharacteristic* characteristic) override;
  void RememberCharacteristicForSubsequentAction(
      BluetoothRemoteGattCharacteristic* characteristic) override;
  void SimulateGattCharacteristicRead(
      BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;
  void SimulateGattCharacteristicReadError(
      BluetoothRemoteGattCharacteristic* characteristic,
      BluetoothGattService::GattErrorCode error_code) override;
  void SimulateGattCharacteristicWrite(
      BluetoothRemoteGattCharacteristic* characteristic) override;
  void SimulateGattCharacteristicWriteError(
      BluetoothRemoteGattCharacteristic* characteristic,
      BluetoothGattService::GattErrorCode error_code) override;
  void RememberDeviceForSubsequentAction(BluetoothDevice* device) override;
  void DeleteDevice(BluetoothDevice* device) override;
  void SimulateGattDescriptor(BluetoothRemoteGattCharacteristic* characteristic,
                              const std::string& uuid) override;
  void SimulateGattNotifySessionStarted(
      BluetoothRemoteGattCharacteristic* characteristic) override;
  void SimulateGattNotifySessionStartError(
      BluetoothRemoteGattCharacteristic* characteristic,
      BluetoothGattService::GattErrorCode error_code) override;
  void SimulateGattCharacteristicChanged(
      BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;

  // win::BluetoothLowEnergyWrapperFake::Observer overrides.
  void OnReadGattCharacteristicValue() override;
  void OnWriteGattCharacteristicValue(
      const PBTH_LE_GATT_CHARACTERISTIC_VALUE value) override;
  void OnStartCharacteristicNotification() override;
  void OnWriteGattDescriptorValue(const std::vector<uint8_t>& value) override;

 private:
  scoped_refptr<base::TestSimpleTaskRunner> ui_task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> bluetooth_task_runner_;

  raw_ptr<win::BluetoothLowEnergyWrapperFake> fake_bt_le_wrapper_;

  // This is used for retaining access to a single deleted device.
  std::string remembered_device_address_;

  void AdapterInitCallback();
  win::GattService* GetSimulatedService(win::BLEDevice* device,
                                        BluetoothRemoteGattService* service);
  win::GattCharacteristic* GetSimulatedCharacteristic(
      BluetoothRemoteGattCharacteristic* characteristic);

  // Run pending Bluetooth tasks until the first callback that the test fixture
  // tracks is called.
  void RunPendingTasksUntilCallback();
  void ForceRefreshDevice();
  void FinishPendingTasks();
};

// Defines common test fixture name. Use TEST_F(BluetoothTest, YourTestName).
typedef BluetoothTestWin BluetoothTest;

struct BluetoothTestWinrtParam {
  // The feature state of |kNewBLEWinImplementation|.
  bool new_ble_implementation_enabled;
  // The feature state of |kNewBLEGattSessionHandling|.
  bool new_gatt_session_handling_enabled;

  friend std::ostream& operator<<(std::ostream& os,
                                  const BluetoothTestWinrtParam& p) {
    return os << "{new_ble_implementation_enabled="
              << p.new_ble_implementation_enabled
              << ", new_gatt_session_handling_enabled="
              << p.new_gatt_session_handling_enabled << "}";
  }
};

constexpr BluetoothTestWinrtParam kBluetoothTestWinrtParamAll[] = {
    {false, false},
    {false, true},
    {true, false},
    {true, true},
};

constexpr BluetoothTestWinrtParam kBluetoothTestWinrtParamWinrtOnly[] = {
    {true, false},
    {true, true},
};

constexpr BluetoothTestWinrtParam kBluetoothTestWinrtParamWin32Only[] = {
    {false, false},
    {false, true},
};

// This test suite represents tests that should run with the new BLE
// implementation both enabled and disabled. This requires declaring tests
// in the following way: TEST_P(BluetoothTestWinrt, YourTestName).
//
// Test suites inheriting from this class should be instantiated as
//
// INSTANTIATE_TEST_SUITE_P(
//     All, FooTestSuiteWinrt,
//     ::testing::ValuesIn(
//         <kBluetoothTestWinrtParamWin32Only |
//          kBluetoothTestWinrtParamWinrtOnly |
//          kBluetoothTestWinrtParamAll>));
//
// depending on whether they should run only the old or new implementation or
// both.
class BluetoothTestWinrt
    : public BluetoothTestWin,
      public ::testing::WithParamInterface<BluetoothTestWinrtParam> {
 public:
  BluetoothTestWinrt();

  BluetoothTestWinrt(const BluetoothTestWinrt&) = delete;
  BluetoothTestWinrt& operator=(const BluetoothTestWinrt&) = delete;

  ~BluetoothTestWinrt() override;

  bool UsesNewBleImplementation() const;
  bool UsesNewGattSessionHandling() const;

  // Simulate a fake adapter whose power status cannot be
  // controlled because of a Windows Privacy setting.
  void InitFakeAdapterWithRadioAccessDenied();
  void SimulateSpuriousRadioStateChangedEvent();

  // BluetoothTestBase:
  bool PlatformSupportsLowEnergy() override;
  void InitWithDefaultAdapter() override;
  void InitWithoutDefaultAdapter() override;
  void InitWithFakeAdapter() override;
  void InitFakeAdapterWithoutRadio() override;
  void SimulateAdapterPowerFailure() override;
  void SimulateAdapterPoweredOn() override;
  void SimulateAdapterPoweredOff() override;
  BluetoothDevice* SimulateLowEnergyDevice(int device_ordinal) override;
  void SimulateLowEnergyDiscoveryFailure() override;
  void SimulateDevicePaired(BluetoothDevice* device, bool is_paired) override;
  void SimulatePairingPinCode(BluetoothDevice* device,
                              std::string pin_code) override;
  // Currently only Win derived class has this function for create ConfirmOnly /
  // DisplayPin tests.  If in future we find that other platform need to test
  // for pairing_kind we should promote this function as virtual
  void SimulateConfirmOnly(BluetoothDevice* device);
  void SimulateDisplayPin(BluetoothDevice* device,
                          base::StringPiece display_pin);
  void SimulateAdvertisementStarted(
      BluetoothAdvertisement* advertisement) override;
  void SimulateAdvertisementStopped(
      BluetoothAdvertisement* advertisement) override;
  void SimulateAdvertisementError(
      BluetoothAdvertisement* advertisement,
      BluetoothAdvertisement::ErrorCode error_code) override;
  void SimulateGattConnection(BluetoothDevice* device) override;
  void SimulateGattConnectionError(
      BluetoothDevice* device,
      BluetoothDevice::ConnectErrorCode error_code) override;
  void SimulateGattDisconnection(BluetoothDevice* device) override;
  void SimulateDeviceBreaksConnection(BluetoothDevice* device) override;
  void SimulateGattNameChange(BluetoothDevice* device,
                              const std::string& new_name) override;
  void SimulateStatusChangeToDisconnect(BluetoothDevice* device) override;
  void SimulateGattServicesDiscovered(
      BluetoothDevice* device,
      const std::vector<std::string>& uuids,
      const std::vector<std::string>& blocked_uuids = {}) override;
  void SimulateGattServicesChanged(BluetoothDevice* device) override;
  void SimulateGattServiceRemoved(BluetoothRemoteGattService* service) override;
  void SimulateGattServicesDiscoveryError(BluetoothDevice* device) override;
  void SimulateGattCharacteristic(BluetoothRemoteGattService* service,
                                  const std::string& uuid,
                                  int properties) override;
  void SimulateGattNotifySessionStarted(
      BluetoothRemoteGattCharacteristic* characteristic) override;
  void SimulateGattNotifySessionStartError(
      BluetoothRemoteGattCharacteristic* characteristic,
      BluetoothGattService::GattErrorCode error_code) override;
  void SimulateGattNotifySessionStopped(
      BluetoothRemoteGattCharacteristic* characteristic) override;
  void SimulateGattNotifySessionStopError(
      BluetoothRemoteGattCharacteristic* characteristic,
      BluetoothGattService::GattErrorCode error_code) override;
  void SimulateGattCharacteristicChanged(
      BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;
  void SimulateGattCharacteristicRead(
      BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;
  void SimulateGattCharacteristicReadError(
      BluetoothRemoteGattCharacteristic* characteristic,
      BluetoothGattService::GattErrorCode error_code) override;
  void SimulateGattCharacteristicWrite(
      BluetoothRemoteGattCharacteristic* characteristic) override;
  void SimulateGattCharacteristicWriteError(
      BluetoothRemoteGattCharacteristic* characteristic,
      BluetoothGattService::GattErrorCode error_code) override;
  void SimulateGattDescriptor(BluetoothRemoteGattCharacteristic* characteristic,
                              const std::string& uuid) override;
  void SimulateGattDescriptorRead(BluetoothRemoteGattDescriptor* descriptor,
                                  const std::vector<uint8_t>& value) override;
  void SimulateGattDescriptorReadError(
      BluetoothRemoteGattDescriptor* descriptor,
      BluetoothGattService::GattErrorCode error_code) override;
  void SimulateGattDescriptorWrite(
      BluetoothRemoteGattDescriptor* descriptor) override;
  void SimulateGattDescriptorWriteError(
      BluetoothRemoteGattDescriptor* descriptor,
      BluetoothGattService::GattErrorCode error_code) override;
  void DeleteDevice(BluetoothDevice* device) override;

  void OnFakeBluetoothDeviceConnectGattAttempt();
  void OnFakeBluetoothDeviceGattServiceDiscoveryAttempt();
  void OnFakeBluetoothGattDisconnect();
  void OnFakeBluetoothCharacteristicReadValue();
  void OnFakeBluetoothCharacteristicWriteValue(std::vector<uint8_t> value);
  void OnFakeBluetoothGattSetCharacteristicNotification(NotifyValueState state);
  void OnFakeBluetoothDescriptorReadValue();
  void OnFakeBluetoothDescriptorWriteValue(std::vector<uint8_t> value);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  absl::optional<base::win::ScopedWinrtInitializer> scoped_winrt_initializer_;
};

using BluetoothTestWinrtOnly = BluetoothTestWinrt;

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_WIN_H_
