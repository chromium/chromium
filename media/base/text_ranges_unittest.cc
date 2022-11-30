// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/text_ranges.h"

#include <stddef.h>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class TextRangesTest : public ::testing::Test {
 protected:
  bool AddCue(int seconds) { return ranges_.AddCue(base::Seconds(seconds)); }

  void Reset() {
    ranges_.Reset();
  }

  size_t RangeCount() {
    return ranges_.RangeCountForTesting();
  }

  TextRanges ranges_;
};

TEST_F(TextRangesTest, TestEmptyRanges) {
  // Create a new active range, with t=5.
  EXPECT_TRUE(AddCue(5));

  // Create a new active range, with t=2.
  Reset();
  EXPECT_TRUE(AddCue(2));

  // Create a new active range, with t=8.
  Reset();
  EXPECT_TRUE(AddCue(8));

  Reset();

  // Make range [2, 2] active.
  EXPECT_FALSE(AddCue(2));
  EXPECT_EQ(RangeCount(), 3U);

  // Coalesce first two ranges: [2, 5].
  EXPECT_FALSE(AddCue(5));
  EXPECT_EQ(RangeCount(), 2U);

  // Coalesce first two ranges: [2, 8].
  EXPECT_FALSE(AddCue(8));
  EXPECT_EQ(RangeCount(), 1U);

  // Add new cue to end of (only) range.
  EXPECT_TRUE(AddCue(9));
  EXPECT_EQ(RangeCount(), 1U);
}

TEST_F(TextRangesTest, TestOneRange) {
  // Create a new active range, with t=0.
  EXPECT_TRUE(AddCue(0));

  // Add cues to end of existing range.
  EXPECT_TRUE(AddCue(1));
  EXPECT_TRUE(AddCue(4));

  Reset();
  EXPECT_FALSE(AddCue(2));
  EXPECT_FALSE(AddCue(3));
  EXPECT_FALSE(AddCue(4));
}

TEST_F(TextRangesTest, TestDuplicateLast) {
  // Create a new active range, with t=0.
  EXPECT_TRUE(AddCue(0));
  EXPECT_TRUE(AddCue(1));

  Reset();
  EXPECT_FALSE(AddCue(1));
  EXPECT_TRUE(AddCue(1));
}

TEST_F(TextRangesTest, TestTwoRanges) {
  // Create a new active range, with t=0.
  EXPECT_TRUE(AddCue(0));

  // Add cue to end of existing range.
  EXPECT_TRUE(AddCue(2));

  Reset();

  // Create a new active range, with t=4.
  EXPECT_TRUE(AddCue(4));

  // Add a new cue to end of last (active) range.
  EXPECT_TRUE(AddCue(5));

  Reset();

  // Make first range active.
  EXPECT_FALSE(AddCue(0));
  EXPECT_FALSE(AddCue(2));

  // Expand first range.
  EXPECT_TRUE(AddCue(3));

  // Coalesce first and second ranges.
  EXPECT_FALSE(AddCue(4));
  EXPECT_EQ(RangeCount(), 1U);
}

TEST_F(TextRangesTest, TestThreeRanges) {
  // Create a new active range, with t=0.
  EXPECT_TRUE(AddCue(0));

  // Add cue to end of existing range.
  EXPECT_TRUE(AddCue(2));

  Reset();

  // Create a new active range, with t=4.
  EXPECT_TRUE(AddCue(4));

  // Add a new cue to end of last (active) range.
  EXPECT_TRUE(AddCue(5));

  Reset();

  // Create a new active range, in between the other two.
  EXPECT_TRUE(AddCue(3));

  // Coalesce middle and last ranges.
  EXPECT_FALSE(AddCue(4));

  Reset();

  // Make first range active.
  EXPECT_FALSE(AddCue(0));
  EXPECT_FALSE(AddCue(2));

  // Coalesce first and last ranges.
  EXPECT_FALSE(AddCue(3));
  EXPECT_EQ(RangeCount(), 1U);
}

}  // namespace media
