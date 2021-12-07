// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_util.h"

#include <limits>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using mojom::SensorType;

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

TEST(PlatformSensorUtil, IsSignificantlyDifferentAmbientLight) {
  // Test for AMBIENT_LIGHT as it has different significance threshold compared
  // to others.
  const double kTestValue = 100.0;  // Made up test value.

  // Ambient light sensor has threshold points around initial_reading.
  // IsSignificantlyDifferent() returns false when value is located between
  // upper and lower threshold point and true when it is outside of threshold
  // points. Below text shows these threshold points and different areas.
  //
  // Smaller value                   Large values
  //      [--------------X--------------]
  // (1) (2) (3)        (4)        (5) (6) (7)
  //
  // Selected test values are:
  // 1. just below lower threshold point in significantly different lower area
  // 2. lower threshold point
  // 3. just above lower threshold point in not significantly different area
  // 4. center (new reading is same as initial reading)
  // 5. just below upper threshold point in not significantly different area
  // 6. upper threshold point
  // 7. just above upper threshold point in significantly different upper area
  const struct {
    const bool expectation;
    const double new_reading;
  } kTestCases[] = {
      {true, kTestValue - kAlsSignificanceThreshold / 2 - 1},
      {true, kTestValue - kAlsSignificanceThreshold / 2},
      {false, kTestValue - kAlsSignificanceThreshold / 2 + 1},
      {false, kTestValue},
      {false, kTestValue + kAlsSignificanceThreshold / 2 - 1},
      {true, kTestValue + kAlsSignificanceThreshold / 2},
      {true, kTestValue + kAlsSignificanceThreshold / 2 + 1},
  };

  SensorReading initial_reading;
  initial_reading.als.value = kTestValue;

  for (const auto& test_case : kTestCases) {
    SensorReading new_reading;
    new_reading.als.value = test_case.new_reading;
    EXPECT_THAT(IsSignificantlyDifferent(initial_reading, new_reading,
                                         SensorType::AMBIENT_LIGHT),
                test_case.expectation);
  }
}

TEST(PlatformSensorUtil, IsSignificantlyDifferentPressure) {
  // Test for standard sensor with single value.
  const double kTestValue = 100.0;  // Made up test value.
  const double kSmallDelta = 0.0005;
  SensorReading last_reading;
  SensorReading new_reading;

  // No difference in values does not count as a significant change.
  last_reading.pressure.value = kTestValue;
  EXPECT_FALSE(IsSignificantlyDifferent(last_reading, last_reading,
                                        SensorType::PRESSURE));

  // Check that different values are reported as significantly different.
  new_reading.pressure.value = last_reading.pressure.value + kSmallDelta;
  EXPECT_TRUE(IsSignificantlyDifferent(last_reading, new_reading,
                                       SensorType::PRESSURE));

  // Check that different values are reported as significantly different.
  new_reading.pressure.value = last_reading.pressure.value - kSmallDelta;
  EXPECT_TRUE(IsSignificantlyDifferent(last_reading, new_reading,
                                       SensorType::PRESSURE));
}

TEST(PlatformSensorUtil, IsSignificantlyDifferentMagnetometer) {
  // Test for standard sensor with three values.
  const double kTestValue = 100.0;  // Made up test value.
  const double kSmallDelta = 0.0005;
  SensorReading last_reading;
  SensorReading new_reading;

  // No difference in values does not count as a significant change.
  last_reading.magn.x = kTestValue;
  last_reading.magn.y = kTestValue;
  last_reading.magn.z = kTestValue;
  EXPECT_FALSE(IsSignificantlyDifferent(last_reading, last_reading,
                                        SensorType::MAGNETOMETER));

  // Check that different values on one axis are reported as significantly
  // different.
  new_reading.magn.x = last_reading.magn.x;
  new_reading.magn.y = last_reading.magn.y + kSmallDelta;
  new_reading.magn.z = last_reading.magn.z;
  EXPECT_TRUE(IsSignificantlyDifferent(last_reading, new_reading,
                                       SensorType::MAGNETOMETER));

  // Check that different values on all axis are reported as significantly
  // different.
  new_reading.magn.x = last_reading.magn.x + kSmallDelta;
  new_reading.magn.y = last_reading.magn.y + kSmallDelta;
  new_reading.magn.z = last_reading.magn.z + kSmallDelta;
  EXPECT_TRUE(IsSignificantlyDifferent(last_reading, new_reading,
                                       SensorType::MAGNETOMETER));

  // Check that different values on all axis are reported as significantly
  // different.
  new_reading.magn.x = last_reading.magn.x - kSmallDelta;
  new_reading.magn.y = last_reading.magn.y - kSmallDelta;
  new_reading.magn.z = last_reading.magn.z - kSmallDelta;
  EXPECT_TRUE(IsSignificantlyDifferent(last_reading, new_reading,
                                       SensorType::MAGNETOMETER));
}

}  // namespace device
