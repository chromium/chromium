// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/bluetooth/bluetooth_uuid.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_unsignedlong.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(BluetoothUUIDTest, GetBluetoothUUIDFromV8Value_CanonicalUUID) {
  const String expected_uuid("9260c06d-a6d7-4a0f-9817-0b0d5556461f");
  V8UnionStringOrUnsignedLong v8_uuid(expected_uuid);
  String uuid = GetBluetoothUUIDFromV8Value(&v8_uuid);
  EXPECT_EQ(uuid, expected_uuid);
}

TEST(BluetoothUUIDTest, GetBluetoothUUIDFromV8Value_16bitUUID) {
  const String expected_uuid("00001101-0000-1000-8000-00805f9b34fb");
  V8UnionStringOrUnsignedLong v8_uuid(0x1101);
  String uuid = GetBluetoothUUIDFromV8Value(&v8_uuid);
  EXPECT_EQ(uuid, expected_uuid);
}

TEST(BluetoothUUIDTest, GetBluetoothUUIDFromV8Value_EmptyString) {
  V8UnionStringOrUnsignedLong v8_uuid("");
  String uuid = GetBluetoothUUIDFromV8Value(&v8_uuid);
  EXPECT_TRUE(uuid.empty());
}

TEST(BluetoothUUIDTest, GetBluetoothUUIDFromV8Value_BluetoothName) {
  // GetBluetoothUUIDFromV8Value doesn't support UUID names - verify that.
  V8UnionStringOrUnsignedLong v8_uuid("height");
  String uuid = GetBluetoothUUIDFromV8Value(&v8_uuid);
  EXPECT_TRUE(uuid.empty());
}

TEST(BluetoothUUIDTest, GetBluetoothUUIDFromV8Value_InvalidUUID) {
  V8UnionStringOrUnsignedLong v8_uuid("00000000-0000-0000-0000-000000000000-X");
  String uuid = GetBluetoothUUIDFromV8Value(&v8_uuid);
  EXPECT_TRUE(uuid.empty());
}

}  // namespace blink
