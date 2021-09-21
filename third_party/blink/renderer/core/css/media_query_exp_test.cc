// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/media_query_exp.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

MediaQueryExpValue IdentValue(CSSValueID id) {
  return MediaQueryExpValue(id);
}

MediaQueryExpValue RatioValue(unsigned numerator, unsigned denominator) {
  return MediaQueryExpValue(numerator, denominator);
}

MediaQueryExpValue PxValue(double value) {
  return MediaQueryExpValue(value, CSSPrimitiveValue::UnitType::kPixels);
}

MediaQueryExpValue EmValue(double value) {
  return MediaQueryExpValue(value, CSSPrimitiveValue::UnitType::kEms);
}

MediaQueryExpValue InvalidValue() {
  return MediaQueryExpValue();
}

}  // namespace

TEST(MediaQueryExpTest, ValuesType) {
  EXPECT_TRUE(IdentValue(CSSValueID::kTop).IsId());
  EXPECT_TRUE(PxValue(10).IsNumeric());
  EXPECT_TRUE(RatioValue(0, 1).IsRatio());
}

TEST(MediaQueryExpTest, ValueEquality) {
  EXPECT_EQ(PxValue(10), PxValue(10));
  EXPECT_EQ(EmValue(10), EmValue(10));
  EXPECT_EQ(IdentValue(CSSValueID::kTop), IdentValue(CSSValueID::kTop));
  EXPECT_EQ(IdentValue(CSSValueID::kTop), IdentValue(CSSValueID::kTop));
  EXPECT_EQ(RatioValue(1, 2), RatioValue(1, 2));

  // Mismatched values:
  EXPECT_NE(PxValue(10), PxValue(20));
  EXPECT_NE(EmValue(20), EmValue(10));
  EXPECT_NE(IdentValue(CSSValueID::kTop), IdentValue(CSSValueID::kLeft));
  EXPECT_NE(RatioValue(16, 9), RatioValue(4, 3));

  // Mismatched unit:
  EXPECT_NE(PxValue(10), EmValue(10));

  // Mismatched types:
  EXPECT_NE(PxValue(10), IdentValue(CSSValueID::kTop));
  EXPECT_NE(PxValue(10), RatioValue(1, 2));

  // Mismatched validity:
  EXPECT_NE(PxValue(10), InvalidValue());
  EXPECT_NE(PxValue(0), InvalidValue());
  EXPECT_NE(RatioValue(0, 1), InvalidValue());
}

}  // namespace blink
