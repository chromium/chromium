// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/gravity_fusion_algorithm_using_accelerometer.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "services/device/generic_sensor/fake_platform_sensor_fusion.h"
#include "services/device/generic_sensor/generic_sensor_consts.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class GravityFusionAlgorithmUsingAccelerometerTest : public testing::Test {
 public:
  GravityFusionAlgorithmUsingAccelerometerTest() {
    auto fusion_algorithm =
        std::make_unique<GravityFusionAlgorithmUsingAccelerometer>();
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
                                          /*sensor_reading_success=*/true);

    SensorReading fused_reading;
    EXPECT_FALSE(fusion_algorithm_->GetFusedData(
        mojom::SensorType::ACCELEROMETER, &fused_reading));
  }

  void VerifyGravity(double acceleration_x,
                     double acceleration_y,
                     double acceleration_z,
                     double timestamp,
                     double expected_gravity_x,
                     double expected_gravity_y,
                     double expected_gravity_z) {
    SensorReading reading;
    reading.accel.x = acceleration_x;
    reading.accel.y = acceleration_y;
    reading.accel.z = acceleration_z;
    reading.accel.timestamp.value() = timestamp;
    fake_fusion_sensor_->SetSensorReading(mojom::SensorType::ACCELEROMETER,
                                          reading,
                                          /*sensor_reading_success=*/true);

    SensorReading fused_reading;
    EXPECT_TRUE(fusion_algorithm_->GetFusedData(
        mojom::SensorType::ACCELEROMETER, &fused_reading));

    EXPECT_NEAR(expected_gravity_x, fused_reading.accel.x, kEpsilon);
    EXPECT_NEAR(expected_gravity_y, fused_reading.accel.y, kEpsilon);
    EXPECT_NEAR(expected_gravity_z, fused_reading.accel.z, kEpsilon);
  }

  void VerifyGravityWhenAccelerometerReadingDifferentNonZeroXYZ(
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
    double expected_gravity_x = 3.3333333333;
    double expected_gravity_y = 4.166666667;
    double expected_gravity_z = 5.0;
    VerifyGravity(acceleration_x, acceleration_y, acceleration_z, timestamp2,
                  expected_gravity_x, expected_gravity_y, expected_gravity_z);

    acceleration_x = 7.0;
    acceleration_y = 8.0;
    acceleration_z = 9.0;
    expected_gravity_x = 6.52173913;
    expected_gravity_y = 7.5;
    expected_gravity_z = 8.47826087;
    VerifyGravity(acceleration_x, acceleration_y, acceleration_z, timestamp3,
                  expected_gravity_x, expected_gravity_y, expected_gravity_z);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<FakePlatformSensorFusion> fake_fusion_sensor_;
  raw_ptr<GravityFusionAlgorithmUsingAccelerometer> fusion_algorithm_;
};

}  // namespace

TEST_F(GravityFusionAlgorithmUsingAccelerometerTest, NoAccelerometerReading) {
  fusion_algorithm_->SetFrequency(10.0);

  SensorReading reading;
  fake_fusion_sensor_->SetSensorReading(mojom::SensorType::ACCELEROMETER,
                                        reading,
                                        /*sensor_reading_success=*/false);

  SensorReading fused_reading;
  EXPECT_FALSE(fusion_algorithm_->GetFusedData(mojom::SensorType::ACCELEROMETER,
                                               &fused_reading));
}

TEST_F(GravityFusionAlgorithmUsingAccelerometerTest,
       NoFusedDataOnFirstReading) {
  fusion_algorithm_->SetFrequency(10.0);

  double acceleration_x = 1.0;
  double acceleration_y = 2.0;
  double acceleration_z = 3.0;
  double timestamp = 1.0;

  VerifyNoFusedDataOnFirstReading(acceleration_x, acceleration_y,
                                  acceleration_z, timestamp);
}

TEST_F(GravityFusionAlgorithmUsingAccelerometerTest,
       AccelerometerReadingAllZero) {
  fusion_algorithm_->SetFrequency(10.0);

  double acceleration_x = 0.0;
  double acceleration_y = 0.0;
  double acceleration_z = 0.0;
  double timestamp = 1.0;
  VerifyNoFusedDataOnFirstReading(acceleration_x, acceleration_y,
                                  acceleration_z, timestamp);

  timestamp = 2.0;
  double expected_gravity_x = 0.0;
  double expected_gravity_y = 0.0;
  double expected_gravity_z = 0.0;
  VerifyGravity(acceleration_x, acceleration_y, acceleration_z, timestamp,
                expected_gravity_x, expected_gravity_y, expected_gravity_z);
}

TEST_F(GravityFusionAlgorithmUsingAccelerometerTest,
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
  double expected_gravity_x = 1.6666666666;
  double expected_gravity_y = 0.0;
  double expected_gravity_z = 0.0;
  VerifyGravity(acceleration_x, acceleration_y, acceleration_z, timestamp,
                expected_gravity_x, expected_gravity_y, expected_gravity_z);
}

TEST_F(GravityFusionAlgorithmUsingAccelerometerTest,
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
  double expected_gravity_x = 0.0;
  double expected_gravity_y = 1.6666666666;
  double expected_gravity_z = 0.0;
  VerifyGravity(acceleration_x, acceleration_y, acceleration_z, timestamp,
                expected_gravity_x, expected_gravity_y, expected_gravity_z);
}

TEST_F(GravityFusionAlgorithmUsingAccelerometerTest,
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
  double expected_gravity_x = 0.0;
  double expected_gravity_y = 0.0;
  double expected_gravity_z = 1.6666666666;
  VerifyGravity(acceleration_x, acceleration_y, acceleration_z, timestamp,
                expected_gravity_x, expected_gravity_y, expected_gravity_z);
}

TEST_F(GravityFusionAlgorithmUsingAccelerometerTest,
       AccelerometerReadingSameNonZeroXYZ) {
  fusion_algorithm_->SetFrequency(10.0);

  double acceleration = 1.0;
  double timestamp = 1.0;
  VerifyNoFusedDataOnFirstReading(acceleration, acceleration, acceleration,
                                  timestamp);

  acceleration = 2.0;
  timestamp = 2.0;
  double expected_gravity = 1.6666666666;
  VerifyGravity(acceleration, acceleration, acceleration, timestamp,
                expected_gravity, expected_gravity, expected_gravity);

  acceleration = 3.0;
  timestamp = 3.0;
  expected_gravity = 2.826086957;
  VerifyGravity(acceleration, acceleration, acceleration, timestamp,
                expected_gravity, expected_gravity, expected_gravity);
}

TEST_F(GravityFusionAlgorithmUsingAccelerometerTest,
       AccelerometerReadingDifferentNonZeroXYZ) {
  fusion_algorithm_->SetFrequency(10.0);

  double timestamp1 = 1.0;
  double timestamp2 = 2.0;
  double timestamp3 = 3.0;
  VerifyGravityWhenAccelerometerReadingDifferentNonZeroXYZ(
      timestamp1, timestamp2, timestamp3);
}

TEST_F(GravityFusionAlgorithmUsingAccelerometerTest,
       StopSensorShouldClearAllInternalStatisticalData) {
  fusion_algorithm_->SetFrequency(10.0);

  double timestamp1 = 1.0;
  double timestamp2 = 2.0;
  double timestamp3 = 3.0;
  VerifyGravityWhenAccelerometerReadingDifferentNonZeroXYZ(
      timestamp1, timestamp2, timestamp3);

  fusion_algorithm_->Reset();

  // After sensor stops, all internal statistical data are reset. When using
  // the same accelerometer data but different timestamps, the gravity
  // fused data should be the same as before.
  fusion_algorithm_->SetFrequency(10.0);
  double timestamp4 = 4.0;
  double timestamp5 = 5.0;
  double timestamp6 = 6.0;
  VerifyGravityWhenAccelerometerReadingDifferentNonZeroXYZ(
      timestamp4, timestamp5, timestamp6);
}

}  // namespace device
