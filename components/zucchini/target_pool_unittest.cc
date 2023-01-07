// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/target_pool.h"

#include <cmath>
#include <deque>
#include <string>
#include <utility>

#include "components/zucchini/image_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

namespace {

using OffsetDeque = std::deque<offset_t>;

}  // namespace

TEST(TargetPoolTest, InsertTargetsFromReferences) {
  auto test_insert = [](std::vector<Reference>&& references) -> OffsetDeque {
    TargetPool target_pool;
    target_pool.InsertTargets(references);
    // Return copy since |target_pool| goes out of scope.
    return target_pool.targets();
  };

  EXPECT_EQ(OffsetDeque(), test_insert({}));
  EXPECT_EQ(OffsetDeque({0, 1}), test_insert({{0, 0}, {10, 1}}));
  EXPECT_EQ(OffsetDeque({0, 1}), test_insert({{0, 1}, {10, 0}}));
  EXPECT_EQ(OffsetDeque({0, 1, 2}), test_insert({{0, 1}, {10, 0}, {20, 2}}));
  EXPECT_EQ(OffsetDeque({0}), test_insert({{0, 0}, {10, 0}}));
  EXPECT_EQ(OffsetDeque({0, 1}), test_insert({{0, 0}, {10, 0}, {20, 1}}));
}

TEST(TargetPoolTest, KeyOffset) {
  auto test_key_offset = [](const std::string& nearest_offsets_key,
                            OffsetDeque&& targets) {
    TargetPool target_pool(std::move(targets));
    for (offset_t offset : target_pool.targets()) {
      offset_t key = target_pool.KeyForOffset(offset);
      EXPECT_LT(key, target_pool.size());
      EXPECT_EQ(offset, target_pool.OffsetForKey(key));
    }
    for (offset_t offset = 0; offset < nearest_offsets_key.size(); ++offset) {
      key_t key = target_pool.KeyForNearestOffset(offset);
      EXPECT_EQ(key, static_cast<key_t>(nearest_offsets_key[offset] - '0'));
    }
  };
  test_key_offset("0000000000000000", {});
  test_key_offset("0000000000000000", {0});
  test_key_offset("0000000000000000", {1});
  test_key_offset("0111111111111111", {0, 1});
  test_key_offset("0011111111111111", {0, 2});
  test_key_offset("0011111111111111", {1, 2});
  test_key_offset("0001111111111111", {1, 3});
  test_key_offset("0001112223334444", {1, 3, 7, 9, 13});
  test_key_offset("0000011112223333", {1, 7, 9, 13});
}

}  // namespace zucchini
