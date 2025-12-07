// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/indexed_pair_set.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;

namespace disk_cache {

class IndexedPairSetTest : public testing::Test {
 protected:
  IndexedPairSet<int64_t, int64_t> set_;
};

TEST_F(IndexedPairSetTest, EmptyInitially) {
  EXPECT_TRUE(set_.empty());
  EXPECT_EQ(0u, set_.size());
}

TEST_F(IndexedPairSetTest, InsertAndFindSingleValue) {
  EXPECT_TRUE(set_.Insert(10, 100));
  EXPECT_FALSE(set_.empty());
  EXPECT_EQ(1u, set_.size());
  EXPECT_TRUE(set_.Contains(10));

  std::vector<int64_t> values = set_.Find(10);
  ASSERT_EQ(1u, values.size());
  EXPECT_EQ(100, values[0]);
}

TEST_F(IndexedPairSetTest, InsertDuplicatePair) {
  EXPECT_TRUE(set_.Insert(10, 100));
  EXPECT_EQ(1u, set_.size());

  // Inserting the exact same pair should do nothing and return false.
  EXPECT_FALSE(set_.Insert(10, 100));
  EXPECT_EQ(1u, set_.size());

  EXPECT_TRUE(set_.Insert(10, 200));
  EXPECT_EQ(2u, set_.size());

  // Inserting the exact same pair which exists in the secondary map should do
  // nothing and return false.
  EXPECT_FALSE(set_.Insert(10, 200));
  EXPECT_EQ(2u, set_.size());
}

TEST_F(IndexedPairSetTest, InsertDuplicateKey) {
  EXPECT_TRUE(set_.Insert(10, 100));
  EXPECT_TRUE(set_.Insert(10, 200));
  EXPECT_EQ(2u, set_.size());

  EXPECT_THAT(set_.Find(10), UnorderedElementsAre(100, 200));

  EXPECT_TRUE(set_.Insert(10, 300));
  EXPECT_EQ(3u, set_.size());
  EXPECT_THAT(set_.Find(10), UnorderedElementsAre(100, 200, 300));
}

TEST_F(IndexedPairSetTest, Contains) {
  EXPECT_TRUE(set_.Insert(10, 100));
  EXPECT_TRUE(set_.Insert(20, 200));
  EXPECT_TRUE(set_.Insert(10, 300));

  EXPECT_TRUE(set_.Contains(10));
  EXPECT_TRUE(set_.Contains(20));
  EXPECT_FALSE(set_.Contains(99));  // Non-existent key
}

TEST_F(IndexedPairSetTest, Clear) {
  EXPECT_TRUE(set_.Insert(10, 100));
  EXPECT_TRUE(set_.Insert(20, 200));
  EXPECT_FALSE(set_.empty());

  set_.Clear();
  EXPECT_TRUE(set_.empty());
  EXPECT_EQ(0u, set_.size());
  EXPECT_FALSE(set_.Contains(10));
  EXPECT_TRUE(set_.Find(20).empty());
}

TEST_F(IndexedPairSetTest, RemoveNonExistent) {
  EXPECT_TRUE(set_.Insert(10, 100));
  EXPECT_FALSE(set_.Remove(99, 999));  // Non-existent key
  EXPECT_FALSE(set_.Remove(10, 999));  // Non-existent value
  EXPECT_EQ(1u, set_.size());
}

TEST_F(IndexedPairSetTest, RemoveSingleValue) {
  EXPECT_TRUE(set_.Insert(10, 100));
  EXPECT_TRUE(set_.Remove(10, 100));

  EXPECT_EQ(0u, set_.size());
  EXPECT_TRUE(set_.empty());
  EXPECT_FALSE(set_.Contains(10));
  EXPECT_TRUE(set_.Find(10).empty());
}

TEST_F(IndexedPairSetTest, RemoveMultipleValuesForKey) {
  EXPECT_TRUE(set_.Insert(10, 100));
  EXPECT_TRUE(set_.Insert(10, 200));
  EXPECT_TRUE(set_.Insert(10, 300));
  EXPECT_EQ(3u, set_.size());

  // Remove a value from the secondary_map_.
  EXPECT_TRUE(set_.Remove(10, 200));
  EXPECT_EQ(2u, set_.size());
  EXPECT_TRUE(set_.Contains(10));
  EXPECT_THAT(set_.Find(10), UnorderedElementsAre(100, 300));

  // Remove a value from the primary_map_, triggering a promotion.
  EXPECT_TRUE(set_.Remove(10, 100));
  EXPECT_EQ(1u, set_.size());
  EXPECT_THAT(set_.Find(10), UnorderedElementsAre(300));

  // Remove the final value.
  EXPECT_TRUE(set_.Remove(10, 300));
  EXPECT_EQ(0u, set_.size());
  EXPECT_FALSE(set_.Contains(10));
}

TEST_F(IndexedPairSetTest, RemovePromotesFromAdditionalMap) {
  set_.Insert(10, 100);
  set_.Insert(10, 200);
  set_.Insert(10, 300);

  // This will remove 100 from the primary_map_ and promote one of the values
  // from secondary_map_ (either 200 or 300).
  EXPECT_TRUE(set_.Remove(10, 100));
  EXPECT_EQ(2u, set_.size());
  EXPECT_THAT(set_.Find(10), UnorderedElementsAre(200, 300));
}

TEST_F(IndexedPairSetTest, RemoveLastFromAdditionalMapEmptiesSet) {
  set_.Insert(10, 100);
  set_.Insert(10, 200);

  // Remove the value from the secondary_map_.
  EXPECT_TRUE(set_.Remove(10, 200));
  EXPECT_EQ(1u, set_.size());
  EXPECT_THAT(set_.Find(10), UnorderedElementsAre(100));

  // The secondary_map_ should now be empty for key 10.
  // Inserting a new value should work as expected.
  EXPECT_TRUE(set_.Insert(10, 300));
  EXPECT_EQ(2u, set_.size());
  EXPECT_THAT(set_.Find(10), UnorderedElementsAre(100, 300));
}

TEST_F(IndexedPairSetTest, RemoveFromAdditionalMapThenInsertNewValue) {
  // Setup:
  // primary_map_ will contain: {10, 100}, {20, 300}
  // secondary_map_ will contain: {10, {200}}
  EXPECT_TRUE(set_.Insert(10, 100));
  EXPECT_TRUE(set_.Insert(10, 200));
  EXPECT_TRUE(set_.Insert(20, 300));
  EXPECT_EQ(3u, set_.size());

  // Action: Remove the only value for key 10 from the secondary_map_.
  EXPECT_TRUE(set_.Remove(10, 200));
  EXPECT_EQ(2u, set_.size());

  // Verification: The secondary_map_ should no longer have an entry for
  // key 10. Inserting a new value for key 10 should add it to the
  // secondary_map_.
  EXPECT_TRUE(set_.Insert(10, 400));
  EXPECT_EQ(3u, set_.size());

  EXPECT_THAT(set_.Find(10), UnorderedElementsAre(100, 400));
}

TEST_F(IndexedPairSetTest, MoveConstructor) {
  set_.Insert(10, 100);
  set_.Insert(20, 200);
  EXPECT_EQ(2u, set_.size());

  IndexedPairSet<int64_t, int64_t> new_set(std::move(set_));

  // The old set should be empty.
  EXPECT_EQ(0u, set_.size());
  EXPECT_TRUE(set_.empty());

  // The new set should have the data.
  EXPECT_EQ(2u, new_set.size());
  EXPECT_TRUE(new_set.Contains(10));
  EXPECT_TRUE(new_set.Contains(20));
  EXPECT_THAT(new_set.Find(10), UnorderedElementsAre(100));
  EXPECT_THAT(new_set.Find(20), UnorderedElementsAre(200));
}

TEST_F(IndexedPairSetTest, MoveAssignment) {
  set_.Insert(10, 100);
  set_.Insert(20, 200);

  IndexedPairSet<int64_t, int64_t> new_set;
  new_set.Insert(30, 300);

  new_set = std::move(set_);

  // The old set should be empty.
  EXPECT_EQ(0u, set_.size());
  EXPECT_TRUE(set_.empty());

  // The new set should have the moved data.
  EXPECT_EQ(2u, new_set.size());
  EXPECT_TRUE(new_set.Contains(10));
  EXPECT_TRUE(new_set.Contains(20));
  EXPECT_THAT(new_set.Find(10), UnorderedElementsAre(100));
  EXPECT_THAT(new_set.Find(20), UnorderedElementsAre(200));
  EXPECT_FALSE(new_set.Contains(30));  // Original data should be gone.
}

TEST_F(IndexedPairSetTest, MoveAssignmentSelf) {
  set_.Insert(10, 100);
  set_.Insert(20, 200);
  EXPECT_EQ(2u, set_.size());

  set_ = std::move(set_);

  // The set should be unchanged.
  EXPECT_EQ(2u, set_.size());
  EXPECT_TRUE(set_.Contains(10));
  EXPECT_TRUE(set_.Contains(20));
  EXPECT_THAT(set_.Find(10), UnorderedElementsAre(100));
  EXPECT_THAT(set_.Find(20), UnorderedElementsAre(200));
}

TEST_F(IndexedPairSetTest, RemoveLastValueFromSecondaryMapRemovesKey) {
  set_.Insert(10, 100);
  set_.Insert(10, 200);
  EXPECT_TRUE(set_.HasMultipleValues(10));

  // Remove the value from the secondary_map.
  EXPECT_TRUE(set_.Remove(10, 200));
  EXPECT_EQ(1u, set_.size());
  EXPECT_THAT(set_.Find(10), UnorderedElementsAre(100));

  // The key should no longer exist in the secondary_map.
  EXPECT_FALSE(set_.HasMultipleValues(10));
}

TEST_F(IndexedPairSetTest, RemoveAndPromoteWithEmptyingSecondaryMapRemovesKey) {
  set_.Insert(10, 100);
  set_.Insert(10, 200);
  EXPECT_TRUE(set_.HasMultipleValues(10));

  // This will remove 100 from the primary_map and promote 200 from the
  // secondary_map. After promotion, the secondary_map should be empty for key
  // 10, so the key should be removed.
  EXPECT_TRUE(set_.Remove(10, 100));
  EXPECT_EQ(1u, set_.size());
  EXPECT_THAT(set_.Find(10), UnorderedElementsAre(200));

  // The key should no longer exist in the secondary_map.
  EXPECT_FALSE(set_.HasMultipleValues(10));
}

TEST_F(IndexedPairSetTest, TryGetSingleValue) {
  EXPECT_EQ(set_.TryGetSingleValue(10), std::nullopt);

  EXPECT_TRUE(set_.Insert(10, 100));
  EXPECT_THAT(set_.TryGetSingleValue(10), 100);

  EXPECT_TRUE(set_.Insert(10, 200));
  EXPECT_EQ(set_.TryGetSingleValue(10), std::nullopt);

  EXPECT_TRUE(set_.Remove(10, 200));
  EXPECT_THAT(set_.TryGetSingleValue(10), 100);

  EXPECT_TRUE(set_.Remove(10, 100));
  EXPECT_EQ(set_.TryGetSingleValue(10), std::nullopt);
}

}  // namespace disk_cache
