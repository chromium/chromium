// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_TEST_BLUETOOTH_LOCAL_GATT_SERVICE_DELEGATE_H_
#define DEVICE_BLUETOOTH_TEST_TEST_BLUETOOTH_LOCAL_GATT_SERVICE_DELEGATE_H_

#include <cstdint>
#include <map>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"

namespace device {

class TestBluetoothLocalGattServiceDelegate
    : public BluetoothLocalGattService::Delegate {
 public:
  TestBluetoothLocalGattServiceDelegate();

  TestBluetoothLocalGattServiceDelegate(
      const TestBluetoothLocalGattServiceDelegate&) = delete;
  TestBluetoothLocalGattServiceDelegate& operator=(
      const TestBluetoothLocalGattServiceDelegate&) = delete;

  virtual ~TestBluetoothLocalGattServiceDelegate();

  // BluetoothLocalGattService::Delegate overrides:
  void OnCharacteristicReadRequest(
      const BluetoothDevice* device,
      const BluetoothLocalGattCharacteristic* characteristic,
      int offset,
      ValueCallback callback) override;
  void OnCharacteristicWriteRequest(
      const BluetoothDevice* device,
      const BluetoothLocalGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value,
      int offset,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;
  void OnCharacteristicPrepareWriteRequest(
      const BluetoothDevice* device,
      const BluetoothLocalGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value,
      int offset,
      bool has_subsequent_request,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;
  void OnDescriptorReadRequest(const BluetoothDevice* device,
                               const BluetoothLocalGattDescriptor* descriptor,
                               int offset,
                               ValueCallback callback) override;

  void OnDescriptorWriteRequest(const BluetoothDevice* device,
                                const BluetoothLocalGattDescriptor* descriptor,
                                const std::vector<uint8_t>& value,
                                int offset,
                                base::OnceClosure callback,
                                ErrorCallback error_callback) override;
  void OnNotificationsStart(
      const BluetoothDevice* device,
      device::BluetoothGattCharacteristic::NotificationType notification_type,
      const BluetoothLocalGattCharacteristic* characteristic) override;
  void OnNotificationsStop(
      const BluetoothDevice* device,
      const BluetoothLocalGattCharacteristic* characteristic) override;

  bool NotificationStatusForCharacteristic(
      BluetoothLocalGattCharacteristic* characteristic);

  void set_expected_service(BluetoothLocalGattService* service) {
    expected_service_ = service;
  }

  void set_expected_characteristic(
      BluetoothLocalGattCharacteristic* characteristic) {
    expected_characteristic_ = characteristic;
  }

  void set_expected_descriptor(BluetoothLocalGattDescriptor* descriptor) {
    expected_descriptor_ = descriptor;
  }

  bool should_fail_;
  uint64_t last_written_value_;
  uint64_t value_to_write_;
  std::string last_seen_device_;

 private:
  raw_ptr<BluetoothLocalGattService> expected_service_;
  raw_ptr<BluetoothLocalGattCharacteristic, DanglingUntriaged>
      expected_characteristic_;
  raw_ptr<BluetoothLocalGattDescriptor, DanglingUntriaged> expected_descriptor_;

  std::map<std::string, bool> notifications_started_for_characteristic_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_TEST_BLUETOOTH_LOCAL_GATT_SERVICE_DELEGATE_H_
