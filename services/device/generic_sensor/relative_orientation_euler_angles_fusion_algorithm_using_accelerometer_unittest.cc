// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "base/memory/ref_counted.h"
#include "base/numerics/math_constants.h"
#include "base/test/task_environment.h"
#include "services/device/generic_sensor/fake_platform_sensor_fusion.h"
#include "services/device/generic_sensor/relative_orientation_euler_angles_fusion_algorithm_using_accelerometer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerTest
    : public testing::Test {
 public:
  RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerTest() {
    auto fusion_algorithm = std::make_unique<
        RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer>();
    fusion_algorithm_ = fusion_algorithm.get();
    fake_fusion_sensor_ = base::MakeRefCounted<FakePlatformSensorFusion>(
        std::move(fusion_algorithm));
    fusion_algorithm_->set_fusion_sensor(fake_fusion_sensor_.get());
    EXPECT_EQ(1UL, fusion_algorithm_->source_types().size());
  }

  void VerifyRelativeOrientationEulerAngles(double acceleration_x,
                                            double acceleration_y,
                                            double acceleration_z,
                                            double expected_beta_in_degrees,
                                            double expected_gamma_in_degrees) {
    SensorReading reading;
    reading.accel.x = acceleration_x;
    reading.accel.y = acceleration_y;
    reading.accel.z = acceleration_z;

    fake_fusion_sensor_->SetSensorReading(mojom::SensorType::ACCELEROMETER,
                                          reading,
                                          true /* sensor_reading_success */);

    SensorReading fused_reading;
    EXPECT_TRUE(fusion_algorithm_->GetFusedData(
        mojom::SensorType::ACCELEROMETER, &fused_reading));

    EXPECT_TRUE(
        std::isnan(fused_reading.orientation_euler.z.value() /* alpha */));
    EXPECT_DOUBLE_EQ(expected_beta_in_degrees,
                     fused_reading.orientation_euler.x /* beta */);
    EXPECT_DOUBLE_EQ(expected_gamma_in_degrees,
                     fused_reading.orientation_euler.y /* gamma */);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<FakePlatformSensorFusion> fake_fusion_sensor_;
  RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer*
      fusion_algorithm_;
};

}  // namespace

TEST_F(RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerTest,
       NoAccelerometerReading) {
  SensorReading reading;
  fake_fusion_sensor_->SetSensorReading(mojom::SensorType::ACCELEROMETER,
                                        reading,
                                        false /* sensor_reading_success */);
  SensorReading fused_reading;
  EXPECT_FALSE(fusion_algorithm_->GetFusedData(mojom::SensorType::ACCELEROMETER,
                                               &fused_reading));
}

// Tests a device resting flat.
TEST_F(RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerTest,
       NeutralOrientation) {
  double acceleration_x = 0.0;
  double acceleration_y = 0.0;
  double acceleration_z = base::kMeanGravityDouble;

  double expected_beta_in_degrees = 0.0;
  double expected_gamma_in_degrees = 0.0;

  VerifyRelativeOrientationEulerAngles(acceleration_x, acceleration_y,
                                       acceleration_z, expected_beta_in_degrees,
                                       expected_gamma_in_degrees);
}

// Tests an upside-down device, such that the W3C boundary [-180, 180) causes
// the beta value to become negative.
TEST_F(RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerTest,
       UpsideDown) {
  double acceleration_x = 0.0;
  double acceleration_y = 0.0;
  double acceleration_z = -base::kMeanGravityDouble;

  double expected_beta_in_degrees = -180.0;
  double expected_gamma_in_degrees = 0.0;

  VerifyRelativeOrientationEulerAngles(acceleration_x, acceleration_y,
                                       acceleration_z, expected_beta_in_degrees,
                                       expected_gamma_in_degrees);
}

// Tests for positive beta value before the device is completely upside-down.
TEST_F(RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerTest,
       BeforeUpsideDownBoundary) {
  double acceleration_x = 0.0;
  double acceleration_y = -base::kMeanGravityDouble / 2.0;
  double acceleration_z = -base::kMeanGravityDouble / 2.0;

  double expected_beta_in_degrees = 135.0;
  double expected_gamma_in_degrees = 0.0;

  VerifyRelativeOrientationEulerAngles(acceleration_x, acceleration_y,
                                       acceleration_z, expected_beta_in_degrees,
                                       expected_gamma_in_degrees);
}

// Tests a device lying on its top-edge.
TEST_F(RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerTest,
       TopEdge) {
  double acceleration_x = 0.0;
  double acceleration_y = base::kMeanGravityDouble;
  double acceleration_z = 0.0;

  double expected_beta_in_degrees = -90.0;
  double expected_gamma_in_degrees = 0.0;

  VerifyRelativeOrientationEulerAngles(acceleration_x, acceleration_y,
                                       acceleration_z, expected_beta_in_degrees,
                                       expected_gamma_in_degrees);
}

// Tests before a device is completely on its top-edge.
TEST_F(RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerTest,
       BeforeTopEdgeBoundary) {
  double acceleration_x = 0.0;
  double acceleration_y = base::kMeanGravityDouble / 2.0;
  double acceleration_z = base::kMeanGravityDouble / 2.0;

  double expected_beta_in_degrees = -45.0;
  double expected_gamma_in_degrees = 0.0;

  VerifyRelativeOrientationEulerAngles(acceleration_x, acceleration_y,
                                       acceleration_z, expected_beta_in_degrees,
                                       expected_gamma_in_degrees);
}

// Tests a device lying on its bottom-edge.
TEST_F(RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerTest,
       BottomEdge) {
  double acceleration_x = 0.0;
  double acceleration_y = -base::kMeanGravityDouble;
  double acceleration_z = 0.0;

  double expected_beta_in_degrees = 90.0;
  double expected_gamma_in_degrees = 0.0;

  VerifyRelativeOrientationEulerAngles(acceleration_x, acceleration_y,
                                       acceleration_z, expected_beta_in_degrees,
                                       expected_gamma_in_degrees);
}

// Tests before a device is completely on its bottom-edge.
TEST_F(RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerTest,
       BeforeBottomEdgeBoundary) {
  double acceleration_x = 0.0;
  double acceleration_y = -base::kMeanGravityDouble / 2.0;
  double acceleration_z = base::kMeanGravityDouble / 2.0;

  double expected_beta_in_degrees = 45.0;
  double expected_gamma_in_degrees = 0.0;

  VerifyRelativeOrientationEulerAngles(acceleration_x, acceleration_y,
                                       acceleration_z, expected_beta_in_degrees,
                                       expected_gamma_in_degrees);
}

// Tests a device lying on its left-edge.
TEST_F(RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerTest,
       LeftEdge) {
  double acceleration_x = -base::kMeanGravityDouble;
  double acceleration_y = 0.0;
  double acceleration_z = 0.0;

  double expected_beta_in_degrees = 0.0;
  double expected_gamma_in_degrees = -90.0;

  VerifyRelativeOrientationEulerAngles(acceleration_x, acceleration_y,
                                       acceleration_z, expected_beta_in_degrees,
                                       expected_gamma_in_degrees);
}

// Tests for negative gamma value before the device is completely on its left
// side.
TEST_F(RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerTest,
       BeforeLeftEdgeBoundary) {
  double acceleration_x = -base::kMeanGravityDouble / std::sqrt(2.0);
  double acceleration_y = 0.0;
  double acceleration_z = base::kMeanGravityDouble / std::sqrt(2.0);

  double expected_beta_in_degrees = 0.0;
  double expected_gamma_in_degrees = -45.0;

  VerifyRelativeOrientationEulerAngles(acceleration_x, acceleration_y,
                                       acceleration_z, expected_beta_in_degrees,
                                       expected_gamma_in_degrees);
}

// Tests a device lying on its right-edge, such that the W3C boundary [-90, 90)
// causes the gamma value to become negative.
TEST_F(RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerTest,
       RightEdge) {
  double acceleration_x = base::kMeanGravityDouble;
  double acceleration_y = 0.0;
  double acceleration_z = 0.0;

  double expected_beta_in_degrees = 0.0;
  double expected_gamma_in_degrees = -90.0;

  VerifyRelativeOrientationEulerAngles(acceleration_x, acceleration_y,
                                       acceleration_z, expected_beta_in_degrees,
                                       expected_gamma_in_degrees);
}

// Tests for positive gamma value before the device is completely on its right
// side.
TEST_F(RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerTest,
       BeforeRightEdgeBoundary) {
  double acceleration_x = base::kMeanGravityDouble / std::sqrt(2.0);
  double acceleration_y = 0.0;
  double acceleration_z = base::kMeanGravityDouble / std::sqrt(2.0);

  double expected_beta_in_degrees = 0.0;
  double expected_gamma_in_degrees = 45.0;

  VerifyRelativeOrientationEulerAngles(acceleration_x, acceleration_y,
                                       acceleration_z, expected_beta_in_degrees,
                                       expected_gamma_in_degrees);
}

}  // namespace device
