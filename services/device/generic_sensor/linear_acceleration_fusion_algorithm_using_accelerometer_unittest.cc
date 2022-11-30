// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/linear_acceleration_fusion_algorithm_using_accelerometer.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "services/device/generic_sensor/fake_platform_sensor_fusion.h"
#include "services/device/generic_sensor/generic_sensor_consts.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class LinearAccelerationFusionAlgorithmUsingAccelerometerTest
    : public testing::Test {
 public:
  LinearAccelerationFusionAlgorithmUsingAccelerometerTest() {
    auto fusion_algorithm =
        std::make_unique<LinearAccelerationFusionAlgorithmUsingAccelerometer>();
    fusion_algorithm_ = fusion_algorithm.get();
    fake_fusion_sensor_ = base::MakeRefCounted<FakePlatformSensorFusion>(
        std::move(fusion_algorithm));
    fusion_algorithm_->set_fusion_sensor(fake_fusion_sensor_.get());
    EXPECT_EQ(1UL, fusion_algorithm_->source_types().size());
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
    fake_fusion_sensor_->SetSensorReading(mojom::SensorType::ACCELEROMETER,
                                          reading,
                                          true /* sensor_reading_success */);

    SensorReading fused_reading;
    EXPECT_FALSE(fusion_algorithm_->GetFusedData(
        mojom::SensorType::ACCELEROMETER, &fused_reading));
  }

  void VerifyLinearAcceleration(double acceleration_x,
                                double acceleration_y,
                                double acceleration_z,
                                double timestamp,
                                double expected_linear_acceleration_x,
                                double expected_linear_acceleration_y,
                                double expected_linear_acceleration_z) {
    SensorReading reading;
    reading.accel.x = acceleration_x;
    reading.accel.y = acceleration_y;
    reading.accel.z = acceleration_z;
    reading.accel.timestamp.value() = timestamp;
    fake_fusion_sensor_->SetSensorReading(mojom::SensorType::ACCELEROMETER,
                                          reading,
                                          true /* sensor_reading_success */);

    SensorReading fused_reading;
    EXPECT_TRUE(fusion_algorithm_->GetFusedData(
        mojom::SensorType::ACCELEROMETER, &fused_reading));

    EXPECT_NEAR(expected_linear_acceleration_x, fused_reading.accel.x,
                kEpsilon);
    EXPECT_NEAR(expected_linear_acceleration_y, fused_reading.accel.y,
                kEpsilon);
    EXPECT_NEAR(expected_linear_acceleration_z, fused_reading.accel.z,
                kEpsilon);
  }

  void VerifyLinearAccelerationWhenAccelerometerReadingDifferentNonZeroXYZ(
      double timestamp1,
      double timestamp2,
      double timestamp3) {
    double acceleration_x = 1.0;
    double acceleration_y = 2.0;
    double acceleration_z = 3.0;
    VerifyNoFusedDataOnFirstReading(acceleration_x, acceleration_y,
                                    acceleration_z, timestamp1);

    acceleration_x = 4.0;
    acceleration_y = 5.0;
    acceleration_z = 6.0;
    double expected_linear_acceleration_x = 0.6666666666;
    double expected_linear_acceleration_y = 0.8333333333;
    double expected_linear_acceleration_z = 1.0;
    VerifyLinearAcceleration(acceleration_x, acceleration_y, acceleration_z,
                             timestamp2, expected_linear_acceleration_x,
                             expected_linear_acceleration_y,
                             expected_linear_acceleration_z);

    acceleration_x = 7.0;
    acceleration_y = 8.0;
    acceleration_z = 9.0;
    expected_linear_acceleration_x = 0.4782608695;
    expected_linear_acceleration_y = 0.5;
    expected_linear_acceleration_z = 0.5217391304;
    VerifyLinearAcceleration(acceleration_x, acceleration_y, acceleration_z,
                             timestamp3, expected_linear_acceleration_x,
                             expected_linear_acceleration_y,
                             expected_linear_acceleration_z);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<FakePlatformSensorFusion> fake_fusion_sensor_;
  raw_ptr<LinearAccelerationFusionAlgorithmUsingAccelerometer>
      fusion_algorithm_;
};

}  // namespace

TEST_F(LinearAccelerationFusionAlgorithmUsingAccelerometerTest,
       NoAccelerometerReading) {
  fusion_algorithm_->SetFrequency(10.0);

  SensorReading reading;
  fake_fusion_sensor_->SetSensorReading(mojom::SensorType::ACCELEROMETER,
                                        reading,
                                        false /* sensor_reading_success */);

  SensorReading fused_reading;
  EXPECT_FALSE(fusion_algorithm_->GetFusedData(mojom::SensorType::ACCELEROMETER,
                                               &fused_reading));
}

TEST_F(LinearAccelerationFusionAlgorithmUsingAccelerometerTest,
       NoFusedDataOnFirstReading) {
  fusion_algorithm_->SetFrequency(10.0);

  double acceleration_x = 1.0;
  double acceleration_y = 2.0;
  double acceleration_z = 3.0;
  double timestamp = 1.0;

  VerifyNoFusedDataOnFirstReading(acceleration_x, acceleration_y,
                                  acceleration_z, timestamp);
}

TEST_F(LinearAccelerationFusionAlgorithmUsingAccelerometerTest,
       AccelerometerReadingAllZero) {
  fusion_algorithm_->SetFrequency(10.0);

  double acceleration_x = 0.0;
  double acceleration_y = 0.0;
  double acceleration_z = 0.0;
  double timestamp = 1.0;
  VerifyNoFusedDataOnFirstReading(acceleration_x, acceleration_y,
                                  acceleration_z, timestamp);

  timestamp = 2.0;
  double expected_linear_acceleration_x = 0.0;
  double expected_linear_acceleration_y = 0.0;
  double expected_linear_acceleration_z = 0.0;
  VerifyLinearAcceleration(acceleration_x, acceleration_y, acceleration_z,
                           timestamp, expected_linear_acceleration_x,
                           expected_linear_acceleration_y,
                           expected_linear_acceleration_z);
}

TEST_F(LinearAccelerationFusionAlgorithmUsingAccelerometerTest,
       AccelerometerReadingNonZeroX) {
  fusion_algorithm_->SetFrequency(10.0);

  double acceleration_x = 1.0;
  double acceleration_y = 0.0;
  double acceleration_z = 0.0;
  double timestamp = 1.0;
  VerifyNoFusedDataOnFirstReading(acceleration_x, acceleration_y,
                                  acceleration_z, timestamp);

  acceleration_x = 2.0;
  timestamp = 2.0;
  double expected_linear_acceleration_x = 0.3333333333;
  double expected_linear_acceleration_y = 0.0;
  double expected_linear_acceleration_z = 0.0;
  VerifyLinearAcceleration(acceleration_x, acceleration_y, acceleration_z,
                           timestamp, expected_linear_acceleration_x,
                           expected_linear_acceleration_y,
                           expected_linear_acceleration_z);
}

TEST_F(LinearAccelerationFusionAlgorithmUsingAccelerometerTest,
       AccelerometerReadingNonZeroY) {
  fusion_algorithm_->SetFrequency(10.0);

  double acceleration_x = 0.0;
  double acceleration_y = 1.0;
  double acceleration_z = 0.0;
  double timestamp = 1.0;
  VerifyNoFusedDataOnFirstReading(acceleration_x, acceleration_y,
                                  acceleration_z, timestamp);

  acceleration_y = 2.0;
  timestamp = 2.0;
  double expected_linear_acceleration_x = 0.0;
  double expected_linear_acceleration_y = 0.3333333333;
  double expected_linear_acceleration_z = 0.0;
  VerifyLinearAcceleration(acceleration_x, acceleration_y, acceleration_z,
                           timestamp, expected_linear_acceleration_x,
                           expected_linear_acceleration_y,
                           expected_linear_acceleration_z);
}

TEST_F(LinearAccelerationFusionAlgorithmUsingAccelerometerTest,
       AccelerometerReadingNonZeroZ) {
  fusion_algorithm_->SetFrequency(10.0);

  double acceleration_x = 0.0;
  double acceleration_y = 0.0;
  double acceleration_z = 1.0;
  double timestamp = 1.0;
  VerifyNoFusedDataOnFirstReading(acceleration_x, acceleration_y,
                                  acceleration_z, timestamp);

  acceleration_z = 2.0;
  timestamp = 2.0;
  double expected_linear_acceleration_x = 0.0;
  double expected_linear_acceleration_y = 0.0;
  double expected_linear_acceleration_z = 0.3333333333;
  VerifyLinearAcceleration(acceleration_x, acceleration_y, acceleration_z,
                           timestamp, expected_linear_acceleration_x,
                           expected_linear_acceleration_y,
                           expected_linear_acceleration_z);
}

TEST_F(LinearAccelerationFusionAlgorithmUsingAccelerometerTest,
       AccelerometerReadingSameNonZeroXYZ) {
  fusion_algorithm_->SetFrequency(10.0);

  double acceleration = 1.0;
  double timestamp = 1.0;
  VerifyNoFusedDataOnFirstReading(acceleration, acceleration, acceleration,
                                  timestamp);

  acceleration = 2.0;
  timestamp = 2.0;
  double expected_linear_acceleration = 0.3333333333;
  VerifyLinearAcceleration(acceleration, acceleration, acceleration, timestamp,
                           expected_linear_acceleration,
                           expected_linear_acceleration,
                           expected_linear_acceleration);

  acceleration = 3.0;
  timestamp = 3.0;
  expected_linear_acceleration = 0.1739130434;
  VerifyLinearAcceleration(acceleration, acceleration, acceleration, timestamp,
                           expected_linear_acceleration,
                           expected_linear_acceleration,
                           expected_linear_acceleration);
}

TEST_F(LinearAccelerationFusionAlgorithmUsingAccelerometerTest,
       AccelerometerReadingDifferentNonZeroXYZ) {
  fusion_algorithm_->SetFrequency(10.0);

  double timestamp1 = 1.0;
  double timestamp2 = 2.0;
  double timestamp3 = 3.0;
  VerifyLinearAccelerationWhenAccelerometerReadingDifferentNonZeroXYZ(
      timestamp1, timestamp2, timestamp3);
}

TEST_F(LinearAccelerationFusionAlgorithmUsingAccelerometerTest,
       StopSensorShouldClearAllInternalStatisticalData) {
  fusion_algorithm_->SetFrequency(10.0);

  double timestamp1 = 1.0;
  double timestamp2 = 2.0;
  double timestamp3 = 3.0;
  VerifyLinearAccelerationWhenAccelerometerReadingDifferentNonZeroXYZ(
      timestamp1, timestamp2, timestamp3);

  fusion_algorithm_->Reset();

  // After sensor stops, all internal statistical data are reset. When using
  // the same accelerometer data but different timestamps, the linear
  // acceleration fused data should be the same as before.
  fusion_algorithm_->SetFrequency(10.0);
  double timestamp4 = 4.0;
  double timestamp5 = 5.0;
  double timestamp6 = 6.0;
  VerifyLinearAccelerationWhenAccelerometerReadingDifferentNonZeroXYZ(
      timestamp4, timestamp5, timestamp6);
}

}  // namespace device
