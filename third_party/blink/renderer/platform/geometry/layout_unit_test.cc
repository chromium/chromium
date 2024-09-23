/*
 * Copyright (c) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

#include <limits.h>
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

template <class T>
class LayoutUnitTypedTest : public testing::Test {};
using LayoutUnitTypes =
    ::testing::Types<LayoutUnit, TextRunLayoutUnit, InlineLayoutUnit>;
TYPED_TEST_SUITE(LayoutUnitTypedTest, LayoutUnitTypes);

TEST(LayoutUnitTest, LayoutUnitInt) {
  EXPECT_EQ(LayoutUnit::kIntMin, LayoutUnit(INT_MIN).ToInt());
  EXPECT_EQ(LayoutUnit::kIntMin, LayoutUnit(INT_MIN / 2).ToInt());
  EXPECT_EQ(LayoutUnit::kIntMin, LayoutUnit(LayoutUnit::kIntMin - 1).ToInt());
  EXPECT_EQ(LayoutUnit::kIntMin, LayoutUnit(LayoutUnit::kIntMin).ToInt());
  EXPECT_EQ(LayoutUnit::kIntMin + 1,
            LayoutUnit(LayoutUnit::kIntMin + 1).ToInt());
  EXPECT_EQ(LayoutUnit::kIntMin / 2,
            LayoutUnit(LayoutUnit::kIntMin / 2).ToInt());
  EXPECT_EQ(-10000, LayoutUnit(-10000).ToInt());
  EXPECT_EQ(-1000, LayoutUnit(-1000).ToInt());
  EXPECT_EQ(-100, LayoutUnit(-100).ToInt());
  EXPECT_EQ(-10, LayoutUnit(-10).ToInt());
  EXPECT_EQ(-1, LayoutUnit(-1).ToInt());
  EXPECT_EQ(0, LayoutUnit(0).ToInt());
  EXPECT_EQ(1, LayoutUnit(1).ToInt());
  EXPECT_EQ(100, LayoutUnit(100).ToInt());
  EXPECT_EQ(1000, LayoutUnit(1000).ToInt());
  EXPECT_EQ(10000, LayoutUnit(10000).ToInt());
  EXPECT_EQ(LayoutUnit::kIntMax / 2,
            LayoutUnit(LayoutUnit::kIntMax / 2).ToInt());
  EXPECT_EQ(LayoutUnit::kIntMax - 1,
            LayoutUnit(LayoutUnit::kIntMax - 1).ToInt());
  EXPECT_EQ(LayoutUnit::kIntMax, LayoutUnit(LayoutUnit::kIntMax).ToInt());
  EXPECT_EQ(LayoutUnit::kIntMax, LayoutUnit(LayoutUnit::kIntMax + 1).ToInt());
  EXPECT_EQ(LayoutUnit::kIntMax, LayoutUnit(INT_MAX / 2).ToInt());
  EXPECT_EQ(LayoutUnit::kIntMax, LayoutUnit(INT_MAX).ToInt());

  // Test the raw unsaturated value
  EXPECT_EQ(0, LayoutUnit(0).RawValue());
  // Internally the max number we can represent (without saturating)
  // is all the (non-sign) bits set except for the bottom n fraction bits
  const int max_internal_representation =
      std::numeric_limits<int>::max() ^
      ((1 << LayoutUnit::kFractionalBits) - 1);
  EXPECT_EQ(max_internal_representation,
            LayoutUnit(LayoutUnit::kIntMax).RawValue());
  EXPECT_EQ(GetMaxSaturatedSetResultForTesting(),
            LayoutUnit(LayoutUnit::kIntMax + 100).RawValue());
  EXPECT_EQ((LayoutUnit::kIntMax - 100) << LayoutUnit::kFractionalBits,
            LayoutUnit(LayoutUnit::kIntMax - 100).RawValue());
  EXPECT_EQ(GetMinSaturatedSetResultForTesting(),
            LayoutUnit(LayoutUnit::kIntMin).RawValue());
  EXPECT_EQ(GetMinSaturatedSetResultForTesting(),
            LayoutUnit(LayoutUnit::kIntMin - 100).RawValue());
  // Shifting negative numbers left has undefined behavior, so use
  // multiplication instead of direct shifting here.
  EXPECT_EQ((LayoutUnit::kIntMin + 100) * (1 << LayoutUnit::kFractionalBits),
            LayoutUnit(LayoutUnit::kIntMin + 100).RawValue());
}

TEST(LayoutUnitTest, LayoutUnitUnsigned) {
  // Test the raw unsaturated value
  EXPECT_EQ(0, LayoutUnit((unsigned)0).RawValue());
  EXPECT_EQ(GetMaxSaturatedSetResultForTesting(),
            LayoutUnit((unsigned)LayoutUnit::kIntMax).RawValue());
  const unsigned kOverflowed = LayoutUnit::kIntMax + 100;
  EXPECT_EQ(GetMaxSaturatedSetResultForTesting(),
            LayoutUnit(kOverflowed).RawValue());
  const unsigned kNotOverflowed = LayoutUnit::kIntMax - 100;
  EXPECT_EQ((LayoutUnit::kIntMax - 100) << LayoutUnit::kFractionalBits,
            LayoutUnit(kNotOverflowed).RawValue());
}

TEST(LayoutUnitTest, Int64) {
  constexpr int raw_min = std::numeric_limits<int>::min();
  EXPECT_EQ(LayoutUnit(static_cast<int64_t>(raw_min) - 100), LayoutUnit::Min());

  constexpr int raw_max = std::numeric_limits<int>::max();
  EXPECT_EQ(LayoutUnit(static_cast<int64_t>(raw_max) + 100), LayoutUnit::Max());
  EXPECT_EQ(LayoutUnit(static_cast<uint64_t>(raw_max) + 100),
            LayoutUnit::Max());
}

TEST(LayoutUnitTest, LayoutUnitFloat) {
  const float kTolerance = 1.0f / LayoutUnit::kFixedPointDenominator;
  EXPECT_FLOAT_EQ(1.0f, LayoutUnit(1.0f).ToFloat());
  EXPECT_FLOAT_EQ(1.25f, LayoutUnit(1.25f).ToFloat());
  EXPECT_EQ(LayoutUnit(1.25f), LayoutUnit(1.25f + kTolerance / 2));
  EXPECT_EQ(LayoutUnit(-2.0f), LayoutUnit(-2.0f - kTolerance / 2));
  EXPECT_NEAR(LayoutUnit(1.1f).ToFloat(), 1.1f, kTolerance);
  EXPECT_NEAR(LayoutUnit(1.33f).ToFloat(), 1.33f, kTolerance);
  EXPECT_NEAR(LayoutUnit(1.3333f).ToFloat(), 1.3333f, kTolerance);
  EXPECT_NEAR(LayoutUnit(1.53434f).ToFloat(), 1.53434f, kTolerance);
  EXPECT_NEAR(LayoutUnit(345634).ToFloat(), 345634.0f, kTolerance);
  EXPECT_NEAR(LayoutUnit(345634.12335f).ToFloat(), 345634.12335f, kTolerance);
  EXPECT_NEAR(LayoutUnit(-345634.12335f).ToFloat(), -345634.12335f, kTolerance);
  EXPECT_NEAR(LayoutUnit(-345634).ToFloat(), -345634.0f, kTolerance);

  using Limits = std::numeric_limits<float>;
  // Larger than Max()
  EXPECT_EQ(LayoutUnit::Max(), LayoutUnit(Limits::max()));
  EXPECT_EQ(LayoutUnit::Max(), LayoutUnit(Limits::infinity()));
  // Smaller than Min()
  EXPECT_EQ(LayoutUnit::Min(), LayoutUnit(Limits::lowest()));
  EXPECT_EQ(LayoutUnit::Min(), LayoutUnit(-Limits::infinity()));

  EXPECT_EQ(LayoutUnit(), LayoutUnit::Clamp(Limits::quiet_NaN()));
}

// Test that `constexpr` constructors are constant expressions.
TEST(LayoutUnitTest, ConstExprCtor) {
  [[maybe_unused]] constexpr LayoutUnit from_int(1);
  [[maybe_unused]] constexpr LayoutUnit from_float(1.0f);
  [[maybe_unused]] constexpr LayoutUnit from_raw = LayoutUnit::FromRawValue(1);
  [[maybe_unused]] constexpr LayoutUnit from_raw_with_clamp =
      LayoutUnit::FromRawValueWithClamp(1);
}

TEST(LayoutUnitTest, FromFloatCeil) {
  const float kTolerance = 1.0f / LayoutUnit::kFixedPointDenominator;
  EXPECT_EQ(LayoutUnit(1.25f), LayoutUnit::FromFloatCeil(1.25f));
  EXPECT_EQ(LayoutUnit(1.25f + kTolerance),
            LayoutUnit::FromFloatCeil(1.25f + kTolerance / 2));
  EXPECT_EQ(LayoutUnit(), LayoutUnit::FromFloatCeil(-kTolerance / 2));

  using Limits = std::numeric_limits<float>;
  // Larger than Max()
  EXPECT_EQ(LayoutUnit::Max(), LayoutUnit::FromFloatCeil(Limits::max()));
  EXPECT_EQ(LayoutUnit::Max(), LayoutUnit::FromFloatCeil(Limits::infinity()));
  // Smaller than Min()
  EXPECT_EQ(LayoutUnit::Min(), LayoutUnit::FromFloatCeil(Limits::lowest()));
  EXPECT_EQ(LayoutUnit::Min(), LayoutUnit::FromFloatCeil(-Limits::infinity()));

  EXPECT_EQ(LayoutUnit(), LayoutUnit::FromFloatCeil(Limits::quiet_NaN()));
}

TEST(LayoutUnitTest, FromFloatFloor) {
  const float kTolerance = 1.0f / LayoutUnit::kFixedPointDenominator;
  EXPECT_EQ(LayoutUnit(1.25f), LayoutUnit::FromFloatFloor(1.25f));
  EXPECT_EQ(LayoutUnit(1.25f),
            LayoutUnit::FromFloatFloor(1.25f + kTolerance / 2));
  EXPECT_EQ(LayoutUnit(-kTolerance),
            LayoutUnit::FromFloatFloor(-kTolerance / 2));

  using Limits = std::numeric_limits<float>;
  // Larger than Max()
  EXPECT_EQ(LayoutUnit::Max(), LayoutUnit::FromFloatFloor(Limits::max()));
  EXPECT_EQ(LayoutUnit::Max(), LayoutUnit::FromFloatFloor(Limits::infinity()));
  // Smaller than Min()
  EXPECT_EQ(LayoutUnit::Min(), LayoutUnit::FromFloatFloor(Limits::lowest()));
  EXPECT_EQ(LayoutUnit::Min(), LayoutUnit::FromFloatFloor(-Limits::infinity()));

  EXPECT_EQ(LayoutUnit(), LayoutUnit::FromFloatFloor(Limits::quiet_NaN()));
}

TEST(LayoutUnitTest, FromFloatRound) {
  const float kTolerance = 1.0f / LayoutUnit::kFixedPointDenominator;
  EXPECT_EQ(LayoutUnit(1.25f), LayoutUnit::FromFloatRound(1.25f));
  EXPECT_EQ(LayoutUnit(1.25f),
            LayoutUnit::FromFloatRound(1.25f + kTolerance / 4));
  EXPECT_EQ(LayoutUnit(1.25f + kTolerance),
            LayoutUnit::FromFloatRound(1.25f + kTolerance * 3 / 4));
  EXPECT_EQ(LayoutUnit(-kTolerance),
            LayoutUnit::FromFloatRound(-kTolerance * 3 / 4));

  using Limits = std::numeric_limits<float>;
  // Larger than Max()
  EXPECT_EQ(LayoutUnit::Max(), LayoutUnit::FromFloatRound(Limits::max()));
  EXPECT_EQ(LayoutUnit::Max(), LayoutUnit::FromFloatRound(Limits::infinity()));
  // Smaller than Min()
  EXPECT_EQ(LayoutUnit::Min(), LayoutUnit::FromFloatRound(Limits::lowest()));
  EXPECT_EQ(LayoutUnit::Min(), LayoutUnit::FromFloatRound(-Limits::infinity()));

  EXPECT_EQ(LayoutUnit(), LayoutUnit::FromFloatRound(Limits::quiet_NaN()));
}

TEST(LayoutUnitTest, LayoutUnitRounding) {
  EXPECT_EQ(-2, LayoutUnit(-1.9f).Round());
  EXPECT_EQ(-2, LayoutUnit(-1.6f).Round());
  EXPECT_EQ(-2, LayoutUnit::FromFloatRound(-1.51f).Round());
  EXPECT_EQ(-1, LayoutUnit::FromFloatRound(-1.5f).Round());
  EXPECT_EQ(-1, LayoutUnit::FromFloatRound(-1.49f).Round());
  EXPECT_EQ(-1, LayoutUnit(-1.0f).Round());
  EXPECT_EQ(-1, LayoutUnit::FromFloatRound(-0.99f).Round());
  EXPECT_EQ(-1, LayoutUnit::FromFloatRound(-0.51f).Round());
  EXPECT_EQ(0, LayoutUnit::FromFloatRound(-0.50f).Round());
  EXPECT_EQ(0, LayoutUnit::FromFloatRound(-0.49f).Round());
  EXPECT_EQ(0, LayoutUnit(-0.1f).Round());
  EXPECT_EQ(0, LayoutUnit(0.0f).Round());
  EXPECT_EQ(0, LayoutUnit(0.1f).Round());
  EXPECT_EQ(0, LayoutUnit::FromFloatRound(0.49f).Round());
  EXPECT_EQ(1, LayoutUnit::FromFloatRound(0.50f).Round());
  EXPECT_EQ(1, LayoutUnit::FromFloatRound(0.51f).Round());
  EXPECT_EQ(1, LayoutUnit(0.99f).Round());
  EXPECT_EQ(1, LayoutUnit(1.0f).Round());
  EXPECT_EQ(1, LayoutUnit::FromFloatRound(1.49f).Round());
  EXPECT_EQ(2, LayoutUnit::FromFloatRound(1.5f).Round());
  EXPECT_EQ(2, LayoutUnit::FromFloatRound(1.51f).Round());
  // The fractional part of LayoutUnit::Max() is 0x3f, so it should round up.
  EXPECT_EQ(
      ((std::numeric_limits<int>::max() / LayoutUnit::kFixedPointDenominator) +
       1),
      LayoutUnit::Max().Round());
  // The fractional part of LayoutUnit::Min() is 0, so the next bigger possible
  // value should round down.
  LayoutUnit epsilon;
  epsilon.SetRawValue(1);
  EXPECT_EQ(
      ((std::numeric_limits<int>::min() / LayoutUnit::kFixedPointDenominator)),
      (LayoutUnit::Min() + epsilon).Round());
}

TEST(LayoutUnitTest, LayoutUnitSnapSizeToPixel) {
  EXPECT_EQ(1, SnapSizeToPixel(LayoutUnit(1), LayoutUnit(0)));
  EXPECT_EQ(1, SnapSizeToPixel(LayoutUnit(1), LayoutUnit(0.5)));
  EXPECT_EQ(2, SnapSizeToPixel(LayoutUnit(1.5), LayoutUnit(0)));
  EXPECT_EQ(2, SnapSizeToPixel(LayoutUnit(1.5), LayoutUnit(0.49)));
  EXPECT_EQ(1, SnapSizeToPixel(LayoutUnit(1.5), LayoutUnit(0.5)));
  EXPECT_EQ(1, SnapSizeToPixel(LayoutUnit(1.5), LayoutUnit(0.75)));
  EXPECT_EQ(1, SnapSizeToPixel(LayoutUnit(1.5), LayoutUnit(0.99)));
  EXPECT_EQ(2, SnapSizeToPixel(LayoutUnit(1.5), LayoutUnit(1)));

  // 0.046875 is 3/64, lower than 4 * LayoutUnit::Epsilon()
  EXPECT_EQ(0, SnapSizeToPixel(LayoutUnit(0.046875), LayoutUnit(0)));
  // 0.078125 is 5/64, higher than 4 * LayoutUnit::Epsilon()
  EXPECT_EQ(1, SnapSizeToPixel(LayoutUnit(0.078125), LayoutUnit(0)));

  // Negative versions
  EXPECT_EQ(0, SnapSizeToPixel(LayoutUnit(-0.046875), LayoutUnit(0)));
  EXPECT_EQ(-1, SnapSizeToPixel(LayoutUnit(-0.078125), LayoutUnit(0)));

  // The next 2 would snap to zero but for the requirement that we not snap
  // sizes greater than 4 * LayoutUnit::Epsilon() to 0.
  EXPECT_EQ(1, SnapSizeToPixel(LayoutUnit(0.5), LayoutUnit(1.5)));
  EXPECT_EQ(1, SnapSizeToPixel(LayoutUnit(0.99), LayoutUnit(1.5)));

  EXPECT_EQ(1, SnapSizeToPixel(LayoutUnit(1.0), LayoutUnit(1.5)));
  EXPECT_EQ(1, SnapSizeToPixel(LayoutUnit(1.49), LayoutUnit(1.5)));
  EXPECT_EQ(1, SnapSizeToPixel(LayoutUnit(1.5), LayoutUnit(1.5)));

  EXPECT_EQ(101, SnapSizeToPixel(LayoutUnit(100.5), LayoutUnit(100)));
  EXPECT_EQ(LayoutUnit::kIntMax,
            SnapSizeToPixel(LayoutUnit(LayoutUnit::kIntMax), LayoutUnit(0.3)));
  EXPECT_EQ(LayoutUnit::kIntMin,
            SnapSizeToPixel(LayoutUnit(LayoutUnit::kIntMin), LayoutUnit(-0.3)));
}

TEST(LayoutUnitTest, LayoutUnitMultiplication) {
  EXPECT_EQ(1, (LayoutUnit(1) * LayoutUnit(1)).ToInt());
  EXPECT_EQ(2, (LayoutUnit(1) * LayoutUnit(2)).ToInt());
  EXPECT_EQ(2, (LayoutUnit(2) * LayoutUnit(1)).ToInt());
  EXPECT_EQ(1, (LayoutUnit(2) * LayoutUnit(0.5)).ToInt());
  EXPECT_EQ(1, (LayoutUnit(0.5) * LayoutUnit(2)).ToInt());
  EXPECT_EQ(100, (LayoutUnit(100) * LayoutUnit(1)).ToInt());

  EXPECT_EQ(-1, (LayoutUnit(-1) * LayoutUnit(1)).ToInt());
  EXPECT_EQ(-2, (LayoutUnit(-1) * LayoutUnit(2)).ToInt());
  EXPECT_EQ(-2, (LayoutUnit(-2) * LayoutUnit(1)).ToInt());
  EXPECT_EQ(-1, (LayoutUnit(-2) * LayoutUnit(0.5)).ToInt());
  EXPECT_EQ(-1, (LayoutUnit(-0.5) * LayoutUnit(2)).ToInt());
  EXPECT_EQ(-100, (LayoutUnit(-100) * LayoutUnit(1)).ToInt());

  EXPECT_EQ(1, (LayoutUnit(-1) * LayoutUnit(-1)).ToInt());
  EXPECT_EQ(2, (LayoutUnit(-1) * LayoutUnit(-2)).ToInt());
  EXPECT_EQ(2, (LayoutUnit(-2) * LayoutUnit(-1)).ToInt());
  EXPECT_EQ(1, (LayoutUnit(-2) * LayoutUnit(-0.5)).ToInt());
  EXPECT_EQ(1, (LayoutUnit(-0.5) * LayoutUnit(-2)).ToInt());
  EXPECT_EQ(100, (LayoutUnit(-100) * LayoutUnit(-1)).ToInt());

  EXPECT_EQ(333, (LayoutUnit(100) * LayoutUnit(3.33)).Round());
  EXPECT_EQ(-333, (LayoutUnit(-100) * LayoutUnit(3.33)).Round());
  EXPECT_EQ(333, (LayoutUnit(-100) * LayoutUnit(-3.33)).Round());

  size_t a_hundred_size_t = 100;
  EXPECT_EQ(100, (LayoutUnit(a_hundred_size_t) * LayoutUnit(1)).ToInt());
  EXPECT_EQ(400, (a_hundred_size_t * LayoutUnit(4)).ToInt());
  EXPECT_EQ(400, (LayoutUnit(4) * a_hundred_size_t).ToInt());

  int quarter_max = LayoutUnit::kIntMax / 4;
  EXPECT_EQ(quarter_max * 2, (LayoutUnit(quarter_max) * LayoutUnit(2)).ToInt());
  EXPECT_EQ(quarter_max * 3, (LayoutUnit(quarter_max) * LayoutUnit(3)).ToInt());
  EXPECT_EQ(quarter_max * 4, (LayoutUnit(quarter_max) * LayoutUnit(4)).ToInt());
  EXPECT_EQ(LayoutUnit::kIntMax,
            (LayoutUnit(quarter_max) * LayoutUnit(5)).ToInt());

  size_t overflow_int_size_t = LayoutUnit::kIntMax * 4;
  EXPECT_EQ(LayoutUnit::kIntMax,
            (LayoutUnit(overflow_int_size_t) * LayoutUnit(2)).ToInt());
  EXPECT_EQ(LayoutUnit::kIntMax, (overflow_int_size_t * LayoutUnit(4)).ToInt());
  EXPECT_EQ(LayoutUnit::kIntMax, (LayoutUnit(4) * overflow_int_size_t).ToInt());

  {
    // Multiple by float 1.0 can produce a different value.
    LayoutUnit source = LayoutUnit::FromRawValue(2147483009);
    EXPECT_NE(source, LayoutUnit(source * 1.0f));
    LayoutUnit updated = source;
    updated *= 1.0f;
    EXPECT_NE(source, updated);
  }
}

TYPED_TEST(LayoutUnitTypedTest, MultiplicationByInt) {
  const auto quarter_max = TypeParam::kIntMax / 4;
  EXPECT_EQ(TypeParam(quarter_max * 2), TypeParam(quarter_max) * 2);
  EXPECT_EQ(TypeParam(quarter_max * 3), TypeParam(quarter_max) * 3);
  EXPECT_EQ(TypeParam(quarter_max * 4), TypeParam(quarter_max) * 4);
  EXPECT_EQ(TypeParam::Max(), TypeParam(quarter_max) * 5);
}

TEST(LayoutUnitTest, LayoutUnitDivision) {
  EXPECT_EQ(1, (LayoutUnit(1) / LayoutUnit(1)).ToInt());
  EXPECT_EQ(0, (LayoutUnit(1) / LayoutUnit(2)).ToInt());
  EXPECT_EQ(2, (LayoutUnit(2) / LayoutUnit(1)).ToInt());
  EXPECT_EQ(4, (LayoutUnit(2) / LayoutUnit(0.5)).ToInt());
  EXPECT_EQ(0, (LayoutUnit(0.5) / LayoutUnit(2)).ToInt());
  EXPECT_EQ(10, (LayoutUnit(100) / LayoutUnit(10)).ToInt());
  EXPECT_FLOAT_EQ(0.5f, (LayoutUnit(1) / LayoutUnit(2)).ToFloat());
  EXPECT_FLOAT_EQ(0.25f, (LayoutUnit(0.5) / LayoutUnit(2)).ToFloat());

  EXPECT_EQ(-1, (LayoutUnit(-1) / LayoutUnit(1)).ToInt());
  EXPECT_EQ(0, (LayoutUnit(-1) / LayoutUnit(2)).ToInt());
  EXPECT_EQ(-2, (LayoutUnit(-2) / LayoutUnit(1)).ToInt());
  EXPECT_EQ(-4, (LayoutUnit(-2) / LayoutUnit(0.5)).ToInt());
  EXPECT_EQ(0, (LayoutUnit(-0.5) / LayoutUnit(2)).ToInt());
  EXPECT_EQ(-10, (LayoutUnit(-100) / LayoutUnit(10)).ToInt());
  EXPECT_FLOAT_EQ(-0.5f, (LayoutUnit(-1) / LayoutUnit(2)).ToFloat());
  EXPECT_FLOAT_EQ(-0.25f, (LayoutUnit(-0.5) / LayoutUnit(2)).ToFloat());

  EXPECT_EQ(1, (LayoutUnit(-1) / LayoutUnit(-1)).ToInt());
  EXPECT_EQ(0, (LayoutUnit(-1) / LayoutUnit(-2)).ToInt());
  EXPECT_EQ(2, (LayoutUnit(-2) / LayoutUnit(-1)).ToInt());
  EXPECT_EQ(4, (LayoutUnit(-2) / LayoutUnit(-0.5)).ToInt());
  EXPECT_EQ(0, (LayoutUnit(-0.5) / LayoutUnit(-2)).ToInt());
  EXPECT_EQ(10, (LayoutUnit(-100) / LayoutUnit(-10)).ToInt());
  EXPECT_FLOAT_EQ(0.5f, (LayoutUnit(-1) / LayoutUnit(-2)).ToFloat());
  EXPECT_FLOAT_EQ(0.25f, (LayoutUnit(-0.5) / LayoutUnit(-2)).ToFloat());

  size_t a_hundred_size_t = 100;
  EXPECT_EQ(50, (LayoutUnit(a_hundred_size_t) / LayoutUnit(2)).ToInt());
  EXPECT_EQ(25, (a_hundred_size_t / LayoutUnit(4)).ToInt());
  EXPECT_EQ(4, (LayoutUnit(400) / a_hundred_size_t).ToInt());

  EXPECT_EQ(LayoutUnit::kIntMax / 2,
            (LayoutUnit(LayoutUnit::kIntMax) / LayoutUnit(2)).ToInt());
  EXPECT_EQ(LayoutUnit::kIntMax,
            (LayoutUnit(LayoutUnit::kIntMax) / LayoutUnit(0.5)).ToInt());
}

TEST(LayoutUnitTest, LayoutUnitDivisionByInt) {
  EXPECT_EQ(LayoutUnit(1), LayoutUnit(1) / 1);
  EXPECT_EQ(LayoutUnit(0.5), LayoutUnit(1) / 2);
  EXPECT_EQ(LayoutUnit(-0.5), LayoutUnit(1) / -2);
  EXPECT_EQ(LayoutUnit(-0.5), LayoutUnit(-1) / 2);
  EXPECT_EQ(LayoutUnit(0.5), LayoutUnit(-1) / -2);

  EXPECT_DOUBLE_EQ(LayoutUnit::kIntMax / 2.0,
                   (LayoutUnit(LayoutUnit::kIntMax) / 2).ToDouble());
  EXPECT_DOUBLE_EQ(
      InlineLayoutUnit::kIntMax / 2.0,
      (InlineLayoutUnit(InlineLayoutUnit::kIntMax) / 2).ToDouble());
}

TEST(LayoutUnitTest, LayoutUnitMulDiv) {
  const LayoutUnit kMaxValue = LayoutUnit::Max();
  const LayoutUnit kMinValue = LayoutUnit::Min();
  const LayoutUnit kEpsilon = LayoutUnit().AddEpsilon();
  EXPECT_EQ(kMaxValue, kMaxValue.MulDiv(kMaxValue, kMaxValue));
  EXPECT_EQ(kMinValue, kMinValue.MulDiv(kMinValue, kMinValue));
  EXPECT_EQ(kMinValue, kMaxValue.MulDiv(kMinValue, kMaxValue));
  EXPECT_EQ(kMaxValue, kMinValue.MulDiv(kMinValue, kMaxValue));
  EXPECT_EQ(kMinValue + kEpsilon * 2, kMaxValue.MulDiv(kMaxValue, kMinValue));

  EXPECT_EQ(kMaxValue, kMaxValue.MulDiv(LayoutUnit(2), kEpsilon));
  EXPECT_EQ(kMinValue, kMinValue.MulDiv(LayoutUnit(2), kEpsilon));

  const LayoutUnit kLargerInt(16384);
  const LayoutUnit kLargerInt2(32768);
  EXPECT_EQ(LayoutUnit(8192), kLargerInt.MulDiv(kLargerInt, kLargerInt2));
}

TEST(LayoutUnitTest, LayoutUnitCeil) {
  EXPECT_EQ(0, LayoutUnit(0).Ceil());
  EXPECT_EQ(1, LayoutUnit(0.1).Ceil());
  EXPECT_EQ(1, LayoutUnit(0.5).Ceil());
  EXPECT_EQ(1, LayoutUnit(0.9).Ceil());
  EXPECT_EQ(1, LayoutUnit(1.0).Ceil());
  EXPECT_EQ(2, LayoutUnit(1.1).Ceil());

  EXPECT_EQ(0, LayoutUnit(-0.1).Ceil());
  EXPECT_EQ(0, LayoutUnit(-0.5).Ceil());
  EXPECT_EQ(0, LayoutUnit(-0.9).Ceil());
  EXPECT_EQ(-1, LayoutUnit(-1.0).Ceil());

  EXPECT_EQ(LayoutUnit::kIntMax, LayoutUnit(LayoutUnit::kIntMax).Ceil());
  EXPECT_EQ(LayoutUnit::kIntMax,
            (LayoutUnit(LayoutUnit::kIntMax) - LayoutUnit(0.5)).Ceil());
  EXPECT_EQ(LayoutUnit::kIntMax - 1,
            (LayoutUnit(LayoutUnit::kIntMax) - LayoutUnit(1)).Ceil());

  EXPECT_EQ(LayoutUnit::kIntMin, LayoutUnit(LayoutUnit::kIntMin).Ceil());
}

TEST(LayoutUnitTest, LayoutUnitFloor) {
  EXPECT_EQ(0, LayoutUnit(0).Floor());
  EXPECT_EQ(0, LayoutUnit(0.1).Floor());
  EXPECT_EQ(0, LayoutUnit(0.5).Floor());
  EXPECT_EQ(0, LayoutUnit(0.9).Floor());
  EXPECT_EQ(1, LayoutUnit(1.0).Floor());
  EXPECT_EQ(1, LayoutUnit(1.1).Floor());

  EXPECT_EQ(-1, LayoutUnit(-0.1).Floor());
  EXPECT_EQ(-1, LayoutUnit(-0.5).Floor());
  EXPECT_EQ(-1, LayoutUnit(-0.9).Floor());
  EXPECT_EQ(-1, LayoutUnit(-1.0).Floor());

  EXPECT_EQ(LayoutUnit::kIntMax, LayoutUnit(LayoutUnit::kIntMax).Floor());

  EXPECT_EQ(LayoutUnit::kIntMin, LayoutUnit(LayoutUnit::kIntMin).Floor());
  EXPECT_EQ(LayoutUnit::kIntMin,
            (LayoutUnit(LayoutUnit::kIntMin) + LayoutUnit(0.5)).Floor());
  EXPECT_EQ(LayoutUnit::kIntMin + 1,
            (LayoutUnit(LayoutUnit::kIntMin) + LayoutUnit(1)).Floor());
}

TEST(LayoutUnitTest, LayoutUnitFloatOverflow) {
  // These should overflow to the max/min according to their sign.
  EXPECT_EQ(LayoutUnit::kIntMax, LayoutUnit(176972000.0f).ToInt());
  EXPECT_EQ(LayoutUnit::kIntMin, LayoutUnit(-176972000.0f).ToInt());
  EXPECT_EQ(LayoutUnit::kIntMax, LayoutUnit(176972000.0).ToInt());
  EXPECT_EQ(LayoutUnit::kIntMin, LayoutUnit(-176972000.0).ToInt());
}

TEST(LayoutUnitTest, UnaryMinus) {
  EXPECT_EQ(LayoutUnit(), -LayoutUnit());
  EXPECT_EQ(LayoutUnit(999), -LayoutUnit(-999));
  EXPECT_EQ(LayoutUnit(-999), -LayoutUnit(999));

  LayoutUnit negative_max;
  negative_max.SetRawValue(LayoutUnit::Min().RawValue() + 1);
  EXPECT_EQ(negative_max, -LayoutUnit::Max());
  EXPECT_EQ(LayoutUnit::Max(), -negative_max);

  // -LayoutUnit::min() is saturated to LayoutUnit::max()
  EXPECT_EQ(LayoutUnit::Max(), -LayoutUnit::Min());
}

TEST(LayoutUnitTest, LayoutUnitPlusPlus) {
  EXPECT_EQ(LayoutUnit(-1), LayoutUnit(-2)++);
  EXPECT_EQ(LayoutUnit(0), LayoutUnit(-1)++);
  EXPECT_EQ(LayoutUnit(1), LayoutUnit(0)++);
  EXPECT_EQ(LayoutUnit(2), LayoutUnit(1)++);
  EXPECT_EQ(LayoutUnit::Max(), LayoutUnit(LayoutUnit::Max())++);
}

TEST(LayoutUnitTest, IntMod) {
  EXPECT_EQ(LayoutUnit(5), IntMod(LayoutUnit(55), LayoutUnit(10)));
  EXPECT_EQ(LayoutUnit(5), IntMod(LayoutUnit(55), LayoutUnit(-10)));
  EXPECT_EQ(LayoutUnit(-5), IntMod(LayoutUnit(-55), LayoutUnit(10)));
  EXPECT_EQ(LayoutUnit(-5), IntMod(LayoutUnit(-55), LayoutUnit(-10)));
  EXPECT_EQ(LayoutUnit(1.5), IntMod(LayoutUnit(7.5), LayoutUnit(3)));
  EXPECT_EQ(LayoutUnit(1.25), IntMod(LayoutUnit(7.5), LayoutUnit(3.125)));
  EXPECT_EQ(LayoutUnit(), IntMod(LayoutUnit(7.5), LayoutUnit(2.5)));
  EXPECT_EQ(LayoutUnit(), IntMod(LayoutUnit(), LayoutUnit(123)));
}

TEST(LayoutUnitTest, Fraction) {
  EXPECT_TRUE(LayoutUnit(-1.9f).HasFraction());
  EXPECT_TRUE(LayoutUnit(-1.6f).HasFraction());
  EXPECT_TRUE(LayoutUnit::FromFloatRound(-1.51f).HasFraction());
  EXPECT_TRUE(LayoutUnit::FromFloatRound(-1.5f).HasFraction());
  EXPECT_TRUE(LayoutUnit::FromFloatRound(-1.49f).HasFraction());
  EXPECT_FALSE(LayoutUnit(-1.0f).HasFraction());
  EXPECT_TRUE(LayoutUnit::FromFloatRound(-0.95f).HasFraction());
  EXPECT_TRUE(LayoutUnit::FromFloatRound(-0.51f).HasFraction());
  EXPECT_TRUE(LayoutUnit::FromFloatRound(-0.50f).HasFraction());
  EXPECT_TRUE(LayoutUnit::FromFloatRound(-0.49f).HasFraction());
  EXPECT_TRUE(LayoutUnit(-0.1f).HasFraction());
  EXPECT_FALSE(LayoutUnit(-1.0f).HasFraction());
  EXPECT_FALSE(LayoutUnit(0.0f).HasFraction());
  EXPECT_TRUE(LayoutUnit(0.1f).HasFraction());
  EXPECT_TRUE(LayoutUnit::FromFloatRound(0.49f).HasFraction());
  EXPECT_TRUE(LayoutUnit::FromFloatRound(0.50f).HasFraction());
  EXPECT_TRUE(LayoutUnit::FromFloatRound(0.51f).HasFraction());
  EXPECT_TRUE(LayoutUnit(0.95f).HasFraction());
  EXPECT_FALSE(LayoutUnit(1.0f).HasFraction());
}

TEST(LayoutUnitTest, FixedConsts) {
  EXPECT_EQ(LayoutUnit::kFractionalBits, 6u);
  EXPECT_EQ(LayoutUnit::kIntegralBits, 26u);
  EXPECT_EQ(TextRunLayoutUnit::kFractionalBits, 16u);
  EXPECT_EQ(TextRunLayoutUnit::kIntegralBits, 16u);
  EXPECT_EQ(InlineLayoutUnit::kFractionalBits, 16u);
  EXPECT_EQ(InlineLayoutUnit::kIntegralBits, 48u);
}

TEST(LayoutUnitTest, Fixed) {
  constexpr int raw_value16 = 0x12345678;
  constexpr int raw_value6 = raw_value16 >> 10;
  const auto value16 = TextRunLayoutUnit::FromRawValue(raw_value16);
  const auto value6 = LayoutUnit::FromRawValue(raw_value6);
  EXPECT_EQ(value16.To<LayoutUnit>(), value6);
}

TEST(LayoutUnitTest, Raw64FromInt32) {
  constexpr int32_t int32_max_plus = LayoutUnit::kIntMax + 10;
  LayoutUnit int32_max_plus_32(int32_max_plus);
  EXPECT_NE(int32_max_plus_32.ToInt(), int32_max_plus);
  InlineLayoutUnit int32_max_plus_64(int32_max_plus);
  EXPECT_EQ(int32_max_plus_64.ToInt(), int32_max_plus);

  constexpr int32_t int32_min_minus = LayoutUnit::kIntMin - 10;
  LayoutUnit int32_min_minus_32(int32_min_minus);
  EXPECT_NE(int32_min_minus_32.ToInt(), int32_min_minus);
  InlineLayoutUnit int32_min_minus_64(int32_min_minus);
  EXPECT_EQ(int32_min_minus_64.ToInt(), int32_min_minus);

  constexpr int64_t raw32_max_plus =
      static_cast<int64_t>(LayoutUnit::kRawValueMax) + 10;
  LayoutUnit raw32_max_plus_32(raw32_max_plus);
  EXPECT_NE(raw32_max_plus_32.ToInt(), raw32_max_plus);
  InlineLayoutUnit raw32_max_plus_64(raw32_max_plus);
  EXPECT_EQ(raw32_max_plus_64.ToInt(), raw32_max_plus);

  constexpr int64_t raw32_min_minus =
      static_cast<int64_t>(LayoutUnit::kRawValueMin) - 10;
  LayoutUnit raw32_min_minus_32(raw32_min_minus);
  EXPECT_NE(raw32_min_minus_32.ToInt(), raw32_min_minus);
  InlineLayoutUnit raw32_min_minus_64(raw32_min_minus);
  EXPECT_EQ(raw32_min_minus_64.ToInt(), raw32_min_minus);
}

TEST(LayoutUnitTest, Raw64FromRaw32) {
  constexpr float value = 1.f + LayoutUnit::Epsilon() * 234;
  LayoutUnit value32_6(value);
  EXPECT_EQ(InlineLayoutUnit(value32_6), InlineLayoutUnit(value));
  TextRunLayoutUnit value32_16(value);
  EXPECT_EQ(InlineLayoutUnit(value32_16), InlineLayoutUnit(value));

  // The following code should fail to compile.
  // TextRunLayoutUnit back_to_32{InlineLayoutUnit(value)};
}

TEST(LayoutUnitTest, To) {
#define TEST_ROUND_TRIP(T1, T2)                      \
  EXPECT_EQ(T1(value), T2(value).To<T1>()) << value; \
  EXPECT_EQ(T2(value), T1(value).To<T2>()) << value;

  for (const float value : {1.0f, 1.5f, -1.0f}) {
    TEST_ROUND_TRIP(LayoutUnit, TextRunLayoutUnit);
    TEST_ROUND_TRIP(LayoutUnit, InlineLayoutUnit);
    TEST_ROUND_TRIP(TextRunLayoutUnit, InlineLayoutUnit);
  }
#undef TEST_ROUND_TRIP
}

TEST(LayoutUnitTest, ToClampSameFractional64To32) {
  EXPECT_EQ(
      TextRunLayoutUnit::Max(),
      InlineLayoutUnit(TextRunLayoutUnit::kIntMax + 1).To<TextRunLayoutUnit>());
  EXPECT_EQ(
      TextRunLayoutUnit::Min(),
      InlineLayoutUnit(TextRunLayoutUnit::kIntMin - 1).To<TextRunLayoutUnit>());
}

TEST(LayoutUnitTest, ToClampLessFractional64To32) {
  EXPECT_EQ(LayoutUnit::Max(),
            InlineLayoutUnit(LayoutUnit::kIntMax + 1).To<LayoutUnit>());
  EXPECT_EQ(LayoutUnit::Min(),
            InlineLayoutUnit(LayoutUnit::kIntMin - 1).To<LayoutUnit>());
}

TEST(LayoutUnitTest, ToClampMoreFractional) {
  EXPECT_EQ(TextRunLayoutUnit::Max(),
            LayoutUnit(TextRunLayoutUnit::kIntMax + 1).To<TextRunLayoutUnit>());
  EXPECT_EQ(TextRunLayoutUnit::Min(),
            LayoutUnit(TextRunLayoutUnit::kIntMin - 1).To<TextRunLayoutUnit>());
}

TEST(LayoutUnitTest, Raw64Ceil) {
  LayoutUnit layout(1.234);
  InlineLayoutUnit inline_value(layout);
  EXPECT_EQ(layout, inline_value.ToCeil<LayoutUnit>());

  inline_value = inline_value.AddEpsilon();
  EXPECT_NE(layout, inline_value.ToCeil<LayoutUnit>());
  EXPECT_EQ(layout.AddEpsilon(), inline_value.ToCeil<LayoutUnit>());
}

}  // namespace blink
