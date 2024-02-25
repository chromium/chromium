// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/ng_shape_cache.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"

namespace blink {

class NGShapeCacheTest : public FontTestBase {
 protected:
  void SetUp() override { cache = MakeGarbageCollected<NGShapeCache>(); }
  Persistent<NGShapeCache> cache;
};

TEST_F(NGShapeCacheTest, AddEntriesAndCacheHits) {
  auto ShapeResultFunc = []() -> const ShapeResult* {
    // For the purposes of this test the actual internals of the shape result
    // doesn't matter.
    Font font;
    return MakeGarbageCollected<ShapeResult>(&font, 0, 0, TextDirection::kLtr);
  };

  // Adding an entry is successful.
  const auto* entry_A_LTR =
      cache->GetOrCreate("A", TextDirection::kLtr, ShapeResultFunc);
  ASSERT_TRUE(entry_A_LTR);

  // Adding the same entry again hits cache.
  EXPECT_EQ(cache->GetOrCreate("A", TextDirection::kLtr, ShapeResultFunc),
            entry_A_LTR);

  // Adding the an entry with different text does not hit cache.
  const auto* entry_B_LTR =
      cache->GetOrCreate("B", TextDirection::kLtr, ShapeResultFunc);
  ASSERT_TRUE(entry_B_LTR);
  EXPECT_NE(entry_B_LTR, entry_A_LTR);

  // Adding the same entry again hits cache.
  EXPECT_EQ(cache->GetOrCreate("B", TextDirection::kLtr, ShapeResultFunc),
            entry_B_LTR);

  // Adding the an entry with different direction does not hit cache.
  const auto* entry_A_RTL =
      cache->GetOrCreate("A", TextDirection::kRtl, ShapeResultFunc);
  ASSERT_TRUE(entry_A_RTL);
  EXPECT_NE(entry_A_RTL, entry_A_LTR);
  EXPECT_NE(entry_A_RTL, entry_B_LTR);

  // Adding the same entry again hits cache.
  EXPECT_EQ(cache->GetOrCreate("A", TextDirection::kRtl, ShapeResultFunc),
            entry_A_RTL);
}

}  // namespace blink
