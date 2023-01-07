// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/page_range.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PageRangeTest, RangeMerge) {
  printing::PageRanges ranges;
  ranges.push_back({1, 3});
  ranges.push_back({10, 12});
  ranges.push_back({2, 5});
  ranges.push_back({12, 13});
  ranges.push_back({14, 14});

  printing::PageRange::Normalize(ranges);
  EXPECT_THAT(ranges, testing::ElementsAreArray<printing::PageRange>(
                          {{1, 5}, {10, 14}}));
}

TEST(PageRangeTest, Empty) {
  printing::PageRanges ranges;
  printing::PageRange::Normalize(ranges);
  EXPECT_THAT(ranges, testing::IsEmpty());
}

TEST(PageRangeTest, SingleEntry) {
  printing::PageRanges ranges;
  ranges.push_back({1, 1});
  EXPECT_THAT(ranges, testing::ElementsAre(printing::PageRange{1, 1}));
}
