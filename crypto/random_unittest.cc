// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/random.h"

#include <stddef.h>

#include <algorithm>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

// Basic functionality tests. Does NOT test the security of the random data.

// Ensures we don't have all trivial data, i.e. that the data is indeed random.
// Currently, that means the bytes cannot be all the same (e.g. all zeros).
bool IsTrivial(base::span<const uint8_t> bytes) {
  const uint8_t first_byte = bytes.front();
  return std::ranges::all_of(bytes,
                             [=](uint8_t byte) { return byte == first_byte; });
}

TEST(RandBytes, RandBytes) {
  std::array<uint8_t, 16> bytes;
  crypto::RandBytes(bytes);
  EXPECT_FALSE(IsTrivial(bytes));
}

TEST(RandBytes, RandBytesAsVector) {
  std::vector<uint8_t> vector = crypto::RandBytesAsVector(16);
  EXPECT_FALSE(IsTrivial(vector));
}
