// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"

#import "ios/chrome/browser/shared/model/web_state_list/tab_group_range.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class RemovingIndexesTest : public PlatformTest {
 public:
  // Represents one test case for RangeAfterRemoval(...).
  struct TabGroupRangeTestCase {
    TabGroupRange input;
    TabGroupRange expected;
  };

  // Checks whether `removing_indexes` returns the expected output for
  // RangeAfterRemoval(...) for all tests in `test_cases`.
  void CheckRangeAfterRemoval(
      RemovingIndexes removing_indexes,
      base::span<const TabGroupRangeTestCase> test_cases) {
    for (const TabGroupRangeTestCase& test_case : test_cases) {
      const TabGroupRange output =
          removing_indexes.RangeAfterRemoval(test_case.input);
      if (test_case.expected.valid()) {
        EXPECT_EQ(test_case.expected, output);
      } else {
        EXPECT_FALSE(output.valid());
      }
    }
  }
};

// Tests that RemovingIndexes reports the correct count of closed tabs (as
// a tab cannot be closed multiple times, duplicates should not be counted).
TEST_F(RemovingIndexesTest, Count) {
  EXPECT_EQ(RemovingIndexes({}).count(), 0);
  EXPECT_EQ(RemovingIndexes({1}).count(), 1);
  EXPECT_EQ(RemovingIndexes({1, 1}).count(), 1);
  EXPECT_EQ(RemovingIndexes({1, 2}).count(), 2);
  EXPECT_EQ(RemovingIndexes({2, 1, 2, 1}).count(), 2);
  EXPECT_EQ(RemovingIndexes({.start = 2, .count = 3}).count(), 3);
}

// Tests that Span returns an empty range when no tabs are removed.
TEST_F(RemovingIndexesTest, SpanEmpty) {
  const RemovingIndexes removing_indexes({});
  const RemovingIndexes::Range span = removing_indexes.span();
  EXPECT_EQ(span.start, -1);
  EXPECT_EQ(span.count, 0);
}

// Tests that Span returns an range with one element when one tab is removed.
TEST_F(RemovingIndexesTest, SpanOneTab) {
  const RemovingIndexes removing_indexes({4});
  const RemovingIndexes::Range span = removing_indexes.span();
  EXPECT_EQ(span.start, 4);
  EXPECT_EQ(span.count, 1);
}

// Tests that Span returns the range passed to the constructor when closing
// a contiguous range of tabs.
TEST_F(RemovingIndexesTest, SpanRangeOfTabs) {
  const RemovingIndexes removing_indexes({.start = 2, .count = 3});
  const RemovingIndexes::Range span = removing_indexes.span();
  EXPECT_EQ(span.start, 2);
  EXPECT_EQ(span.count, 3);
}

// Tests that Span returns the minimum range that cover all closed tabs when
// a disjoint set of tabs is closed.
TEST_F(RemovingIndexesTest, SpanMultipleTabs) {
  const RemovingIndexes removing_indexes({1, 3, 7});
  const RemovingIndexes::Range span = removing_indexes.span();
  EXPECT_EQ(span.start, 1);
  EXPECT_EQ(span.count, 7);
}

// Tests that RemovingIndexes correctly returns the correct updated value
// when asked for index if no tabs are removed.
TEST_F(RemovingIndexesTest, IndexAfterRemovalEmpty) {
  const RemovingIndexes removing_indexes({});
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(0), 0);  // no removal before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(1), 1);  // no removal before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(2), 2);  // no removal before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(3), 3);  // no removal before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(4), 4);  // no removal before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(5), 5);  // no removal before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(6), 6);  // no removal before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(7), 7);  // no removal before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(8), 8);  // no removal before

  // kInvalidIndex should always be mapped to kInvalidIndex.
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(WebStateList::kInvalidIndex),
            WebStateList::kInvalidIndex);
}

// Tests that RemovingIndexes correctly returns the correct updated value
// when asked for index if one tab is removed.
TEST_F(RemovingIndexesTest, IndexAfterRemovalOneTab) {
  const RemovingIndexes removing_indexes({4});
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(0), 0);
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(1), 1);
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(2), 2);
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(3), 3);
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(4), WebStateList::kInvalidIndex);
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(5), 4);  // one removals before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(6), 5);  // one removals before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(7), 6);  // one removals before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(8), 7);  // one removals before

  // kInvalidIndex should always be mapped to kInvalidIndex.
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(WebStateList::kInvalidIndex),
            WebStateList::kInvalidIndex);
}

// Tests that RemovingIndexes correctly returns the correct updated value
// when asked for index if a range of tabs are removed.
TEST_F(RemovingIndexesTest, IndexAfterRemovalRangeOfTabs) {
  const RemovingIndexes removing_indexes({.start = 2, .count = 3});
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(0), 0);
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(1), 1);
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(2), WebStateList::kInvalidIndex);
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(3), WebStateList::kInvalidIndex);
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(4), WebStateList::kInvalidIndex);
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(5), 2);  // three removals before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(6), 3);  // three removals before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(7), 4);  // three removals before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(8), 5);  // three removals before

  // kInvalidIndex should always be mapped to kInvalidIndex.
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(WebStateList::kInvalidIndex),
            WebStateList::kInvalidIndex);
}

// Tests that RemovingIndexes correctly returns the correct updated value
// when asked for index if multiple tabs have been removed.
TEST_F(RemovingIndexesTest, IndexAfterRemovalMultipleTabs) {
  const RemovingIndexes removing_indexes({1, 3, 7});
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(0), 0);  // no removal before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(1), WebStateList::kInvalidIndex);
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(2), 1);  // one removals before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(3), WebStateList::kInvalidIndex);
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(4), 2);  // two removals before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(5), 3);  // two removals before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(6), 4);  // two removals before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(7), WebStateList::kInvalidIndex);
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(8), 5);  // three removals before

  // kInvalidIndex should always be mapped to kInvalidIndex.
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(WebStateList::kInvalidIndex),
            WebStateList::kInvalidIndex);
}

// Tests that RemovingIndexes correctly returns whether it contains the
// index when no tabs are removed.
TEST_F(RemovingIndexesTest, ContainsEmpty) {
  const RemovingIndexes removing_indexes({});
  EXPECT_FALSE(removing_indexes.Contains(0));
  EXPECT_FALSE(removing_indexes.Contains(1));
  EXPECT_FALSE(removing_indexes.Contains(2));
  EXPECT_FALSE(removing_indexes.Contains(3));
  EXPECT_FALSE(removing_indexes.Contains(4));
  EXPECT_FALSE(removing_indexes.Contains(5));
  EXPECT_FALSE(removing_indexes.Contains(6));
  EXPECT_FALSE(removing_indexes.Contains(7));
  EXPECT_FALSE(removing_indexes.Contains(8));
}

// Tests that RemovingIndexes correctly returns whether it contains the
// index when one tabs is removed.
TEST_F(RemovingIndexesTest, ContainsOneTab) {
  const RemovingIndexes removing_indexes({4});
  EXPECT_FALSE(removing_indexes.Contains(0));
  EXPECT_FALSE(removing_indexes.Contains(1));
  EXPECT_FALSE(removing_indexes.Contains(2));
  EXPECT_FALSE(removing_indexes.Contains(3));
  EXPECT_TRUE(removing_indexes.Contains(4));
  EXPECT_FALSE(removing_indexes.Contains(5));
  EXPECT_FALSE(removing_indexes.Contains(6));
  EXPECT_FALSE(removing_indexes.Contains(7));
  EXPECT_FALSE(removing_indexes.Contains(8));
}

// Tests that RemovingIndexes correctly returns whether it contains the
// index when a range of tabs are removed.
TEST_F(RemovingIndexesTest, ContainsRangeOfTabs) {
  const RemovingIndexes removing_indexes({.start = 2, .count = 3});
  EXPECT_FALSE(removing_indexes.Contains(0));
  EXPECT_FALSE(removing_indexes.Contains(1));
  EXPECT_TRUE(removing_indexes.Contains(2));
  EXPECT_TRUE(removing_indexes.Contains(3));
  EXPECT_TRUE(removing_indexes.Contains(4));
  EXPECT_FALSE(removing_indexes.Contains(5));
  EXPECT_FALSE(removing_indexes.Contains(6));
  EXPECT_FALSE(removing_indexes.Contains(7));
  EXPECT_FALSE(removing_indexes.Contains(8));
}

// Tests that RemovingIndexes correctly returns whether it contains the
// index when multiple tabs have been removed.
TEST_F(RemovingIndexesTest, ContainsMultipleTabs) {
  const RemovingIndexes removing_indexes({1, 3, 7});
  EXPECT_FALSE(removing_indexes.Contains(0));
  EXPECT_TRUE(removing_indexes.Contains(1));
  EXPECT_FALSE(removing_indexes.Contains(2));
  EXPECT_TRUE(removing_indexes.Contains(3));
  EXPECT_FALSE(removing_indexes.Contains(4));
  EXPECT_FALSE(removing_indexes.Contains(5));
  EXPECT_FALSE(removing_indexes.Contains(6));
  EXPECT_TRUE(removing_indexes.Contains(7));
  EXPECT_FALSE(removing_indexes.Contains(8));
}

// Tests that empty RemovingIndexes returns the input range.
TEST_F(RemovingIndexesTest, TabGroupRange_Empty) {
  // Consider "| a b c d", removing nothing.
  const RemovingIndexes removing_indexes({});
  const TabGroupRange invalid_range = TabGroupRange::InvalidRange();

  const TabGroupRangeTestCase test_cases[] = {
      // An invalid range should always be mapped to an invalid range.
      {.input = {1, 0}, .expected = invalid_range},
      // "| [ 0 a ] b c d" → "| [ 0 a ] b c d"
      {.input = {0, 1}, .expected = {0, 1}},
      // "| a [ 0 b ] c d" → "| a [ 0 b ] c d"
      {.input = {1, 1}, .expected = {1, 1}},
      // "| a b c [ 0 d ]" → "| a b c [ 0 d ]"
      {.input = {3, 1}, .expected = {3, 1}},
      // "| a [ 0 b c ] d" → "| a [ 0 b c ] d"
      {.input = {1, 2}, .expected = {1, 2}},
      // "| [ 0 a b c d ]" → "| [ 0 a b c d ]"
      {.input = {0, 3}, .expected = {0, 3}},
  };

  CheckRangeAfterRemoval(removing_indexes, base::make_span(test_cases));
}

// Tests that one-tab RemovingIndexes returns the correct range.
TEST_F(RemovingIndexesTest, TabGroupRange_OneTab) {
  // Consider "| a b c d e f g", removing 'd'.
  const RemovingIndexes removing_indexes({3});
  const TabGroupRange invalid_range = TabGroupRange::InvalidRange();

  const TabGroupRangeTestCase test_cases[] = {
      // An invalid range should always be mapped to an invalid range.
      {.input = {1, 0}, .expected = invalid_range},
      // "| [ 0 a ] b c d e f g" → "| [ 0 a ] b c e f g"
      {.input = {0, 1}, .expected = {0, 1}},
      // "| a b c [ 0 d ] e f g" → "| a b c e f g"
      {.input = {3, 1}, .expected = invalid_range},
      // "| a b c d e f [ 0 g ]" → "| a b c e f [ 0 g ]"
      {.input = {6, 1}, .expected = {5, 1}},
      // "| [ 0 a b c ] d e f g" → "| [ 0 a b c ] e f g"
      {.input = {0, 3}, .expected = {0, 3}},
      // "| a [ 0 b c d ] e f g" → "| a [ 0 b c ] e f g"
      {.input = {1, 3}, .expected = {1, 2}},
      // "| a b [ 0 c d e ] f g" → "| a b [ 0 c e ] f g"
      {.input = {2, 3}, .expected = {2, 2}},
      // "| a b c [ 0 d e f ] g" → "| a b c [ 0 e f ] g"
      {.input = {3, 3}, .expected = {3, 2}},
      // "| a b c d [ 0 e f g ]" → "| a b c [ 0 e f g ]"
      {.input = {4, 3}, .expected = {3, 3}},
  };

  CheckRangeAfterRemoval(removing_indexes, base::make_span(test_cases));
}

// Tests that RemovingIndexes from a range of tabs returns the correct tab
// group range.
TEST_F(RemovingIndexesTest, TabGroupRange_RangeOfTabs) {
  // Consider "| a b c d e f", removing {c, d, e}.
  const RemovingIndexes removing_indexes({.start = 2, .count = 3});
  const TabGroupRange invalid_range = TabGroupRange::InvalidRange();

  const TabGroupRangeTestCase test_cases[] = {
      // An invalid range should always be mapped to an invalid range.
      {.input = {1, 0}, .expected = invalid_range},
      // "| a [ 0 b ] c d e f g" → "| a [ 0 b ] f g"
      {.input = {1, 1}, .expected = {1, 1}},
      // "| a b [ 0 c ] d e f g" → "| a b f g"
      {.input = {2, 1}, .expected = invalid_range},
      // "| a b c [ 0 d ] e f g" → "| a b f g"
      {.input = {3, 1}, .expected = invalid_range},
      // "| a b c d [ 0 e ] f g" → "| a b f g"
      {.input = {4, 1}, .expected = invalid_range},
      // "| a b c d e [ 0 f ] g" → "| a b [ 0 f ] g"
      {.input = {5, 1}, .expected = {2, 1}},
      // "| [ 0 a b ] c d e f g" → "| [ 0 a b ] f g"
      {.input = {0, 2}, .expected = {0, 2}},
      // "| a [ 0 b c ] d e f g" → "| a [ 0 b ] f g"
      {.input = {1, 2}, .expected = {1, 1}},
      // "| a b [ 0 c d ] e f g" → "| a b f g"
      {.input = {2, 2}, .expected = invalid_range},
      // "| a b c [ 0 d e ] f g" → "| a b f g"
      {.input = {3, 2}, .expected = invalid_range},
      // "| a b c d [ 0 e f ] g" → "| a b [ 0 f ] g"
      {.input = {4, 2}, .expected = {2, 1}},
      // "| a b c d e [ 0 f g ]" → "| a b [ 0 f g ]"
      {.input = {5, 2}, .expected = {2, 2}},
      // "| [ 0 a b c ] d e f g" → "| [ 0 a b ] f g"
      {.input = {0, 3}, .expected = {0, 2}},
      // "| a [ 0 b c d ] e f g" → "| a [ 0 b ] f g"
      {.input = {1, 3}, .expected = {1, 1}},
      // "| a b [ 0 c d e ] f g" → "| a b f g"
      {.input = {2, 3}, .expected = invalid_range},
      // "| a b c [ 0 d e f ] g" → "| a b [ 0 f ] g"
      {.input = {3, 3}, .expected = {2, 1}},
      // "| a b c d [ 0 e f g]" → "| a b [ 0 f g ]"
      {.input = {4, 3}, .expected = {2, 2}},
      // "| [ 0 a b c d ] e f g" → "| [ 0 a b ] f g"
      {.input = {0, 4}, .expected = {0, 2}},
      // "| a [ 0 b c d e ] f g" → "| a [ 0 b ] f g"
      {.input = {1, 4}, .expected = {1, 1}},
      // "| a b [ 0 c d e f ] g" → "| a b [ 0 f ] g"
      {.input = {2, 4}, .expected = {2, 1}},
      // "| a b c [ 0 d e f g ]" → "| a b [ 0 f g ]"
      {.input = {3, 4}, .expected = {2, 2}},
      // "| [ 0 a b c d e ] f g" → "| [ 0 a b ] f g"
      {.input = {0, 5}, .expected = {0, 2}},
      // "| a [ 0 b c d e f ] g" → "| a [ 0 b f ] g"
      {.input = {1, 5}, .expected = {1, 2}},
      // "| a b [ 0 c d e f g ]" → "| a b [ 0 f g ]"
      {.input = {2, 5}, .expected = {2, 2}},
  };

  CheckRangeAfterRemoval(removing_indexes, base::make_span(test_cases));
}

// Tests that RemovingIndexes from a set of tabs returns the correct tab
// group range.
TEST_F(RemovingIndexesTest, TabGroupRange_MultipleTabs) {
  // Consider "| a b c d e f g", removing {b, d, e}.
  const RemovingIndexes removing_indexes({1, 3, 4});
  const TabGroupRange invalid_range = TabGroupRange::InvalidRange();

  const TabGroupRangeTestCase test_cases[] = {
      // An invalid range should always be mapped to an invalid range.
      {.input = {1, 0}, .expected = invalid_range},
      // "| [ 0 a ] b c d e f g" → "| [ 0 a ] c f g"
      {.input = {0, 1}, .expected = {0, 1}},
      // "| a [ 0 b ] c d e f g" → "| a c f g"
      {.input = {1, 1}, .expected = invalid_range},
      // "| a b [ 0 c ] d e f g" → "| a [ 0 c ] f g"
      {.input = {2, 1}, .expected = {1, 1}},
      // "| a b c [ 0 d ] e f g" → "| a c f g"
      {.input = {3, 1}, .expected = invalid_range},
      // "| a b c d [ 0 e ] f g" → "| a c f g"
      {.input = {4, 1}, .expected = invalid_range},
      // "| a b c d e [ 0 f ] g" → "| a c [ 0 f ] g"
      {.input = {5, 1}, .expected = {2, 1}},
      // "| [ 0 a b ] c d e f g" → "| [ 0 a ] c f g"
      {.input = {0, 2}, .expected = {0, 1}},
      // "| a [ 0 b c ] d e f g" → "| a [ 0 c ] f g"
      {.input = {1, 2}, .expected = {1, 1}},
      // "| a b [ 0 c d ] e f g" → "| a [ 0 c ] f g"
      {.input = {2, 2}, .expected = {1, 1}},
      // "| a b c [ 0 d e ] f g" → "| a c f g"
      {.input = {3, 2}, .expected = invalid_range},
      // "| a b c d [ 0 e f ] g" → "| a c [ 0 f ] g"
      {.input = {4, 2}, .expected = {2, 1}},
      // "| a b c d e [ 0 f g ]" → "| a c [ 0 f g ]"
      {.input = {5, 2}, .expected = {2, 2}},
      // "| [ 0 a b c ] d e f g" → "| [ 0 a c ] f g"
      {.input = {0, 3}, .expected = {0, 2}},
      // "| a [ 0 b c d ] e f g" → "| a [ 0 c ] f g"
      {.input = {1, 3}, .expected = {1, 1}},
      // "| a b [ 0 c d e ] f g" → "| a [ 0 c ] f g"
      {.input = {2, 3}, .expected = {1, 1}},
      // "| a b c [ 0 d e f ] g" → "| a c [ 0 f ] g"
      {.input = {3, 3}, .expected = {2, 1}},
      // "| a b c d [ 0 e f g ]" → "| a c [ 0 f g ]"
      {.input = {4, 3}, .expected = {2, 2}},
      // "| [ 0 a b c d ] e f g" → "| [ 0 a c ] f g"
      {.input = {0, 4}, .expected = {0, 2}},
      // "| a [ 0 b c d e ] f g" → "| a [ 0 c ] f g"
      {.input = {1, 4}, .expected = {1, 1}},
      // "| a b [ 0 c d e f ] g" → "| a [ 0 c f ] g"
      {.input = {2, 4}, .expected = {1, 2}},
      // "| a b c [ 0 d e f g ]" → "| a c [ 0 f g ]"
      {.input = {3, 4}, .expected = {2, 2}},
      // "| [ 0 a b c d e ] f g" → "| [ 0 a c ] f g"
      {.input = {0, 5}, .expected = {0, 2}},
      // "| a [ 0 b c d e f ] g" → "| a [ 0 c f ] g"
      {.input = {1, 5}, .expected = {1, 2}},
      // "| a b [ 0 c d e f g ]" → "| a [ 0 c f g ]"
      {.input = {2, 5}, .expected = {1, 3}},
      // "| [ 0 a b c d e f ] g" → "| [ 0 a c f ] g"
      {.input = {0, 6}, .expected = {0, 3}},
  };

  CheckRangeAfterRemoval(removing_indexes, base::make_span(test_cases));
}
