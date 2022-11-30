// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_ANDROID_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_ANDROID_H_

#include <stdint.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "device/bluetooth/test/bluetooth_test.h"

namespace device {

// Android implementation of BluetoothTestBase.
class BluetoothTestAndroid : public BluetoothTestBase {
 public:
  BluetoothTestAndroid();
  ~BluetoothTestAndroid() override;

  // Test overrides:
  void SetUp() override;
  void TearDown() override;

  // BluetoothTestBase overrides:
  bool PlatformSupportsLowEnergy() override;
  void InitWithDefaultAdapter() override;
  void InitWithoutDefaultAdapter() override;
  void InitWithFakeAdapter() override;
  bool DenyPermission() override;
  BluetoothDevice* SimulateLowEnergyDevice(int device_ordinal) override;
  void RememberDeviceForSubsequentAction(BluetoothDevice* device) override;
  void SimulateGattConnection(BluetoothDevice* device) override;
  void SimulateGattConnectionError(BluetoothDevice* device,
                                   BluetoothDevice::ConnectErrorCode) override;
  void SimulateGattDisconnection(BluetoothDevice* device) override;
  void SimulateGattServicesDiscovered(
      BluetoothDevice* device,
      const std::vector<std::string>& uuids,
      const std::vector<std::string>& blocked_uuids = {}) override;
  void SimulateGattServicesDiscoveryError(BluetoothDevice* device) override;
  void SimulateGattCharacteristic(BluetoothRemoteGattService* service,
                                  const std::string& uuid,
                                  int properties) override;
  void RememberCharacteristicForSubsequentAction(
      BluetoothRemoteGattCharacteristic* characteristic) override;
  void RememberCCCDescriptorForSubsequentAction(
      BluetoothRemoteGattCharacteristic* characteristic) override;
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
  void SimulateGattCharacteristicSetNotifyWillFailSynchronouslyOnce(
      BluetoothRemoteGattCharacteristic* characteristic) override;
  void SimulateGattCharacteristicChanged(
      BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;

  void SimulateGattCharacteristicRead(
      BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;
  void SimulateGattCharacteristicReadError(
      BluetoothRemoteGattCharacteristic* characteristic,
      BluetoothGattService::GattErrorCode) override;
  void SimulateGattCharacteristicReadWillFailSynchronouslyOnce(
      BluetoothRemoteGattCharacteristic* characteristic) override;

  void SimulateGattCharacteristicWrite(
      BluetoothRemoteGattCharacteristic* characteristic) override;
  void SimulateGattCharacteristicWriteError(
      BluetoothRemoteGattCharacteristic* characteristic,
      BluetoothGattService::GattErrorCode) override;
  void SimulateGattCharacteristicWriteWillFailSynchronouslyOnce(
      BluetoothRemoteGattCharacteristic* characteristic) override;

  void SimulateGattDescriptor(BluetoothRemoteGattCharacteristic* characteristic,
                              const std::string& uuid) override;
  void RememberDescriptorForSubsequentAction(
      BluetoothRemoteGattDescriptor* descriptor) override;

  void SimulateGattDescriptorRead(BluetoothRemoteGattDescriptor* descriptor,
                                  const std::vector<uint8_t>& value) override;
  void SimulateGattDescriptorReadError(
      BluetoothRemoteGattDescriptor* descriptor,
      BluetoothGattService::GattErrorCode) override;
  void SimulateGattDescriptorReadWillFailSynchronouslyOnce(
      BluetoothRemoteGattDescriptor* descriptor) override;

  void SimulateGattDescriptorWrite(
      BluetoothRemoteGattDescriptor* descriptor) override;
  void SimulateGattDescriptorWriteError(
      BluetoothRemoteGattDescriptor* descriptor,
      BluetoothGattService::GattErrorCode) override;
  void SimulateGattDescriptorWriteWillFailSynchronouslyOnce(
      BluetoothRemoteGattDescriptor* descriptor) override;

  // Instruct the fake adapter to claim that location services are off for the
  // device.
  void SimulateLocationServicesOff();

  // Instruct the fake adapter to throw an IllegalStateException for
  // startScan and stopScan.
  void ForceIllegalStateException();

  // Records that Java FakeBluetoothDevice connectGatt was called.
  void OnFakeBluetoothDeviceConnectGattCalled(JNIEnv* env);

  // Records that Java FakeBluetoothGatt disconnect was called.
  void OnFakeBluetoothGattDisconnect(JNIEnv* env);

  // Records that Java FakeBluetoothGatt close was called.
  void OnFakeBluetoothGattClose(JNIEnv* env);

  // Records that Java FakeBluetoothGatt discoverServices was called.
  void OnFakeBluetoothGattDiscoverServices(JNIEnv* env);

  // Records that Java FakeBluetoothGatt setCharacteristicNotification was
  // called.
  void OnFakeBluetoothGattSetCharacteristicNotification(JNIEnv* env);

  // Records that Java FakeBluetoothGatt readCharacteristic was called.
  void OnFakeBluetoothGattReadCharacteristic(JNIEnv* env);

  // Records that Java FakeBluetoothGatt writeCharacteristic was called.
  void OnFakeBluetoothGattWriteCharacteristic(
      JNIEnv* env,
      const base::android::JavaParamRef<jbyteArray>& value);

  // Records that Java FakeBluetoothGatt readDescriptor was called.
  void OnFakeBluetoothGattReadDescriptor(JNIEnv* env);

  // Records that Java FakeBluetoothGatt writeDescriptor was called.
  void OnFakeBluetoothGattWriteDescriptor(
      JNIEnv* env,
      const base::android::JavaParamRef<jbyteArray>& value);

  // Records that Java FakeBluetoothAdapter onAdapterStateChanged was called.
  void OnFakeAdapterStateChanged(JNIEnv* env, const bool powered);

  // Posts a task to be run on the current message loop.
  void PostTaskFromJava(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& runnable);

  base::android::ScopedJavaGlobalRef<jobject> j_fake_bluetooth_adapter_;

  int gatt_open_connections_ = 0;
  raw_ptr<BluetoothRemoteGattDescriptor> remembered_ccc_descriptor_ = nullptr;
};

// Defines common test fixture name. Use TEST_F(BluetoothTest, YourTestName).
typedef BluetoothTestAndroid BluetoothTest;

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_ANDROID_H_
