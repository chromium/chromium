// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/obsolete/sha1.h"

#include "testing/gtest/include/gtest/gtest.h"

using Sha1Test = ::testing::Test;

namespace crypto::obsolete {

TEST_F(Sha1Test, KnownAnswer) {
  const std::string input = "The quick brown fox jumps over the lazy dog";
  // clang-format off
 constexpr auto expected = std::to_array<uint8_t>({
   0x2f, 0xd4, 0xe1, 0xc6, 0x7a, 0x2d, 0x28, 0xfc,
   0xed, 0x84, 0x9e, 0xe1, 0xbb, 0x76, 0xe7, 0x39,
   0x1b, 0x93, 0xeb, 0x12,
 });
 // clang-format on

  EXPECT_EQ(expected, crypto::obsolete::Sha1::Hash(base::as_byte_span(input)));
}

}  // namespace crypto::obsolete
