// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/ng_shape_cache.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"

namespace blink {

class NGShapeCacheTest : public FontTestBase {
 protected:
  void SetUp() override { cache = std::make_unique<NGShapeCache>(); }
  std::unique_ptr<NGShapeCache> cache;
};

TEST_F(NGShapeCacheTest, AddEntriesAndCacheHits) {
  // Adding an entry is successful.
  ShapeCacheEntry* entry_A_LTR = cache->Add("A", TextDirection::kLtr);
  ASSERT_TRUE(entry_A_LTR);

  // Adding the same entry again hits cache.
  EXPECT_EQ(cache->Add("A", TextDirection::kLtr), entry_A_LTR);

  // Adding the an entry with different text does not hit cache.
  ShapeCacheEntry* entry_B_LTR = cache->Add("B", TextDirection::kLtr);
  ASSERT_TRUE(entry_B_LTR);
  EXPECT_NE(entry_B_LTR, entry_A_LTR);

  // Adding the same entry again hits cache.
  EXPECT_EQ(cache->Add("B", TextDirection::kLtr), entry_B_LTR);

  // Adding the an entry with different direction does not hit cache.
  ShapeCacheEntry* entry_A_RTL = cache->Add("A", TextDirection::kRtl);
  ASSERT_TRUE(entry_A_RTL);
  EXPECT_NE(entry_A_RTL, entry_A_LTR);
  EXPECT_NE(entry_A_RTL, entry_B_LTR);

  // Adding the same entry again hits cache.
  EXPECT_EQ(cache->Add("A", TextDirection::kRtl), entry_A_RTL);
}

}  // namespace blink
