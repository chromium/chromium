// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

#if DCHECK_IS_ON()
TEST(DisplayItemTest, DebugStringsExist) {
  for (int type = 0; type <= DisplayItem::kTypeLast; type++) {
    String debug_string =
        DisplayItem::TypeAsDebugString(static_cast<DisplayItem::Type>(type));
    EXPECT_FALSE(debug_string.empty());
    EXPECT_NE("Unknown", debug_string);
  }
}
#endif  // DCHECK_IS_ON()

TEST(DisplayItemTest, AllZeroIsTombstone) {
  alignas(alignof(DisplayItem)) uint8_t buffer[sizeof(DisplayItem)] = {0};
  EXPECT_TRUE(reinterpret_cast<const DisplayItem*>(buffer)->IsTombstone());
}

}  // namespace
}  // namespace blink
