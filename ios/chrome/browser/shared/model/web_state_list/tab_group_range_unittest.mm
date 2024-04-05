// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/tab_group_range.h"

#import <vector>

#import "testing/platform_test.h"

using TabGroupRangeTest = PlatformTest;

// Tests properties of the invalid range.
TEST_F(TabGroupRangeTest, InvalidRange) {
  TabGroupRange range = TabGroupRange::InvalidRange();

  EXPECT_FALSE(range.valid());
}

// Tests properties of the zero range.
TEST_F(TabGroupRangeTest, ZeroRange) {
  TabGroupRange range(0, 0);

  EXPECT_FALSE(range.valid());
  EXPECT_EQ(0, range.range_begin());
  EXPECT_EQ(0, range.count());
  EXPECT_EQ(0, range.range_end());

  EXPECT_FALSE(range.contains(-1));
  EXPECT_FALSE(range.contains(0));
  EXPECT_FALSE(range.contains(1));

  EXPECT_EQ(TabGroupRange(0, 0), range);
  EXPECT_NE(TabGroupRange(0, 1), range);
  EXPECT_NE(TabGroupRange(1, 0), range);
  EXPECT_NE(TabGroupRange(1, 1), range);
  EXPECT_NE(TabGroupRange::InvalidRange(), range);
}

// Tests properties of a non-particular range.
TEST_F(TabGroupRangeTest, SomeRange) {
  TabGroupRange range(1, 2);

  EXPECT_TRUE(range.valid());
  EXPECT_EQ(1, range.range_begin());
  EXPECT_EQ(2, range.count());
  EXPECT_EQ(3, range.range_end());

  EXPECT_FALSE(range.contains(-1));
  EXPECT_FALSE(range.contains(0));
  EXPECT_TRUE(range.contains(1));
  EXPECT_TRUE(range.contains(2));
  EXPECT_FALSE(range.contains(3));

  EXPECT_NE(TabGroupRange(0, 0), range);
  EXPECT_NE(TabGroupRange(0, 1), range);
  EXPECT_NE(TabGroupRange(1, 0), range);
  EXPECT_NE(TabGroupRange(1, 1), range);
  EXPECT_EQ(TabGroupRange(1, 2), range);
  EXPECT_NE(TabGroupRange::InvalidRange(), range);
}

// Tests that moving a range moves the start but not the count.
TEST_F(TabGroupRangeTest, Move) {
  TabGroupRange range(1, 2);

  range.Move(3);
  EXPECT_EQ(TabGroupRange(4, 2), range);

  range.Move(-2);
  EXPECT_EQ(TabGroupRange(2, 2), range);

  range.Move(-2);
  EXPECT_EQ(TabGroupRange(0, 2), range);

  range.Move(10);
  EXPECT_EQ(TabGroupRange(10, 2), range);

  range.Move(0);
  EXPECT_EQ(TabGroupRange(10, 2), range);
}

// Tests that moving a range left and right moves the start but not the count.
TEST_F(TabGroupRangeTest, MoveLeftRight) {
  TabGroupRange range(1, 2);

  range.MoveLeft();
  EXPECT_EQ(TabGroupRange(0, 2), range);

  range.MoveRight();
  EXPECT_EQ(TabGroupRange(1, 2), range);

  range.MoveRight();
  EXPECT_EQ(TabGroupRange(2, 2), range);

  range.MoveLeft(2);
  EXPECT_EQ(TabGroupRange(0, 2), range);

  range.MoveRight(3);
  EXPECT_EQ(TabGroupRange(3, 2), range);

  range.MoveLeft(0);
  EXPECT_EQ(TabGroupRange(3, 2), range);

  range.MoveRight(0);
  EXPECT_EQ(TabGroupRange(3, 2), range);
}

// Tests that expanding a range moves the count and potentially the start.
TEST_F(TabGroupRangeTest, Expand) {
  TabGroupRange range(1, 2);

  range.ExpandLeft();
  EXPECT_EQ(TabGroupRange(0, 3), range);

  range.ExpandRight();
  EXPECT_EQ(TabGroupRange(0, 4), range);
}

// Tests that contracting a range moves the count and potentially the start.
TEST_F(TabGroupRangeTest, Contract) {
  TabGroupRange range(1, 2);

  range.ContractLeft();
  EXPECT_EQ(TabGroupRange(2, 1), range);

  range.ContractRight();
  EXPECT_EQ(TabGroupRange(2, 0), range);
}

// Tests that the `AsSet` getter returns the correct set of indices.
TEST_F(TabGroupRangeTest, AsSet) {
  TabGroupRange range(12, 3);
  std::set<int> expected = {12, 13, 14};

  EXPECT_EQ(expected, range.AsSet());
}

// Tests iterating over the indices from a range in a range-based for-loop lists
// all indices correctly.
TEST_F(TabGroupRangeTest, ForLoop) {
  TabGroupRange range(12, 3);
  std::vector<int> indices;

  for (int i : range) {
    indices.push_back(i);
  }

  std::vector<int> expected = {12, 13, 14};
  EXPECT_EQ(expected, indices);
}
