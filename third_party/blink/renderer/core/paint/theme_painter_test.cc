// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/theme_painter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/theme_types.h"

namespace blink {
namespace {

gfx::Rect SnapAt(float offset_y,
                 float width,
                 float height,
                 AppearanceValue appearance) {
  return ThemePainter::SnapRectForPaint(
      PhysicalRect(PhysicalOffset(LayoutUnit(), LayoutUnit(offset_y)),
                   PhysicalSize(LayoutUnit(width), LayoutUnit(height))),
      appearance);
}

// A fractional-size checkbox must paint at the same size no matter where it
// sits on the pixel grid (crbug.com/41288048).
TEST(ThemePainterTest, CheckboxPaintSizeIndependentOfPosition) {
  const gfx::Rect low = SnapAt(10.2, 13.4, 9.6, AppearanceValue::kCheckbox);
  const gfx::Rect high = SnapAt(10.6, 13.4, 9.6, AppearanceValue::kCheckbox);
  EXPECT_EQ(low.size(), high.size());
  EXPECT_EQ(gfx::Size(13, 10), low.size());
}

TEST(ThemePainterTest, RadioPaintSizeIndependentOfPosition) {
  EXPECT_EQ(SnapAt(10.2, 9.6, 9.6, AppearanceValue::kRadio).size(),
            SnapAt(10.6, 9.6, 9.6, AppearanceValue::kRadio).size());
}

// Whole-pixel controls, the common case, are not affected.
TEST(ThemePainterTest, IntegerSizedCheckboxIsEdgeSnappedAsBefore) {
  const PhysicalRect paint_rect(PhysicalOffset(LayoutUnit(), LayoutUnit(10.2)),
                                PhysicalSize(LayoutUnit(13), LayoutUnit(13)));
  EXPECT_EQ(
      ToPixelSnappedRect(paint_rect),
      ThemePainter::SnapRectForPaint(paint_rect, AppearanceValue::kCheckbox));
}

// Other parts keep snapping both edges, so their bottom edge still lands on
// the nearest pixel boundary and stays flush with adjacent boxes.
TEST(ThemePainterTest, NonCheckboxKeepsEdgeSnappedSize) {
  EXPECT_EQ(10, SnapAt(10.2, 13.4, 9.6, AppearanceValue::kTextField).height());
  EXPECT_EQ(9, SnapAt(10.6, 13.4, 9.6, AppearanceValue::kTextField).height());
}

TEST(ThemePainterTest, CheckboxKeepsEdgeSnappingWithFeatureDisabled) {
  ScopedConsistentFormControlPaintSnappingForTest disabled(false);
  EXPECT_EQ(10, SnapAt(10.2, 13.4, 9.6, AppearanceValue::kCheckbox).height());
  EXPECT_EQ(9, SnapAt(10.6, 13.4, 9.6, AppearanceValue::kCheckbox).height());
}

}  // namespace
}  // namespace blink
