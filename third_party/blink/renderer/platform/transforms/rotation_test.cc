// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/transforms/rotation.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

const double kAxisTolerance = 1e-6;
const double kAngleTolerance = 1e-4;

#define EXPECT_AXIS(expected, actual)                      \
  do {                                                     \
    EXPECT_NEAR(expected.X(), actual.X(), kAxisTolerance); \
    EXPECT_NEAR(expected.Y(), actual.Y(), kAxisTolerance); \
    EXPECT_NEAR(expected.Z(), actual.Z(), kAxisTolerance); \
  } while (false)

#define EXPECT_ANGLE(expected, actual) \
  EXPECT_NEAR(expected, actual, kAngleTolerance)

TEST(RotationTest, GetCommonAxisTest) {
  FloatPoint3D axis;
  double angle_a;
  double angle_b;

  EXPECT_TRUE(Rotation::GetCommonAxis(Rotation(FloatPoint3D(0, 0, 0), 0),
                                      Rotation(FloatPoint3D(1, 2, 3), 100),
                                      axis, angle_a, angle_b));
  FloatPoint3D expected_axis = FloatPoint3D(1, 2, 3);
  expected_axis.Normalize();
  EXPECT_AXIS(expected_axis, axis);
  EXPECT_EQ(0, angle_a);
  EXPECT_EQ(100, angle_b);

  EXPECT_TRUE(Rotation::GetCommonAxis(Rotation(FloatPoint3D(1, 2, 3), 100),
                                      Rotation(FloatPoint3D(0, 0, 0), 0), axis,
                                      angle_a, angle_b));
  EXPECT_AXIS(expected_axis, axis);
  EXPECT_EQ(100, angle_a);
  EXPECT_EQ(0, angle_b);

  EXPECT_TRUE(Rotation::GetCommonAxis(Rotation(FloatPoint3D(0, 0, 0), 100),
                                      Rotation(FloatPoint3D(1, 2, 3), 100),
                                      axis, angle_a, angle_b));
  EXPECT_AXIS(expected_axis, axis);
  EXPECT_EQ(0, angle_a);
  EXPECT_EQ(100, angle_b);

  EXPECT_TRUE(Rotation::GetCommonAxis(Rotation(FloatPoint3D(3, 2, 1), 0),
                                      Rotation(FloatPoint3D(1, 2, 3), 100),
                                      axis, angle_a, angle_b));
  EXPECT_AXIS(expected_axis, axis);
  EXPECT_EQ(0, angle_a);
  EXPECT_EQ(100, angle_b);

  EXPECT_TRUE(Rotation::GetCommonAxis(Rotation(FloatPoint3D(1, 2, 3), 50),
                                      Rotation(FloatPoint3D(1, 2, 3), 100),
                                      axis, angle_a, angle_b));
  EXPECT_AXIS(expected_axis, axis);
  EXPECT_EQ(50, angle_a);
  EXPECT_EQ(100, angle_b);

  EXPECT_TRUE(Rotation::GetCommonAxis(Rotation(FloatPoint3D(1, 2, 3), 50),
                                      Rotation(FloatPoint3D(2, 4, 6), 100),
                                      axis, angle_a, angle_b));
  EXPECT_AXIS(expected_axis, axis);
  EXPECT_EQ(50, angle_a);
  EXPECT_EQ(100, angle_b);

  EXPECT_FALSE(Rotation::GetCommonAxis(Rotation(FloatPoint3D(1, 2, 3), 50),
                                       Rotation(FloatPoint3D(3, 2, 1), 100),
                                       axis, angle_a, angle_b));

  EXPECT_FALSE(Rotation::GetCommonAxis(Rotation(FloatPoint3D(1, 2, 3), 50),
                                       Rotation(FloatPoint3D(-1, -2, -3), 100),
                                       axis, angle_a, angle_b));
}

TEST(RotationTest, AddTest) {
  // Test accumulation around common axis.
  Rotation x_rotation = Rotation::Add(Rotation(FloatPoint3D(1, 0, 0), 60),
                                      Rotation(FloatPoint3D(1, 0, 0), 30));
  EXPECT_AXIS(FloatPoint3D(1, 0, 0), x_rotation.axis);
  EXPECT_ANGLE(90, x_rotation.angle);

  Rotation y_rotation = Rotation::Add(Rotation(FloatPoint3D(0, 1, 0), 60),
                                      Rotation(FloatPoint3D(0, 1, 0), 30));
  EXPECT_AXIS(FloatPoint3D(0, 1, 0), y_rotation.axis);
  EXPECT_ANGLE(90, y_rotation.angle);

  Rotation z_rotation = Rotation::Add(Rotation(FloatPoint3D(0, 0, 1), 60),
                                      Rotation(FloatPoint3D(0, 0, 1), 30));
  EXPECT_AXIS(FloatPoint3D(0, 0, 1), z_rotation.axis);
  EXPECT_ANGLE(90, z_rotation.angle);

  // Test axis pairs
  Rotation xy_rotation = Rotation::Add(Rotation(FloatPoint3D(1, 0, 0), 90),
                                       Rotation(FloatPoint3D(0, 1, 0), 90));
  double root3_inv = 1 / std::sqrt(3);
  FloatPoint3D expected_axis = FloatPoint3D(root3_inv, root3_inv, root3_inv);
  EXPECT_AXIS(expected_axis, xy_rotation.axis);
  EXPECT_ANGLE(120, xy_rotation.angle);

  Rotation yz_rotation = Rotation::Add(Rotation(FloatPoint3D(0, 1, 0), 90),
                                       Rotation(FloatPoint3D(0, 0, 1), 90));
  EXPECT_AXIS(expected_axis, yz_rotation.axis);
  EXPECT_ANGLE(120, yz_rotation.angle);

  Rotation zx_rotation = Rotation::Add(Rotation(FloatPoint3D(0, 0, 1), 90),
                                       Rotation(FloatPoint3D(1, 0, 0), 90));
  EXPECT_AXIS(expected_axis, zx_rotation.axis);
  EXPECT_ANGLE(120, zx_rotation.angle);
}

TEST(RotationTest, SlerpTest) {
  // Common axis case.
  Rotation x_rotation =
      Rotation::Slerp(Rotation(FloatPoint3D(1, 0, 0), 30),
                      Rotation(FloatPoint3D(1, 0, 0), 60), 0.5);
  EXPECT_AXIS(FloatPoint3D(1, 0, 0), x_rotation.axis);
  EXPECT_ANGLE(45, x_rotation.angle);

  // General case without a common rotation axis.
  Rotation xy_rotation =
      Rotation::Slerp(Rotation(FloatPoint3D(1, 0, 0), 90),
                      Rotation(FloatPoint3D(0, 1, 0), 90), 0.5);
  double root2_inv = 1 / std::sqrt(2);  // half angle is 60 degrees
  EXPECT_AXIS(FloatPoint3D(root2_inv, root2_inv, 0), xy_rotation.axis);
  double expected_angle = rad2deg(std::acos(1.0 / 3.0));
  EXPECT_ANGLE(expected_angle, xy_rotation.angle);
}

}  // namespace blink
