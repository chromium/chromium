// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/float_quad.h"

#include <limits>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TEST(FloatQuadTest, ToString) {
  FloatQuad quad(FloatPoint(2, 3), FloatPoint(5, 7), FloatPoint(11, 13),
                 FloatPoint(17, 19));
  EXPECT_EQ("2,3; 5,7; 11,13; 17,19", quad.ToString());
}

TEST(FloatQuadTest, BoundingBox) {
  FloatQuad quad(FloatPoint(2, 3), FloatPoint(5, 7), FloatPoint(11, 13),
                 FloatPoint(17, 19));
  FloatRect rect = quad.BoundingBox();
  EXPECT_EQ(rect.X(), 2);
  EXPECT_EQ(rect.Y(), 3);
  EXPECT_EQ(rect.Width(), 17 - 2);
  EXPECT_EQ(rect.Height(), 19 - 3);
}

TEST(FloatQuadTest, BoundingBoxSaturateInf) {
  constexpr double inf = std::numeric_limits<double>::infinity();
  FloatQuad quad(FloatPoint(-inf, 3), FloatPoint(5, inf), FloatPoint(11, 13),
                 FloatPoint(17, 19));
  FloatRect rect = quad.BoundingBox();
  EXPECT_EQ(rect.X(), std::numeric_limits<int>::min());
  EXPECT_EQ(rect.Y(), 3.0f);
  EXPECT_EQ(rect.Width(), 17.0f - std::numeric_limits<int>::min());
  EXPECT_EQ(rect.Height(),
            static_cast<float>(std::numeric_limits<int>::max()) - 3.0f);
}

TEST(FloatQuadTest, BoundingBoxSaturateLarge) {
  constexpr double large = std::numeric_limits<float>::max() * 4;
  FloatQuad quad(FloatPoint(-large, 3), FloatPoint(5, large),
                 FloatPoint(11, 13), FloatPoint(17, 19));
  FloatRect rect = quad.BoundingBox();
  EXPECT_EQ(rect.X(), std::numeric_limits<int>::min());
  EXPECT_EQ(rect.Y(), 3.0f);
  EXPECT_EQ(rect.Width(), 17.0f - std::numeric_limits<int>::min());
  EXPECT_EQ(rect.Height(),
            static_cast<float>(std::numeric_limits<int>::max()) - 3.0f);
}

TEST(FloatQuadTest, RectIntersectionIsInclusive) {
  // A rectilinear quad at (10, 10) with dimensions 10x10.
  FloatQuad quad(FloatRect(10, 10, 10, 10));

  // A rect fully contained in the quad should intersect.
  EXPECT_TRUE(quad.IntersectsRect(FloatRect(11, 11, 8, 8)));

  // A point fully contained in the quad should intersect.
  EXPECT_TRUE(quad.IntersectsRect(FloatRect(11, 11, 0, 0)));

  // A rect that touches the quad only at the point (10, 10) should intersect.
  EXPECT_TRUE(quad.IntersectsRect(FloatRect(9, 9, 1, 1)));

  // A rect that touches the quad only on the left edge should intersect.
  EXPECT_TRUE(quad.IntersectsRect(FloatRect(9, 11, 1, 1)));

  // A rect that touches the quad only on the top edge should intersect.
  EXPECT_TRUE(quad.IntersectsRect(FloatRect(11, 9, 1, 1)));

  // A rect that touches the quad only on the right edge should intersect.
  EXPECT_TRUE(quad.IntersectsRect(FloatRect(20, 11, 1, 1)));

  // A rect that touches the quad only on the bottom edge should intersect.
  EXPECT_TRUE(quad.IntersectsRect(FloatRect(11, 20, 1, 1)));

  // A rect that is fully outside the quad should not intersect.
  EXPECT_FALSE(quad.IntersectsRect(FloatRect(8, 8, 1, 1)));

  // A point that is fully outside the quad should not intersect.
  EXPECT_FALSE(quad.IntersectsRect(FloatRect(9, 9, 0, 0)));
}

TEST(FloatQuadTest, CircleIntersectionIsInclusive) {
  // A rectilinear quad at (10, 10) with dimensions 10x10.
  FloatQuad quad(FloatRect(10, 10, 10, 10));

  // A circle fully contained in the top-left of the quad should intersect.
  EXPECT_TRUE(quad.IntersectsCircle(FloatPoint(12, 12), 1));

  // A point fully contained in the top-left of the quad should intersect.
  EXPECT_TRUE(quad.IntersectsCircle(FloatPoint(12, 12), 0));

  // A circle that touches the left edge should intersect.
  EXPECT_TRUE(quad.IntersectsCircle(FloatPoint(9, 11), 1));

  // A circle that touches the top edge should intersect.
  EXPECT_TRUE(quad.IntersectsCircle(FloatPoint(11, 9), 1));

  // A circle that touches the right edge should intersect.
  EXPECT_TRUE(quad.IntersectsCircle(FloatPoint(21, 11), 1));

  // A circle that touches the bottom edge should intersect.
  EXPECT_TRUE(quad.IntersectsCircle(FloatPoint(11, 21), 1));

  // A point that touches the left edge should intersect.
  EXPECT_TRUE(quad.IntersectsCircle(FloatPoint(10, 11), 0));

  // A point that touches the top edge should intersect.
  EXPECT_TRUE(quad.IntersectsCircle(FloatPoint(11, 10), 0));

  // A point that touches the right edge should intersect.
  EXPECT_TRUE(quad.IntersectsCircle(FloatPoint(20, 11), 0));

  // A point that touches the bottom edge should intersect.
  EXPECT_TRUE(quad.IntersectsCircle(FloatPoint(11, 20), 0));

  // A circle that is fully outside the quad should not intersect.
  EXPECT_FALSE(quad.IntersectsCircle(FloatPoint(9, 9), 1));

  // A point that is fully outside the quad should not intersect.
  EXPECT_FALSE(quad.IntersectsCircle(FloatPoint(9, 9), 0));
}

}  // namespace blink
