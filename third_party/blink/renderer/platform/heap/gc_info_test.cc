// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/gc_info.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(GCInfoTest, InitialEmpty) {
  GCInfoTable table;
  EXPECT_EQ(0u, table.GcInfoIndex());
}

TEST(GCInfoTest, ResizeToMaxIndex) {
  GCInfoTable table;
  GCInfo info = {nullptr, nullptr, nullptr, false};
  std::atomic<std::uint32_t> slot{0};
  for (uint32_t i = 0; i < (GCInfoTable::kMaxIndex - 1); i++) {
    slot = 0;
    uint32_t index = table.EnsureGCInfoIndex(&info, &slot);
    EXPECT_EQ(index, slot);
    EXPECT_LT(0u, slot);
    EXPECT_EQ(&info, table.GCInfoFromIndex(slot));
  }
}

}  // namespace blink
