// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::WebBluetoothDeviceId;

namespace {

const char kValidDeviceId1[] = "123456789012345678901A==";
const char kValidDeviceId2[] = "AbCdEfGhIjKlMnOpQrS+/Q==";
const char kInvalidLongDeviceId[] = "12345678901234567890123=";
const char kInvalidShortDeviceId[] = "12345678901234567890";
const char kInvalidCharacterDeviceId[] = "123456789012345678901*==";
// A base64 string should have a length of a multiple of 4.
const char kInvalidLengthDeviceId[] = "123456789012345678901";

}  // namespace

TEST(WebBluetoothDeviceIdTest, DefaultConstructor) {
  WebBluetoothDeviceId default_id1;
  WebBluetoothDeviceId default_id2;
  WebBluetoothDeviceId valid_id(kValidDeviceId1);

  ASSERT_DEATH_IF_SUPPORTED(default_id1.str(), "");
  ASSERT_DEATH_IF_SUPPORTED(default_id2.str(), "");
  ASSERT_TRUE(WebBluetoothDeviceId::IsValid(valid_id.str()));

  EXPECT_DEATH_IF_SUPPORTED([&]() { return default_id1 == default_id2; }(), "");
  EXPECT_DEATH_IF_SUPPORTED([&]() { return default_id1 != default_id2; }(), "");

  EXPECT_DEATH_IF_SUPPORTED([&]() { return default_id1 == valid_id; }(), "");
  EXPECT_DEATH_IF_SUPPORTED([&]() { return valid_id == default_id1; }(), "");

  EXPECT_DEATH_IF_SUPPORTED([&]() { return default_id1 != valid_id; }(), "");
  EXPECT_DEATH_IF_SUPPORTED([&]() { return valid_id != default_id1; }(), "");
}

TEST(WebBluetoothDeviceIdTest, StrConstructor) {
  WebBluetoothDeviceId valid1(kValidDeviceId1);
  WebBluetoothDeviceId valid2(kValidDeviceId2);

  EXPECT_TRUE(valid1 == valid1);
  EXPECT_TRUE(valid2 == valid2);

  EXPECT_TRUE(valid1 != valid2);

  EXPECT_DEATH_IF_SUPPORTED(WebBluetoothDeviceId(""), "");
  EXPECT_DEATH_IF_SUPPORTED(
      [&]() { return WebBluetoothDeviceId(kInvalidLongDeviceId); }(), "");
  EXPECT_DEATH_IF_SUPPORTED(
      [&]() { return WebBluetoothDeviceId(kInvalidShortDeviceId); }(), "");
  EXPECT_DEATH_IF_SUPPORTED(
      [&]() { return WebBluetoothDeviceId(kInvalidCharacterDeviceId); }(), "");
  EXPECT_DEATH_IF_SUPPORTED(
      [&]() { return WebBluetoothDeviceId(kInvalidLengthDeviceId); }(), "");
}

TEST(WebBluetoothDeviceIdTest, IsValid_Valid) {
  EXPECT_TRUE(WebBluetoothDeviceId::IsValid(kValidDeviceId1));
  EXPECT_TRUE(WebBluetoothDeviceId::IsValid(kValidDeviceId2));
}

TEST(WebBluetoothDeviceIdTest, IsValid_Invalid) {
  EXPECT_FALSE(WebBluetoothDeviceId::IsValid(""));
  EXPECT_FALSE(WebBluetoothDeviceId::IsValid(kInvalidLongDeviceId));
  EXPECT_FALSE(WebBluetoothDeviceId::IsValid(kInvalidShortDeviceId));
  EXPECT_FALSE(WebBluetoothDeviceId::IsValid(kInvalidCharacterDeviceId));
  EXPECT_FALSE(WebBluetoothDeviceId::IsValid(kInvalidLengthDeviceId));
}

TEST(WebBluetoothDeviceIdTest, Create) {
  // Tests that Create generates a valid Device Id.
  EXPECT_TRUE(
      WebBluetoothDeviceId::IsValid(WebBluetoothDeviceId::Create().str()));
}
