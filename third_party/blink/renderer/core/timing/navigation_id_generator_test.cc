// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/navigation_id_generator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
constexpr uint32_t kMinNavigationId = 100;
constexpr uint32_t kMaxNavigationId = std::numeric_limits<int32_t>::max();
constexpr uint32_t kMaxNavigationIdForReset = 10 * 1000;

TEST(NavigationIdGeneratorTest, InitialValueIsRandomForHardNavigations) {
  NavigationIdGenerator generator;
  uint32_t id = generator.NavigationId();
  EXPECT_NE(id, kNavigationIdAbsentValue);
  EXPECT_GE(id, kMinNavigationId);
  EXPECT_LE(id, kMaxNavigationIdForReset);
}

TEST(NavigationIdGeneratorTest, HardNavigations) {
  std::vector<uint32_t> ids;
  for (int i = 0; i < 100; ++i) {
    ids.push_back(NavigationIdGenerator().NavigationId());
  }
  // Check that the ids are mostly unique - but we allow 10 collisions,
  // since the ids are randomly generated between 100 and 10000.
  auto last = std::unique(ids.begin(), ids.end());
  auto num_collisions = std::distance(last, ids.end());
  EXPECT_LT(num_collisions, 10u);
  ids.erase(last, ids.end());
  // Check that all ids are in the expected range.
  for (uint32_t id : ids) {
    EXPECT_GE(id, kMinNavigationId);
    EXPECT_LE(id, kMaxNavigationIdForReset);
  }
  // Check that the ids are not appearing in order.
  std::vector<uint32_t> sorted_ids(ids.begin(), ids.end());
  std::sort(sorted_ids.begin(), sorted_ids.end());
  EXPECT_NE(sorted_ids, ids);
}

TEST(NavigationIdGeneratorTest, SoftNavigations) {
  NavigationIdGenerator generator;  // Starts with a hard navigation.
  std::vector<uint32_t> ids;
  ids.push_back(generator.NavigationId());
  for (int i = 0; i < 100; ++i) {
    generator.IncrementNavigationId();
    ids.push_back(generator.NavigationId());
  }
  // Check that all ids are unique.
  auto last = std::unique(ids.begin(), ids.end());
  EXPECT_EQ(last, ids.end());
  // Check that all ids are in the expected range.
  for (uint32_t id : ids) {
    EXPECT_GE(id, kMinNavigationId);
    EXPECT_LE(id, kMaxNavigationId);
  }
  // Check that the ids are appearing in order.
  std::vector<uint32_t> sorted_ids(ids.begin(), ids.end());
  std::sort(sorted_ids.begin(), sorted_ids.end());
  EXPECT_EQ(sorted_ids, ids);
}

TEST(NavigationIdGeneratorTest, SoftNavigationsOverflow) {
  NavigationIdGenerator generator;
  generator.navigation_id_ = kMaxNavigationId - 1;
  generator.IncrementNavigationId();
  uint32_t id = generator.NavigationId();
  EXPECT_GE(id, kMinNavigationId);
  EXPECT_LE(id, kMaxNavigationId);
  generator.IncrementNavigationId();
  EXPECT_GT(generator.NavigationId(), id);
}

}  // namespace blink
