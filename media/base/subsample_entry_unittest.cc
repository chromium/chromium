// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/subsample_entry.h"

#include <limits>

#include "base/numerics/safe_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

constexpr uint32_t kMax = std::numeric_limits<uint32_t>::max();

TEST(SubsampleEntryTest, NoEntries) {
  EXPECT_TRUE(VerifySubsamplesMatchSize({}, 0));
  EXPECT_FALSE(VerifySubsamplesMatchSize({}, 100));
}

TEST(SubsampleEntryTest, OneEntry) {
  EXPECT_TRUE(VerifySubsamplesMatchSize({{0, 50}}, 50));
  EXPECT_TRUE(VerifySubsamplesMatchSize({{100, 00}}, 100));
  EXPECT_TRUE(VerifySubsamplesMatchSize({{150, 200}}, 350));
}

TEST(SubsampleEntryTest, MultipleEntries) {
  EXPECT_TRUE(VerifySubsamplesMatchSize({{0, 50}, {100, 00}, {150, 200}}, 500));
}

TEST(SubsampleEntryTest, NoOverflow) {
  EXPECT_TRUE(
      VerifySubsamplesMatchSize({{kMax, 0}}, base::strict_cast<size_t>(kMax)));
  EXPECT_TRUE(
      VerifySubsamplesMatchSize({{0, kMax}}, base::strict_cast<size_t>(kMax)));
}

}  // namespace media
