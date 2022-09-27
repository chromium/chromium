// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/clip_rect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/paint/float_clip_rect.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ClipRectTest : public testing::Test {};

TEST_F(ClipRectTest, IsInfinite) {
  ClipRect rect;
  EXPECT_TRUE(rect.IsInfinite());

  rect.SetRect(FloatClipRect());
  EXPECT_TRUE(rect.IsInfinite());

  rect.SetRect(PhysicalRect());
  EXPECT_FALSE(rect.IsInfinite());
}

TEST_F(ClipRectTest, HasRadius) {
  ClipRect rect;
  EXPECT_FALSE(rect.HasRadius());

  rect.SetRect(FloatClipRect());
  EXPECT_FALSE(rect.HasRadius());

  FloatClipRect float_clip_rect;
  float_clip_rect.SetHasRadius();
  rect.SetRect(float_clip_rect);
  EXPECT_TRUE(rect.HasRadius());

  rect.SetRect(PhysicalRect());
  EXPECT_FALSE(rect.HasRadius());

  rect.SetHasRadius(true);
  EXPECT_TRUE(rect.HasRadius());

  rect.SetRect(FloatClipRect());
  EXPECT_FALSE(rect.HasRadius());
  EXPECT_TRUE(rect.IsInfinite());
}

TEST_F(ClipRectTest, IntersectClipRect) {
  ClipRect rect;
  rect.SetRect(PhysicalRect(100, 200, 300, 400));
  EXPECT_FALSE(rect.HasRadius());

  ClipRect rect2;
  rect2.SetRect(PhysicalRect(100, 100, 200, 300));
  rect2.SetHasRadius(true);
  rect.Intersect(rect2);
  EXPECT_TRUE(rect.HasRadius());
  EXPECT_FALSE(rect.IsInfinite());
  EXPECT_EQ(PhysicalRect(100, 200, 200, 200), rect.Rect());
}

TEST_F(ClipRectTest, IntersectEmptyRect) {
  ClipRect rect;
  rect.Intersect(PhysicalRect());
  EXPECT_FALSE(rect.IsInfinite());
}

TEST_F(ClipRectTest, IntersectsInfinite) {
  ClipRect rect;

  EXPECT_TRUE(rect.Intersects(HitTestLocation(gfx::PointF(100000, -3333333))));
}

TEST_F(ClipRectTest, ToString) {
  ClipRect rect;
  rect.SetRect(PhysicalRect(0, 0, 100, 100));
  EXPECT_EQ(String("0,0 100x100 noRadius notInfinite"), rect.ToString());

  rect.SetHasRadius(true);
  EXPECT_EQ(String("0,0 100x100 hasRadius notInfinite"), rect.ToString());
}

}  // namespace blink
