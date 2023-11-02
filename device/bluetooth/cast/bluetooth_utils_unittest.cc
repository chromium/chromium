// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/cast/bluetooth_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace device {

TEST(BluetoothUtilsTest, TestGetCanonicalBluetoothAddress) {
  // Test that the correct canonical address is returned for a variety of
  // addresses.
  ASSERT_EQ("AA:BB:CC:DD:EE:FF", GetCanonicalBluetoothAddress(
                                     {{0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa}}));
  ASSERT_EQ("44:55:66:77:88:99", GetCanonicalBluetoothAddress(
                                     {{0x99, 0x88, 0x77, 0x66, 0x55, 0x44}}));
  ASSERT_EQ("00:00:00:00:00:00", GetCanonicalBluetoothAddress(
                                     {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}));
}

TEST(BluetoothUtilsTest, TestUuidToBluetoothUUID_128bit) {
  // Test a 128-bit UUID.
  BluetoothUUID uuid =
      UuidToBluetoothUUID({{0x12, 0x3e, 0x45, 0x67, 0xe8, 0x9b, 0x12, 0xd3,
                            0xa4, 0x56, 0x42, 0x66, 0x55, 0x44, 0x00, 0x00}});
  ASSERT_TRUE(uuid.IsValid());
  ASSERT_EQ(BluetoothUUID::kFormat128Bit, uuid.format());
  ASSERT_EQ("123e4567-e89b-12d3-a456-426655440000", uuid.value());
  ASSERT_EQ("123e4567-e89b-12d3-a456-426655440000", uuid.canonical_value());
}

TEST(BluetoothUtilsTest, TestUuidToBluetoothUUID_16bit) {
  // Test a 16-bit UUID. Note that since chromecast::bluetooth_v2_shlib::Uuid
  // always has 128 bits, the underlying value of every BluetoothUUID returned
  // from this function will look like "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx".
  // For information on how a 16-bit UUID is represented as a 128-bit UUID, see
  // http://www.argenox.com/a-ble-advertising-primer.

  // Get BluetoothUUID for 0xFE34.
  BluetoothUUID uuid =
      UuidToBluetoothUUID({{0x00, 0x00, 0xfe, 0x34, 0x00, 0x00, 0x10, 0x00,
                            0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB}});
  ASSERT_TRUE(uuid.IsValid());
  ASSERT_EQ(BluetoothUUID::kFormat128Bit, uuid.format());
  ASSERT_EQ("0000fe34-0000-1000-8000-00805f9b34fb", uuid.value());
  ASSERT_EQ("0000fe34-0000-1000-8000-00805f9b34fb", uuid.canonical_value());
}

TEST(BluetoothUtilsTest, TestGetCanonicalBluetoothUuid) {
  std::string uuid = GetCanonicalBluetoothUuid(
      {{0x12, 0x3e, 0x45, 0x67, 0xe8, 0x9b, 0x12, 0xd3, 0xa4, 0x56, 0x42, 0x66,
        0x55, 0x44, 0x00, 0x00}});
  ASSERT_EQ("123e4567-e89b-12d3-a456-426655440000", uuid);
}

}  // namespace device
