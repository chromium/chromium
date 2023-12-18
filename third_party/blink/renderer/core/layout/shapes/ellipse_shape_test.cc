// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/shapes/ellipse_shape.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

#define EXPECT_INVALID_EXCLUDED_INTERVAL(shape, top, height)            \
  do {                                                                  \
    auto segment =                                                      \
        shape.GetExcludedInterval(LayoutUnit(top), LayoutUnit(height)); \
    ASSERT_FALSE(segment.is_valid);                                     \
    EXPECT_FLOAT_EQ(0, segment.logical_left);                           \
    EXPECT_FLOAT_EQ(0, segment.logical_right);                          \
  } while (false)

#define EXPECT_EXCLUDED_INTERVAL(shape, top, height, expected_left,     \
                                 expected_right)                        \
  do {                                                                  \
    auto segment =                                                      \
        shape.GetExcludedInterval(LayoutUnit(top), LayoutUnit(height)); \
    ASSERT_TRUE(segment.is_valid);                                      \
    EXPECT_NEAR(expected_left, segment.logical_left, 0.01f);            \
    EXPECT_NEAR(expected_right, segment.logical_right, 0.01f);          \
  } while (false)

TEST(EllipseShapeTest, ZeroRadii) {
  test::TaskEnvironment task_environment;
  EllipseShape shape(gfx::PointF(), 0, 0);
  EXPECT_TRUE(shape.IsEmpty());
  EXPECT_EQ(LogicalRect(), shape.ShapeMarginLogicalBoundingBox());
  EXPECT_INVALID_EXCLUDED_INTERVAL(shape, 0, 0);
  EXPECT_INVALID_EXCLUDED_INTERVAL(shape, -100, 200);
}

TEST(EllipseShapeTest, ZeroRadiusX) {
  test::TaskEnvironment task_environment;
  EllipseShape shape(gfx::PointF(), 0, 10);
  EXPECT_TRUE(shape.IsEmpty());
  EXPECT_EQ(LogicalRect(0, -10, 0, 20), shape.ShapeMarginLogicalBoundingBox());
  EXPECT_INVALID_EXCLUDED_INTERVAL(shape, 0, 0);
  EXPECT_INVALID_EXCLUDED_INTERVAL(shape, -100, 200);
}

TEST(EllipseShapeTest, ZeroRadiusY) {
  test::TaskEnvironment task_environment;
  EllipseShape shape(gfx::PointF(), 10, 0);
  EXPECT_TRUE(shape.IsEmpty());
  EXPECT_EQ(LogicalRect(-10, 0, 20, 0), shape.ShapeMarginLogicalBoundingBox());
  EXPECT_INVALID_EXCLUDED_INTERVAL(shape, 0, 0);
  EXPECT_INVALID_EXCLUDED_INTERVAL(shape, -100, 200);
}

TEST(EllipseShapeTest, ZeroRadiiWithMargin) {
  test::TaskEnvironment task_environment;
  EllipseShape shape(gfx::PointF(10, 20), 0, 0);
  shape.SetShapeMarginForTesting(5);
  EXPECT_TRUE(shape.IsEmpty());
  EXPECT_EQ(LogicalRect(5, 15, 10, 10), shape.ShapeMarginLogicalBoundingBox());
  // Both y1 and y2 are above the ellipse.
  EXPECT_INVALID_EXCLUDED_INTERVAL(shape, -100, 0);
  EXPECT_INVALID_EXCLUDED_INTERVAL(shape, -100, 114);
  // y2 crosses the upper half of the ellipse.
  EXPECT_EXCLUDED_INTERVAL(shape, 15, 0, 10, 10);
  EXPECT_EXCLUDED_INTERVAL(shape, 10, 7.5, 5.66, 14.33);
  EXPECT_EXCLUDED_INTERVAL(shape, 17.5, 0, 5.66, 14.33);
  // y1 crosses the bottom half of the ellipse.
  EXPECT_EXCLUDED_INTERVAL(shape, 22.5, 0, 5.66, 14.33);
  EXPECT_EXCLUDED_INTERVAL(shape, 22.5, 30, 5.66, 14.33);
  // The interval between y1 and y2 contains the center of the ellipse.
  EXPECT_EXCLUDED_INTERVAL(shape, 17.5, 2.5, 5, 15);
  EXPECT_EXCLUDED_INTERVAL(shape, 20, 2.5, 5, 15);
  EXPECT_EXCLUDED_INTERVAL(shape, 17.5, 5, 5, 15);
  EXPECT_EXCLUDED_INTERVAL(shape, 15, 10, 5, 15);
  EXPECT_EXCLUDED_INTERVAL(shape, -100, 200, 5, 15);
  // Both y1 and y2 are below the ellipse.
  EXPECT_INVALID_EXCLUDED_INTERVAL(shape, 25, 0);
  EXPECT_INVALID_EXCLUDED_INTERVAL(shape, 25, 100);
  EXPECT_INVALID_EXCLUDED_INTERVAL(shape, 100, 1);
}

TEST(EllipseShapeTest, NonZeroRadiiWithMargin) {
  test::TaskEnvironment task_environment;
  EllipseShape shape(gfx::PointF(10, 20), 20, 10);
  shape.SetShapeMarginForTesting(5);
  EXPECT_FALSE(shape.IsEmpty());
  EXPECT_EQ(LogicalRect(-15, 5, 50, 30), shape.ShapeMarginLogicalBoundingBox());
  // Both y1 and y2 are above the ellipse.
  EXPECT_INVALID_EXCLUDED_INTERVAL(shape, -100, 0);
  EXPECT_INVALID_EXCLUDED_INTERVAL(shape, -100, 104);
  // y2 crosses the upper half of the ellipse.
  EXPECT_EXCLUDED_INTERVAL(shape, 5, 0, 10, 10);
  EXPECT_EXCLUDED_INTERVAL(shape, 0, 10, -8.63, 28.63);
  EXPECT_EXCLUDED_INTERVAL(shape, 10, 0, -8.63, 28.63);
  // y1 crosses the bottom half of the ellipse.
  EXPECT_EXCLUDED_INTERVAL(shape, 30, 0, -8.63, 28.63);
  EXPECT_EXCLUDED_INTERVAL(shape, 30, 30, -8.63, 28.63);
  // The interval between y1 and y2 contains the center of the ellipse.
  EXPECT_EXCLUDED_INTERVAL(shape, 20, 10, -15, 35);
  EXPECT_EXCLUDED_INTERVAL(shape, 20, 2.5, -15, 35);
  EXPECT_EXCLUDED_INTERVAL(shape, 10, 10, -15, 35);
  EXPECT_EXCLUDED_INTERVAL(shape, 15, 30, -15, 35);
  EXPECT_EXCLUDED_INTERVAL(shape, -100, 200, -15, 35);
  // Both y1 and y2 are below the ellipse.
  EXPECT_INVALID_EXCLUDED_INTERVAL(shape, 35, 0);
  EXPECT_INVALID_EXCLUDED_INTERVAL(shape, 35, 100);
  EXPECT_INVALID_EXCLUDED_INTERVAL(shape, 100, 1);
}

TEST(EllipseShapeTest, ShapeMarginLogicalBoundingBoxWithFloatValues) {
  test::TaskEnvironment task_environment;
  EXPECT_EQ(LogicalRect(LayoutUnit(-2.25f), LayoutUnit(-2.125f), LayoutUnit(7),
                        LayoutUnit(9.75f)),
            EllipseShape(gfx::PointF(1.25f, 2.75f), 3.5f, 4.875f)
                .ShapeMarginLogicalBoundingBox());
  EXPECT_EQ(LogicalRect(LayoutUnit::Min(), LayoutUnit(), LayoutUnit::Max(),
                        LayoutUnit()),
            EllipseShape(gfx::PointF(), 1e20f, 1e-20f)
                .ShapeMarginLogicalBoundingBox());
}

}  // namespace blink
