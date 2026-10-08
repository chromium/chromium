// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/heap_auto_reset.h"

#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {
class GCed final : public GarbageCollected<GCed> {
 public:
  void Trace(Visitor*) const {}
};
}  // namespace

TEST(HeapAutoReset, MoveAssignSameVariable) {
  GCed* original_gced = MakeGarbageCollected<GCed>();
  GCed* gced = original_gced;
  {
    GCed* other_gced = MakeGarbageCollected<GCed>();
    HeapAutoReset<GCed> resetter(&gced, other_gced);
    EXPECT_EQ(other_gced, gced);
  }
  EXPECT_EQ(original_gced, gced);
  {
    HeapAutoReset<GCed> resetter(&gced, nullptr);
    EXPECT_EQ(nullptr, gced);
  }
  EXPECT_EQ(original_gced, gced);
}

}  // namespace blink
