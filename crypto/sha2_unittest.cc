// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/sha2.h"

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "testing/gtest/include/gtest/gtest.h"

TEST(Sha256Test, Empty) {
  const auto hash = crypto::SHA256Hash(base::span<const uint8_t>());
  const auto expected = std::to_array<uint8_t>({
      0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4,
      0xc8, 0x99, 0x6f, 0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b,
      0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55,
  });

  static_assert(hash.size() == expected.size());
  EXPECT_EQ(base::as_byte_span(expected), base::as_byte_span(hash));
}

TEST(Sha256Test, Test1) {
  // Example B.1 from FIPS 180-2: one-block message.
  const std::string input = "abc";
  const auto hash = crypto::SHA256Hash(base::as_byte_span(input));
  const auto expected = std::to_array<uint8_t>({
      0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
      0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
      0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad,
  });

  static_assert(hash.size() == expected.size());
  EXPECT_EQ(base::as_byte_span(expected), base::as_byte_span(hash));
}

TEST(Sha256Test, Test1_String) {
  // Same as the above, but using the wrapper that returns a std::string.
  // Example B.1 from FIPS 180-2: one-block message.
  const std::string input = "abc";
  const std::string hash = crypto::SHA256HashString(input);
  const auto expected = std::to_array<uint8_t>({
      0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
      0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
      0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad,
  });

  ASSERT_EQ(crypto::kSHA256Length, hash.size());
  EXPECT_EQ(base::as_byte_span(expected), base::as_byte_span(hash));
}

TEST(Sha256Test, Test2) {
  // Example B.2 from FIPS 180-2: multi-block message.
  const std::string input2 =
      "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  const auto hash = crypto::SHA256Hash(base::as_byte_span(input2));
  const auto expected = std::to_array<uint8_t>({
      0x24, 0x8d, 0x6a, 0x61, 0xd2, 0x06, 0x38, 0xb8, 0xe5, 0xc0, 0x26,
      0x93, 0x0c, 0x3e, 0x60, 0x39, 0xa3, 0x3c, 0xe4, 0x59, 0x64, 0xff,
      0x21, 0x67, 0xf6, 0xec, 0xed, 0xd4, 0x19, 0xdb, 0x06, 0xc1,
  });

  static_assert(expected.size() == hash.size());
  EXPECT_EQ(base::as_byte_span(expected), base::as_byte_span(hash));
}

TEST(Sha256Test, Test3) {
  // Example B.3 from FIPS 180-2: long message.
  const std::string input3(1000000, 'a');  // 'a' repeated a million times
  const auto hash = crypto::SHA256Hash(base::as_byte_span(input3));
  const auto expected = std::to_array<uint8_t>({
      0xcd, 0xc7, 0x6e, 0x5c, 0x99, 0x14, 0xfb, 0x92, 0x81, 0xa1, 0xc7,
      0xe2, 0x84, 0xd7, 0x3e, 0x67, 0xf1, 0x80, 0x9a, 0x48, 0xa4, 0x97,
      0x20, 0x0e, 0x04, 0x6d, 0x39, 0xcc, 0xc7, 0x11, 0x2c, 0xd0,
  });

  static_assert(expected.size() == hash.size());
  EXPECT_EQ(base::as_byte_span(expected), base::as_byte_span(hash));
}
