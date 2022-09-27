// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_numeric_value_type.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using UnitType = CSSPrimitiveValue::UnitType;
using BaseType = CSSNumericValueType::BaseType;

TEST(CSSNumericValueType, ApplyingPercentHintMovesPowerAndSetsPercentHint) {
  CSSNumericValueType type(UnitType::kPixels);
  type.SetExponent(BaseType::kPercent, 5);
  EXPECT_EQ(5, type.Exponent(BaseType::kPercent));
  EXPECT_EQ(1, type.Exponent(BaseType::kLength));
  EXPECT_FALSE(type.HasPercentHint());

  type.ApplyPercentHint(BaseType::kLength);
  EXPECT_EQ(0, type.Exponent(BaseType::kPercent));
  EXPECT_EQ(6, type.Exponent(BaseType::kLength));
  ASSERT_TRUE(type.HasPercentHint());
  EXPECT_EQ(BaseType::kLength, type.PercentHint());
}

TEST(CSSNumericValueType, MatchesBaseTypePercentage) {
  CSSNumericValueType type;
  EXPECT_FALSE(type.MatchesBaseType(BaseType::kLength));
  EXPECT_FALSE(type.MatchesBaseTypePercentage(BaseType::kLength));

  type.SetExponent(BaseType::kLength, 1);
  EXPECT_TRUE(type.MatchesBaseType(BaseType::kLength));
  EXPECT_TRUE(type.MatchesBaseTypePercentage(BaseType::kLength));

  type.SetExponent(BaseType::kLength, 2);
  EXPECT_FALSE(type.MatchesBaseType(BaseType::kLength));
  EXPECT_FALSE(type.MatchesBaseTypePercentage(BaseType::kLength));

  type.SetExponent(BaseType::kLength, 1);
  EXPECT_TRUE(type.MatchesBaseType(BaseType::kLength));
  EXPECT_TRUE(type.MatchesBaseTypePercentage(BaseType::kLength));

  type.ApplyPercentHint(BaseType::kLength);
  EXPECT_FALSE(type.MatchesBaseType(BaseType::kLength));
  EXPECT_TRUE(type.MatchesBaseTypePercentage(BaseType::kLength));
}

TEST(CSSNumericValueType, MatchesPercentage) {
  CSSNumericValueType type;
  EXPECT_FALSE(type.MatchesPercentage());

  type.SetExponent(BaseType::kPercent, 1);
  EXPECT_TRUE(type.MatchesPercentage());

  type.SetExponent(BaseType::kPercent, 2);
  EXPECT_FALSE(type.MatchesPercentage());

  type.ApplyPercentHint(BaseType::kLength);
  EXPECT_FALSE(type.MatchesPercentage());

  type.SetExponent(BaseType::kLength, 0);
  type.SetExponent(BaseType::kPercent, 1);
  EXPECT_TRUE(type.MatchesPercentage());
}

TEST(CSSNumericValueType, MatchesNumberPercentage) {
  CSSNumericValueType type;
  EXPECT_TRUE(type.MatchesNumber());
  EXPECT_TRUE(type.MatchesNumberPercentage());

  type.SetExponent(BaseType::kLength, 1);
  EXPECT_FALSE(type.MatchesNumber());
  EXPECT_FALSE(type.MatchesNumberPercentage());

  type.SetExponent(BaseType::kLength, 0);
  EXPECT_TRUE(type.MatchesNumber());
  EXPECT_TRUE(type.MatchesNumberPercentage());

  type.SetExponent(BaseType::kPercent, 1);
  EXPECT_FALSE(type.MatchesNumber());
  EXPECT_TRUE(type.MatchesNumberPercentage());
}

}  // namespace blink
