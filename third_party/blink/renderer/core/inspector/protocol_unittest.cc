// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace protocol {
namespace {
TEST(ProtocolBinaryTest, base64EmptyArgs) {
  EXPECT_EQ(protocol::String(), Binary().toBase64());

  bool success = false;
  Binary decoded = Binary::fromBase64("", &success);
  EXPECT_TRUE(success);
  Vector<uint8_t> decoded_bytes;
  decoded_bytes.Append(decoded.data(), decoded.size());
  EXPECT_EQ(Vector<uint8_t>(), decoded_bytes);
}

TEST(ProtocolStringTest, AllBytesBase64Roundtrip) {
  Vector<uint8_t> all_bytes;
  for (int ii = 0; ii < 255; ++ii)
    all_bytes.push_back(ii);
  Binary binary = Binary::fromVector(all_bytes);
  bool success = false;
  Binary decoded = Binary::fromBase64(binary.toBase64(), &success);
  EXPECT_TRUE(success);
  Vector<uint8_t> decoded_bytes;
  decoded_bytes.Append(decoded.data(), decoded.size());
  EXPECT_EQ(all_bytes, decoded_bytes);
}

TEST(ProtocolStringTest, HelloWorldBase64Roundtrip) {
  const char* kMsg = "Hello, world.";
  Vector<uint8_t> msg;
  msg.Append(reinterpret_cast<const uint8_t*>(kMsg), strlen(kMsg));
  EXPECT_EQ(strlen(kMsg), msg.size());

  protocol::String encoded = Binary::fromVector(msg).toBase64();
  EXPECT_EQ("SGVsbG8sIHdvcmxkLg==", encoded);
  bool success = false;
  Binary decoded_binary = Binary::fromBase64(encoded, &success);
  EXPECT_TRUE(success);
  Vector<uint8_t> decoded;
  decoded.Append(decoded_binary.data(), decoded_binary.size());
  EXPECT_EQ(msg, decoded);
}

TEST(ProtocolBinaryTest, InvalidBase64Decode) {
  bool success = true;
  Binary binary = Binary::fromBase64("This is not base64.", &success);
  EXPECT_FALSE(success);
}
}  // namespace
}  // namespace protocol
}  // namespace blink
