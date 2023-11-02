// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/consecutive_range_visitor.h"

#include <stddef.h>

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace courgette {

TEST(ConsecutiveRangeVisitorTest, Basic) {
  std::string s = "AAAAABZZZZOO";
  ConsecutiveRangeVisitor<std::string::iterator> vis(s.begin(), s.end());
  EXPECT_TRUE(vis.has_more());
  EXPECT_EQ('A', *vis.cur());
  EXPECT_EQ(5U, vis.repeat());
  vis.advance();

  EXPECT_TRUE(vis.has_more());
  EXPECT_EQ('B', *vis.cur());
  EXPECT_EQ(1U, vis.repeat());
  vis.advance();

  EXPECT_TRUE(vis.has_more());
  EXPECT_EQ('Z', *vis.cur());
  EXPECT_EQ(4U, vis.repeat());
  vis.advance();

  EXPECT_TRUE(vis.has_more());
  EXPECT_EQ('O', *vis.cur());
  EXPECT_EQ(2U, vis.repeat());
  vis.advance();

  EXPECT_FALSE(vis.has_more());
}

TEST(ConsecutiveRangeVisitorTest, UnitRanges) {
  // Unsorted, no consecutive characters.
  const char s[] = "elephant elephant";
  ConsecutiveRangeVisitor<const char*> vis(std::begin(s), std::end(s) - 1);
  for (const char* scan = &s[0]; *scan; ++scan) {
    EXPECT_TRUE(vis.has_more());
    EXPECT_EQ(*scan, *vis.cur());
    EXPECT_EQ(1U, vis.repeat());
    vis.advance();
  }
  EXPECT_FALSE(vis.has_more());
}

TEST(ConsecutiveRangeVisitorTest, SingleRange) {
  for (size_t len = 1U; len < 10U; ++len) {
    std::vector<int> v(len, 137);
    ConsecutiveRangeVisitor<std::vector<int>::iterator> vis(v.begin(), v.end());
    EXPECT_TRUE(vis.has_more());
    EXPECT_EQ(137, *vis.cur());
    EXPECT_EQ(len, vis.repeat());
    vis.advance();
    EXPECT_FALSE(vis.has_more());
  }
}

TEST(ConsecutiveRangeVisitorTest, Empty) {
  std::string s;
  ConsecutiveRangeVisitor<std::string::iterator> vis(s.begin(), s.end());
  EXPECT_FALSE(vis.has_more());
}

}  // namespace courgette
