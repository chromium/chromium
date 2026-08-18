// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/service_transfer_cache.h"

#include "base/memory_coordinator/memory_coordinator_features.h"
#include "base/memory_coordinator/utils.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time_override.h"
#include "cc/paint/raw_memory_transfer_cache_entry.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

constexpr int kDecoderId = 2;
constexpr auto kEntryType = cc::TransferCacheEntryType::kRawMemory;

std::unique_ptr<cc::ServiceTransferCacheEntry> CreateEntry(size_t size) {
  auto entry = std::make_unique<cc::ServiceRawMemoryTransferCacheEntry>();
  std::vector<uint8_t> data(size, 0u);
  entry->Deserialize(/*gr_context=*/nullptr, /*graphite_recorder=*/nullptr,
                     data);
  return entry;
}

TEST(ServiceTransferCacheTest, EnforcesOnReleaseMemory) {
  ServiceTransferCache cache{GpuPreferences(), base::RepeatingClosure()};
  uint32_t entry_id = 0u;
  size_t entry_size = 1024u;
  uint32_t number_of_entry = 4u;

  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(kDecoderId, kEntryType, ++entry_id),
      CreateEntry(entry_size));
  EXPECT_EQ(cache.cache_size_for_testing(), entry_size);
  cache.OnReleaseMemory(base::kCriticalMemoryPressureThreshold);
  EXPECT_EQ(cache.cache_size_for_testing(), 0u);

  cache.SetCacheSizeLimitForTesting(entry_size * number_of_entry);
  // Create 4 entries, all in the cache.
  for (uint32_t i = 0; i < number_of_entry; i++) {
    cache.CreateLocalEntry(
        ServiceTransferCache::EntryKey(kDecoderId, kEntryType, ++entry_id),
        CreateEntry(entry_size));
    EXPECT_EQ(cache.cache_size_for_testing(), entry_size * (i + 1));
  }

  // The 5th entry creates successfully. But the 1st entry will be purged due to
  // cache limits.
  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(kDecoderId, kEntryType, ++entry_id),
      CreateEntry(entry_size));
  EXPECT_EQ(cache.cache_size_for_testing(), entry_size * 4);

  cache.OnReleaseMemory(base::kModerateMemoryPressureThreshold);
  // Only 1/4 of cache limits remains.
  EXPECT_EQ(cache.cache_size_for_testing(), entry_size);
}

TEST(ServiceTransferCacheTest, OnUpdateMemoryLimitStateful) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(base::kStatefulMemoryPressure);

  ServiceTransferCache cache{GpuPreferences(), base::RepeatingClosure()};
  uint32_t entry_id = 0u;
  size_t entry_size = 1024u;
  uint32_t number_of_entry = 4u;

  cache.SetCacheSizeLimitForTesting(entry_size * number_of_entry);
  for (uint32_t i = 0; i < number_of_entry; i++) {
    cache.CreateLocalEntry(
        ServiceTransferCache::EntryKey(kDecoderId, kEntryType, ++entry_id),
        CreateEntry(entry_size));
  }
  EXPECT_EQ(cache.cache_size_for_testing(), entry_size * 4);

  // Moderate memory limit (50%). OnUpdateMemoryLimit must NOT release memory,
  // so existing usage remains 4 * entry_size.
  cache.OnUpdateMemoryLimit(base::kModerateMemoryPressureThreshold);
  EXPECT_EQ(cache.cache_size_for_testing(), entry_size * 4);

  // OnReleaseMemory performs the actual eviction down to 1/4 (1024 bytes).
  cache.OnReleaseMemory(base::kModerateMemoryPressureThreshold);
  EXPECT_EQ(cache.cache_size_for_testing(), entry_size);

  // Critical memory pressure (0%) purges all entries and persists limit = 0.
  cache.OnReleaseMemory(base::kCriticalMemoryPressureThreshold);
  EXPECT_EQ(cache.cache_size_for_testing(), 0u);
  // Creating a new entry while critical pressure persists immediately evicts
  // it.
  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(kDecoderId, kEntryType, ++entry_id),
      CreateEntry(entry_size));
  EXPECT_EQ(cache.cache_size_for_testing(), 0u);

  // Restore limit to 100%
  cache.OnUpdateMemoryLimit(base::kNoMemoryPressureThreshold);
  cache.OnReleaseMemory(base::kNoMemoryPressureThreshold);
  // Now we can add up to 4 entries again
  for (uint32_t i = 0; i < 4; i++) {
    cache.CreateLocalEntry(
        ServiceTransferCache::EntryKey(kDecoderId, kEntryType, ++entry_id),
        CreateEntry(entry_size));
  }
  EXPECT_EQ(cache.cache_size_for_testing(), entry_size * 4);
}

TEST(ServiceTransferCache, MultipleDecoderUse) {
  ServiceTransferCache cache{GpuPreferences(), base::RepeatingClosure()};
  const uint32_t entry_id = 0u;
  const size_t entry_size = 1024u;

  // Decoder 1 entry.
  int decoder1 = 1;
  auto decoder_1_entry = CreateEntry(entry_size);
  auto* decoder_1_entry_ptr = decoder_1_entry.get();
  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(decoder1, kEntryType, entry_id),
      std::move(decoder_1_entry));

  // Decoder 2 entry.
  int decoder2 = 2;
  auto decoder_2_entry = CreateEntry(entry_size);
  auto* decoder_2_entry_ptr = decoder_2_entry.get();
  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(decoder2, kEntryType, entry_id),
      std::move(decoder_2_entry));

  EXPECT_EQ(decoder_1_entry_ptr, cache.GetEntry(ServiceTransferCache::EntryKey(
                                     decoder1, kEntryType, entry_id)));
  EXPECT_EQ(decoder_2_entry_ptr, cache.GetEntry(ServiceTransferCache::EntryKey(
                                     decoder2, kEntryType, entry_id)));
}

TEST(ServiceTransferCache, DeleteEntriesForDecoder) {
  ServiceTransferCache cache{GpuPreferences(), base::RepeatingClosure()};
  const size_t entry_size = 1024u;
  const size_t cache_size = 4 * entry_size;
  cache.SetCacheSizeLimitForTesting(cache_size);

  // Add 2 entries for decoder 1.
  cache.CreateLocalEntry(ServiceTransferCache::EntryKey(1, kEntryType, 1),
                         CreateEntry(entry_size));
  cache.CreateLocalEntry(ServiceTransferCache::EntryKey(1, kEntryType, 2),
                         CreateEntry(entry_size));

  // Add 2 entries for decoder 2.
  cache.CreateLocalEntry(ServiceTransferCache::EntryKey(2, kEntryType, 1),
                         CreateEntry(entry_size));
  cache.CreateLocalEntry(ServiceTransferCache::EntryKey(2, kEntryType, 2),
                         CreateEntry(entry_size));

  // Erase all entries for decoder 1.
  EXPECT_EQ(cache.entries_count_for_testing(), 4u);
  cache.DeleteAllEntriesForDecoder(1);
  EXPECT_EQ(cache.entries_count_for_testing(), 2u);
  EXPECT_NE(cache.GetEntry(ServiceTransferCache::EntryKey(2, kEntryType, 1)),
            nullptr);
  EXPECT_NE(cache.GetEntry(ServiceTransferCache::EntryKey(2, kEntryType, 2)),
            nullptr);
}

TEST(ServiceTransferCacheTest, PurgeEntryOnTimer) {
  static base::TimeTicks now_value = base::TimeTicks::Now();
  base::subtle::ScopedTimeClockOverrides time_override(
      nullptr, []() { return now_value; }, nullptr);

  bool flush_called = false;
  ServiceTransferCache cache{
      GpuPreferences(),
      base::BindLambdaForTesting([&]() { flush_called = true; })};

  uint32_t entry_id = 0u;
  size_t entry_size = 1024u;
  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(kDecoderId, kEntryType, ++entry_id),
      CreateEntry(entry_size));
  EXPECT_EQ(cache.entries_count_for_testing(), 1u);

  now_value = now_value + base::Minutes(1);
  cache.PruneOldEntries();
  EXPECT_EQ(cache.entries_count_for_testing(), 0u);
  EXPECT_TRUE(flush_called);
}

}  // namespace gpu
