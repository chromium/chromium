// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <vector>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/inspector_protocol/crdtp/chromium/protocol_traits.h"

namespace content {
namespace protocol {
namespace {

using crdtp::Binary;

TEST(ProtocolBinaryTest, base64EmptyArgs) {
  EXPECT_EQ(std::string(), Binary().toBase64());

  bool success = false;
  Binary decoded = Binary::fromBase64("", &success);
  EXPECT_TRUE(success);
  EXPECT_EQ(
      std::vector<uint8_t>(),
      std::vector<uint8_t>(decoded.data(), decoded.data() + decoded.size()));
}

TEST(ProtocolStringTest, AllBytesBase64Roundtrip) {
  std::vector<uint8_t> all_bytes;
  for (int ii = 0; ii < 255; ++ii)
    all_bytes.push_back(ii);
  Binary binary = Binary::fromVector(all_bytes);
  bool success = false;
  Binary decoded = Binary::fromBase64(binary.toBase64(), &success);
  EXPECT_TRUE(success);
  std::vector<uint8_t> decoded_bytes(decoded.data(),
                                     decoded.data() + decoded.size());
  EXPECT_EQ(all_bytes, decoded_bytes);
}

TEST(ProtocolStringTest, HelloWorldBase64Roundtrip) {
  const char* kMsg = "Hello, world.";
  std::vector<uint8_t> msg(kMsg, kMsg + strlen(kMsg));
  EXPECT_EQ(strlen(kMsg), msg.size());

  auto encoded = Binary::fromVector(msg).toBase64();
  EXPECT_EQ("SGVsbG8sIHdvcmxkLg==", encoded);
  bool success = false;
  Binary decoded_binary = Binary::fromBase64(encoded, &success);
  EXPECT_TRUE(success);
  std::vector<uint8_t> decoded(decoded_binary.data(),
                               decoded_binary.data() + decoded_binary.size());
  EXPECT_EQ(msg, decoded);
}

TEST(ProtocolBinaryTest, InvalidBase64Decode) {
  bool success = true;
  Binary binary = Binary::fromBase64("This is not base64.", &success);
  EXPECT_FALSE(success);
}
}  // namespace
}  // namespace protocol
}  // namespace content
