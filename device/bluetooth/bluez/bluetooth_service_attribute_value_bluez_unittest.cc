// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_service_attribute_value_bluez.h"

#include <cstdint>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bluez {

namespace {

using Type = BluetoothServiceAttributeValueBlueZ::Type;
using Sequence = BluetoothServiceAttributeValueBlueZ::Sequence;

constexpr char kServiceUuid[] = "00001801-0000-1000-8000-00805f9b34fb";

void CheckUuidValue(const BluetoothServiceAttributeValueBlueZ& value,
                    const std::string& uuid) {
  EXPECT_EQ(Type::UUID, value.type());
  EXPECT_TRUE(value.value().is_string());
  EXPECT_EQ(uuid, value.value().GetString());
}

void CheckIntValue(const BluetoothServiceAttributeValueBlueZ& value,
                   uint32_t val) {
  EXPECT_EQ(Type::INT, value.type());
  EXPECT_TRUE(value.value().is_int());
  EXPECT_EQ(val, static_cast<uint32_t>(value.value().GetInt()));
}

// MakeUnique can't use a initializer list directly, since it can't derive the
// template type for it unless we explicitly use std::initializer_list({...})
// in the call. This function helps us avoid all that ugly syntax.
std::unique_ptr<Sequence> MakeSequence(
    const std::initializer_list<BluetoothServiceAttributeValueBlueZ> list) {
  return std::make_unique<Sequence>(list);
}

}  // namespace

TEST(BluetoothServiceAttributeBlueZTest, BasicValue) {
  BluetoothServiceAttributeValueBlueZ value1(Type::UUID, 16,
                                             base::Value(kServiceUuid));
  BluetoothServiceAttributeValueBlueZ value2 = value1;

  CheckUuidValue(value2, kServiceUuid);
}

TEST(BluetoothServiceAttributeBlueZTest, Sequence) {
  BluetoothServiceAttributeValueBlueZ value1(Type::UUID, 16,
                                             base::Value(kServiceUuid));
  BluetoothServiceAttributeValueBlueZ value2(Type::INT, 4, base::Value(0x1337));
  BluetoothServiceAttributeValueBlueZ value3(Type::INT, 4, base::Value(0x7331));

  BluetoothServiceAttributeValueBlueZ* value4 =
      new BluetoothServiceAttributeValueBlueZ(
          MakeSequence({value1, value2, value3}));

  BluetoothServiceAttributeValueBlueZ value = *value4;
  delete value4;

  EXPECT_EQ(3u, value.size());
  const Sequence& s = value.sequence();
  EXPECT_EQ(3u, s.size());

  CheckUuidValue(s[0], kServiceUuid);
  CheckIntValue(s[2], 0x7331);  // Checking out of order by intention.
  CheckIntValue(s[1], 0x1337);
}

TEST(BluetoothServiceAttributeBlueZTest, NestedValue) {
  BluetoothServiceAttributeValueBlueZ value1(Type::UUID, 16,
                                             base::Value(kServiceUuid));
  BluetoothServiceAttributeValueBlueZ value2(Type::INT, 4, base::Value(0x1337));
  BluetoothServiceAttributeValueBlueZ value3(MakeSequence({value1, value2}));
  BluetoothServiceAttributeValueBlueZ value4(MakeSequence({value3}));

  BluetoothServiceAttributeValueBlueZ* value5 =
      new BluetoothServiceAttributeValueBlueZ(
          MakeSequence({value1, value2, value3, value4}));
  BluetoothServiceAttributeValueBlueZ value = *value5;
  delete value5;

  // Check outer layer first.
  EXPECT_EQ(4u, value.size());
  const Sequence& v5 = value.sequence();
  EXPECT_EQ(4u, v5.size());
  CheckUuidValue(v5[0], kServiceUuid);
  CheckIntValue(v5[1], 0x1337);

  // Check value3.
  EXPECT_EQ(2u, v5[2].size());
  EXPECT_EQ(Type::SEQUENCE, v5[2].type());
  const Sequence& v3 = v5[2].sequence();
  EXPECT_EQ(2u, v3.size());
  CheckUuidValue(v3[0], kServiceUuid);
  CheckIntValue(v3[1], 0x1337);

  // Check value4 now.
  EXPECT_EQ(1u, v5[3].size());
  EXPECT_EQ(Type::SEQUENCE, v5[3].type());
  const Sequence& v4 = v5[3].sequence();
  EXPECT_EQ(1u, v4.size());

  // Check value3 again.
  EXPECT_EQ(2u, v4[0].size());
  EXPECT_EQ(Type::SEQUENCE, v4[0].type());
  const Sequence& v31 = v4[0].sequence();
  EXPECT_EQ(2u, v31.size());
  CheckUuidValue(v31[0], kServiceUuid);
  CheckIntValue(v31[1], 0x1337);
}

TEST(BluetoothServiceAttributeBlueZTest, CopyAssignment) {
  BluetoothServiceAttributeValueBlueZ value1(Type::UUID, 16,
                                             base::Value(kServiceUuid));
  BluetoothServiceAttributeValueBlueZ value2(Type::INT, 4, base::Value(0x1337));
  BluetoothServiceAttributeValueBlueZ value3(Type::INT, 4, base::Value(0x7331));
  std::unique_ptr<BluetoothServiceAttributeValueBlueZ> value4(
      new BluetoothServiceAttributeValueBlueZ(
          MakeSequence({value1, value2, value3})));

  BluetoothServiceAttributeValueBlueZ value;

  value = *value4;
  value4 = nullptr;

  EXPECT_EQ(3u, value.size());
  const Sequence& s = value.sequence();
  EXPECT_EQ(3u, s.size());

  CheckUuidValue(s[0], kServiceUuid);
  CheckIntValue(s[2], 0x7331);
  CheckIntValue(s[1], 0x1337);
}

}  // namespace bluez
