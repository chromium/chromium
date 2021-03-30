// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_primitive_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
namespace {

class CSSPrimitiveValueTest : public PageTestBase,
                              private ScopedCSSCalcInfinityAndNaNForTest {
 public:
  CSSPrimitiveValueTest() : ScopedCSSCalcInfinityAndNaNForTest(true) {}
};

using UnitType = CSSPrimitiveValue::UnitType;

struct UnitValue {
  double value;
  UnitType unit_type;
};

CSSNumericLiteralValue* Create(UnitValue v) {
  return CSSNumericLiteralValue::Create(v.value, v.unit_type);
}

CSSPrimitiveValue* CreateAddition(UnitValue a, UnitValue b) {
  return CSSMathFunctionValue::Create(CSSMathExpressionBinaryOperation::Create(
      CSSMathExpressionNumericLiteral::Create(Create(a)),
      CSSMathExpressionNumericLiteral::Create(Create(b)),
      CSSMathOperator::kAdd));
}

CSSPrimitiveValue* CreateNonNegativeSubtraction(UnitValue a, UnitValue b) {
  return CSSMathFunctionValue::Create(
      CSSMathExpressionBinaryOperation::Create(
          CSSMathExpressionNumericLiteral::Create(Create(a)),
          CSSMathExpressionNumericLiteral::Create(Create(b)),
          CSSMathOperator::kSubtract),
      kValueRangeNonNegative);
}

TEST_F(CSSPrimitiveValueTest, IsTime) {
  EXPECT_FALSE(Create({5.0, UnitType::kNumber})->IsTime());
  EXPECT_FALSE(Create({5.0, UnitType::kDegrees})->IsTime());
  EXPECT_TRUE(Create({5.0, UnitType::kSeconds})->IsTime());
  EXPECT_TRUE(Create({5.0, UnitType::kMilliseconds})->IsTime());
}

TEST_F(CSSPrimitiveValueTest, IsTimeCalc) {
  {
    UnitValue a = {1.0, UnitType::kSeconds};
    UnitValue b = {1000.0, UnitType::kMilliseconds};
    EXPECT_TRUE(CreateAddition(a, b)->IsTime());
  }
  {
    UnitValue a = {1.0, UnitType::kDegrees};
    UnitValue b = {1000.0, UnitType::kGradians};
    EXPECT_FALSE(CreateAddition(a, b)->IsTime());
  }
}

TEST_F(CSSPrimitiveValueTest, ClampTimeToNonNegative) {
  UnitValue a = {4926, UnitType::kMilliseconds};
  UnitValue b = {5, UnitType::kSeconds};
  EXPECT_EQ(0.0, CreateNonNegativeSubtraction(a, b)->ComputeSeconds());
}

TEST_F(CSSPrimitiveValueTest, ClampAngleToNonNegative) {
  UnitValue a = {89, UnitType::kDegrees};
  UnitValue b = {0.25, UnitType::kTurns};
  EXPECT_EQ(0.0, CreateNonNegativeSubtraction(a, b)->ComputeDegrees());
}

TEST_F(CSSPrimitiveValueTest, IsResolution) {
  EXPECT_FALSE(Create({5.0, UnitType::kNumber})->IsResolution());
  EXPECT_FALSE(Create({5.0, UnitType::kDegrees})->IsResolution());
  EXPECT_TRUE(Create({5.0, UnitType::kDotsPerPixel})->IsResolution());
  EXPECT_TRUE(Create({5.0, UnitType::kDotsPerCentimeter})->IsResolution());
}

// https://crbug.com/999875
TEST_F(CSSPrimitiveValueTest, Zooming) {
  // Tests that the conversion CSSPrimitiveValue -> Length -> CSSPrimitiveValue
  // yields the same value under zooming.

  UnitValue a = {100, UnitType::kPixels};
  UnitValue b = {10, UnitType::kPercentage};
  CSSPrimitiveValue* original = CreateAddition(a, b);

  CSSToLengthConversionData conversion_data;
  conversion_data.SetZoom(0.5);

  Length length = original->ConvertToLength(conversion_data);
  EXPECT_TRUE(length.IsCalculated());
  EXPECT_EQ(50.0, length.GetPixelsAndPercent().pixels);
  EXPECT_EQ(10.0, length.GetPixelsAndPercent().percent);

  CSSPrimitiveValue* converted =
      CSSPrimitiveValue::CreateFromLength(length, conversion_data.Zoom());
  EXPECT_TRUE(converted->IsMathFunctionValue());
  EXPECT_EQ("calc(10% + 100px)", converted->CustomCSSText());
}

TEST_F(CSSPrimitiveValueTest, PositiveInfinityLengthClamp) {
  UnitValue a = {std::numeric_limits<double>::infinity(), UnitType::kPixels};
  UnitValue b = {1, UnitType::kPixels};
  CSSPrimitiveValue* value = CreateAddition(a, b);
  CSSToLengthConversionData conversion_data;
  EXPECT_EQ(std::numeric_limits<double>::max(),
            value->ComputeLength<double>(conversion_data));
}

TEST_F(CSSPrimitiveValueTest, NegativeInfinityLengthClamp) {
  UnitValue a = {-std::numeric_limits<double>::infinity(), UnitType::kPixels};
  UnitValue b = {1, UnitType::kPixels};
  CSSPrimitiveValue* value = CreateAddition(a, b);
  CSSToLengthConversionData conversion_data;
  EXPECT_EQ(std::numeric_limits<double>::lowest(),
            value->ComputeLength<double>(conversion_data));
}

TEST_F(CSSPrimitiveValueTest, NaNLengthClamp) {
  UnitValue a = {-std::numeric_limits<double>::quiet_NaN(), UnitType::kPixels};
  UnitValue b = {1, UnitType::kPixels};
  CSSPrimitiveValue* value = CreateAddition(a, b);
  CSSToLengthConversionData conversion_data;
  EXPECT_EQ(std::numeric_limits<double>::max(),
            value->ComputeLength<double>(conversion_data));
}

TEST_F(CSSPrimitiveValueTest, PositiveInfinityPercentLengthClamp) {
  CSSPrimitiveValue* value =
      Create({std::numeric_limits<double>::infinity(), UnitType::kPercentage});
  CSSToLengthConversionData conversion_data;
  Length length = value->ConvertToLength(conversion_data);
  EXPECT_EQ(std::numeric_limits<float>::max(), length.Percent());
}

TEST_F(CSSPrimitiveValueTest, NegativeInfinityPercentLengthClamp) {
  CSSPrimitiveValue* value =
      Create({-std::numeric_limits<double>::infinity(), UnitType::kPercentage});
  CSSToLengthConversionData conversion_data;
  Length length = value->ConvertToLength(conversion_data);
  EXPECT_EQ(std::numeric_limits<float>::lowest(), length.Percent());
}

TEST_F(CSSPrimitiveValueTest, NaNPercentLengthClamp) {
  CSSPrimitiveValue* value = Create(
      {-std::numeric_limits<double>::quiet_NaN(), UnitType::kPercentage});
  CSSToLengthConversionData conversion_data;
  Length length = value->ConvertToLength(conversion_data);
  EXPECT_EQ(std::numeric_limits<float>::max(), length.Percent());
}

TEST_F(CSSPrimitiveValueTest, GetDoubleValueWithoutClampingAllowNaN) {
  CSSPrimitiveValue* value =
      Create({std::numeric_limits<double>::quiet_NaN(), UnitType::kPixels});
  EXPECT_TRUE(std::isnan(value->GetDoubleValueWithoutClamping()));
}

TEST_F(CSSPrimitiveValueTest,
       GetDoubleValueWithoutClampingAllowPositveInfinity) {
  CSSPrimitiveValue* value =
      Create({std::numeric_limits<double>::infinity(), UnitType::kPixels});
  EXPECT_TRUE(std::isinf(value->GetDoubleValueWithoutClamping()) &&
              value->GetDoubleValueWithoutClamping() > 0);
}

TEST_F(CSSPrimitiveValueTest,
       GetDoubleValueWithoutClampingAllowNegativeInfinity) {
  CSSPrimitiveValue* value =
      Create({-std::numeric_limits<double>::infinity(), UnitType::kPixels});

  EXPECT_TRUE(std::isinf(value->GetDoubleValueWithoutClamping()) &&
              value->GetDoubleValueWithoutClamping() < 0);
}

TEST_F(CSSPrimitiveValueTest, GetDoubleValueClampNaN) {
  CSSPrimitiveValue* value =
      Create({std::numeric_limits<double>::quiet_NaN(), UnitType::kPixels});
  EXPECT_EQ(std::numeric_limits<double>::max(), value->GetDoubleValue());
}

TEST_F(CSSPrimitiveValueTest, GetDoubleValueClampPositiveInfinity) {
  CSSPrimitiveValue* value =
      Create({std::numeric_limits<double>::infinity(), UnitType::kPixels});
  EXPECT_EQ(std::numeric_limits<double>::max(), value->GetDoubleValue());
}

TEST_F(CSSPrimitiveValueTest, GetDoubleValueClampNegativeInfinity) {
  CSSPrimitiveValue* value =
      Create({-std::numeric_limits<double>::infinity(), UnitType::kPixels});
  EXPECT_EQ(std::numeric_limits<double>::lowest(), value->GetDoubleValue());
}
}  // namespace
}  // namespace blink
