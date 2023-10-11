// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/layout_rect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

TEST(DeprecatedLayoutRectTest, ToString) {
  DeprecatedLayoutRect empty_rect = DeprecatedLayoutRect();
  EXPECT_EQ("0,0 0x0", empty_rect.ToString());

  DeprecatedLayoutRect rect(1, 2, 3, 4);
  EXPECT_EQ("1,2 3x4", rect.ToString());

  DeprecatedLayoutRect granular_rect(LayoutUnit(1.6f), LayoutUnit(2.7f),
                                     LayoutUnit(3.8f), LayoutUnit(4.9f));
  EXPECT_EQ("1.59375,2.6875 3.796875x4.890625", granular_rect.ToString());
}

}  // namespace blink
