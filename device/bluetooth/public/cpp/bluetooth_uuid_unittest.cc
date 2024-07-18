// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

#include <stddef.h>

#include <sstream>

#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <rpc.h>
#endif

namespace device {

TEST(BluetoothUUIDTest, BluetoothUUID) {
  const char kValid128Bit0[] = "12345678-1234-5678-9abc-def123456789";
  const char kValid128Bit1[] = "00001101-0000-1000-8000-00805f9b34fb";
  const char kInvalid36Char0[] = "1234567-1234-5678-9abc-def123456789";
  const char kInvalid36Char1[] = "0x00001101-0000-1000-8000-00805f9b34fb";
  const char kInvalid4Char[] = "Z101";
  const char kValid16Bit[] = "0x1101";
  const char kValid32Bit[] = "00001101";
  const std::vector<uint8_t> kValid128BitInBytes0 = {
      0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x56, 0x78,
      0x9a, 0xbc, 0xde, 0xf1, 0x23, 0x45, 0x67, 0x89};
  const std::vector<uint8_t> kValid128BitInBytes1 = {
      0x00, 0x00, 0x11, 0x01, 0x00, 0x00, 0x10, 0x00,
      0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb};
  const std::vector<uint8_t> kInvalid128BitInBytes0 = {
      0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x56, 0x78,
      0x9a, 0xbc, 0xde, 0xf1, 0x23, 0x45, 0x67,
  };
  const std::vector<uint8_t> kInvalid128BitInBytes1 = {
      0x00, 0x00, 0x11, 0x01, 0x00, 0x00, 0x10, 0x00, 0x80,
      0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb, 0x00};

  // Valid 128-bit custom UUID.
  BluetoothUUID uuid0(kValid128Bit0);
  EXPECT_TRUE(uuid0.IsValid());
  EXPECT_EQ(BluetoothUUID::kFormat128Bit, uuid0.format());
  EXPECT_EQ(uuid0.value(), uuid0.canonical_value());
  EXPECT_THAT(uuid0.GetBytes(),
              ::testing::ElementsAreArray(kValid128BitInBytes0));

  // Valid 128-bit UUID.
  BluetoothUUID uuid1(kValid128Bit1);
  EXPECT_TRUE(uuid1.IsValid());
  EXPECT_EQ(BluetoothUUID::kFormat128Bit, uuid1.format());
  EXPECT_EQ(uuid1.value(), uuid1.canonical_value());
  EXPECT_THAT(uuid1.GetBytes(),
              ::testing::ElementsAreArray(kValid128BitInBytes1));

  EXPECT_NE(uuid0, uuid1);

  // Invalid 128-bit UUID.
  BluetoothUUID uuid2(kInvalid36Char0);
  EXPECT_FALSE(uuid2.IsValid());
  EXPECT_EQ(BluetoothUUID::kFormatInvalid, uuid2.format());
  EXPECT_TRUE(uuid2.value().empty());
  EXPECT_TRUE(uuid2.canonical_value().empty());
  EXPECT_THAT(uuid2.GetBytes(), ::testing::ElementsAre());

  // Invalid 128-bit UUID.
  BluetoothUUID uuid3(kInvalid36Char1);
  EXPECT_FALSE(uuid3.IsValid());
  EXPECT_EQ(BluetoothUUID::kFormatInvalid, uuid3.format());
  EXPECT_TRUE(uuid3.value().empty());
  EXPECT_TRUE(uuid3.canonical_value().empty());
  EXPECT_THAT(uuid3.GetBytes(), ::testing::ElementsAre());

  // Invalid 16-bit UUID.
  BluetoothUUID uuid4(kInvalid4Char);
  EXPECT_FALSE(uuid4.IsValid());
  EXPECT_EQ(BluetoothUUID::kFormatInvalid, uuid4.format());
  EXPECT_TRUE(uuid4.value().empty());
  EXPECT_TRUE(uuid4.canonical_value().empty());
  EXPECT_THAT(uuid4.GetBytes(), ::testing::ElementsAre());

  // Valid 16-bit UUID.
  BluetoothUUID uuid5(kValid16Bit);
  EXPECT_TRUE(uuid5.IsValid());
  EXPECT_EQ(BluetoothUUID::kFormat16Bit, uuid5.format());
  EXPECT_NE(uuid5.value(), uuid5.canonical_value());
  EXPECT_EQ("1101", uuid5.value());
  EXPECT_EQ(kValid128Bit1, uuid5.canonical_value());
  EXPECT_THAT(
      uuid5.GetBytes(),
      ::testing::ElementsAre(0x00, 0x00, 0x11, 0x01, 0x00, 0x00, 0x10, 0x00,
                             0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb));

  // Valid 32-bit UUID.
  BluetoothUUID uuid6(kValid32Bit);
  EXPECT_TRUE(uuid6.IsValid());
  EXPECT_EQ(BluetoothUUID::kFormat32Bit, uuid6.format());
  EXPECT_NE(uuid6.value(), uuid6.canonical_value());
  EXPECT_EQ("00001101", uuid6.value());
  EXPECT_EQ(kValid128Bit1, uuid6.canonical_value());
  EXPECT_THAT(
      uuid6.GetBytes(),
      ::testing::ElementsAre(0x00, 0x00, 0x11, 0x01, 0x00, 0x00, 0x10, 0x00,
                             0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb));

  // uuid5, uuid6, and uuid1 are equivalent.
  EXPECT_EQ(uuid5, uuid6);
  EXPECT_EQ(uuid1, uuid5);
  EXPECT_EQ(uuid1, uuid6);

  // Valid byte array
  BluetoothUUID uuid7(kValid128BitInBytes0);
  EXPECT_EQ(uuid7, uuid0);
  EXPECT_NE(uuid7, uuid1);

  // Valid byte array
  BluetoothUUID uuid8(kValid128BitInBytes1);
  EXPECT_EQ(uuid8, uuid1);
  EXPECT_NE(uuid8, uuid0);

  // Invalid byte array
  BluetoothUUID uuid9(kInvalid128BitInBytes0);
  EXPECT_FALSE(uuid9.IsValid());
  EXPECT_EQ(BluetoothUUID::kFormatInvalid, uuid9.format());
  EXPECT_TRUE(uuid9.value().empty());
  EXPECT_TRUE(uuid9.canonical_value().empty());
  EXPECT_THAT(uuid9.GetBytes(), ::testing::ElementsAre());

  // Invalid byte array
  BluetoothUUID uuid10(kInvalid128BitInBytes1);
  EXPECT_FALSE(uuid10.IsValid());
  EXPECT_EQ(BluetoothUUID::kFormatInvalid, uuid10.format());
  EXPECT_TRUE(uuid10.value().empty());
  EXPECT_TRUE(uuid10.canonical_value().empty());
  EXPECT_THAT(uuid10.GetBytes(), ::testing::ElementsAre());
}

#if BUILDFLAG(IS_WIN)
TEST(BluetoothUUIDTest, BluetoothUUID_GUID) {
  const char kValid128Bit0[] = "12345678-1234-5678-9abc-def123456789";
  GUID guid;
  guid.Data1 = 0x12345678;
  guid.Data2 = 0x1234;
  guid.Data3 = 0x5678;
  guid.Data4[0] = 0x9a;
  guid.Data4[1] = 0xbc;
  guid.Data4[2] = 0xde;
  guid.Data4[3] = 0xf1;
  guid.Data4[4] = 0x23;
  guid.Data4[5] = 0x45;
  guid.Data4[6] = 0x67;
  guid.Data4[7] = 0x89;

  BluetoothUUID uuid(guid);
  EXPECT_TRUE(uuid.IsValid());
  EXPECT_EQ(BluetoothUUID::kFormat128Bit, uuid.format());
  EXPECT_EQ(kValid128Bit0, uuid.value());
  EXPECT_EQ(kValid128Bit0, uuid.canonical_value());
}

TEST(BluetoothUUIDTest, GetCanonicalValueAsGUID) {
  const char kValid128Bit0[] = "12345678-1234-5678-9abc-def123456789";
  GUID guid = BluetoothUUID::GetCanonicalValueAsGUID(kValid128Bit0);

  EXPECT_EQ(0x12345678u, guid.Data1);
  EXPECT_EQ(0x1234, guid.Data2);
  EXPECT_EQ(0x5678, guid.Data3);
  EXPECT_EQ(0x9a, guid.Data4[0]);
  EXPECT_EQ(0xbc, guid.Data4[1]);
  EXPECT_EQ(0xde, guid.Data4[2]);
  EXPECT_EQ(0xf1, guid.Data4[3]);
  EXPECT_EQ(0x23, guid.Data4[4]);
  EXPECT_EQ(0x45, guid.Data4[5]);
  EXPECT_EQ(0x67, guid.Data4[6]);
  EXPECT_EQ(0x89, guid.Data4[7]);
}
#endif  // BUILDFLAG(IS_WIN)

// Verify that UUIDs are parsed case-insensitively
TEST(BluetoothUUIDTest, BluetoothUUID_CaseInsensitive) {
  const char k16Bit[] = "1abc";
  const char k32Bit[] = "00001abc";
  const char k128Bit[] = "00001abc-0000-1000-8000-00805f9b34fb";

  struct TestCase {
    const std::string input_uuid;
    const std::string expected_value;
  } test_cases[] = {
      {"1abc", k16Bit},
      {"1ABC", k16Bit},
      {"1aBc", k16Bit},
      {"00001abc", k32Bit},
      {"00001ABC", k32Bit},
      {"00001aBc", k32Bit},
      {"00001abc-0000-1000-8000-00805f9b34fb", k128Bit},
      {"00001ABC-0000-1000-8000-00805F9B34FB", k128Bit},
      {"00001aBc-0000-1000-8000-00805F9b34fB", k128Bit},
  };

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    SCOPED_TRACE("Input UUID: " + test_cases[i].input_uuid);
    BluetoothUUID uuid(test_cases[i].input_uuid);
    EXPECT_TRUE(uuid.IsValid());
    EXPECT_EQ(test_cases[i].expected_value, uuid.value());
    EXPECT_EQ(k128Bit, uuid.canonical_value());
  }
}

TEST(BluetoothUUIDTest, BluetoothUUID_stream_insertion_operator) {
  const std::string uuid_str("00001abc-0000-1000-8000-00805f9b34fb");

  BluetoothUUID uuid(uuid_str);
  std::ostringstream ss;
  ss << uuid;
  const std::string out = ss.str();

  EXPECT_EQ(uuid_str, out);
}

}  // namespace device
