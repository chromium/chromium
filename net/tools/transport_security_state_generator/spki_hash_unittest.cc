// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/tools/transport_security_state_generator/spki_hash.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::transport_security_state {

namespace {

TEST(SPKIHashTest, FromString) {
  SPKIHash hash;

  // Valid SHA256.
  EXPECT_TRUE(
      hash.FromString("sha256/1111111111111111111111111111111111111111111="));
  std::vector<uint8_t> hash_vector(hash.data(), hash.data() + hash.size());
  EXPECT_THAT(
      hash_vector,
      testing::ElementsAreArray(
          {0xD7, 0x5D, 0x75, 0xD7, 0x5D, 0x75, 0xD7, 0x5D, 0x75, 0xD7, 0x5D,
           0x75, 0xD7, 0x5D, 0x75, 0xD7, 0x5D, 0x75, 0xD7, 0x5D, 0x75, 0xD7,
           0x5D, 0x75, 0xD7, 0x5D, 0x75, 0xD7, 0x5D, 0x75, 0xD7, 0x5D}));

  SPKIHash hash2;
  EXPECT_TRUE(
      hash2.FromString("sha256/4osU79hfY3P2+WJGlT2mxmSL+5FIwLEVxTQcavyBNgQ="));
  std::vector<uint8_t> hash_vector2(hash2.data(), hash2.data() + hash2.size());
  EXPECT_THAT(
      hash_vector2,
      testing::ElementsAreArray(
          {0xE2, 0x8B, 0x14, 0xEF, 0xD8, 0x5F, 0x63, 0x73, 0xF6, 0xF9, 0x62,
           0x46, 0x95, 0x3D, 0XA6, 0xC6, 0x64, 0x8B, 0xFB, 0x91, 0x48, 0xC0,
           0xB1, 0x15, 0xC5, 0x34, 0x1C, 0x6A, 0xFC, 0x81, 0x36, 0x04}));

  SPKIHash hash3;

  // Valid SHA1 should be rejected.
  EXPECT_FALSE(hash3.FromString("sha1/111111111111111111111111111="));
  EXPECT_FALSE(hash3.FromString("sha1/gzF+YoVCU9bXeDGQ7JGQVumRueM="));

  // SHA1 disguised as SHA256.
  EXPECT_FALSE(hash3.FromString("sha256/111111111111111111111111111="));

  // SHA512 disguised as SHA256.
  EXPECT_FALSE(
      hash3.FromString("sha256/ns3smS51SK/4P7uSVhSlCIMNAxkD+r6C/ZZA/"
                       "07vac0uyMdRS4jKfqlvk3XxLFP1v5aMIxM5cdTM7FHNwxagQg=="));

  // Invalid BASE64.
  EXPECT_FALSE(hash3.FromString("sha256/hsts-preload"));
  EXPECT_FALSE(hash3.FromString("sha256/1. 2. 3. security!="));
}

}  // namespace

}  // namespace net::transport_security_state
