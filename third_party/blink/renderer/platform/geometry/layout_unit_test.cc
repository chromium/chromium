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

TEST(LayoutUnitTest, LayoutUnitInt) {
  EXPECT_EQ(kIntMinForLayoutUnit, LayoutUnit(INT_MIN).ToInt());
  EXPECT_EQ(kIntMinForLayoutUnit, LayoutUnit(INT_MIN / 2).ToInt());
  EXPECT_EQ(kIntMinForLayoutUnit, LayoutUnit(kIntMinForLayoutUnit - 1).ToInt());
  EXPECT_EQ(kIntMinForLayoutUnit, LayoutUnit(kIntMinForLayoutUnit).ToInt());
  EXPECT_EQ(kIntMinForLayoutUnit + 1,
            LayoutUnit(kIntMinForLayoutUnit + 1).ToInt());
  EXPECT_EQ(kIntMinForLayoutUnit / 2,
            LayoutUnit(kIntMinForLayoutUnit / 2).ToInt());
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
  EXPECT_EQ(kIntMaxForLayoutUnit / 2,
            LayoutUnit(kIntMaxForLayoutUnit / 2).ToInt());
  EXPECT_EQ(kIntMaxForLayoutUnit - 1,
            LayoutUnit(kIntMaxForLayoutUnit - 1).ToInt());
  EXPECT_EQ(kIntMaxForLayoutUnit, LayoutUnit(kIntMaxForLayoutUnit).ToInt());
  EXPECT_EQ(kIntMaxForLayoutUnit, LayoutUnit(kIntMaxForLayoutUnit + 1).ToInt());
  EXPECT_EQ(kIntMaxForLayoutUnit, LayoutUnit(INT_MAX / 2).ToInt());
  EXPECT_EQ(kIntMaxForLayoutUnit, LayoutUnit(INT_MAX).ToInt());

  // Test the raw unsaturated value
  EXPECT_EQ(0, LayoutUnit(0).RawValue());
  // Internally the max number we can represent (without saturating)
  // is all the (non-sign) bits set except for the bottom n fraction bits
  const int max_internal_representation =
      std::numeric_limits<int>::max() ^ ((1 << kLayoutUnitFractionalBits) - 1);
  EXPECT_EQ(max_internal_representation,
            LayoutUnit(kIntMaxForLayoutUnit).RawValue());
  EXPECT_EQ(GetMaxSaturatedSetResultForTesting(),
            LayoutUnit(kIntMaxForLayoutUnit + 100).RawValue());
  EXPECT_EQ((kIntMaxForLayoutUnit - 100) << kLayoutUnitFractionalBits,
            LayoutUnit(kIntMaxForLayoutUnit - 100).RawValue());
  EXPECT_EQ(GetMinSaturatedSetResultForTesting(),
            LayoutUnit(kIntMinForLayoutUnit).RawValue());
  EXPECT_EQ(GetMinSaturatedSetResultForTesting(),
            LayoutUnit(kIntMinForLayoutUnit - 100).RawValue());
  // Shifting negative numbers left has undefined behavior, so use
  // multiplication instead of direct shifting here.
  EXPECT_EQ((kIntMinForLayoutUnit + 100) * (1 << kLayoutUnitFractionalBits),
            LayoutUnit(kIntMinForLayoutUnit + 100).RawValue());
}

TEST(LayoutUnitTest, LayoutUnitUnsigned) {
  // Test the raw unsaturated value
  EXPECT_EQ(0, LayoutUnit((unsigned)0).RawValue());
  EXPECT_EQ(GetMaxSaturatedSetResultForTesting(),
            LayoutUnit((unsigned)kIntMaxForLayoutUnit).RawValue());
  const unsigned kOverflowed = kIntMaxForLayoutUnit + 100;
  EXPECT_EQ(GetMaxSaturatedSetResultForTesting(),
            LayoutUnit(kOverflowed).RawValue());
  const unsigned kNotOverflowed = kIntMaxForLayoutUnit - 100;
  EXPECT_EQ((kIntMaxForLayoutUnit - 100) << kLayoutUnitFractionalBits,
            LayoutUnit(kNotOverflowed).RawValue());
}

TEST(LayoutUnitTest, LayoutUnitFloat) {
  const float kTolerance = 1.0f / kFixedPointDenominator;
  EXPECT_FLOAT_EQ(1.0f, LayoutUnit(1.0f).ToFloat());
  EXPECT_FLOAT_EQ(1.25f, LayoutUnit(1.25f).ToFloat());
  EXPECT_NEAR(LayoutUnit(1.1f).ToFloat(), 1.1f, kTolerance);
  EXPECT_NEAR(LayoutUnit(1.33f).ToFloat(), 1.33f, kTolerance);
  EXPECT_NEAR(LayoutUnit(1.3333f).ToFloat(), 1.3333f, kTolerance);
  EXPECT_NEAR(LayoutUnit(1.53434f).ToFloat(), 1.53434f, kTolerance);
  EXPECT_NEAR(LayoutUnit(345634).ToFloat(), 345634.0f, kTolerance);
  EXPECT_NEAR(LayoutUnit(345634.12335f).ToFloat(), 345634.12335f, kTolerance);
  EXPECT_NEAR(LayoutUnit(-345634.12335f).ToFloat(), -345634.12335f, kTolerance);
  EXPECT_NEAR(LayoutUnit(-345634).ToFloat(), -345634.0f, kTolerance);
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
  EXPECT_EQ(((std::numeric_limits<int>::max() / kFixedPointDenominator) + 1),
            LayoutUnit::Max().Round());
  // The fractional part of LayoutUnit::Min() is 0, so the next bigger possible
  // value should round down.
  LayoutUnit epsilon;
  epsilon.SetRawValue(1);
  EXPECT_EQ(((std::numeric_limits<int>::min() / kFixedPointDenominator)),
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

  EXPECT_EQ(0, SnapSizeToPixel(LayoutUnit(0.5), LayoutUnit(1.5)));
  EXPECT_EQ(0, SnapSizeToPixel(LayoutUnit(0.99), LayoutUnit(1.5)));
  EXPECT_EQ(1, SnapSizeToPixel(LayoutUnit(1.0), LayoutUnit(1.5)));
  EXPECT_EQ(1, SnapSizeToPixel(LayoutUnit(1.49), LayoutUnit(1.5)));
  EXPECT_EQ(1, SnapSizeToPixel(LayoutUnit(1.5), LayoutUnit(1.5)));

  EXPECT_EQ(101, SnapSizeToPixel(LayoutUnit(100.5), LayoutUnit(100)));
  EXPECT_EQ(kIntMaxForLayoutUnit,
            SnapSizeToPixel(LayoutUnit(kIntMaxForLayoutUnit), LayoutUnit(0.3)));
  EXPECT_EQ(
      kIntMinForLayoutUnit,
      SnapSizeToPixel(LayoutUnit(kIntMinForLayoutUnit), LayoutUnit(-0.3)));
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

  int quarter_max = kIntMaxForLayoutUnit / 4;
  EXPECT_EQ(quarter_max * 2, (LayoutUnit(quarter_max) * LayoutUnit(2)).ToInt());
  EXPECT_EQ(quarter_max * 3, (LayoutUnit(quarter_max) * LayoutUnit(3)).ToInt());
  EXPECT_EQ(quarter_max * 4, (LayoutUnit(quarter_max) * LayoutUnit(4)).ToInt());
  EXPECT_EQ(kIntMaxForLayoutUnit,
            (LayoutUnit(quarter_max) * LayoutUnit(5)).ToInt());

  size_t overflow_int_size_t = kIntMaxForLayoutUnit * 4;
  EXPECT_EQ(kIntMaxForLayoutUnit,
            (LayoutUnit(overflow_int_size_t) * LayoutUnit(2)).ToInt());
  EXPECT_EQ(kIntMaxForLayoutUnit,
            (overflow_int_size_t * LayoutUnit(4)).ToInt());
  EXPECT_EQ(kIntMaxForLayoutUnit,
            (LayoutUnit(4) * overflow_int_size_t).ToInt());
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

  EXPECT_EQ(kIntMaxForLayoutUnit / 2,
            (LayoutUnit(kIntMaxForLayoutUnit) / LayoutUnit(2)).ToInt());
  EXPECT_EQ(kIntMaxForLayoutUnit,
            (LayoutUnit(kIntMaxForLayoutUnit) / LayoutUnit(0.5)).ToInt());
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

  EXPECT_EQ(kIntMaxForLayoutUnit, LayoutUnit(kIntMaxForLayoutUnit).Ceil());
  EXPECT_EQ(kIntMaxForLayoutUnit,
            (LayoutUnit(kIntMaxForLayoutUnit) - LayoutUnit(0.5)).Ceil());
  EXPECT_EQ(kIntMaxForLayoutUnit - 1,
            (LayoutUnit(kIntMaxForLayoutUnit) - LayoutUnit(1)).Ceil());

  EXPECT_EQ(kIntMinForLayoutUnit, LayoutUnit(kIntMinForLayoutUnit).Ceil());
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

  EXPECT_EQ(kIntMaxForLayoutUnit, LayoutUnit(kIntMaxForLayoutUnit).Floor());

  EXPECT_EQ(kIntMinForLayoutUnit, LayoutUnit(kIntMinForLayoutUnit).Floor());
  EXPECT_EQ(kIntMinForLayoutUnit,
            (LayoutUnit(kIntMinForLayoutUnit) + LayoutUnit(0.5)).Floor());
  EXPECT_EQ(kIntMinForLayoutUnit + 1,
            (LayoutUnit(kIntMinForLayoutUnit) + LayoutUnit(1)).Floor());
}

TEST(LayoutUnitTest, LayoutUnitFloatOverflow) {
  // These should overflow to the max/min according to their sign.
  EXPECT_EQ(kIntMaxForLayoutUnit, LayoutUnit(176972000.0f).ToInt());
  EXPECT_EQ(kIntMinForLayoutUnit, LayoutUnit(-176972000.0f).ToInt());
  EXPECT_EQ(kIntMaxForLayoutUnit, LayoutUnit(176972000.0).ToInt());
  EXPECT_EQ(kIntMinForLayoutUnit, LayoutUnit(-176972000.0).ToInt());
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

TEST(LayoutUnitTest, LayoutMod) {
#define CHECK_LAYOUT_MOD(a, b) EXPECT_EQ(a, (a / b) * b + LayoutMod(a, b))
  CHECK_LAYOUT_MOD(LayoutUnit(55), LayoutUnit(10));
  CHECK_LAYOUT_MOD(LayoutUnit(1234), LayoutUnit(789));
  CHECK_LAYOUT_MOD(LayoutUnit::Max(), LayoutUnit::Max());
  CHECK_LAYOUT_MOD(LayoutUnit::Max(), LayoutUnit::Min());
  CHECK_LAYOUT_MOD(LayoutUnit::Min(), LayoutUnit::Max());
  CHECK_LAYOUT_MOD(LayoutUnit::Min(), LayoutUnit::Min());

  EXPECT_EQ(LayoutUnit(), LayoutMod(LayoutUnit(123), 2));
  EXPECT_EQ(LayoutUnit(LayoutUnit::Epsilon()),
            LayoutMod(LayoutUnit(123 + LayoutUnit::Epsilon()), 2));
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

}  // namespace blink
