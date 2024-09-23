// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/zucchini/crc32.h"

#include <stdint.h>

#include <iterator>

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

constexpr uint8_t bytes[] = {0x10, 0x32, 0x54, 0x76, 0x98,
                             0xBA, 0xDC, 0xFE, 0x10, 0x00};

TEST(Crc32Test, All) {
  // Results can be verified with any CRC-32 calculator found online.

  // Empty region.
  EXPECT_EQ(0x00000000U, CalculateCrc32(std::begin(bytes), std::begin(bytes)));

  // Single byte.
  EXPECT_EQ(0xCFB5FFE9U,
            CalculateCrc32(std::begin(bytes), std::begin(bytes) + 1));

  // Same byte (0x10) appearing at different location.
  EXPECT_EQ(0xCFB5FFE9U,
            CalculateCrc32(std::begin(bytes) + 8, std::begin(bytes) + 9));

  // Single byte of 0.
  EXPECT_EQ(0xD202EF8DU,
            CalculateCrc32(std::begin(bytes) + 9, std::end(bytes)));

  // Whole region.
  EXPECT_EQ(0xA86FD7D6U, CalculateCrc32(std::begin(bytes), std::end(bytes)));

  // Whole region excluding 0 at end.
  EXPECT_EQ(0x0762F38BU,
            CalculateCrc32(std::begin(bytes), std::begin(bytes) + 9));

#if GTEST_HAS_DEATH_TEST
  EXPECT_DCHECK_DEATH(CalculateCrc32(std::begin(bytes) + 1, std::begin(bytes)));
#endif
}

}  // namespace zucchini
