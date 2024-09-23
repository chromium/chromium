// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_local_gatt_descriptor.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/test/bluetooth_gatt_server_test.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class BluetoothLocalGattDescriptorTest : public BluetoothGattServerTest {
 public:
  void SetUp() override {
    BluetoothGattServerTest::SetUp();

    StartGattSetup();
    // We will need this device to use with simulating read/write attribute
    // value events.
    device_ = SimulateLowEnergyDevice(1);
    characteristic_ = service_->CreateCharacteristic(
        BluetoothUUID(kTestUUIDGenericAttribute),
        device::BluetoothLocalGattCharacteristic::Properties(),
        device::BluetoothLocalGattCharacteristic::Permissions());
    read_descriptor_ = BluetoothLocalGattDescriptor::Create(
        BluetoothUUID(kTestUUIDGenericAttribute),
        device::BluetoothLocalGattCharacteristic::PERMISSION_READ,
        characteristic_.get());
    write_descriptor_ = BluetoothLocalGattDescriptor::Create(
        BluetoothUUID(kTestUUIDGenericAttribute),
        device::BluetoothLocalGattCharacteristic::
            PERMISSION_WRITE_ENCRYPTED_AUTHENTICATED,
        characteristic_.get());
    EXPECT_LT(0u, read_descriptor_->GetIdentifier().size());
    EXPECT_LT(0u, write_descriptor_->GetIdentifier().size());
    CompleteGattSetup();
  }

 protected:
  base::WeakPtr<BluetoothLocalGattCharacteristic> characteristic_;
  base::WeakPtr<BluetoothLocalGattDescriptor> read_descriptor_;
  base::WeakPtr<BluetoothLocalGattDescriptor> write_descriptor_;
  raw_ptr<BluetoothDevice, DanglingUntriaged> device_;
};

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_ReadLocalDescriptorValue ReadLocalDescriptorValue
#else
#define MAYBE_ReadLocalDescriptorValue DISABLED_ReadLocalDescriptorValue
#endif
TEST_F(BluetoothLocalGattDescriptorTest, MAYBE_ReadLocalDescriptorValue) {
  delegate_->value_to_write_ = 0x1337;
  SimulateLocalGattDescriptorValueReadRequest(
      device_, read_descriptor_.get(),
      GetReadValueCallback(Call::EXPECTED, Result::SUCCESS));

  EXPECT_EQ(delegate_->value_to_write_, GetInteger(last_read_value_));
  EXPECT_EQ(device_->GetIdentifier(), delegate_->last_seen_device_);
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_WriteLocalDescriptorValue WriteLocalDescriptorValue
#else
#define MAYBE_WriteLocalDescriptorValue DISABLED_WriteLocalDescriptorValue
#endif
TEST_F(BluetoothLocalGattDescriptorTest, MAYBE_WriteLocalDescriptorValue) {
  const uint64_t kValueToWrite = 0x7331ul;
  SimulateLocalGattDescriptorValueWriteRequest(
      device_, write_descriptor_.get(), GetValue(kValueToWrite),
      GetCallback(Call::EXPECTED), GetCallback(Call::NOT_EXPECTED));

  EXPECT_EQ(kValueToWrite, delegate_->last_written_value_);
  EXPECT_EQ(device_->GetIdentifier(), delegate_->last_seen_device_);
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_ReadLocalDescriptorValueFail ReadLocalDescriptorValueFail
#else
#define MAYBE_ReadLocalDescriptorValueFail DISABLED_ReadLocalDescriptorValueFail
#endif
TEST_F(BluetoothLocalGattDescriptorTest, MAYBE_ReadLocalDescriptorValueFail) {
  delegate_->value_to_write_ = 0x1337;
  delegate_->should_fail_ = true;
  SimulateLocalGattDescriptorValueReadRequest(
      device_, read_descriptor_.get(),
      GetReadValueCallback(Call::EXPECTED, Result::FAILURE));

  EXPECT_NE(delegate_->value_to_write_, GetInteger(last_read_value_));
  EXPECT_NE(device_->GetIdentifier(), delegate_->last_seen_device_);
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_WriteLocalDescriptorValueFail WriteLocalDescriptorValueFail
#else
#define MAYBE_WriteLocalDescriptorValueFail \
  DISABLED_WriteLocalDescriptorValueFail
#endif
TEST_F(BluetoothLocalGattDescriptorTest, MAYBE_WriteLocalDescriptorValueFail) {
  const uint64_t kValueToWrite = 0x7331ul;
  delegate_->should_fail_ = true;
  SimulateLocalGattDescriptorValueWriteRequest(
      device_, write_descriptor_.get(), GetValue(kValueToWrite),
      GetCallback(Call::NOT_EXPECTED), GetCallback(Call::EXPECTED));

  EXPECT_NE(kValueToWrite, delegate_->last_written_value_);
  EXPECT_NE(device_->GetIdentifier(), delegate_->last_seen_device_);
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_ReadLocalDescriptorValueWrongPermissions \
  ReadLocalDescriptorValueWrongPermissions
#else
#define MAYBE_ReadLocalDescriptorValueWrongPermissions \
  DISABLED_ReadLocalDescriptorValueWrongPermissions
#endif
TEST_F(BluetoothLocalGattDescriptorTest,
       MAYBE_ReadLocalDescriptorValueWrongPermissions) {
  delegate_->value_to_write_ = 0x1337;
  SimulateLocalGattDescriptorValueReadRequest(
      device_, write_descriptor_.get(),
      GetReadValueCallback(Call::EXPECTED, Result::FAILURE));

  EXPECT_NE(delegate_->value_to_write_, GetInteger(last_read_value_));
  EXPECT_NE(device_->GetIdentifier(), delegate_->last_seen_device_);
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_WriteLocalDescriptorValueWrongPermissions \
  WriteLocalDescriptorValueWrongPermissions
#else
#define MAYBE_WriteLocalDescriptorValueWrongPermissions \
  DISABLED_WriteLocalDescriptorValueWrongPermissions
#endif
TEST_F(BluetoothLocalGattDescriptorTest,
       MAYBE_WriteLocalDescriptorValueWrongPermissions) {
  const uint64_t kValueToWrite = 0x7331ul;
  SimulateLocalGattDescriptorValueWriteRequest(
      device_, read_descriptor_.get(), GetValue(kValueToWrite),
      GetCallback(Call::NOT_EXPECTED), GetCallback(Call::EXPECTED));

  EXPECT_NE(kValueToWrite, delegate_->last_written_value_);
  EXPECT_NE(device_->GetIdentifier(), delegate_->last_seen_device_);
}

}  // namespace device
