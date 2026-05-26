// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_persistent_store_in_memory_index.h"

#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {

namespace {

const CacheEntryKeyHash kHash1(1);
const SqlPersistentStoreResId kResId1(1);
const CacheEntryKeyHash kHash2(2);
const SqlPersistentStoreResId kResId2(2);

}  // namespace

class SqlPersistentStoreInMemoryIndexTest
    : public testing::TestWithParam<bool> {
 public:
  SqlPersistentStoreInMemoryIndexTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        net::features::kDiskCacheBackendExperiment,
        {{"SqlDiskCacheConsolidatedInMemoryIndex",
          GetParam() ? "true" : "false"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(SqlPersistentStoreInMemoryIndexTest, Insert) {
  SqlPersistentStoreInMemoryIndex index;
  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  EXPECT_TRUE(index.Contains(kHash1));
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, InsertDuplicateResId) {
  SqlPersistentStoreInMemoryIndex index;
  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  EXPECT_TRUE(index.Insert(kHash2, kResId1));
  EXPECT_TRUE(index.Contains(kHash1));
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, InsertSameHash) {
  SqlPersistentStoreInMemoryIndex index;
  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  EXPECT_TRUE(index.Insert(kHash1, kResId2));
  EXPECT_TRUE(index.Contains(kHash1));
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, RemoveWithHashAndResId) {
  SqlPersistentStoreInMemoryIndex index;
  index.Insert(kHash1, kResId1);
  EXPECT_TRUE(index.Remove(kHash1, kResId1));
  EXPECT_FALSE(index.Contains(kHash1));
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, RemoveWithResId) {
  SqlPersistentStoreInMemoryIndex index;
  index.Insert(kHash1, kResId1);
  EXPECT_TRUE(index.Remove(kHash1, kResId1));
  EXPECT_FALSE(index.Contains(kHash1));
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, RemoveNonExistent) {
  SqlPersistentStoreInMemoryIndex index;
  index.Insert(kHash1, kResId1);
  EXPECT_FALSE(index.Remove(kHash2, kResId2));
  EXPECT_FALSE(index.Remove(kHash2, kResId1));
  EXPECT_FALSE(index.Remove(kHash1, kResId2));
  EXPECT_TRUE(index.Contains(kHash1));
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, Clear) {
  SqlPersistentStoreInMemoryIndex index;
  index.Insert(kHash1, kResId1);
  index.Insert(kHash2, kResId2);
  index.Clear();
  EXPECT_FALSE(index.Contains(kHash1));
  EXPECT_FALSE(index.Contains(kHash2));
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, MultipleEntries) {
  SqlPersistentStoreInMemoryIndex index;
  index.Insert(kHash1, kResId1);
  index.Insert(kHash2, kResId2);

  EXPECT_TRUE(index.Contains(kHash1));
  EXPECT_TRUE(index.Contains(kHash2));

  EXPECT_TRUE(index.Remove(kHash1, kResId1));
  EXPECT_FALSE(index.Contains(kHash1));
  EXPECT_TRUE(index.Contains(kHash2));

  EXPECT_TRUE(index.Remove(kHash2, kResId2));
  EXPECT_FALSE(index.Contains(kHash2));
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, BehavesCorrectlyWithBothMaps) {
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
  EXPECT_TRUE(index.Remove(kHash1, kResId1));
  EXPECT_EQ(1u, index.size());
  EXPECT_FALSE(index.Contains(kHash1));

  // Trying to remove the already removed entry should fail.
  EXPECT_FALSE(index.Remove(kHash1, kResId1));

  // It should be possible to re-insert and remove the same entry.
  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  EXPECT_TRUE(index.Remove(kHash1, kResId1));

  // Remove the entry from the 64-bit map.
  EXPECT_TRUE(index.Remove(kHashLarge, kResIdLarge));
  EXPECT_EQ(0u, index.size());

  // It should be possible to re-insert and remove the same entry.
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

TEST_P(SqlPersistentStoreInMemoryIndexTest, MoveOperations) {
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

TEST_P(SqlPersistentStoreInMemoryIndexTest, MoveOperationsWithResId64) {
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

TEST_P(SqlPersistentStoreInMemoryIndexTest, TryGetSingleResIdNoEntry) {
  SqlPersistentStoreInMemoryIndex index;
  EXPECT_EQ(index.TryGetSingleResId(kHash1), std::nullopt);
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, TryGetSingleResIdOneEntrySmall) {
  SqlPersistentStoreInMemoryIndex index;
  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  EXPECT_THAT(index.TryGetSingleResId(kHash1), kResId1);
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, TryGetSingleResIdOneEntryLarge) {
  SqlPersistentStoreInMemoryIndex index;
  const CacheEntryKeyHash kHashLarge(3);
  const SqlPersistentStoreResId kResIdLarge(
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 1);

  EXPECT_TRUE(index.Insert(kHashLarge, kResIdLarge));
  EXPECT_THAT(index.TryGetSingleResId(kHashLarge), kResIdLarge);
}

TEST_P(SqlPersistentStoreInMemoryIndexTest,
       TryGetSingleResIdCollisionSmallSmall) {
  SqlPersistentStoreInMemoryIndex index;
  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  EXPECT_TRUE(index.Insert(kHash1, kResId2));

  // Should fail because there are multiple entries for the same hash.
  EXPECT_EQ(index.TryGetSingleResId(kHash1), std::nullopt);

  // After removing one, it should be unique again.
  EXPECT_TRUE(index.Remove(kHash1, kResId2));
  EXPECT_THAT(index.TryGetSingleResId(kHash1), kResId1);
}

TEST_P(SqlPersistentStoreInMemoryIndexTest,
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

  EXPECT_TRUE(index.Remove(kHashLarge, kResIdLarge2));
  EXPECT_THAT(index.TryGetSingleResId(kHashLarge), kResIdLarge1);
}

TEST_P(SqlPersistentStoreInMemoryIndexTest,
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

  EXPECT_TRUE(index.Remove(kHashCollision, kResIdLarge));
  EXPECT_THAT(index.TryGetSingleResId(kHashCollision), kResIdSmall);
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, EntryDataHintsOneEntrySmall) {
  SqlPersistentStoreInMemoryIndex index;
  const MemoryEntryDataHints kHints(1);

  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  index.SetEntryDataHints(kHash1, kResId1, kHints);

  EXPECT_THAT(index.GetEntryDataHints(kHash1), kHints);
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, EntryDataHintsOneEntryLarge) {
  SqlPersistentStoreInMemoryIndex index;
  const CacheEntryKeyHash kHashLarge(3);
  const SqlPersistentStoreResId kResIdLarge(
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 1);
  const MemoryEntryDataHints kHints(3);

  EXPECT_TRUE(index.Insert(kHashLarge, kResIdLarge));
  index.SetEntryDataHints(kHashLarge, kResIdLarge, kHints);

  EXPECT_THAT(index.GetEntryDataHints(kHashLarge), kHints);
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, EntryDataHintsCollisionSmallSmall) {
  SqlPersistentStoreInMemoryIndex index;
  const MemoryEntryDataHints kHints1(1);

  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  index.SetEntryDataHints(kHash1, kResId1, kHints1);

  EXPECT_TRUE(index.Insert(kHash1, kResId2));

  // Even if we don't set hints for the second one, ambiguous hash lookups
  // should fail.
  EXPECT_EQ(index.GetEntryDataHints(kHash1), std::nullopt);

  EXPECT_TRUE(index.Remove(kHash1, kResId2));
  EXPECT_THAT(index.GetEntryDataHints(kHash1), kHints1);
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, EntryDataHintsCollisionLargeLarge) {
  SqlPersistentStoreInMemoryIndex index;
  const CacheEntryKeyHash kHashLarge(3);
  const SqlPersistentStoreResId kResIdLarge1(
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 1);
  const SqlPersistentStoreResId kResIdLarge2(
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 2);
  const MemoryEntryDataHints kHints(3);

  EXPECT_TRUE(index.Insert(kHashLarge, kResIdLarge1));
  index.SetEntryDataHints(kHashLarge, kResIdLarge1, kHints);
  EXPECT_TRUE(index.Insert(kHashLarge, kResIdLarge2));

  EXPECT_EQ(index.GetEntryDataHints(kHashLarge), std::nullopt);

  EXPECT_TRUE(index.Remove(kHashLarge, kResIdLarge2));
  EXPECT_THAT(index.GetEntryDataHints(kHashLarge), kHints);
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, EntryDataHintsCollisionSmallLarge) {
  SqlPersistentStoreInMemoryIndex index;
  const CacheEntryKeyHash kHashCollision(10);
  const SqlPersistentStoreResId kResIdSmall(100);
  const SqlPersistentStoreResId kResIdLarge(
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 1);
  const MemoryEntryDataHints kHints(1);

  EXPECT_TRUE(index.Insert(kHashCollision, kResIdSmall));
  index.SetEntryDataHints(kHashCollision, kResIdSmall, kHints);
  EXPECT_TRUE(index.Insert(kHashCollision, kResIdLarge));

  EXPECT_EQ(index.GetEntryDataHints(kHashCollision), std::nullopt);

  EXPECT_TRUE(index.Remove(kHashCollision, kResIdLarge));
  EXPECT_THAT(index.GetEntryDataHints(kHashCollision), kHints);
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, EntryDataHintsNoHint) {
  SqlPersistentStoreInMemoryIndex index;
  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  EXPECT_EQ(index.GetEntryDataHints(kHash1), std::nullopt);
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, EntryDataHintsCollision32Unique64) {
  SqlPersistentStoreInMemoryIndex index;
  const SqlPersistentStoreResId kResId32_1(100);
  const SqlPersistentStoreResId kResId32_2(101);
  const SqlPersistentStoreResId kResId64(
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 1);
  const MemoryEntryDataHints kHints(1);

  EXPECT_TRUE(index.Insert(kHash1, kResId32_1));
  EXPECT_TRUE(index.Insert(kHash1, kResId32_2));
  EXPECT_TRUE(index.Insert(kHash1, kResId64));
  index.SetEntryDataHints(kHash1, kResId64, kHints);

  // This should be nullopt because of the collision in the 32-bit map.
  EXPECT_EQ(index.GetEntryDataHints(kHash1), std::nullopt);
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, EntryDataHintsCollision64Unique32) {
  SqlPersistentStoreInMemoryIndex index;
  const SqlPersistentStoreResId kResId32(100);
  const SqlPersistentStoreResId kResId64_1(
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 1);
  const SqlPersistentStoreResId kResId64_2(
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 2);
  const MemoryEntryDataHints kHints(1);

  EXPECT_TRUE(index.Insert(kHash1, kResId32));
  EXPECT_TRUE(index.Insert(kHash1, kResId64_1));
  EXPECT_TRUE(index.Insert(kHash1, kResId64_2));
  index.SetEntryDataHints(kHash1, kResId32, kHints);

  // This should be nullopt because of the collision in the 64-bit map.
  EXPECT_EQ(index.GetEntryDataHints(kHash1), std::nullopt);
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, GetResIdsWithHints) {
  SqlPersistentStoreInMemoryIndex index;
  const CacheEntryKeyHash kHashLarge(3);
  const SqlPersistentStoreResId kResIdLarge(
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 1);
  const CacheEntryKeyHash kHashBoth(4);
  const SqlPersistentStoreResId kResIdBoth(4);

  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  EXPECT_TRUE(index.Insert(kHash2, kResId2));
  EXPECT_TRUE(index.Insert(kHashLarge, kResIdLarge));
  EXPECT_TRUE(index.Insert(kHashBoth, kResIdBoth));

  const MemoryEntryDataHints kHint1(1 << 0);
  const MemoryEntryDataHints kHint2(1 << 1);

  // Set hints.
  index.SetEntryDataHints(kHash1, kResId1, kHint1);
  index.SetEntryDataHints(kHash2, kResId2, kHint2);
  index.SetEntryDataHints(kHashLarge, kResIdLarge, kHint1);
  index.SetEntryDataHints(
      kHashBoth, kResIdBoth,
      MemoryEntryDataHints(kHint1.value() | kHint2.value()));

  // Entries with kHint1 (at least).
  EXPECT_THAT(index.GetResIdsWithHints(kHint1),
              testing::UnorderedElementsAre(kResId1, kResIdLarge, kResIdBoth));

  // Entries with kHint2 (at least).
  EXPECT_THAT(index.GetResIdsWithHints(kHint2),
              testing::UnorderedElementsAre(kResId2, kResIdBoth));

  // Entries that have BOTH hints.
  EXPECT_THAT(index.GetResIdsWithHints(
                  MemoryEntryDataHints(kHint1.value() | kHint2.value())),
              testing::UnorderedElementsAre(kResIdBoth));
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, ConsolidatedFlag) {
  SqlPersistentStoreInMemoryIndex index;
  EXPECT_EQ(index.IsConsolidatedInMemoryIndexEnabled(), GetParam());
}

TEST_P(SqlPersistentStoreInMemoryIndexTest,
       GetEntryDataHintsReturnsNulloptOnCollision) {
  SqlPersistentStoreInMemoryIndex index;
  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  EXPECT_TRUE(index.Insert(kHash1, kResId2));

  // Case 1: No entries for the hash.
  EXPECT_EQ(index.GetEntryDataHints(kHash2), std::nullopt);

  // Case 2: Multiple entries for the hash (collision).
  EXPECT_EQ(index.GetEntryDataHints(kHash1), std::nullopt);
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, ForEach) {
  if (!GetParam()) {
    // ForEach is only supported when consolidated in-memory index is enabled.
    return;
  }
  SqlPersistentStoreInMemoryIndex index;
  const MemoryEntryDataHints hints(1);
  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  index.SetEntryDataHints(kHash1, kResId1, hints);

  size_t call_count = 0;
  index.ForEach([&](CacheEntryKeyHash hash, SqlPersistentStoreResId res_id,
                    MemoryEntryDataHints in_memory_data) {
    EXPECT_EQ(hash, kHash1);
    EXPECT_EQ(res_id, kResId1);
    EXPECT_EQ(in_memory_data, hints);
    ++call_count;
  });
  EXPECT_EQ(call_count, 1u);

  // Test impl64_ path
  const CacheEntryKeyHash kHashLarge(3);
  const SqlPersistentStoreResId kResIdLarge((static_cast<uint64_t>(1) << 33) +
                                            1);
  const MemoryEntryDataHints hints_large(2);
  EXPECT_TRUE(index.Insert(kHashLarge, kResIdLarge));
  index.SetEntryDataHints(kHashLarge, kResIdLarge, hints_large);
  call_count = 0;
  index.ForEach([&](CacheEntryKeyHash hash, SqlPersistentStoreResId res_id,
                    MemoryEntryDataHints in_memory_data) {
    EXPECT_TRUE(res_id == kResId1 || res_id == kResIdLarge);
    ++call_count;
    if (res_id == kResId1) {
      EXPECT_EQ(res_id, kResId1);
      EXPECT_EQ(in_memory_data, hints);
    } else if (res_id == kResIdLarge) {
      EXPECT_EQ(hash, kHashLarge);
      EXPECT_EQ(in_memory_data, hints_large);
    }
  });
  EXPECT_EQ(call_count, 2u);
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, SetEntryLastUsedAndUsage) {
  if (!GetParam()) {
    return;
  }
  SqlPersistentStoreInMemoryIndex index;
  const base::Time last_used = base::Time::FromSecondsSinceUnixEpoch(100);
  const uint64_t bytes_usage = 512;

  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  index.SetEntryLastUsedAndUsage(kHash1, kResId1, last_used, bytes_usage);

  std::optional<SqlPersistentStoreInMemoryIndex::Metadata> metadata =
      index.GetEntryMetadataForTesting(kHash1, kResId1);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_EQ(metadata->last_used, last_used);
  EXPECT_EQ(metadata->bytes_usage, bytes_usage);

  // Test impl64_ path
  const CacheEntryKeyHash kHashLarge(3);
  const SqlPersistentStoreResId kResIdLarge((static_cast<uint64_t>(1) << 33) +
                                            1);
  EXPECT_TRUE(index.Insert(kHashLarge, kResIdLarge));
  index.SetEntryLastUsedAndUsage(kHashLarge, kResIdLarge, last_used,
                                 bytes_usage);

  metadata = index.GetEntryMetadataForTesting(kHashLarge, kResIdLarge);
  ASSERT_TRUE(metadata.has_value());
  EXPECT_EQ(metadata->last_used, last_used);
  EXPECT_EQ(metadata->bytes_usage, bytes_usage);
}

TEST_P(SqlPersistentStoreInMemoryIndexTest,
       SetEntryLastUsedAndUsageBoundaryValues) {
  if (!GetParam()) {
    return;
  }
  SqlPersistentStoreInMemoryIndex index;
  const base::Time last_used = base::Time::FromSecondsSinceUnixEpoch(100);

  EXPECT_TRUE(index.Insert(kHash1, kResId1));

  auto verify_usage = [&](uint64_t bytes_usage,
                          uint64_t expected_stored_usage) {
    index.SetEntryLastUsedAndUsage(kHash1, kResId1, last_used, bytes_usage);
    auto metadata = index.GetEntryMetadataForTesting(kHash1, kResId1);
    ASSERT_TRUE(metadata.has_value());
    EXPECT_EQ(metadata->bytes_usage, expected_stored_usage);
  };

  verify_usage(0, 0);
  verify_usage(1, 256);
  verify_usage(255, 256);
  verify_usage(256, 256);
  verify_usage(257, 512);

  const uint64_t kMaxChunks = (1ull << 30) - 1;
  const uint64_t kMaxBytes = kMaxChunks << 8;
  verify_usage(kMaxBytes, kMaxBytes);
  verify_usage(kMaxBytes + 1, kMaxBytes);  // Exceeds max, capped to kMaxBytes
  verify_usage(1ull << 40, kMaxBytes);     // Very large, capped to kMaxBytes
}

TEST_P(SqlPersistentStoreInMemoryIndexTest, GetEntryMetadataForTestingNullopt) {
  if (!GetParam()) {
    return;
  }
  SqlPersistentStoreInMemoryIndex index;
  const CacheEntryKeyHash kHashLarge(3);
  const SqlPersistentStoreResId kResIdLarge((static_cast<uint64_t>(1) << 33) +
                                            1);

  EXPECT_TRUE(index.Insert(kHash1, kResId1));
  EXPECT_TRUE(index.Insert(kHashLarge, kResIdLarge));

  // Test ConsolidatedImpl::GetEntryMetadataForTesting !entry
  EXPECT_EQ(index.GetEntryMetadataForTesting(kHash2, kResId1), std::nullopt);
  EXPECT_EQ(index.GetEntryMetadataForTesting(kHash2, kResIdLarge),
            std::nullopt);
}

TEST_P(SqlPersistentStoreInMemoryIndexTest,
       GetEntryMetadataForTestingNoImpl64) {
  if (!GetParam()) {
    return;
  }
  SqlPersistentStoreInMemoryIndex index;
  const CacheEntryKeyHash kHashLarge(3);
  const SqlPersistentStoreResId kResIdLarge((static_cast<uint64_t>(1) << 33) +
                                            1);
  // We don't insert a large ID, so impl64_ is not created.
  EXPECT_EQ(index.GetEntryMetadataForTesting(kHashLarge, kResIdLarge),
            std::nullopt);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SqlPersistentStoreInMemoryIndexTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Consolidated" : "Original";
                         });

}  // namespace disk_cache
