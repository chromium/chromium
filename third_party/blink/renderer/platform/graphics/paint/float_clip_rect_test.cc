// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/float_clip_rect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

class FloatClipRectTest : public testing::Test {
 public:
};

TEST_F(FloatClipRectTest, InfiniteRect) {
  FloatClipRect rect;
  EXPECT_TRUE(rect.IsInfinite());
  EXPECT_FALSE(rect.HasRadius());
  EXPECT_TRUE(rect.IsTight());

  FloatClipRect rect2(gfx::RectF(1, 2, 3, 4));
  EXPECT_FALSE(rect2.IsInfinite());
  EXPECT_FALSE(rect.HasRadius());
  EXPECT_TRUE(rect.IsTight());
}

TEST_F(FloatClipRectTest, MoveBy) {
  FloatClipRect rect;
  rect.Move(gfx::Vector2dF(1, 2));
  EXPECT_EQ(rect.Rect(), FloatClipRect().Rect());
  EXPECT_TRUE(rect.IsInfinite());
  EXPECT_FALSE(rect.HasRadius());
  EXPECT_TRUE(rect.IsTight());

  FloatClipRect rect2(gfx::RectF(1, 2, 3, 4));
  rect2.SetHasRadius();
  rect2.Move(gfx::Vector2dF(5, 6));
  EXPECT_EQ(gfx::RectF(6, 8, 3, 4), rect2.Rect());
  EXPECT_TRUE(rect2.HasRadius());
  EXPECT_FALSE(rect2.IsTight());
}

TEST_F(FloatClipRectTest, Intersect) {
  FloatClipRect rect;
  FloatClipRect rect1(gfx::RectF(1, 2, 3, 4));
  FloatClipRect rect2(gfx::RectF(3, 4, 5, 6));
  rect2.SetHasRadius();

  rect.Intersect(rect1);
  EXPECT_FALSE(rect.IsInfinite());
  EXPECT_EQ(gfx::RectF(1, 2, 3, 4), rect.Rect());
  EXPECT_FALSE(rect.HasRadius());
  EXPECT_TRUE(rect.IsTight());

  rect.Intersect(rect2);
  EXPECT_FALSE(rect.IsInfinite());
  EXPECT_EQ(gfx::RectF(3, 4, 1, 2), rect.Rect());
  EXPECT_TRUE(rect.HasRadius());
  EXPECT_FALSE(rect.IsTight());
}

TEST_F(FloatClipRectTest, IntersectWithInfinite) {
  FloatClipRect infinite;
  gfx::RectF large(0, 0, static_cast<float>(std::numeric_limits<int>::max()),
                   static_cast<float>(std::numeric_limits<int>::max()));
  FloatClipRect unclipped(large);

  unclipped.Intersect(infinite);
  EXPECT_FALSE(unclipped.IsInfinite());
  EXPECT_EQ(large, unclipped.Rect());
}

TEST_F(FloatClipRectTest, InclusiveIntersectWithInfinite) {
  FloatClipRect infinite;
  gfx::RectF large(0, 0, static_cast<float>(std::numeric_limits<int>::max()),
                   static_cast<float>(std::numeric_limits<int>::max()));
  FloatClipRect unclipped(large);

  ASSERT_TRUE(unclipped.InclusiveIntersect(infinite));
  EXPECT_FALSE(unclipped.IsInfinite());
  EXPECT_EQ(large, unclipped.Rect());
}

TEST_F(FloatClipRectTest, SetHasRadius) {
  FloatClipRect rect;
  rect.SetHasRadius();
  EXPECT_FALSE(rect.IsInfinite());
  EXPECT_TRUE(rect.HasRadius());
  EXPECT_FALSE(rect.IsTight());
}

TEST_F(FloatClipRectTest, ClearIsTight) {
  FloatClipRect rect;
  rect.ClearIsTight();
  EXPECT_TRUE(rect.IsInfinite());
  EXPECT_FALSE(rect.HasRadius());
  EXPECT_FALSE(rect.IsTight());
}

TEST_F(FloatClipRectTest, Map) {
  FloatClipRect rect;
  gfx::Transform identity;
  gfx::Transform translation = gfx::Transform::MakeTranslation(10, 20);
  gfx::Transform rotate;
  rotate.Rotate(45);

  rect.Map(rotate);
  EXPECT_TRUE(rect.IsInfinite());
  EXPECT_FALSE(rect.IsTight());

  FloatClipRect rect2(gfx::RectF(1, 2, 3, 4));
  rect2.Map(identity);
  EXPECT_EQ(gfx::RectF(1, 2, 3, 4), rect2.Rect());
  EXPECT_TRUE(rect2.IsTight());

  rect2.Map(translation);
  EXPECT_EQ(gfx::RectF(11, 22, 3, 4), rect2.Rect());
  EXPECT_TRUE(rect2.IsTight());
}

}  // namespace blink
