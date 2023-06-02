// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/fixed_size_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(FixedSizeCacheTest, Basic) {
  FixedSizeCache<int, int> cache;

  EXPECT_EQ(nullptr, cache.Find(1));

  cache.Insert(1, 100);
  ASSERT_NE(nullptr, cache.Find(1));
  EXPECT_EQ(100, *cache.Find(1));

  // Try to crowd out the element with things we'll never look for again.
  for (int i = 2; i < 10000; ++i) {
    cache.Insert(i, i * 100);
  }

  // 1 should still be visible due to the Find() above putting it into
  // a privileged spot (as should the last inserted value, because nothing
  // has been able to push it out yet).
  ASSERT_NE(nullptr, cache.Find(1));
  EXPECT_EQ(100, *cache.Find(1));

  ASSERT_NE(nullptr, cache.Find(9999));
  EXPECT_EQ(999900, *cache.Find(9999));
}

}  // namespace blink
