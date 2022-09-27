// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"

namespace blink {
namespace {
const float kEpsilon = 0.001f;
}

TEST(CSSUnitValueTest, PixelToOtherUnit) {
  CSSUnitValue* pxValue =
      CSSUnitValue::Create(96, CSSPrimitiveValue::UnitType::kPixels);

  EXPECT_NEAR(96, pxValue->to(CSSPrimitiveValue::UnitType::kPixels)->value(),
              kEpsilon);
  EXPECT_NEAR(2.54,
              pxValue->to(CSSPrimitiveValue::UnitType::kCentimeters)->value(),
              kEpsilon);
  EXPECT_NEAR(25.4,
              pxValue->to(CSSPrimitiveValue::UnitType::kMillimeters)->value(),
              kEpsilon);
  EXPECT_NEAR(1, pxValue->to(CSSPrimitiveValue::UnitType::kInches)->value(),
              kEpsilon);
  EXPECT_NEAR(72, pxValue->to(CSSPrimitiveValue::UnitType::kPoints)->value(),
              kEpsilon);
  EXPECT_NEAR(6, pxValue->to(CSSPrimitiveValue::UnitType::kPicas)->value(),
              kEpsilon);
  EXPECT_NEAR(
      101.6,
      pxValue->to(CSSPrimitiveValue::UnitType::kQuarterMillimeters)->value(),
      kEpsilon);
}

TEST(CSSUnitValueTest, CentimeterToOtherUnit) {
  CSSUnitValue* cmValue =
      CSSUnitValue::Create(2.54, CSSPrimitiveValue::UnitType::kCentimeters);

  EXPECT_NEAR(96, cmValue->to(CSSPrimitiveValue::UnitType::kPixels)->value(),
              kEpsilon);
  EXPECT_NEAR(2.54,
              cmValue->to(CSSPrimitiveValue::UnitType::kCentimeters)->value(),
              kEpsilon);
  EXPECT_NEAR(25.4,
              cmValue->to(CSSPrimitiveValue::UnitType::kMillimeters)->value(),
              kEpsilon);
  EXPECT_NEAR(1, cmValue->to(CSSPrimitiveValue::UnitType::kInches)->value(),
              kEpsilon);
  EXPECT_NEAR(72, cmValue->to(CSSPrimitiveValue::UnitType::kPoints)->value(),
              kEpsilon);
  EXPECT_NEAR(6, cmValue->to(CSSPrimitiveValue::UnitType::kPicas)->value(),
              kEpsilon);
  EXPECT_NEAR(
      101.6,
      cmValue->to(CSSPrimitiveValue::UnitType::kQuarterMillimeters)->value(),
      kEpsilon);
}

TEST(CSSUnitValueTest, MillimeterToOtherUnit) {
  CSSUnitValue* mmValue =
      CSSUnitValue::Create(25.4, CSSPrimitiveValue::UnitType::kMillimeters);

  EXPECT_NEAR(96, mmValue->to(CSSPrimitiveValue::UnitType::kPixels)->value(),
              kEpsilon);
  EXPECT_NEAR(2.54,
              mmValue->to(CSSPrimitiveValue::UnitType::kCentimeters)->value(),
              kEpsilon);
  EXPECT_NEAR(25.4,
              mmValue->to(CSSPrimitiveValue::UnitType::kMillimeters)->value(),
              kEpsilon);
  EXPECT_NEAR(1, mmValue->to(CSSPrimitiveValue::UnitType::kInches)->value(),
              kEpsilon);
  EXPECT_NEAR(72, mmValue->to(CSSPrimitiveValue::UnitType::kPoints)->value(),
              kEpsilon);
  EXPECT_NEAR(6, mmValue->to(CSSPrimitiveValue::UnitType::kPicas)->value(),
              kEpsilon);
  EXPECT_NEAR(
      101.6,
      mmValue->to(CSSPrimitiveValue::UnitType::kQuarterMillimeters)->value(),
      kEpsilon);
}

TEST(CSSUnitValueTest, InchesToOtherUnit) {
  CSSUnitValue* inValue =
      CSSUnitValue::Create(1, CSSPrimitiveValue::UnitType::kInches);

  EXPECT_NEAR(96, inValue->to(CSSPrimitiveValue::UnitType::kPixels)->value(),
              kEpsilon);
  EXPECT_NEAR(2.54,
              inValue->to(CSSPrimitiveValue::UnitType::kCentimeters)->value(),
              kEpsilon);
  EXPECT_NEAR(25.4,
              inValue->to(CSSPrimitiveValue::UnitType::kMillimeters)->value(),
              kEpsilon);
  EXPECT_NEAR(1, inValue->to(CSSPrimitiveValue::UnitType::kInches)->value(),
              kEpsilon);
  EXPECT_NEAR(72, inValue->to(CSSPrimitiveValue::UnitType::kPoints)->value(),
              kEpsilon);
  EXPECT_NEAR(6, inValue->to(CSSPrimitiveValue::UnitType::kPicas)->value(),
              kEpsilon);
  EXPECT_NEAR(
      101.6,
      inValue->to(CSSPrimitiveValue::UnitType::kQuarterMillimeters)->value(),
      kEpsilon);
}

TEST(CSSUnitValueTest, PointToOtherUnit) {
  CSSUnitValue* ptValue =
      CSSUnitValue::Create(72, CSSPrimitiveValue::UnitType::kPoints);

  EXPECT_NEAR(96, ptValue->to(CSSPrimitiveValue::UnitType::kPixels)->value(),
              kEpsilon);
  EXPECT_NEAR(2.54,
              ptValue->to(CSSPrimitiveValue::UnitType::kCentimeters)->value(),
              kEpsilon);
  EXPECT_NEAR(25.4,
              ptValue->to(CSSPrimitiveValue::UnitType::kMillimeters)->value(),
              kEpsilon);
  EXPECT_NEAR(1, ptValue->to(CSSPrimitiveValue::UnitType::kInches)->value(),
              kEpsilon);
  EXPECT_NEAR(72, ptValue->to(CSSPrimitiveValue::UnitType::kPoints)->value(),
              kEpsilon);
  EXPECT_NEAR(6, ptValue->to(CSSPrimitiveValue::UnitType::kPicas)->value(),
              kEpsilon);
  EXPECT_NEAR(
      101.6,
      ptValue->to(CSSPrimitiveValue::UnitType::kQuarterMillimeters)->value(),
      kEpsilon);
}

TEST(CSSUnitValueTest, PicaToOtherUnit) {
  CSSUnitValue* pcValue =
      CSSUnitValue::Create(6, CSSPrimitiveValue::UnitType::kPicas);

  EXPECT_NEAR(96, pcValue->to(CSSPrimitiveValue::UnitType::kPixels)->value(),
              kEpsilon);
  EXPECT_NEAR(2.54,
              pcValue->to(CSSPrimitiveValue::UnitType::kCentimeters)->value(),
              kEpsilon);
  EXPECT_NEAR(25.4,
              pcValue->to(CSSPrimitiveValue::UnitType::kMillimeters)->value(),
              kEpsilon);
  EXPECT_NEAR(1, pcValue->to(CSSPrimitiveValue::UnitType::kInches)->value(),
              kEpsilon);
  EXPECT_NEAR(72, pcValue->to(CSSPrimitiveValue::UnitType::kPoints)->value(),
              kEpsilon);
  EXPECT_NEAR(6, pcValue->to(CSSPrimitiveValue::UnitType::kPicas)->value(),
              kEpsilon);
  EXPECT_NEAR(
      101.6,
      pcValue->to(CSSPrimitiveValue::UnitType::kQuarterMillimeters)->value(),
      kEpsilon);
}

TEST(CSSUnitValueTest, QuarterMillimeterToOtherUnit) {
  CSSUnitValue* qValue = CSSUnitValue::Create(
      101.6, CSSPrimitiveValue::UnitType::kQuarterMillimeters);

  EXPECT_NEAR(96, qValue->to(CSSPrimitiveValue::UnitType::kPixels)->value(),
              kEpsilon);
  EXPECT_NEAR(2.54,
              qValue->to(CSSPrimitiveValue::UnitType::kCentimeters)->value(),
              kEpsilon);
  EXPECT_NEAR(25.4,
              qValue->to(CSSPrimitiveValue::UnitType::kMillimeters)->value(),
              kEpsilon);
  EXPECT_NEAR(1, qValue->to(CSSPrimitiveValue::UnitType::kInches)->value(),
              kEpsilon);
  EXPECT_NEAR(72, qValue->to(CSSPrimitiveValue::UnitType::kPoints)->value(),
              kEpsilon);
  EXPECT_NEAR(6, qValue->to(CSSPrimitiveValue::UnitType::kPicas)->value(),
              kEpsilon);
  EXPECT_NEAR(
      101.6,
      qValue->to(CSSPrimitiveValue::UnitType::kQuarterMillimeters)->value(),
      kEpsilon);
}

}  // namespace blink
