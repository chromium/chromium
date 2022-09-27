// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/events/pointer_event_util.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

class AzimuthInValidRangeWithParameterTests
    : public testing::Test,
      public testing::WithParamInterface<std::tuple<double, double>> {
 public:
  void SetUp() override {
    azimuth_angle_ = std::get<0>(GetParam());
    expected_azimuth_angle_ = std::get<1>(GetParam());
  }

 protected:
  double expected_azimuth_angle_;
  double azimuth_angle_;
};

class AltitudeInValidRangeWithParameterTests
    : public testing::Test,
      public testing::WithParamInterface<std::tuple<double, double>> {
 public:
  void SetUp() override {
    altitude_angle_ = std::get<0>(GetParam());
    expected_altitude_angle_ = std::get<1>(GetParam());
  }

 protected:
  double expected_altitude_angle_;
  double altitude_angle_;
};

class TiltInValidRangeWithParameterTests
    : public testing::Test,
      public testing::WithParamInterface<std::tuple<double, double>> {
 public:
  void SetUp() override {
    tilt_angle_ = std::get<0>(GetParam());
    expected_tilt_angle_ = std::get<1>(GetParam());
  }

 protected:
  double expected_tilt_angle_;
  double tilt_angle_;
};

TEST_P(AzimuthInValidRangeWithParameterTests,
       CheckAzimuthTransformedCorrectly) {
  ASSERT_DOUBLE_EQ(
      expected_azimuth_angle_,
      PointerEventUtil::TransformToAzimuthInValidRange(azimuth_angle_));
}

INSTANTIATE_TEST_SUITE_P(
    AzimuthInValidRangeTests,
    AzimuthInValidRangeWithParameterTests,
    ::testing::Values(
        std::make_tuple(0, 0),
        std::make_tuple(kPiOverTwoDouble, kPiOverTwoDouble),
        std::make_tuple(kPiDouble, kPiDouble),
        std::make_tuple(3 * kPiOverTwoDouble, 3 * kPiOverTwoDouble),
        std::make_tuple(kTwoPiDouble, kTwoPiDouble),
        std::make_tuple(3 * kPiDouble, kPiDouble),
        std::make_tuple(5.0 * kPiOverTwoDouble, kPiOverTwoDouble)));

TEST_P(AltitudeInValidRangeWithParameterTests,
       CheckAltitudeTransformedCorrectly) {
  ASSERT_DOUBLE_EQ(
      expected_altitude_angle_,
      PointerEventUtil::TransformToAltitudeInValidRange(altitude_angle_));
}

INSTANTIATE_TEST_SUITE_P(
    AltitudeInValidRangeTests,
    AltitudeInValidRangeWithParameterTests,
    ::testing::Values(std::make_tuple(0, 0),
                      std::make_tuple(kPiOverTwoDouble, kPiOverTwoDouble),
                      std::make_tuple(kPiDouble, kPiOverTwoDouble),
                      std::make_tuple(3 * kPiOverTwoDouble, kPiOverTwoDouble),
                      std::make_tuple(kTwoPiDouble, kPiOverTwoDouble)));

TEST_P(TiltInValidRangeWithParameterTests, CheckTiltTransformedCorrectly) {
  ASSERT_EQ(expected_tilt_angle_,
            PointerEventUtil::TransformToTiltInValidRange(tilt_angle_));
}

INSTANTIATE_TEST_SUITE_P(TiltInValidRangeTests,
                         TiltInValidRangeWithParameterTests,
                         ::testing::Values(std::make_tuple(0, 0),
                                           std::make_tuple(45, 45),
                                           std::make_tuple(90, 90),
                                           std::make_tuple(135, -45),
                                           std::make_tuple(180, 0),
                                           std::make_tuple(225, 45),
                                           std::make_tuple(270, 90),
                                           std::make_tuple(360, 0)));
}  // namespace blink
