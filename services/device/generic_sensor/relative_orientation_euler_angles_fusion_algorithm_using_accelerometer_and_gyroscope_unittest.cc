// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/relative_orientation_euler_angles_fusion_algorithm_using_accelerometer_and_gyroscope.h"

#include <numbers>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/math_constants.h"
#include "base/test/task_environment.h"
#include "services/device/generic_sensor/fake_platform_sensor_fusion.h"
#include "services/device/generic_sensor/generic_sensor_consts.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class
    RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscopeTest
    : public testing::Test {
 public:
  RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscopeTest() {
    auto fusion_algorithm = std::make_unique<
        RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscope>();
    fusion_algorithm_ = fusion_algorithm.get();
    fake_fusion_sensor_ = base::MakeRefCounted<FakePlatformSensorFusion>(
        std::move(fusion_algorithm));
    fusion_algorithm_->set_fusion_sensor(fake_fusion_sensor_.get());
    EXPECT_EQ(2UL, fusion_algorithm_->source_types().size());
  }

  void VerifyRelativeOrientation(double accel_x,
                                 double accel_y,
                                 double accel_z,
                                 double gyro_x,
                                 double gyro_y,
                                 double gyro_z,
                                 double gyro_timestamp,
                                 double expected_relative_orientation_alpha,
                                 double expected_relative_orientation_beta,
                                 double expected_relative_orientation_gamma) {
    SensorReading accel_reading;
    accel_reading.accel.x = accel_x;
    accel_reading.accel.y = accel_y;
    accel_reading.accel.z = accel_z;
    fake_fusion_sensor_->SetSensorReading(mojom::SensorType::ACCELEROMETER,
                                          accel_reading,
                                          true /* sensor_reading_success */);

    SensorReading gyro_reading;
    gyro_reading.gyro.x = gyro_x;
    gyro_reading.gyro.y = gyro_y;
    gyro_reading.gyro.z = gyro_z;
    gyro_reading.gyro.timestamp.value() = gyro_timestamp;
    fake_fusion_sensor_->SetSensorReading(mojom::SensorType::GYROSCOPE,
                                          gyro_reading,
                                          true /* sensor_reading_success */);

    SensorReading fused_reading;
    EXPECT_TRUE(fusion_algorithm_->GetFusedData(mojom::SensorType::GYROSCOPE,
                                                &fused_reading));

    EXPECT_NEAR(expected_relative_orientation_alpha,
                fused_reading.orientation_euler.z, kEpsilon);
    EXPECT_NEAR(expected_relative_orientation_beta,
                fused_reading.orientation_euler.x, kEpsilon);
    EXPECT_NEAR(expected_relative_orientation_gamma,
                fused_reading.orientation_euler.y, kEpsilon);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<FakePlatformSensorFusion> fake_fusion_sensor_;
  raw_ptr<
      RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscope>
      fusion_algorithm_;
};

}  // namespace

TEST_F(
    RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscopeTest,
    GyroscopeReadingNotChanged) {
  SensorReading reading;
  fake_fusion_sensor_->SetSensorReading(mojom::SensorType::ACCELEROMETER,
                                        reading,
                                        true /* sensor_reading_success */);

  fake_fusion_sensor_->SetSensorReading(mojom::SensorType::GYROSCOPE, reading,
                                        true /* sensor_reading_success */);

  SensorReading fused_reading;
  EXPECT_FALSE(fusion_algorithm_->GetFusedData(mojom::SensorType::ACCELEROMETER,
                                               &fused_reading));
}

TEST_F(
    RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscopeTest,
    NoAccelerometerReading) {
  SensorReading reading;
  fake_fusion_sensor_->SetSensorReading(mojom::SensorType::ACCELEROMETER,
                                        reading,
                                        false /* sensor_reading_success */);

  fake_fusion_sensor_->SetSensorReading(mojom::SensorType::GYROSCOPE, reading,
                                        true /* sensor_reading_success */);

  SensorReading fused_reading;
  EXPECT_FALSE(fusion_algorithm_->GetFusedData(mojom::SensorType::GYROSCOPE,
                                               &fused_reading));
}

TEST_F(
    RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscopeTest,
    NoGyroscopeReading) {
  SensorReading reading;
  fake_fusion_sensor_->SetSensorReading(mojom::SensorType::ACCELEROMETER,
                                        reading,
                                        true /* sensor_reading_success */);

  fake_fusion_sensor_->SetSensorReading(mojom::SensorType::GYROSCOPE, reading,
                                        false /* sensor_reading_success */);

  SensorReading fused_reading;
  EXPECT_FALSE(fusion_algorithm_->GetFusedData(mojom::SensorType::GYROSCOPE,
                                               &fused_reading));
}

TEST_F(
    RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscopeTest,
    AccelerometerAndGyroscopeReadingAllZero) {
  double accel_x = 0.0;
  double accel_y = 0.0;
  double accel_z = 0.0;
  double gyro_x = 0.0;
  double gyro_y = 0.0;
  double gyro_z = 0.0;
  double gyro_timestamp = 1.0;
  double expected_relative_orientation_alpha = 0.0;
  double expected_relative_orientation_beta = 0.0;
  double expected_relative_orientation_gamma = 0.0;
  VerifyRelativeOrientation(accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z,
                            gyro_timestamp, expected_relative_orientation_alpha,
                            expected_relative_orientation_beta,
                            expected_relative_orientation_gamma);
}

TEST_F(
    RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscopeTest,
    GryoscopeZReadingPositiveValues) {
  double accel_x = 0.0;
  double accel_y = 0.0;
  double accel_z = base::kMeanGravityDouble;
  double gyro_x = 0.0;
  double gyro_y = 0.0;
  double gyro_z = std::numbers::pi;
  const std::vector<double> gyro_timestamp = {0.5, 1.0, 1.5, 2.0, 2.5};
  const std::vector<double> expected_relative_orientation_alpha = {
      0.0, 90.0, 180.0, 270.0, 0.0};
  double expected_relative_orientation_beta = 0.0;
  double expected_relative_orientation_gamma = 0.0;

  for (size_t i = 0; i < gyro_timestamp.size(); ++i) {
    VerifyRelativeOrientation(accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z,
                              gyro_timestamp[i],
                              expected_relative_orientation_alpha[i],
                              expected_relative_orientation_beta,
                              expected_relative_orientation_gamma);
  }
}

TEST_F(
    RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscopeTest,
    GryoscopeZReadingNegativeValues) {
  double accel_x = 0.0;
  double accel_y = 0.0;
  double accel_z = base::kMeanGravityDouble;
  double gyro_x = 0.0;
  double gyro_y = 0.0;
  double gyro_z = -std::numbers::pi;
  const std::vector<double> gyro_timestamp = {0.5, 1.0, 1.5, 2.0, 2.5};
  const std::vector<double> expected_relative_orientation_alpha = {
      0.0, 270.0, 180.0, 90.0, 0.0};
  double expected_relative_orientation_beta = 0.0;
  double expected_relative_orientation_gamma = 0.0;

  for (size_t i = 0; i < gyro_timestamp.size(); ++i) {
    VerifyRelativeOrientation(accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z,
                              gyro_timestamp[i],
                              expected_relative_orientation_alpha[i],
                              expected_relative_orientation_beta,
                              expected_relative_orientation_gamma);
  }
}

TEST_F(
    RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscopeTest,
    GryoscopeXReadingNonZeroValues) {
  // Test the device rotates around x-axis in 360 degrees with positive |gyro_x|
  // reading, and in each step it rotates pi/6 radians.
  double accel_x = 0.0;
  double accel_y = 0.0;
  double accel_z = 0.0;
  double gyro_x = std::numbers::pi;
  double gyro_y = 0.0;
  double gyro_z = 0.0;
  double gyro_timestamp = 1.0 / 6.0;
  const double kTimestampIncrement = 1.0 / 6.0;
  const double kAngleIncrement = kTimestampIncrement * gyro_x;
  double angle = 0.0;

  double expected_relative_orientation_alpha = 0.0;
  const std::vector<double> expected_relative_orientation_beta = {
      0.0,
      29.3999999999,
      58.212,
      86.4477599999,
      114.1188047999,
      141.2364287039,
      167.8117001299,
      -166.1445338726,
      -133.4216431952,
      -101.3532103313,
      -69.9261461246,
      -39.1276232022,
      -8.9450707381};
  const std::vector<double> expected_relative_orientation_gamma = {
      0.0,           -0.9,          -2.4408457268, -4.1920288122, -5.6670339628,
      -6.4536932835, -6.3246194179, -5.2981270295, -3.6333187621, -1.7606523869,
      -0.1665936123, 0.7367382598,  0.7220034946};

  for (size_t i = 0; i < expected_relative_orientation_beta.size(); ++i) {
    VerifyRelativeOrientation(accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z,
                              gyro_timestamp,
                              expected_relative_orientation_alpha,
                              expected_relative_orientation_beta[i],
                              expected_relative_orientation_gamma[i]);

    gyro_timestamp += kTimestampIncrement;
    angle += kAngleIncrement;
    accel_y = base::kMeanGravityDouble * std::sin(angle);
    accel_z = base::kMeanGravityDouble * std::cos(angle);
  }

  // Test the device rotates around x-axis in 360 degrees with negative |gyro_x|
  // reading, and in each step it rotates std::numbers::pi/6 radians.
  fusion_algorithm_->Reset();
  accel_y = 0.0;
  accel_z = 0.0;
  gyro_x = -std::numbers::pi;
  gyro_timestamp = 1.0 / 6.0;
  angle = 0.0;
  for (size_t i = 0; i < expected_relative_orientation_beta.size(); ++i) {
    VerifyRelativeOrientation(accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z,
                              gyro_timestamp,
                              expected_relative_orientation_alpha,
                              -expected_relative_orientation_beta[i],
                              -expected_relative_orientation_gamma[i]);

    gyro_timestamp += kTimestampIncrement;
    angle += kAngleIncrement;
    // Here the |accel_y| is different from the above because the device
    // rotates around x-axis in the opposite direction.
    accel_y = -base::kMeanGravityDouble * std::sin(angle);
    accel_z = base::kMeanGravityDouble * std::cos(angle);
  }
}

TEST_F(
    RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscopeTest,
    GryoscopeYReadingNonZeroValues) {
  // Test the device rotates around y-axis in 360 degrees with positive |gyro_y|
  // reading, and in each step it rotates std::numbers::pi/6 radians.
  double accel_x = 0.0;
  double accel_y = 0.0;
  double accel_z = 0.0;
  double gyro_x = 0.0;
  double gyro_y = std::numbers::pi;
  double gyro_z = 0.0;
  double gyro_timestamp = 1.0 / 6.0;
  const double kTimestampIncrement = 1.0 / 6.0;
  const double kAngleIncrement = kTimestampIncrement * gyro_y;
  double angle = 0.0;

  double expected_relative_orientation_alpha = 0.0;
  const std::vector<double> expected_relative_orientation_beta = {
      0.0,           -0.9,          -2.4408457268, -4.1920288122, -5.6670339628,
      -6.4536932835, -6.3246194179, -5.2981270295, -3.6333187621, -1.7606523869,
      -0.1665936123, 0.7367382598,  0.7220034946};
  const std::vector<double> expected_relative_orientation_gamma = {
      0.0,           29.3999999999, 58.212,         86.4477599999,
      -65.8811952,   -35.163571296, -5.06029987,    24.4409061273,
      53.3520880047, 81.6850462446, -70.5486546802, -39.7376815866,
      -9.5429279548};

  for (size_t i = 0; i < expected_relative_orientation_beta.size(); ++i) {
    VerifyRelativeOrientation(accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z,
                              gyro_timestamp,
                              expected_relative_orientation_alpha,
                              expected_relative_orientation_beta[i],
                              expected_relative_orientation_gamma[i]);

    gyro_timestamp += kTimestampIncrement;
    angle += kAngleIncrement;
    accel_x = -base::kMeanGravityDouble * std::sin(angle);
    accel_z = base::kMeanGravityDouble * std::cos(angle);
  }

  // Test the device rotates around y-axis in 360 degrees with negative |gyro_y|
  // reading, and in each step it rotates std::numbers::pi/6 radians.
  fusion_algorithm_->Reset();
  accel_x = 0.0;
  accel_z = 0.0;
  gyro_y = -std::numbers::pi;
  gyro_timestamp = 1.0 / 6.0;
  angle = 0.0;
  for (size_t i = 0; i < expected_relative_orientation_beta.size(); ++i) {
    VerifyRelativeOrientation(accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z,
                              gyro_timestamp,
                              expected_relative_orientation_alpha,
                              -expected_relative_orientation_beta[i],
                              -expected_relative_orientation_gamma[i]);

    gyro_timestamp += kTimestampIncrement;
    angle += kAngleIncrement;
    // Here the |accel_x| is different from the above because the device
    // rotates around y-axis in the opposite direction.
    accel_x = base::kMeanGravityDouble * std::sin(angle);
    accel_z = base::kMeanGravityDouble * std::cos(angle);
  }
}

TEST_F(
    RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscopeTest,
    AccelerometerZReadingChangeNotAffectingAlpha) {
  double accel_x = 0.0;
  double accel_y = 0.0;
  const std::vector<double> accel_z = {0.0, 1.0, 2.0, 3.0, 4.0};
  double gyro_x = 0.0;
  double gyro_y = 0.0;
  double gyro_z = std::numbers::pi;
  const std::vector<double> gyro_timestamp = {0.5, 1.0, 1.5, 2.0, 2.5};
  const std::vector<double> expected_relative_orientation_alpha = {
      0.0, 90.0, 180.0, 270.0, 0.0};
  double expected_relative_orientation_beta = 0.0;
  double expected_relative_orientation_gamma = 0.0;

  for (size_t i = 0; i < gyro_timestamp.size(); ++i) {
    VerifyRelativeOrientation(accel_x, accel_y, accel_z[i], gyro_x, gyro_y,
                              gyro_z, gyro_timestamp[i],
                              expected_relative_orientation_alpha[i],
                              expected_relative_orientation_beta,
                              expected_relative_orientation_gamma);
  }
}

TEST_F(
    RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscopeTest,
    StopSensorShouldClearAllInternalStatisticalData) {
  double accel_x_1 = 1.0;
  double accel_y_1 = 2.0;
  double accel_z_1 = 3.0;
  double gyro_x_1 = 4.0;
  double gyro_y_1 = 5.0;
  double gyro_z_1 = 6.0;
  double gyro_timestamp_1 = 1.0;
  double expected_relative_orientation_alpha_1 = 0.0;
  double expected_relative_orientation_beta_1 = 0.48107023544;
  double expected_relative_orientation_gamma_1 = -0.9621404708;
  VerifyRelativeOrientation(accel_x_1, accel_y_1, accel_z_1, gyro_x_1, gyro_y_1,
                            gyro_z_1, gyro_timestamp_1,
                            expected_relative_orientation_alpha_1,
                            expected_relative_orientation_beta_1,
                            expected_relative_orientation_gamma_1);
  double accel_x_2 = 7.0;
  double accel_y_2 = 8.0;
  double accel_z_2 = 9.0;
  double gyro_x_2 = 10.0;
  double gyro_y_2 = 11.0;
  double gyro_z_2 = 12.0;
  double gyro_timestamp_2 = 2.0;
  double expected_relative_orientation_alpha_2 = 327.5493541569;
  double expected_relative_orientation_beta_2 = -157.1252846612;
  double expected_relative_orientation_gamma_2 = 75.6717457411;
  VerifyRelativeOrientation(accel_x_2, accel_y_2, accel_z_2, gyro_x_2, gyro_y_2,
                            gyro_z_2, gyro_timestamp_2,
                            expected_relative_orientation_alpha_2,
                            expected_relative_orientation_beta_2,
                            expected_relative_orientation_gamma_2);

  fusion_algorithm_->Reset();

  // After sensor stops, all internal statistical data are reset. When using
  // the same accelerometer and gyroscope data but different timestamps (as
  // long as the timestamp delta is the same), the relative orientation fused
  // data should be the same as before.
  double gyro_timestamp_3 = 3.0;
  VerifyRelativeOrientation(accel_x_1, accel_y_1, accel_z_1, gyro_x_1, gyro_y_1,
                            gyro_z_1, gyro_timestamp_3,
                            expected_relative_orientation_alpha_1,
                            expected_relative_orientation_beta_1,
                            expected_relative_orientation_gamma_1);
  double gyro_timestamp_4 = 4.0;
  VerifyRelativeOrientation(accel_x_2, accel_y_2, accel_z_2, gyro_x_2, gyro_y_2,
                            gyro_z_2, gyro_timestamp_4,
                            expected_relative_orientation_alpha_2,
                            expected_relative_orientation_beta_2,
                            expected_relative_orientation_gamma_2);
}

}  // namespace device
