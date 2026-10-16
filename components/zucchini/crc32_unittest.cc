// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/crc32.h"

#include <stdint.h>

#include <array>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

constexpr auto kBytes = std::to_array<uint8_t>(
    {0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0x10, 0x00});

TEST(Crc32Test, All) {
  // Results can be verified with any CRC-32 calculator found online.
  const base::span<const uint8_t> data(kBytes);

  // Empty region.
  EXPECT_EQ(0x00000000U, CalculateCrc32(data.first(0u)));

  // Single byte.
  EXPECT_EQ(0xCFB5FFE9U, CalculateCrc32(data.first(1u)));

  // Same byte (0x10) appearing at different location.
  EXPECT_EQ(0xCFB5FFE9U, CalculateCrc32(data.subspan(8u, 1u)));

  // Single byte of 0.
  EXPECT_EQ(0xD202EF8DU, CalculateCrc32(data.subspan(9u)));

  // Whole region.
  EXPECT_EQ(0xA86FD7D6U, CalculateCrc32(data));

  // Whole region excluding 0 at end.
  EXPECT_EQ(0x0762F38BU, CalculateCrc32(data.first(9u)));
}

}  // namespace zucchini
