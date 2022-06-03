// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_util.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"

namespace device {

TEST(PlatformSensorUtil, RoundPositive) {
  EXPECT_DOUBLE_EQ(1.2, RoundToMultiple(1.20, 0.1));
  EXPECT_DOUBLE_EQ(1.2, RoundToMultiple(1.24, 0.1));
  EXPECT_DOUBLE_EQ(1.3, RoundToMultiple(1.25, 0.1));
  EXPECT_DOUBLE_EQ(1.3, RoundToMultiple(1.29, 0.1));
}

TEST(PlatformSensorUtil, RoundNegative) {
  EXPECT_DOUBLE_EQ(-1.2, RoundToMultiple(-1.20, 0.1));
  EXPECT_DOUBLE_EQ(-1.2, RoundToMultiple(-1.24, 0.1));
  EXPECT_DOUBLE_EQ(-1.3, RoundToMultiple(-1.25, 0.1));
  EXPECT_DOUBLE_EQ(-1.3, RoundToMultiple(-1.29, 0.1));
}

TEST(PlatformSensorUtil, RoundZero) {
  EXPECT_DOUBLE_EQ(0.0, RoundToMultiple(0.0, 0.1));
}

TEST(PlatformSensorUtil, RoundMax) {
  EXPECT_DOUBLE_EQ(std::numeric_limits<double>::max(),
                   RoundToMultiple(std::numeric_limits<double>::max(), 0.1));
  EXPECT_DOUBLE_EQ(std::numeric_limits<double>::max(),
                   RoundToMultiple(std::numeric_limits<double>::max(), 5.0));
}

TEST(PlatformSensorUtil, RoundLowest) {
  EXPECT_DOUBLE_EQ(std::numeric_limits<double>::lowest(),
                   RoundToMultiple(std::numeric_limits<double>::lowest(), 0.1));
  EXPECT_DOUBLE_EQ(std::numeric_limits<double>::lowest(),
                   RoundToMultiple(std::numeric_limits<double>::lowest(), 5.0));
}

TEST(PlatformSensorUtil, RoundMin) {
  EXPECT_DOUBLE_EQ(0.0,
                   RoundToMultiple(std::numeric_limits<double>::min(), 0.1));
  EXPECT_DOUBLE_EQ(0.0,
                   RoundToMultiple(std::numeric_limits<double>::min(), 5.0));
}

TEST(PlatformSensorUtil, RoundQuaternion) {
  // A hard coded quaternion and known outputs.
  SensorReadingQuat quat;
  quat.x = 0.408333;
  quat.y = -0.694318;
  quat.z = -0.107955;
  quat.w = 0.582693;

  RoundOrientationQuaternionReading(&quat);
  EXPECT_DOUBLE_EQ(0.40879894045956067, quat.x);
  EXPECT_DOUBLE_EQ(-0.69400288443777802, quat.y);
  EXPECT_DOUBLE_EQ(-0.10747054463443845, quat.z);
  EXPECT_DOUBLE_EQ(0.58283231268278346, quat.w);

  // Test Quaternion with zero angle to detect division by zero.
  quat.x = 0.408333;
  quat.y = -0.694318;
  quat.z = -0.107955;
  quat.w = 1.0;  // This is a zero rotation value.

  RoundOrientationQuaternionReading(&quat);
  EXPECT_DOUBLE_EQ(0.0, quat.x);
  EXPECT_DOUBLE_EQ(0.0, quat.y);
  EXPECT_DOUBLE_EQ(0.0, quat.z);
  EXPECT_DOUBLE_EQ(1.0, quat.w);

  // w is set to cos(angle/2), but when rounding this operation is done:
  //
  //   cos(round(acos(w)*2)/2)
  //
  // Which results is a bit more floating point error than EXPECT_DOUBLE_EQ
  // can tolerate, so explicitly verify less than some small value.
  const double epsilon = 1.0e-15;
  quat.x = std::sqrt(2.0) / 2.0;
  quat.y = 0.0;
  quat.z = std::sqrt(2.0) / 2.0;
  quat.w = 0.0;

  RoundOrientationQuaternionReading(&quat);
  EXPECT_DOUBLE_EQ(std::sqrt(2.0) / 2.0, quat.x);
  EXPECT_LT(std::abs(quat.y), epsilon);
  EXPECT_DOUBLE_EQ(std::sqrt(2.0) / 2.0, quat.z);
  EXPECT_LT(std::abs(quat.w), epsilon);
}

}  // namespace device
