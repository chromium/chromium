// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/sensor_reading_remapper.h"

#include "services/device/public/mojom/sensor.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(SensorReadingRemapperTest, GravityFollowsScreenAngle) {
  struct {
    uint16_t orientation_angle;
    double expected_x;
    double expected_y;
  } kTestCases[] = {
      {0, -9.8, 0.0},
      {90, 0.0, -9.8},
      {180, 9.8, 0.0},
      {270, 0.0, 9.8},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.orientation_angle);
    device::SensorReading reading;
    reading.accel.x = -9.8;
    reading.accel.y = 0.0;
    reading.accel.z = 0.0;

    SensorReadingRemapper::RemapToScreenCoords(
        device::mojom::blink::SensorType::GRAVITY, test_case.orientation_angle,
        &reading);

    EXPECT_DOUBLE_EQ(reading.accel.x, test_case.expected_x);
    EXPECT_DOUBLE_EQ(reading.accel.y, test_case.expected_y);
    EXPECT_DOUBLE_EQ(reading.accel.z, 0.0);
  }
}

}  // namespace blink
