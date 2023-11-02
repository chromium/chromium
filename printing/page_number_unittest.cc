// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/page_number.h"
#include "printing/print_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PageNumberTest, Count) {
  printing::PageRanges ranges;
  printing::PageNumber page;
  EXPECT_EQ(printing::PageNumber::npos(), page);
  page.Init(ranges, 3);
  EXPECT_EQ(0u, page.ToUint());
  EXPECT_NE(printing::PageNumber::npos(), page);
  ++page;
  EXPECT_EQ(1u, page.ToUint());
  EXPECT_NE(printing::PageNumber::npos(), page);

  printing::PageNumber page_copy(page);
  EXPECT_EQ(1u, page_copy.ToUint());
  EXPECT_EQ(1u, page.ToUint());
  ++page;
  EXPECT_EQ(1u, page_copy.ToUint());
  EXPECT_EQ(2u, page.ToUint());
  ++page;
  EXPECT_EQ(printing::PageNumber::npos(), page);
  ++page;
  EXPECT_EQ(printing::PageNumber::npos(), page);
}

TEST(PageNumberTest, GetPages) {
  printing::PageRanges ranges = {{5, 6}, {0, 2}, {9, 9}, {11, 10000}};
  EXPECT_THAT(printing::PageNumber::GetPages(ranges, 8),
              testing::ElementsAre(0, 1, 2, 5, 6));
  EXPECT_THAT(printing::PageNumber::GetPages(ranges, 13),
              testing::ElementsAre(0, 1, 2, 5, 6, 9, 11, 12));
  EXPECT_THAT(printing::PageNumber::GetPages({}, 5),
              testing::ElementsAre(0, 1, 2, 3, 4));
}

TEST(PageNumberTest, GetPagesOutOfRange) {
  printing::PageRanges ranges = {{7, 8}};
  EXPECT_THAT(printing::PageNumber::GetPages(ranges, 7), testing::IsEmpty());
}
