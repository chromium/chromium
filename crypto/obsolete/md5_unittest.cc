// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/obsolete/md5.h"

#include "testing/gtest/include/gtest/gtest.h"

using Md5Test = ::testing::Test;

namespace crypto::obsolete {

TEST_F(Md5Test, KnownAnswer) {
  const std::string input = "The quick brown fox jumps over the lazy dog";
  // clang-format off
  constexpr auto expected = std::to_array<uint8_t>({
    0x9e, 0x10, 0x7d, 0x9d, 0x37, 0x2b, 0xb6, 0x82,
    0x6b, 0xd8, 0x1d, 0x35, 0x42, 0xa4, 0x19, 0xd6,
  });
  // clang-format on

  EXPECT_EQ(expected, crypto::obsolete::Md5::Hash(base::as_byte_span(input)));
}

}  // namespace crypto::obsolete
