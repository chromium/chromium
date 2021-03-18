// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/impl/gc_info.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(GCInfoTest, InitialEmpty) {
  GCInfoTable table;
  EXPECT_EQ(GCInfoTable::kMinIndex, table.NumberOfGCInfos());
}

TEST(GCInfoTest, ResizeToMaxIndex) {
  GCInfoTable table;
  GCInfo info = {nullptr, nullptr, nullptr, false};
  std::atomic<GCInfoIndex> slot{0};
  for (GCInfoIndex i = GCInfoTable::kMinIndex; i < GCInfoTable::kMaxIndex;
       i++) {
    slot = 0;
    GCInfoIndex index = table.EnsureGCInfoIndex(&info, &slot);
    EXPECT_EQ(index, slot);
    EXPECT_LT(0u, slot);
    EXPECT_EQ(&info, &table.GCInfoFromIndex(index));
  }
}

TEST(GCInfoDeathTest, MoreThanMaxIndexInfos) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  GCInfoTable table;
  GCInfo info = {nullptr, nullptr, nullptr, false};
  std::atomic<GCInfoIndex> slot{0};
  // Create GCInfoTable::kMaxIndex entries.
  for (GCInfoIndex i = GCInfoTable::kMinIndex; i < GCInfoTable::kMaxIndex;
       i++) {
    slot = 0;
    table.EnsureGCInfoIndex(&info, &slot);
  }
  slot = 0;
  EXPECT_DEATH(table.EnsureGCInfoIndex(&info, &slot), "");
}

}  // namespace blink
