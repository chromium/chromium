// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_WIN_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_WIN_H_

#include <Windows.Devices.Enumeration.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_pending_task.h"
#include "base/test/test_simple_task_runner.h"
#include "base/win/scoped_winrt_initializer.h"
#include "device/bluetooth/bluetooth_classic_win_fake.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"
#include "device/bluetooth/test/bluetooth_test.h"

namespace device {

// Windows implementation of BluetoothTestBase.
class BluetoothTestWin : public BluetoothTestBase {
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
  std::optional<BluetoothUUID> GetTargetGattService(
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

 private:
  scoped_refptr<base::TestSimpleTaskRunner> ui_task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> bluetooth_task_runner_;

  void AdapterInitCallback();

  void FinishPendingTasks();
};

// Defines common test fixture name. Use TEST_F(BluetoothTest, YourTestName).
typedef BluetoothTestWin BluetoothTest;

struct BluetoothTestWinrtParam {
  // The feature state of |kNewBLEGattSessionHandling|.
  bool new_gatt_session_handling_enabled;

  friend std::ostream& operator<<(std::ostream& os,
                                  const BluetoothTestWinrtParam& p) {
    return os << "{new_gatt_session_handling_enabled="
              << p.new_gatt_session_handling_enabled << "}";
  }
};

constexpr BluetoothTestWinrtParam kBluetoothTestWinrtParam[] = {
    {true},
    {false},
};

// This test suite represents tests that are parameterized on Windows. This
// requires declaring tests in the following way:
//
// TEST_P(BluetoothTestWinrt, YourTestName).
//
// Test suites inheriting from this class should be instantiated as
//
// INSTANTIATE_TEST_SUITE_P(
//     All, FooTestSuiteWinrt,
//     ::testing::ValuesIn(kBluetoothTestWinrtParam>));
class BluetoothTestWinrt
    : public BluetoothTestWin,
      public ::testing::WithParamInterface<BluetoothTestWinrtParam> {
 public:
  BluetoothTestWinrt();

  BluetoothTestWinrt(const BluetoothTestWinrt&) = delete;
  BluetoothTestWinrt& operator=(const BluetoothTestWinrt&) = delete;

  ~BluetoothTestWinrt() override;

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
                          std::string_view display_pin);
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
  base::win::ScopedWinrtInitializer scoped_winrt_initializer_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_WIN_H_
