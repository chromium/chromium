// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "media/base/audio_sample_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

template <typename TestConfig>
class SampleTypeTraitsTest : public testing::Test {};

TYPED_TEST_SUITE_P(SampleTypeTraitsTest);

struct UnsignedInt8ToFloat32TestConfig {
  using SourceTraits = UnsignedInt8SampleTypeTraits;
  using TargetTraits = Float32SampleTypeTraits;
  static const char* config_name() { return "UnsignedInt8ToFloat32TestConfig"; }
  static TargetTraits::ValueType PerformConversion(
      SourceTraits::ValueType source_value) {
    return SourceTraits::ToFloat(source_value);
  }
};

struct SignedInt16ToFloat32TestConfig {
  using SourceTraits = SignedInt16SampleTypeTraits;
  using TargetTraits = Float32SampleTypeTraits;
  static const char* config_name() { return "SignedInt16ToFloat32TestConfig"; }
  static TargetTraits::ValueType PerformConversion(
      SourceTraits::ValueType source_value) {
    return SourceTraits::ToFloat(source_value);
  }
};

struct SignedInt32ToFloat32TestConfig {
  using SourceTraits = SignedInt32SampleTypeTraits;
  using TargetTraits = Float32SampleTypeTraits;
  static const char* config_name() { return "SignedInt32ToFloat32TestConfig"; }
  static TargetTraits::ValueType PerformConversion(
      SourceTraits::ValueType source_value) {
    return SourceTraits::ToFloat(source_value);
  }
};

struct Float32ToUnsignedInt8TestConfig {
  using SourceTraits = Float32SampleTypeTraits;
  using TargetTraits = UnsignedInt8SampleTypeTraits;
  static const char* config_name() { return "Float32ToUnsignedInt8TestConfig"; }
  static TargetTraits::ValueType PerformConversion(
      SourceTraits::ValueType source_value) {
    return TargetTraits::FromFloat(source_value);
  }
};

struct Float32ToSignedInt16TestConfig {
  using SourceTraits = Float32SampleTypeTraits;
  using TargetTraits = SignedInt16SampleTypeTraits;
  static const char* config_name() { return "Float32ToSignedInt16TestConfig"; }
  static TargetTraits::ValueType PerformConversion(
      SourceTraits::ValueType source_value) {
    return TargetTraits::FromFloat(source_value);
  }
};

struct Float32ToSignedInt32TestConfig {
  using SourceTraits = Float32SampleTypeTraits;
  using TargetTraits = SignedInt32SampleTypeTraits;
  static const char* config_name() { return "Float32ToSignedInt32TestConfig"; }
  static TargetTraits::ValueType PerformConversion(
      SourceTraits::ValueType source_value) {
    return TargetTraits::FromFloat(source_value);
  }
};

struct UnsignedInt8ToFloat64TestConfig {
  using SourceTraits = UnsignedInt8SampleTypeTraits;
  using TargetTraits = Float64SampleTypeTraits;
  static const char* config_name() { return "UnsignedInt8ToFloat64TestConfig"; }
  static TargetTraits::ValueType PerformConversion(
      SourceTraits::ValueType source_value) {
    return SourceTraits::ToDouble(source_value);
  }
};

struct SignedInt16ToFloat64TestConfig {
  using SourceTraits = SignedInt16SampleTypeTraits;
  using TargetTraits = Float64SampleTypeTraits;
  static const char* config_name() { return "SignedInt16ToFloat64TestConfig"; }
  static TargetTraits::ValueType PerformConversion(
      SourceTraits::ValueType source_value) {
    return SourceTraits::ToDouble(source_value);
  }
};

struct SignedInt32ToFloat64TestConfig {
  using SourceTraits = SignedInt32SampleTypeTraits;
  using TargetTraits = Float64SampleTypeTraits;
  static const char* config_name() { return "SignedInt32ToFloat64TestConfig"; }
  static TargetTraits::ValueType PerformConversion(
      SourceTraits::ValueType source_value) {
    return SourceTraits::ToDouble(source_value);
  }
};

struct Float64ToUnsignedInt8TestConfig {
  using SourceTraits = Float64SampleTypeTraits;
  using TargetTraits = UnsignedInt8SampleTypeTraits;
  static const char* config_name() { return "Float64ToUnsignedInt8TestConfig"; }
  static TargetTraits::ValueType PerformConversion(
      SourceTraits::ValueType source_value) {
    return TargetTraits::FromDouble(source_value);
  }
};

struct Float64ToSignedInt16TestConfig {
  using SourceTraits = Float64SampleTypeTraits;
  using TargetTraits = SignedInt16SampleTypeTraits;
  static const char* config_name() { return "Float64ToSignedInt16TestConfig"; }
  static TargetTraits::ValueType PerformConversion(
      SourceTraits::ValueType source_value) {
    return TargetTraits::FromDouble(source_value);
  }
};

struct Float64ToSignedInt32TestConfig {
  using SourceTraits = Float64SampleTypeTraits;
  using TargetTraits = SignedInt32SampleTypeTraits;
  static const char* config_name() { return "Float64ToSignedInt32TestConfig"; }
  static TargetTraits::ValueType PerformConversion(
      SourceTraits::ValueType source_value) {
    return TargetTraits::FromDouble(source_value);
  }
};

TYPED_TEST_P(SampleTypeTraitsTest, ConvertExampleValues) {
  using Config = TypeParam;
  using SourceTraits = typename Config::SourceTraits;
  using TargetTraits = typename Config::TargetTraits;
  using SourceType = typename SourceTraits::ValueType;
  using TargetType = typename TargetTraits::ValueType;
  SourceType source_max = SourceTraits::kMaxValue;
  SourceType source_min = SourceTraits::kMinValue;
  SourceType source_zero_point = SourceTraits::kZeroPointValue;
  SourceType source_two = static_cast<SourceType>(2);
  TargetType target_max = TargetTraits::kMaxValue;
  TargetType target_min = TargetTraits::kMinValue;
  TargetType target_zero_point = TargetTraits::kZeroPointValue;
  TargetType target_two = static_cast<TargetType>(2);

  SCOPED_TRACE(Config::config_name());

  {
    SCOPED_TRACE("Convert zero-point value");
    ASSERT_EQ(target_zero_point,
              Config::PerformConversion(SourceTraits::kZeroPointValue));
  }
  {
    SCOPED_TRACE("Convert max value");
    ASSERT_EQ(target_max, Config::PerformConversion(source_max));
  }
  {
    SCOPED_TRACE("Convert min value");
    ASSERT_EQ(target_min, Config::PerformConversion(source_min));
  }

  {
    SCOPED_TRACE("Convert value half way between min and zero point");
    // Note: This somewhat unconventional way of calculating the source and
    // target value is necessary to avoid any intermediate result falling
    // outside the corresponding numeric range.
    auto source_value = source_min + ((source_zero_point / source_two) -
                                      (source_min / source_two));
    auto expected_target_value =
        target_min +
        ((target_zero_point / target_two) - (target_min / target_two));
    ASSERT_EQ(expected_target_value, Config::PerformConversion(source_value));
  }

  {
    SCOPED_TRACE("Convert value half way between zero point and max");

    auto source_value =
        source_zero_point + ((source_max - source_zero_point) / source_two);
    auto expected_target_value =
        target_zero_point + ((target_max - target_zero_point) / target_two);

    if (std::numeric_limits<SourceType>::is_integer &&
        std::is_floating_point<TargetType>::value) {
      // The source value half-way between zero point and max falls in the
      // middle between two integers, so we expect it to be off by 0.5.
      TargetType kTolerance =
          static_cast<TargetType>(0.5) * (source_max - source_zero_point);
      ASSERT_NEAR(expected_target_value,
                  Config::PerformConversion(source_value), kTolerance);
    } else if (std::is_floating_point<SourceType>::value &&
               std::numeric_limits<TargetType>::is_integer) {
      // The quantization error of the scaling factor due to the limited
      // precision of the floating point type can cause the result to be off
      // by 1.
      auto kTolerance = static_cast<TargetType>(1);
      ASSERT_NEAR(expected_target_value,
                  Config::PerformConversion(source_value), kTolerance);
    } else {
      ASSERT_EQ(expected_target_value, Config::PerformConversion(source_value));
    }
  }
}

REGISTER_TYPED_TEST_SUITE_P(SampleTypeTraitsTest, ConvertExampleValues);

typedef ::testing::Types<UnsignedInt8ToFloat32TestConfig,
                         SignedInt16ToFloat32TestConfig,
                         SignedInt32ToFloat32TestConfig,
                         Float32ToUnsignedInt8TestConfig,
                         Float32ToSignedInt16TestConfig,
                         Float32ToSignedInt32TestConfig,
                         UnsignedInt8ToFloat64TestConfig,
                         SignedInt16ToFloat64TestConfig,
                         SignedInt32ToFloat64TestConfig,
                         Float64ToUnsignedInt8TestConfig,
                         Float64ToSignedInt16TestConfig,
                         Float64ToSignedInt32TestConfig>
    TestConfigs;
INSTANTIATE_TYPED_TEST_SUITE_P(CommonTypes, SampleTypeTraitsTest, TestConfigs);

}  // namespace media
