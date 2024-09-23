// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/math_functions.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

enum class TestOperatorType {
  kRoundNearest,
  kRoundUp,
  kRoundDown,
  kRoundToZero,
  kMod,
  kRem,
};

TEST(MathFunctionsTest, EvaluateSteppedValueFunction_SmallB) {
  struct {
    double a;
    double b;
    double expected;
  } tests[] = {
      {42.0, std::numeric_limits<double>::min(), 42.0},
      {42.0, std::numeric_limits<double>::denorm_min(), 42.0},
      {42.0, std::numeric_limits<double>::epsilon(), 42.0},

      {std::numeric_limits<double>::min(), std::numeric_limits<double>::min(),
       std::numeric_limits<double>::min()},
      {std::numeric_limits<double>::min(),
       std::numeric_limits<double>::denorm_min(),
       std::numeric_limits<double>::min()},
      {std::numeric_limits<double>::min(),
       std::numeric_limits<double>::epsilon(), 0.0},

      {std::numeric_limits<double>::denorm_min(),
       std::numeric_limits<double>::min(), 0.0},
      {std::numeric_limits<double>::denorm_min(),
       std::numeric_limits<double>::denorm_min(),
       std::numeric_limits<double>::denorm_min()},
      {std::numeric_limits<double>::denorm_min(),
       std::numeric_limits<double>::epsilon(), 0.0},

      {-42.0, std::numeric_limits<double>::min(), -42.0},
      {-42.0, std::numeric_limits<double>::denorm_min(), -42.0},
      {-42.0, std::numeric_limits<double>::epsilon(), -42.0},

      {-std::numeric_limits<double>::min(), std::numeric_limits<double>::min(),
       -std::numeric_limits<double>::min()},
      {-std::numeric_limits<double>::min(),
       std::numeric_limits<double>::denorm_min(),
       -std::numeric_limits<double>::min()},
      {-std::numeric_limits<double>::min(),
       std::numeric_limits<double>::epsilon(), -0.0},

      {-std::numeric_limits<double>::denorm_min(),
       std::numeric_limits<double>::min(), -0.0},
      {-std::numeric_limits<double>::denorm_min(),
       std::numeric_limits<double>::denorm_min(),
       -std::numeric_limits<double>::denorm_min()},
      {-std::numeric_limits<double>::denorm_min(),
       std::numeric_limits<double>::epsilon(), -0.0},
  };
  for (const auto& test : tests) {
    EXPECT_EQ(EvaluateSteppedValueFunction(TestOperatorType::kRoundNearest,
                                           test.a, test.b),
              test.expected)
        << "a=" << test.a << " b=" << test.b;
  }
}

}  // namespace

}  // namespace blink
