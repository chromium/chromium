// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_persistent_store_in_memory_index.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {

namespace {

const CacheEntryKeyHash kHash1(1);
const SqlPersistentStoreResId kResId1(1);
const CacheEntryKeyHash kHash2(2);
const SqlPersistentStoreResId kResId2(2);

}  // namespace

TEST(SqlPersistentStoreInMemoryIndexTest, Insert) {
  SqlPersistentStoreInMemoryIndex index;
  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  EXPECT_TRUE(index.Contains(kHash1));
}

TEST(SqlPersistentStoreInMemoryIndexTest, InsertDuplicateResId) {
  SqlPersistentStoreInMemoryIndex index;
  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  EXPECT_FALSE(index.Insert(kHash2, kResId1));
  EXPECT_TRUE(index.Contains(kHash1));
}

TEST(SqlPersistentStoreInMemoryIndexTest, InsertSameHash) {
  SqlPersistentStoreInMemoryIndex index;
  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  EXPECT_TRUE(index.Insert(kHash1, kResId2));
  EXPECT_TRUE(index.Contains(kHash1));
}

TEST(SqlPersistentStoreInMemoryIndexTest, RemoveWithHashAndResId) {
  SqlPersistentStoreInMemoryIndex index;
  index.Insert(kHash1, kResId1);
  EXPECT_TRUE(index.Remove(kHash1, kResId1));
  EXPECT_FALSE(index.Contains(kHash1));
}

TEST(SqlPersistentStoreInMemoryIndexTest, RemoveWithResId) {
  SqlPersistentStoreInMemoryIndex index;
  index.Insert(kHash1, kResId1);
  EXPECT_TRUE(index.Remove(kResId1));
  EXPECT_FALSE(index.Contains(kHash1));
}

TEST(SqlPersistentStoreInMemoryIndexTest, RemoveNonExistent) {
  SqlPersistentStoreInMemoryIndex index;
  index.Insert(kHash1, kResId1);
  EXPECT_FALSE(index.Remove(kResId2));
  EXPECT_FALSE(index.Remove(kHash2, kResId2));
  EXPECT_FALSE(index.Remove(kHash2, kResId1));
  EXPECT_FALSE(index.Remove(kHash1, kResId2));
  EXPECT_TRUE(index.Contains(kHash1));
}

TEST(SqlPersistentStoreInMemoryIndexTest, Clear) {
  SqlPersistentStoreInMemoryIndex index;
  index.Insert(kHash1, kResId1);
  index.Insert(kHash2, kResId2);
  index.Clear();
  EXPECT_FALSE(index.Contains(kHash1));
  EXPECT_FALSE(index.Contains(kHash2));
}

TEST(SqlPersistentStoreInMemoryIndexTest, MultipleEntries) {
  SqlPersistentStoreInMemoryIndex index;
  index.Insert(kHash1, kResId1);
  index.Insert(kHash2, kResId2);

  EXPECT_TRUE(index.Contains(kHash1));
  EXPECT_TRUE(index.Contains(kHash2));

  EXPECT_TRUE(index.Remove(kResId1));
  EXPECT_FALSE(index.Contains(kHash1));
  EXPECT_TRUE(index.Contains(kHash2));

  EXPECT_TRUE(index.Remove(kHash2, kResId2));
  EXPECT_FALSE(index.Contains(kHash2));
}

TEST(SqlPersistentStoreInMemoryIndexTest, BehavesCorrectlyWithBothMaps) {
  const CacheEntryKeyHash kHashLarge(3);
  const SqlPersistentStoreResId kResIdLarge(
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 1);

  SqlPersistentStoreInMemoryIndex index;

  // Add to the 32-bit map.
  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  EXPECT_EQ(1u, index.size());
  EXPECT_TRUE(index.Contains(kHash1));

  // Add to the 64-bit map.
  EXPECT_TRUE(index.Insert(kHashLarge, kResIdLarge));
  EXPECT_EQ(2u, index.size());
  EXPECT_TRUE(index.Contains(kHashLarge));

  // Check that both entries are present.
  EXPECT_TRUE(index.Contains(kHash1));

  // Remove the entry from the 32-bit map.
  EXPECT_TRUE(index.Remove(kResId1));
  EXPECT_EQ(1u, index.size());
  EXPECT_FALSE(index.Contains(kHash1));

  // Trying to remove the already removed entry should fail.
  EXPECT_FALSE(index.Remove(kResId1));
  EXPECT_FALSE(index.Remove(kHash1, kResId1));

  // It should be possible to re-insert and remove the same entry.
  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  EXPECT_TRUE(index.Remove(kHash1, kResId1));

  // Remove the entry from the 64-bit map.
  EXPECT_TRUE(index.Remove(kResIdLarge));
  EXPECT_EQ(0u, index.size());

  // It should be possible to re-insert and remove the same entry.
  EXPECT_FALSE(index.Remove(kResIdLarge));
  EXPECT_FALSE(index.Remove(kHashLarge, kResIdLarge));

  // Add entries again to ensure it still works.
  EXPECT_TRUE(index.Insert(kHash2, kResId2));
  EXPECT_TRUE(index.Insert(kHashLarge, kResIdLarge));
  EXPECT_EQ(2u, index.size());

  // Remove an entry from the 64-bit map.
  EXPECT_TRUE(index.Remove(kHashLarge, kResIdLarge));

  // Clear both maps.
  index.Clear();
  EXPECT_EQ(0u, index.size());
  EXPECT_FALSE(index.Contains(kHash2));
  EXPECT_FALSE(index.Contains(kHashLarge));
}

TEST(SqlPersistentStoreInMemoryIndexTest, MoveOperations) {
  // Test move constructor.
  SqlPersistentStoreInMemoryIndex index1;
  index1.Insert(kHash1, kResId1);
  index1.Insert(kHash2, kResId2);

  SqlPersistentStoreInMemoryIndex index2(std::move(index1));
  EXPECT_TRUE(index2.Contains(kHash1));
  EXPECT_TRUE(index2.Contains(kHash2));
  EXPECT_EQ(2u, index2.size());

  // Test move assignment.
  SqlPersistentStoreInMemoryIndex index3;
  index3 = std::move(index2);
  EXPECT_TRUE(index3.Contains(kHash1));
  EXPECT_TRUE(index3.Contains(kHash2));
  EXPECT_EQ(2u, index3.size());
}

TEST(SqlPersistentStoreInMemoryIndexTest, MoveOperationsWithResId64) {
  const CacheEntryKeyHash kHashLarge(3);
  const SqlPersistentStoreResId kResIdLarge(
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 1);

  // Test move constructor with ResId64.
  SqlPersistentStoreInMemoryIndex index1;
  index1.Insert(kHash1, kResId1);
  index1.Insert(kHashLarge, kResIdLarge);

  SqlPersistentStoreInMemoryIndex index2(std::move(index1));
  EXPECT_TRUE(index2.Contains(kHash1));
  EXPECT_TRUE(index2.Contains(kHashLarge));
  EXPECT_EQ(2u, index2.size());

  // Test move assignment with ResId64.
  SqlPersistentStoreInMemoryIndex index3;
  index3 = std::move(index2);
  EXPECT_TRUE(index3.Contains(kHash1));
  EXPECT_TRUE(index3.Contains(kHashLarge));
  EXPECT_EQ(2u, index3.size());
}

TEST(SqlPersistentStoreInMemoryIndexTest, TryGetSingleResIdNoEntry) {
  SqlPersistentStoreInMemoryIndex index;
  EXPECT_EQ(index.TryGetSingleResId(kHash1), std::nullopt);
}

TEST(SqlPersistentStoreInMemoryIndexTest, TryGetSingleResIdOneEntrySmall) {
  SqlPersistentStoreInMemoryIndex index;
  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  EXPECT_THAT(index.TryGetSingleResId(kHash1), kResId1);
}

TEST(SqlPersistentStoreInMemoryIndexTest, TryGetSingleResIdOneEntryLarge) {
  SqlPersistentStoreInMemoryIndex index;
  const CacheEntryKeyHash kHashLarge(3);
  const SqlPersistentStoreResId kResIdLarge(
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 1);

  EXPECT_TRUE(index.Insert(kHashLarge, kResIdLarge));
  EXPECT_THAT(index.TryGetSingleResId(kHashLarge), kResIdLarge);
}

TEST(SqlPersistentStoreInMemoryIndexTest,
     TryGetSingleResIdCollisionSmallSmall) {
  SqlPersistentStoreInMemoryIndex index;
  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  EXPECT_TRUE(index.Insert(kHash1, kResId2));

  // Should fail because there are multiple entries for the same hash.
  EXPECT_EQ(index.TryGetSingleResId(kHash1), std::nullopt);

  // After removing one, it should be unique again.
  EXPECT_TRUE(index.Remove(kResId2));
  EXPECT_THAT(index.TryGetSingleResId(kHash1), kResId1);
}

TEST(SqlPersistentStoreInMemoryIndexTest,
     TryGetSingleResIdCollisionLargeLarge) {
  SqlPersistentStoreInMemoryIndex index;
  const CacheEntryKeyHash kHashLarge(3);
  const SqlPersistentStoreResId kResIdLarge1(
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 1);
  const SqlPersistentStoreResId kResIdLarge2(
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 2);

  EXPECT_TRUE(index.Insert(kHashLarge, kResIdLarge1));
  EXPECT_TRUE(index.Insert(kHashLarge, kResIdLarge2));

  // Should fail because there are multiple entries for the same hash.
  EXPECT_EQ(index.TryGetSingleResId(kHashLarge), std::nullopt);

  EXPECT_TRUE(index.Remove(kResIdLarge2));
  EXPECT_THAT(index.TryGetSingleResId(kHashLarge), kResIdLarge1);
}

TEST(SqlPersistentStoreInMemoryIndexTest,
     TryGetSingleResIdCollisionSmallLarge) {
  SqlPersistentStoreInMemoryIndex index;
  const CacheEntryKeyHash kHashCollision(10);
  const SqlPersistentStoreResId kResIdSmall(100);
  const SqlPersistentStoreResId kResIdLarge(
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 1);

  EXPECT_TRUE(index.Insert(kHashCollision, kResIdSmall));
  EXPECT_TRUE(index.Insert(kHashCollision, kResIdLarge));

  // Should fail because there are multiple entries for the same hash.
  EXPECT_EQ(index.TryGetSingleResId(kHashCollision), std::nullopt);

  EXPECT_TRUE(index.Remove(kResIdLarge));
  EXPECT_THAT(index.TryGetSingleResId(kHashCollision), kResIdSmall);
}

TEST(SqlPersistentStoreInMemoryIndexTest, EntryDataHintsOneEntrySmall) {
  SqlPersistentStoreInMemoryIndex index;
  const MemoryEntryDataHints kHints(1);

  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  index.SetEntryDataHints(kResId1, kHints);

  EXPECT_THAT(index.GetEntryDataHints(kHash1), kHints);
}

TEST(SqlPersistentStoreInMemoryIndexTest, EntryDataHintsOneEntryLarge) {
  SqlPersistentStoreInMemoryIndex index;
  const CacheEntryKeyHash kHashLarge(3);
  const SqlPersistentStoreResId kResIdLarge(
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 1);
  const MemoryEntryDataHints kHints(5);

  EXPECT_TRUE(index.Insert(kHashLarge, kResIdLarge));
  index.SetEntryDataHints(kResIdLarge, kHints);

  EXPECT_THAT(index.GetEntryDataHints(kHashLarge), kHints);
}

TEST(SqlPersistentStoreInMemoryIndexTest, EntryDataHintsCollisionSmallSmall) {
  SqlPersistentStoreInMemoryIndex index;
  const MemoryEntryDataHints kHints1(1);

  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  index.SetEntryDataHints(kResId1, kHints1);

  EXPECT_TRUE(index.Insert(kHash1, kResId2));

  // Even if we don't set hints for the second one, ambiguous hash lookups
  // should fail.
  EXPECT_EQ(index.GetEntryDataHints(kHash1), std::nullopt);

  EXPECT_TRUE(index.Remove(kResId2));
  EXPECT_THAT(index.GetEntryDataHints(kHash1), kHints1);
}

TEST(SqlPersistentStoreInMemoryIndexTest, EntryDataHintsCollisionLargeLarge) {
  SqlPersistentStoreInMemoryIndex index;
  const CacheEntryKeyHash kHashLarge(3);
  const SqlPersistentStoreResId kResIdLarge1(
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 1);
  const SqlPersistentStoreResId kResIdLarge2(
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 2);
  const MemoryEntryDataHints kHints(5);

  EXPECT_TRUE(index.Insert(kHashLarge, kResIdLarge1));
  index.SetEntryDataHints(kResIdLarge1, kHints);
  EXPECT_TRUE(index.Insert(kHashLarge, kResIdLarge2));

  EXPECT_EQ(index.GetEntryDataHints(kHashLarge), std::nullopt);

  EXPECT_TRUE(index.Remove(kResIdLarge2));
  EXPECT_THAT(index.GetEntryDataHints(kHashLarge), kHints);
}

TEST(SqlPersistentStoreInMemoryIndexTest, EntryDataHintsCollisionSmallLarge) {
  SqlPersistentStoreInMemoryIndex index;
  const CacheEntryKeyHash kHashCollision(10);
  const SqlPersistentStoreResId kResIdSmall(100);
  const SqlPersistentStoreResId kResIdLarge(
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 1);
  const MemoryEntryDataHints kHints(1);

  EXPECT_TRUE(index.Insert(kHashCollision, kResIdSmall));
  index.SetEntryDataHints(kResIdSmall, kHints);
  EXPECT_TRUE(index.Insert(kHashCollision, kResIdLarge));

  EXPECT_EQ(index.GetEntryDataHints(kHashCollision), std::nullopt);

  EXPECT_TRUE(index.Remove(kResIdLarge));
  EXPECT_THAT(index.GetEntryDataHints(kHashCollision), kHints);
}

TEST(SqlPersistentStoreInMemoryIndexTest, EntryDataHintsNoHint) {
  SqlPersistentStoreInMemoryIndex index;
  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  EXPECT_EQ(index.GetEntryDataHints(kHash1), std::nullopt);
}

}  // namespace disk_cache
