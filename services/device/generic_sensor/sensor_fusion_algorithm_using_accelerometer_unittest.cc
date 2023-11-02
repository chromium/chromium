// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "services/device/generic_sensor/gravity_fusion_algorithm_using_accelerometer.h"
#include "services/device/generic_sensor/linear_acceleration_fusion_algorithm_using_accelerometer.h"

#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "services/device/generic_sensor/fake_platform_sensor_fusion.h"
#include "services/device/generic_sensor/generic_sensor_consts.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class SensorFusionAlgorithmUsingAccelerometerTest : public testing::Test {
 public:
  SensorFusionAlgorithmUsingAccelerometerTest() {
    // Gravity fusion sensor.
    auto gravity_fusion_algorithm =
        std::make_unique<GravityFusionAlgorithmUsingAccelerometer>();
    gravity_fusion_algorithm_ = gravity_fusion_algorithm.get();
    fake_gravity_fusion_sensor_ =
        base::MakeRefCounted<FakePlatformSensorFusion>(
            std::move(gravity_fusion_algorithm));
    gravity_fusion_algorithm_->set_fusion_sensor(
        fake_gravity_fusion_sensor_.get());
    EXPECT_EQ(1UL, gravity_fusion_algorithm_->source_types().size());

    // Linear acceleration fusion sensor.
    auto linear_acceleration_fusion_algorithm =
        std::make_unique<LinearAccelerationFusionAlgorithmUsingAccelerometer>();
    linear_acceleration_fusion_algorithm_ =
        linear_acceleration_fusion_algorithm.get();
    fake_linear_acceleration_fusion_sensor_ =
        base::MakeRefCounted<FakePlatformSensorFusion>(
            std::move(linear_acceleration_fusion_algorithm));
    linear_acceleration_fusion_algorithm_->set_fusion_sensor(
        fake_linear_acceleration_fusion_sensor_.get());
    EXPECT_EQ(1UL,
              linear_acceleration_fusion_algorithm_->source_types().size());
  }

  void VerifyNoFusedDataOnFirstReading(double acceleration_x,
                                       double acceleration_y,
                                       double acceleration_z,
                                       double timestamp) {
    SensorReading reading;
    reading.accel.x = acceleration_x;
    reading.accel.y = acceleration_y;
    reading.accel.z = acceleration_z;
    reading.accel.timestamp.value() = timestamp;
    fake_gravity_fusion_sensor_->SetSensorReading(
        mojom::SensorType::ACCELEROMETER, reading,
        /*sensor_reading_success=*/true);
    fake_linear_acceleration_fusion_sensor_->SetSensorReading(
        mojom::SensorType::ACCELEROMETER, reading,
        /*sensor_reading_success=*/true);

    SensorReading linear_acceleration_fused_reading;
    SensorReading gravity_fused_reading;
    EXPECT_FALSE(linear_acceleration_fusion_algorithm_->GetFusedData(
        mojom::SensorType::ACCELEROMETER, &linear_acceleration_fused_reading));
    EXPECT_FALSE(gravity_fusion_algorithm_->GetFusedData(
        mojom::SensorType::ACCELEROMETER, &gravity_fused_reading));
  }

  void VerifyResults(double acceleration_x,
                     double acceleration_y,
                     double acceleration_z,
                     double timestamp) {
    SensorReading reading;
    reading.accel.x = acceleration_x;
    reading.accel.y = acceleration_y;
    reading.accel.z = acceleration_z;
    reading.accel.timestamp.value() = timestamp;
    fake_gravity_fusion_sensor_->SetSensorReading(
        mojom::SensorType::ACCELEROMETER, reading,
        /*sensor_reading_success=*/true);
    fake_linear_acceleration_fusion_sensor_->SetSensorReading(
        mojom::SensorType::ACCELEROMETER, reading,
        /*sensor_reading_success=*/true);

    SensorReading gravity_fused_reading;
    SensorReading linear_acceleration_fused_reading;
    EXPECT_TRUE(gravity_fusion_algorithm_->GetFusedData(
        mojom::SensorType::ACCELEROMETER, &gravity_fused_reading));
    EXPECT_TRUE(linear_acceleration_fusion_algorithm_->GetFusedData(
        mojom::SensorType::ACCELEROMETER, &linear_acceleration_fused_reading));

    double combined_acceleration_x = gravity_fused_reading.accel.x +
                                     linear_acceleration_fused_reading.accel.x;
    double combined_acceleration_y = gravity_fused_reading.accel.y +
                                     linear_acceleration_fused_reading.accel.y;
    double combined_acceleration_z = gravity_fused_reading.accel.z +
                                     linear_acceleration_fused_reading.accel.z;

    EXPECT_NEAR(acceleration_x, combined_acceleration_x, kEpsilon);
    EXPECT_NEAR(acceleration_y, combined_acceleration_y, kEpsilon);
    EXPECT_NEAR(acceleration_z, combined_acceleration_z, kEpsilon);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<FakePlatformSensorFusion> fake_gravity_fusion_sensor_;
  scoped_refptr<FakePlatformSensorFusion>
      fake_linear_acceleration_fusion_sensor_;
  raw_ptr<GravityFusionAlgorithmUsingAccelerometer> gravity_fusion_algorithm_;
  raw_ptr<LinearAccelerationFusionAlgorithmUsingAccelerometer>
      linear_acceleration_fusion_algorithm_;
};

}  // namespace

TEST_F(SensorFusionAlgorithmUsingAccelerometerTest,
       AccelerometerReadingNonZeroX) {
  gravity_fusion_algorithm_->SetFrequency(10.0);
  linear_acceleration_fusion_algorithm_->SetFrequency(10.0);

  double acceleration_x = 1.0;
  double acceleration_y = 0.0;
  double acceleration_z = 0.0;
  double timestamp = 1.0;
  VerifyNoFusedDataOnFirstReading(acceleration_x, acceleration_y,
                                  acceleration_z, timestamp);

  acceleration_x = 2.0;
  timestamp = 2.0;
  VerifyResults(acceleration_x, acceleration_y, acceleration_z, timestamp);
}

TEST_F(SensorFusionAlgorithmUsingAccelerometerTest,
       AccelerometerReadingNonZeroY) {
  gravity_fusion_algorithm_->SetFrequency(10.0);
  linear_acceleration_fusion_algorithm_->SetFrequency(10.0);

  double acceleration_x = 0.0;
  double acceleration_y = 1.0;
  double acceleration_z = 0.0;
  double timestamp = 1.0;
  VerifyNoFusedDataOnFirstReading(acceleration_x, acceleration_y,
                                  acceleration_z, timestamp);

  acceleration_y = 2.0;
  timestamp = 2.0;
  VerifyResults(acceleration_x, acceleration_y, acceleration_z, timestamp);
}

TEST_F(SensorFusionAlgorithmUsingAccelerometerTest,
       AccelerometerReadingNonZeroZ) {
  gravity_fusion_algorithm_->SetFrequency(10.0);
  linear_acceleration_fusion_algorithm_->SetFrequency(10.0);

  double acceleration_x = 0.0;
  double acceleration_y = 0.0;
  double acceleration_z = 1.0;
  double timestamp = 1.0;
  VerifyNoFusedDataOnFirstReading(acceleration_x, acceleration_y,
                                  acceleration_z, timestamp);

  acceleration_z = 2.0;
  timestamp = 2.0;
  VerifyResults(acceleration_x, acceleration_y, acceleration_z, timestamp);
}

TEST_F(SensorFusionAlgorithmUsingAccelerometerTest,
       AccelerometerReadingNonZeroXYZ) {
  gravity_fusion_algorithm_->SetFrequency(10.0);
  linear_acceleration_fusion_algorithm_->SetFrequency(10.0);

  double acceleration_x = 1.0;
  double acceleration_y = 1.0;
  double acceleration_z = 1.0;
  double timestamp = 1.0;
  VerifyNoFusedDataOnFirstReading(acceleration_x, acceleration_y,
                                  acceleration_z, timestamp);

  acceleration_x = 1.0;
  acceleration_y = 2.0;
  acceleration_z = 3.0;
  timestamp = 2.0;
  VerifyResults(acceleration_x, acceleration_y, acceleration_z, timestamp);

  acceleration_x = 4.0;
  acceleration_y = 5.0;
  acceleration_z = 6.0;
  timestamp = 3.0;
  VerifyResults(acceleration_x, acceleration_y, acceleration_z, timestamp);

  acceleration_x = 7.0;
  acceleration_y = 8.0;
  acceleration_z = 9.0;
  timestamp = 4.0;
  VerifyResults(acceleration_x, acceleration_y, acceleration_z, timestamp);
}
}  // namespace device
