// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/angle_conversions.h"
#include "base/test/task_environment.h"
#include "services/device/generic_sensor/absolute_orientation_euler_angles_fusion_algorithm_using_accelerometer_and_magnetometer.h"
#include "services/device/generic_sensor/fake_platform_sensor_fusion.h"
#include "services/device/generic_sensor/generic_sensor_consts.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class
    AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometerTest
    : public testing::Test {
 public:
  AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometerTest() {
    auto fusion_algorithm = std::make_unique<
        AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometer>();
    fusion_algorithm_ = fusion_algorithm.get();
    fake_fusion_sensor_ = base::MakeRefCounted<FakePlatformSensorFusion>(
        std::move(fusion_algorithm));
    fusion_algorithm_->set_fusion_sensor(fake_fusion_sensor_.get());
    EXPECT_EQ(2UL, fusion_algorithm_->source_types().size());
  }

  void VerifyAbsoluteOrientationEulerAngles(double gravity_x,
                                            double gravity_y,
                                            double gravity_z,
                                            double geomagnetic_x,
                                            double geomagnetic_y,
                                            double geomagnetic_z,
                                            double expected_alpha_in_degrees,
                                            double expected_beta_in_degrees,
                                            double expected_gamma_in_degrees) {
    SensorReading gravity_reading;
    gravity_reading.accel.x = gravity_x;
    gravity_reading.accel.y = gravity_y;
    gravity_reading.accel.z = gravity_z;
    SensorReading geomagnetic_reading;
    geomagnetic_reading.accel.x = geomagnetic_x;
    geomagnetic_reading.accel.y = geomagnetic_y;
    geomagnetic_reading.accel.z = geomagnetic_z;

    fake_fusion_sensor_->SetSensorReading(mojom::SensorType::ACCELEROMETER,
                                          gravity_reading,
                                          true /* sensor_reading_success */);
    fake_fusion_sensor_->SetSensorReading(mojom::SensorType::MAGNETOMETER,
                                          geomagnetic_reading,
                                          true /* sensor_reading_success */);
    SensorReading fused_reading;
    EXPECT_TRUE(fusion_algorithm_->GetFusedData(
        mojom::SensorType::ACCELEROMETER, &fused_reading));

    EXPECT_NEAR(expected_alpha_in_degrees,
                fused_reading.orientation_euler.z /* alpha */, kEpsilon);
    EXPECT_NEAR(expected_beta_in_degrees,
                fused_reading.orientation_euler.x /* beta */, kEpsilon);
    EXPECT_NEAR(expected_gamma_in_degrees,
                fused_reading.orientation_euler.y /* gamma */, kEpsilon);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<FakePlatformSensorFusion> fake_fusion_sensor_;
  raw_ptr<
      AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometer>
      fusion_algorithm_;
};

}  // namespace

TEST_F(
    AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometerTest,
    AccelerometerReadingNotChanged) {
  SensorReading reading;
  fake_fusion_sensor_->SetSensorReading(mojom::SensorType::ACCELEROMETER,
                                        reading,
                                        true /* sensor_reading_success */);
  fake_fusion_sensor_->SetSensorReading(mojom::SensorType::MAGNETOMETER,
                                        reading,
                                        true /* sensor_reading_success */);
  SensorReading fused_reading;
  EXPECT_FALSE(fusion_algorithm_->GetFusedData(mojom::SensorType::MAGNETOMETER,
                                               &fused_reading));
}

TEST_F(
    AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometerTest,
    NoAccelerometerReading) {
  SensorReading reading;
  fake_fusion_sensor_->SetSensorReading(mojom::SensorType::ACCELEROMETER,
                                        reading,
                                        false /* sensor_reading_success */);
  fake_fusion_sensor_->SetSensorReading(mojom::SensorType::MAGNETOMETER,
                                        reading,
                                        true /* sensor_reading_success */);
  SensorReading fused_reading;
  EXPECT_FALSE(fusion_algorithm_->GetFusedData(mojom::SensorType::ACCELEROMETER,
                                               &fused_reading));
}

TEST_F(
    AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometerTest,
    NoMagnetometerReading) {
  SensorReading reading;
  fake_fusion_sensor_->SetSensorReading(mojom::SensorType::ACCELEROMETER,
                                        reading,
                                        true /* sensor_reading_success */);
  fake_fusion_sensor_->SetSensorReading(mojom::SensorType::MAGNETOMETER,
                                        reading,
                                        false /* sensor_reading_success */);
  SensorReading fused_reading;
  EXPECT_FALSE(fusion_algorithm_->GetFusedData(mojom::SensorType::ACCELEROMETER,
                                               &fused_reading));
}

TEST_F(
    AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometerTest,
    GravityLessThanTenPercentOfNormalValue) {
  SensorReading gravity_reading;
  gravity_reading.accel.x = 0.1;
  gravity_reading.accel.y = 0.2;
  gravity_reading.accel.z = 0.3;
  SensorReading geomagnetic_reading;
  geomagnetic_reading.accel.x = 1.0;
  geomagnetic_reading.accel.y = 2.0;
  geomagnetic_reading.accel.z = 3.0;

  fake_fusion_sensor_->SetSensorReading(mojom::SensorType::ACCELEROMETER,
                                        gravity_reading,
                                        true /* sensor_reading_success */);
  fake_fusion_sensor_->SetSensorReading(mojom::SensorType::MAGNETOMETER,
                                        geomagnetic_reading,
                                        true /* sensor_reading_success */);
  SensorReading fused_reading;
  EXPECT_FALSE(fusion_algorithm_->GetFusedData(mojom::SensorType::ACCELEROMETER,
                                               &fused_reading));
}

TEST_F(
    AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometerTest,
    NormalHIsTooSmall) {
  SensorReading gravity_reading;
  gravity_reading.accel.x = 1.0;
  gravity_reading.accel.y = 1.0;
  gravity_reading.accel.z = 1.0;
  SensorReading geomagnetic_reading;
  geomagnetic_reading.accel.x = 1.0;
  geomagnetic_reading.accel.y = 1.0;
  geomagnetic_reading.accel.z = 1.0;

  fake_fusion_sensor_->SetSensorReading(mojom::SensorType::ACCELEROMETER,
                                        gravity_reading,
                                        true /* sensor_reading_success */);
  fake_fusion_sensor_->SetSensorReading(mojom::SensorType::MAGNETOMETER,
                                        geomagnetic_reading,
                                        true /* sensor_reading_success */);
  SensorReading fused_reading;
  EXPECT_FALSE(fusion_algorithm_->GetFusedData(mojom::SensorType::ACCELEROMETER,
                                               &fused_reading));
}

TEST_F(
    AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometerTest,
    Identity) {
  double gravity_x = 0.0;
  double gravity_y = 0.0;
  double gravity_z = 1.0;
  double geomagnetic_x = 0.0;
  double geomagnetic_y = 1.0;
  double geomagnetic_z = 0.0;

  double expected_alpha_in_degrees = 0.0;
  double expected_beta_in_degrees = 0.0;
  double expected_gamma_in_degrees = 0.0;

  VerifyAbsoluteOrientationEulerAngles(
      gravity_x, gravity_y, gravity_z, geomagnetic_x, geomagnetic_y,
      geomagnetic_z, expected_alpha_in_degrees, expected_beta_in_degrees,
      expected_gamma_in_degrees);
}

TEST_F(
    AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometerTest,
    BetaIs45Degrees) {
  double gravity_x = 0.0;
  double gravity_y = std::sin(base::DegToRad(45.0));
  double gravity_z = std::cos(base::DegToRad(45.0));
  double geomagnetic_x = 0.0;
  double geomagnetic_y = 1.0;
  double geomagnetic_z = 0.0;

  double expected_alpha_in_degrees = 0.0;
  double expected_beta_in_degrees = 45.0;
  double expected_gamma_in_degrees = 0.0;

  VerifyAbsoluteOrientationEulerAngles(
      gravity_x, gravity_y, gravity_z, geomagnetic_x, geomagnetic_y,
      geomagnetic_z, expected_alpha_in_degrees, expected_beta_in_degrees,
      expected_gamma_in_degrees);
}

TEST_F(
    AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometerTest,
    GammaIs45Degrees) {
  double gravity_x = -std::sin(base::DegToRad(45.0));
  double gravity_y = 0.0;
  double gravity_z = std::cos(base::DegToRad(45.0));
  double geomagnetic_x = 0.0;
  double geomagnetic_y = 1.0;
  double geomagnetic_z = 0.0;

  double expected_alpha_in_degrees = 0.0;
  double expected_beta_in_degrees = 0.0;
  double expected_gamma_in_degrees = 45.0;

  VerifyAbsoluteOrientationEulerAngles(
      gravity_x, gravity_y, gravity_z, geomagnetic_x, geomagnetic_y,
      geomagnetic_z, expected_alpha_in_degrees, expected_beta_in_degrees,
      expected_gamma_in_degrees);
}

TEST_F(
    AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometerTest,
    AlphaIs45Degrees) {
  double gravity_x = 0.0;
  double gravity_y = 0.0;
  double gravity_z = 1.0;
  double geomagnetic_x = std::sin(base::DegToRad(45.0));
  double geomagnetic_y = std::cos(base::DegToRad(45.0));
  double geomagnetic_z = 0.0;

  double expected_alpha_in_degrees = 45.0;
  double expected_beta_in_degrees = 0.0;
  double expected_gamma_in_degrees = 0.0;

  VerifyAbsoluteOrientationEulerAngles(
      gravity_x, gravity_y, gravity_z, geomagnetic_x, geomagnetic_y,
      geomagnetic_z, expected_alpha_in_degrees, expected_beta_in_degrees,
      expected_gamma_in_degrees);
}

TEST_F(
    AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometerTest,
    GimbalLock) {
  double gravity_x = 0.0;
  double gravity_y = 1.0;
  double gravity_z = 0.0;
  double geomagnetic_x = std::sin(base::DegToRad(45.0));
  double geomagnetic_y = 0.0;
  double geomagnetic_z = -std::cos(base::DegToRad(45.0));

  // Favor Alpha instead of Gamma.
  double expected_alpha_in_degrees = 45.0;
  double expected_beta_in_degrees = 90.0;
  double expected_gamma_in_degrees = 0.0;

  VerifyAbsoluteOrientationEulerAngles(
      gravity_x, gravity_y, gravity_z, geomagnetic_x, geomagnetic_y,
      geomagnetic_z, expected_alpha_in_degrees, expected_beta_in_degrees,
      expected_gamma_in_degrees);
}

TEST_F(
    AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometerTest,
    BetaIsGreaterThan90Degrees) {
  double gravity_x = 0.0;
  double gravity_y = std::cos(base::DegToRad(45.0));
  double gravity_z = -std::sin(base::DegToRad(45.0));
  double geomagnetic_x = 0.0;
  double geomagnetic_y = 0.0;
  double geomagnetic_z = -1.0;

  double expected_alpha_in_degrees = 0.0;
  double expected_beta_in_degrees = 135.0;
  double expected_gamma_in_degrees = 0.0;

  VerifyAbsoluteOrientationEulerAngles(
      gravity_x, gravity_y, gravity_z, geomagnetic_x, geomagnetic_y,
      geomagnetic_z, expected_alpha_in_degrees, expected_beta_in_degrees,
      expected_gamma_in_degrees);
}

TEST_F(
    AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometerTest,
    GammaIsMinus90Degrees) {
  double gravity_x = -1.0;
  double gravity_y = 0.0;
  double gravity_z = 0.0;
  double geomagnetic_x = 0.0;
  double geomagnetic_y = 1.0;
  double geomagnetic_z = 0.0;

  double expected_alpha_in_degrees = 180.0;
  double expected_beta_in_degrees = -180.0;
  double expected_gamma_in_degrees = -90.0;

  VerifyAbsoluteOrientationEulerAngles(
      gravity_x, gravity_y, gravity_z, geomagnetic_x, geomagnetic_y,
      geomagnetic_z, expected_alpha_in_degrees, expected_beta_in_degrees,
      expected_gamma_in_degrees);
}

}  // namespace device
