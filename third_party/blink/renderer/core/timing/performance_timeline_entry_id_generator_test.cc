// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_timeline_entry_id_generator.h"

#include <algorithm>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(PerformanceTimelineEntryIdGeneratorTest, InitialValueIsRandom) {
  PerformanceTimelineEntryIdGenerator generator;
  const PerformanceTimelineEntryIdInfo value = generator.GetValue();
  EXPECT_NE(value.id, PerformanceTimelineEntryIdInfo::kNoId);
  EXPECT_GE(value.id, PerformanceTimelineEntryIdInfo::kMinId);
  EXPECT_LE(value.id, PerformanceTimelineEntryIdInfo::kMaxIdForReset);
  EXPECT_EQ(value.offset, 0u);
}

TEST(PerformanceTimelineEntryIdGeneratorTest, ResetValues) {
  std::vector<uint64_t> ids;
  for (int i = 0; i < 100; ++i) {
    ids.push_back(PerformanceTimelineEntryIdGenerator().GetValue().id);
  }
  // Check that the ids are mostly unique - but we allow 10 collisions,
  // since the ids are randomly generated between 100 and 10000.
  auto last = std::unique(ids.begin(), ids.end());
  auto num_collisions = std::distance(last, ids.end());
  EXPECT_LT(num_collisions, 10u);
  ids.erase(last, ids.end());
  // Check that all ids are in the expected range.
  for (uint64_t id : ids) {
    EXPECT_GE(id, PerformanceTimelineEntryIdInfo::kMinId);
    EXPECT_LE(id, PerformanceTimelineEntryIdInfo::kMaxIdForReset);
  }
}

TEST(PerformanceTimelineEntryIdGeneratorTest, IncrementValues) {
  PerformanceTimelineEntryIdGenerator generator;
  std::vector<uint64_t> ids;
  ids.push_back(generator.GetValue().id);
  EXPECT_EQ(generator.GetValue().offset, 0u);
  for (uint32_t i = 1; i <= 100; ++i) {
    generator.IncrementId();
    ids.push_back(generator.GetValue().id);
    EXPECT_EQ(generator.GetValue().offset, i);
  }
  // Check that all ids are unique.
  auto last = std::unique(ids.begin(), ids.end());
  EXPECT_EQ(last, ids.end());
  // Check that all ids are in the expected range.
  for (uint64_t id : ids) {
    EXPECT_GE(id, PerformanceTimelineEntryIdInfo::kMinId);
    EXPECT_LE(id, PerformanceTimelineEntryIdInfo::kMaxId);
  }
  // Check that the ids are appearing in order.
  std::vector<uint64_t> sorted_ids(ids.begin(), ids.end());
  std::sort(sorted_ids.begin(), sorted_ids.end());
  EXPECT_EQ(sorted_ids, ids);
}

TEST(PerformanceTimelineEntryIdGeneratorTest, IdOverflow) {
  PerformanceTimelineEntryIdGenerator generator;
  // Test what happens when the id grows up to the limit of allowed values:
  generator.current_value_.id = PerformanceTimelineEntryIdInfo::kMaxId - 1;
  generator.current_value_.offset = 0;
  EXPECT_EQ(generator.GetValue().id,
            PerformanceTimelineEntryIdInfo::kMaxId - 1);

  generator.IncrementId();

  // Should reset id, because kMaxId - 1 + 7 > kMaxId
  const PerformanceTimelineEntryIdInfo value = generator.GetValue();
  EXPECT_GE(value.id, PerformanceTimelineEntryIdInfo::kMinId);
  EXPECT_LE(value.id, PerformanceTimelineEntryIdInfo::kMaxIdForReset);
  // ...But should still increment offset (count) without resetting.
  EXPECT_EQ(value.offset, 1u);

  generator.IncrementId();

  EXPECT_GT(generator.GetValue().id, value.id);
  EXPECT_EQ(generator.GetValue().offset, 2u);
}

}  // namespace blink
