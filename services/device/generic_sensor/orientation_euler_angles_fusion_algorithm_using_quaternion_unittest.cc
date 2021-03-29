// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/orientation_euler_angles_fusion_algorithm_using_quaternion.h"

#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "services/device/generic_sensor/fake_platform_sensor_fusion.h"
#include "services/device/generic_sensor/generic_sensor_consts.h"
#include "services/device/generic_sensor/orientation_test_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class OrientationEulerAnglesFusionAlgorithmUsingQuaternionTest
    : public testing::Test {
 public:
  OrientationEulerAnglesFusionAlgorithmUsingQuaternionTest() {
    auto fusion_algorithm =
        std::make_unique<OrientationEulerAnglesFusionAlgorithmUsingQuaternion>(
            true /* absolute */);
    fusion_algorithm_ = fusion_algorithm.get();
    fake_fusion_sensor_ = base::MakeRefCounted<FakePlatformSensorFusion>(
        std::move(fusion_algorithm));
    fusion_algorithm_->set_fusion_sensor(fake_fusion_sensor_.get());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<FakePlatformSensorFusion> fake_fusion_sensor_;
  OrientationEulerAnglesFusionAlgorithmUsingQuaternion* fusion_algorithm_;
};

TEST_F(OrientationEulerAnglesFusionAlgorithmUsingQuaternionTest,
       ReadSourceSensorFailed) {
  ASSERT_EQ(1UL, fusion_algorithm_->source_types().size());

  mojom::SensorType source_type = fusion_algorithm_->source_types()[0];
  SensorReading reading;
  SensorReading fused_reading;
  fake_fusion_sensor_->SetSensorReading(source_type, reading,
                                        false /* sensor_reading_success */);
  EXPECT_FALSE(fusion_algorithm_->GetFusedData(source_type, &fused_reading));
}

TEST_F(OrientationEulerAnglesFusionAlgorithmUsingQuaternionTest,
       CheckSampleValues) {
  ASSERT_EQ(quaternions_test_values.size(),
            euler_angles_in_degrees_test_values.size());
  ASSERT_EQ(1UL, fusion_algorithm_->source_types().size());

  mojom::SensorType source_type = fusion_algorithm_->source_types()[0];
  SensorReading reading;
  SensorReading fused_reading;

  for (size_t i = 0; i < quaternions_test_values.size(); ++i) {
    reading.orientation_quat.x = quaternions_test_values[i][0];
    reading.orientation_quat.y = quaternions_test_values[i][1];
    reading.orientation_quat.z = quaternions_test_values[i][2];
    reading.orientation_quat.w = quaternions_test_values[i][3];
    fake_fusion_sensor_->SetSensorReading(source_type, reading,
                                          true /* sensor_reading_success */);
    EXPECT_TRUE(fusion_algorithm_->GetFusedData(source_type, &fused_reading));

    EXPECT_NEAR(euler_angles_in_degrees_test_values[i][0],
                fused_reading.orientation_euler.z /* alpha */, kEpsilon)
        << "on test value " << i;
    EXPECT_NEAR(euler_angles_in_degrees_test_values[i][1],
                fused_reading.orientation_euler.x /* beta */, kEpsilon)
        << "on test value " << i;
    EXPECT_NEAR(euler_angles_in_degrees_test_values[i][2],
                fused_reading.orientation_euler.y /* gamma */, kEpsilon)
        << "on test value " << i;
  }
}

}  // namespace device
