// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/bluetooth_gatt_server_test.h"

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/bluetooth_test.h"

namespace device {

BluetoothGattServerTest::BluetoothGattServerTest() = default;

BluetoothGattServerTest::~BluetoothGattServerTest() = default;

void BluetoothGattServerTest::StartGattSetup() {
  service_ = adapter_->CreateLocalGattService(
      BluetoothUUID(kTestUUIDGenericAttribute), true, delegate_.get());
  delegate_->set_expected_service(service_.get());
}

void BluetoothGattServerTest::CompleteGattSetup() {
  delegate_->set_expected_service(nullptr);
  service_->Register(GetCallback(Call::EXPECTED),
                     GetGattErrorCallback(Call::NOT_EXPECTED));
}

void BluetoothGattServerTest::SetUp() {
  BluetoothTest::SetUp();
  last_read_value_ = std::vector<uint8_t>();
  InitWithFakeAdapter();
  delegate_ = std::make_unique<TestBluetoothLocalGattServiceDelegate>();
}

void BluetoothGattServerTest::TearDown() {
  if (service_.get())
    service_->Delete();
  delegate_.reset();
  BluetoothTest::TearDown();
}

// static
uint64_t BluetoothGattServerTest::GetInteger(
    const std::vector<uint8_t>& value) {
  // Handling only up to 4 bytes value for tests.
  CHECK_LE(value.size(), 4u);
  uint64_t int_value = 0;
  uint64_t powers_of_256 = 1;
  for (uint8_t v : value) {
    int_value += v * powers_of_256;
    powers_of_256 *= 256;
  }
  return int_value;
}

// static
std::vector<uint8_t> BluetoothGattServerTest::GetValue(uint64_t int_value) {
  CHECK_LE(int_value, 0xFFFFFFFFul);
  std::vector<uint8_t> value;
  while (int_value) {
    value.push_back(int_value & 0xff);
    int_value >>= 8;
  }
  return value;
}

}  // namespace device
