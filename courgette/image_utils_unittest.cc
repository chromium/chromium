// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/image_utils.h"

#include <stdint.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace courgette {

namespace {

TEST(ImageUtilsTest, Read) {
  uint8_t test_data[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 00};
  EXPECT_EQ(0x2301U, Read16LittleEndian(test_data));
  EXPECT_EQ(0x67452301U, Read32LittleEndian(test_data));
  EXPECT_EQ(0xEFCDAB8967452301ULL, Read64LittleEndian(test_data));

  // These will break big-endian architectures, which we don't yet support.
  EXPECT_EQ(0x2301U, ReadU16(test_data, 0));
  EXPECT_EQ(0x4523U, ReadU16(test_data, 1));
  EXPECT_EQ(0x67452301U, ReadU32(test_data, 0));
  EXPECT_EQ(0x89674523U, ReadU32(test_data, 1));
  EXPECT_EQ(0xEFCDAB8967452301ULL, ReadU64(test_data, 0));
  EXPECT_EQ(0x00EFCDAB89674523ULL, ReadU64(test_data, 1));
}

}  // namespace

}  // namespace courgette
