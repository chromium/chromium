// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/contoured_rect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/infinite_int_rect.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(ContouredRectTest, ToString) {
  gfx::SizeF corner_rect(1, 2);
  ContouredRect rect_with_curvature(
      FloatRoundedRect(gfx::RectF(1, 3, 5, 7),
                       FloatRoundedRect::Radii(corner_rect, corner_rect,
                                               corner_rect, corner_rect)),
      ContouredRect::CornerCurvature(1, 0.2222, 0, 3000));
  EXPECT_EQ(
      "1,3 5x7 radii:(tl:1x2; tr:1x2; bl:1x2; br:1x2) curvature:(tl:1.00; "
      "tr:0.22; bl:3000.00; br:0.00)",
      rect_with_curvature.ToString());
}

}  // namespace blink
