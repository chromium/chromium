// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/min_max_sizes_cache.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

TEST(MinMaxSizesCacheTest, Eviction) {
  test::TaskEnvironment task_environment;
  auto* cache = MakeGarbageCollected<MinMaxSizesCache>();

  // Populate the cache with the max number of entries.
  for (unsigned i = 0u; i < MinMaxSizesCache::kMaxCacheEntries; ++i) {
    cache->Add({LayoutUnit(), LayoutUnit()}, LayoutUnit(i), true);
  }

  // "find" the "0th" entry.
  cache->Find(LayoutUnit(0u));

  // Add a new entry to kick out the "1st" entry.
  cache->Add({LayoutUnit(), LayoutUnit()},
             LayoutUnit(MinMaxSizesCache::kMaxCacheEntries), true);

  EXPECT_TRUE(cache->Find(LayoutUnit(0u)).has_value());
  EXPECT_FALSE(cache->Find(LayoutUnit(1u)).has_value());
}

}  // namespace

}  // namespace blink
