// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/transforms/rotation.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace blink {

const double kAxisTolerance = 1e-6;
const double kAngleTolerance = 1e-4;

#define EXPECT_AXIS(expected, actual)                      \
  do {                                                     \
    EXPECT_NEAR(expected.x(), actual.x(), kAxisTolerance); \
    EXPECT_NEAR(expected.y(), actual.y(), kAxisTolerance); \
    EXPECT_NEAR(expected.z(), actual.z(), kAxisTolerance); \
  } while (false)

#define EXPECT_ANGLE(expected, actual) \
  EXPECT_NEAR(expected, actual, kAngleTolerance)

TEST(RotationTest, GetCommonAxisTest) {
  gfx::Vector3dF axis;
  double angle_a;
  double angle_b;

  EXPECT_TRUE(Rotation::GetCommonAxis(Rotation(gfx::Vector3dF(0, 0, 0), 0),
                                      Rotation(gfx::Vector3dF(1, 2, 3), 100),
                                      axis, angle_a, angle_b));
  gfx::Vector3dF expected_axis(1, 2, 3);
  expected_axis.GetNormalized(&expected_axis);
  EXPECT_AXIS(expected_axis, axis);
  EXPECT_EQ(0, angle_a);
  EXPECT_EQ(100, angle_b);

  EXPECT_TRUE(Rotation::GetCommonAxis(Rotation(gfx::Vector3dF(1, 2, 3), 100),
                                      Rotation(gfx::Vector3dF(0, 0, 0), 0),
                                      axis, angle_a, angle_b));
  EXPECT_AXIS(expected_axis, axis);
  EXPECT_EQ(100, angle_a);
  EXPECT_EQ(0, angle_b);

  EXPECT_TRUE(Rotation::GetCommonAxis(Rotation(gfx::Vector3dF(0, 0, 0), 100),
                                      Rotation(gfx::Vector3dF(1, 2, 3), 100),
                                      axis, angle_a, angle_b));
  EXPECT_AXIS(expected_axis, axis);
  EXPECT_EQ(0, angle_a);
  EXPECT_EQ(100, angle_b);

  EXPECT_TRUE(Rotation::GetCommonAxis(Rotation(gfx::Vector3dF(3, 2, 1), 0),
                                      Rotation(gfx::Vector3dF(1, 2, 3), 100),
                                      axis, angle_a, angle_b));
  EXPECT_AXIS(expected_axis, axis);
  EXPECT_EQ(0, angle_a);
  EXPECT_EQ(100, angle_b);

  EXPECT_TRUE(Rotation::GetCommonAxis(Rotation(gfx::Vector3dF(1, 2, 3), 50),
                                      Rotation(gfx::Vector3dF(1, 2, 3), 100),
                                      axis, angle_a, angle_b));
  EXPECT_AXIS(expected_axis, axis);
  EXPECT_EQ(50, angle_a);
  EXPECT_EQ(100, angle_b);

  EXPECT_TRUE(Rotation::GetCommonAxis(Rotation(gfx::Vector3dF(1, 2, 3), 50),
                                      Rotation(gfx::Vector3dF(2, 4, 6), 100),
                                      axis, angle_a, angle_b));
  EXPECT_AXIS(expected_axis, axis);
  EXPECT_EQ(50, angle_a);
  EXPECT_EQ(100, angle_b);

  EXPECT_FALSE(Rotation::GetCommonAxis(Rotation(gfx::Vector3dF(1, 2, 3), 50),
                                       Rotation(gfx::Vector3dF(3, 2, 1), 100),
                                       axis, angle_a, angle_b));

  EXPECT_FALSE(Rotation::GetCommonAxis(
      Rotation(gfx::Vector3dF(1, 2, 3), 50),
      Rotation(gfx::Vector3dF(-1, -2, -3), 100), axis, angle_a, angle_b));
}

TEST(RotationTest, AddTest) {
  // Test accumulation around common axis.
  Rotation x_rotation = Rotation::Add(Rotation(gfx::Vector3dF(1, 0, 0), 60),
                                      Rotation(gfx::Vector3dF(1, 0, 0), 30));
  EXPECT_AXIS(gfx::Vector3dF(1, 0, 0), x_rotation.axis);
  EXPECT_ANGLE(90, x_rotation.angle);

  Rotation y_rotation = Rotation::Add(Rotation(gfx::Vector3dF(0, 1, 0), 60),
                                      Rotation(gfx::Vector3dF(0, 1, 0), 30));
  EXPECT_AXIS(gfx::Vector3dF(0, 1, 0), y_rotation.axis);
  EXPECT_ANGLE(90, y_rotation.angle);

  Rotation z_rotation = Rotation::Add(Rotation(gfx::Vector3dF(0, 0, 1), 60),
                                      Rotation(gfx::Vector3dF(0, 0, 1), 30));
  EXPECT_AXIS(gfx::Vector3dF(0, 0, 1), z_rotation.axis);
  EXPECT_ANGLE(90, z_rotation.angle);

  // Test axis pairs
  Rotation xy_rotation = Rotation::Add(Rotation(gfx::Vector3dF(1, 0, 0), 90),
                                       Rotation(gfx::Vector3dF(0, 1, 0), 90));
  double root3_inv = 1 / std::sqrt(3);
  gfx::Vector3dF expected_axis(root3_inv, root3_inv, root3_inv);
  EXPECT_AXIS(expected_axis, xy_rotation.axis);
  EXPECT_ANGLE(120, xy_rotation.angle);

  Rotation yz_rotation = Rotation::Add(Rotation(gfx::Vector3dF(0, 1, 0), 90),
                                       Rotation(gfx::Vector3dF(0, 0, 1), 90));
  EXPECT_AXIS(expected_axis, yz_rotation.axis);
  EXPECT_ANGLE(120, yz_rotation.angle);

  Rotation zx_rotation = Rotation::Add(Rotation(gfx::Vector3dF(0, 0, 1), 90),
                                       Rotation(gfx::Vector3dF(1, 0, 0), 90));
  EXPECT_AXIS(expected_axis, zx_rotation.axis);
  EXPECT_ANGLE(120, zx_rotation.angle);
}

TEST(RotationTest, SlerpTest) {
  // Common axis case.
  Rotation x_rotation =
      Rotation::Slerp(Rotation(gfx::Vector3dF(1, 0, 0), 30),
                      Rotation(gfx::Vector3dF(1, 0, 0), 60), 0.5);
  EXPECT_AXIS(gfx::Vector3dF(1, 0, 0), x_rotation.axis);
  EXPECT_ANGLE(45, x_rotation.angle);

  // General case without a common rotation axis.
  Rotation xy_rotation =
      Rotation::Slerp(Rotation(gfx::Vector3dF(1, 0, 0), 90),
                      Rotation(gfx::Vector3dF(0, 1, 0), 90), 0.5);
  double root2_inv = 1 / std::sqrt(2);  // half angle is 60 degrees
  EXPECT_AXIS(gfx::Vector3dF(root2_inv, root2_inv, 0), xy_rotation.axis);
  double expected_angle = Rad2deg(std::acos(1.0 / 3.0));
  EXPECT_ANGLE(expected_angle, xy_rotation.angle);
}

}  // namespace blink
