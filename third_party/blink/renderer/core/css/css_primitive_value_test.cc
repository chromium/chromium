// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_primitive_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
namespace {

class CSSPrimitiveValueTest : public PageTestBase {
 public:
  const CSSPrimitiveValue* ParseValue(const char* text) {
    const CSSPrimitiveValue* value = To<CSSPrimitiveValue>(
        css_test_helpers::ParseValue(GetDocument(), "<length>", text));
    DCHECK(value);
    return value;
  }

  bool HasContainerRelativeUnits(const char* text) {
    return ParseValue(text)->HasContainerRelativeUnits();
  }

  bool HasStaticViewportUnits(const char* text) {
    const CSSPrimitiveValue* value = ParseValue(text);
    CSSPrimitiveValue::LengthTypeFlags length_type_flags;
    value->AccumulateLengthUnitTypes(length_type_flags);
    return CSSPrimitiveValue::HasStaticViewportUnits(length_type_flags);
  }

  bool HasDynamicViewportUnits(const char* text) {
    const CSSPrimitiveValue* value = ParseValue(text);
    CSSPrimitiveValue::LengthTypeFlags length_type_flags;
    value->AccumulateLengthUnitTypes(length_type_flags);
    return CSSPrimitiveValue::HasDynamicViewportUnits(length_type_flags);
  }

  CSSPrimitiveValueTest() = default;
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
  return CSSMathFunctionValue::Create(
      CSSMathExpressionOperation::CreateArithmeticOperation(
          CSSMathExpressionNumericLiteral::Create(Create(a)),
          CSSMathExpressionNumericLiteral::Create(Create(b)),
          CSSMathOperator::kAdd));
}

CSSPrimitiveValue* CreateNonNegativeSubtraction(UnitValue a, UnitValue b) {
  return CSSMathFunctionValue::Create(
      CSSMathExpressionOperation::CreateArithmeticOperation(
          CSSMathExpressionNumericLiteral::Create(Create(a)),
          CSSMathExpressionNumericLiteral::Create(Create(b)),
          CSSMathOperator::kSubtract),
      CSSPrimitiveValue::ValueRange::kNonNegative);
}

UnitType ToCanonicalUnit(CSSPrimitiveValue::UnitType unit) {
  return CSSPrimitiveValue::CanonicalUnitTypeForCategory(
      CSSPrimitiveValue::UnitTypeToUnitCategory(unit));
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
  EXPECT_EQ(0.0, CreateNonNegativeSubtraction(a, b)->ComputeDegrees(
                     CSSToLengthConversionData()));
}

TEST_F(CSSPrimitiveValueTest, IsResolution) {
  EXPECT_FALSE(Create({5.0, UnitType::kNumber})->IsResolution());
  EXPECT_FALSE(Create({5.0, UnitType::kDegrees})->IsResolution());
  EXPECT_TRUE(Create({5.0, UnitType::kDotsPerPixel})->IsResolution());
  EXPECT_TRUE(Create({5.0, UnitType::kX})->IsResolution());
  EXPECT_TRUE(Create({5.0, UnitType::kDotsPerInch})->IsResolution());
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
  EXPECT_EQ(0.0, value->ComputeLength<double>(conversion_data));
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
  EXPECT_EQ(0.0, length.Percent());
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
  EXPECT_EQ(0.0, value->GetDoubleValue());
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

TEST_F(CSSPrimitiveValueTest, TestCanonicalizingNumberUnitCategory) {
  UnitType canonicalized_from_num = ToCanonicalUnit(UnitType::kNumber);
  EXPECT_EQ(canonicalized_from_num, UnitType::kNumber);

  UnitType canonicalized_from_int = ToCanonicalUnit(UnitType::kInteger);
  EXPECT_EQ(canonicalized_from_int, UnitType::kNumber);
}

TEST_F(CSSPrimitiveValueTest, HasContainerRelativeUnits) {
  EXPECT_TRUE(HasContainerRelativeUnits("1cqw"));
  EXPECT_TRUE(HasContainerRelativeUnits("1cqh"));
  EXPECT_TRUE(HasContainerRelativeUnits("1cqi"));
  EXPECT_TRUE(HasContainerRelativeUnits("1cqb"));
  EXPECT_TRUE(HasContainerRelativeUnits("1cqmin"));
  EXPECT_TRUE(HasContainerRelativeUnits("1cqmax"));
  EXPECT_TRUE(HasContainerRelativeUnits("calc(1px + 1cqw)"));
  EXPECT_TRUE(HasContainerRelativeUnits("min(1px, 1cqw)"));

  EXPECT_FALSE(HasContainerRelativeUnits("1px"));
  EXPECT_FALSE(HasContainerRelativeUnits("1em"));
  EXPECT_FALSE(HasContainerRelativeUnits("1vh"));
  EXPECT_FALSE(HasContainerRelativeUnits("1svh"));
  EXPECT_FALSE(HasContainerRelativeUnits("calc(1px + 1px)"));
  EXPECT_FALSE(HasContainerRelativeUnits("calc(1px + 1em)"));
  EXPECT_FALSE(HasContainerRelativeUnits("calc(1px + 1svh)"));
}

TEST_F(CSSPrimitiveValueTest, HasStaticViewportUnits) {
  // v*
  EXPECT_TRUE(HasStaticViewportUnits("1vw"));
  EXPECT_TRUE(HasStaticViewportUnits("1vh"));
  EXPECT_TRUE(HasStaticViewportUnits("1vi"));
  EXPECT_TRUE(HasStaticViewportUnits("1vb"));
  EXPECT_TRUE(HasStaticViewportUnits("1vmin"));
  EXPECT_TRUE(HasStaticViewportUnits("1vmax"));
  EXPECT_TRUE(HasStaticViewportUnits("calc(1px + 1vw)"));
  EXPECT_TRUE(HasStaticViewportUnits("min(1px, 1vw)"));
  EXPECT_FALSE(HasStaticViewportUnits("1px"));
  EXPECT_FALSE(HasStaticViewportUnits("1em"));
  EXPECT_FALSE(HasStaticViewportUnits("1dvh"));
  EXPECT_FALSE(HasStaticViewportUnits("calc(1px + 1px)"));
  EXPECT_FALSE(HasStaticViewportUnits("calc(1px + 1em)"));
  EXPECT_FALSE(HasStaticViewportUnits("calc(1px + 1dvh)"));

  // sv*
  EXPECT_TRUE(HasStaticViewportUnits("1svw"));
  EXPECT_TRUE(HasStaticViewportUnits("1svh"));
  EXPECT_TRUE(HasStaticViewportUnits("1svi"));
  EXPECT_TRUE(HasStaticViewportUnits("1svb"));
  EXPECT_TRUE(HasStaticViewportUnits("1svmin"));
  EXPECT_TRUE(HasStaticViewportUnits("1svmax"));
  EXPECT_TRUE(HasStaticViewportUnits("calc(1px + 1svw)"));
  EXPECT_TRUE(HasStaticViewportUnits("min(1px, 1svw)"));
  EXPECT_FALSE(HasStaticViewportUnits("1px"));
  EXPECT_FALSE(HasStaticViewportUnits("1em"));
  EXPECT_FALSE(HasStaticViewportUnits("1dvh"));
  EXPECT_FALSE(HasStaticViewportUnits("calc(1px + 1px)"));
  EXPECT_FALSE(HasStaticViewportUnits("calc(1px + 1em)"));
  EXPECT_FALSE(HasStaticViewportUnits("calc(1px + 1dvh)"));

  // lv*
  EXPECT_TRUE(HasStaticViewportUnits("1lvw"));
  EXPECT_TRUE(HasStaticViewportUnits("1lvh"));
  EXPECT_TRUE(HasStaticViewportUnits("1lvi"));
  EXPECT_TRUE(HasStaticViewportUnits("1lvb"));
  EXPECT_TRUE(HasStaticViewportUnits("1lvmin"));
  EXPECT_TRUE(HasStaticViewportUnits("1lvmax"));
  EXPECT_TRUE(HasStaticViewportUnits("calc(1px + 1lvw)"));
  EXPECT_TRUE(HasStaticViewportUnits("min(1px, 1lvw)"));
  EXPECT_FALSE(HasStaticViewportUnits("1px"));
  EXPECT_FALSE(HasStaticViewportUnits("1em"));
  EXPECT_FALSE(HasStaticViewportUnits("1dvh"));
  EXPECT_FALSE(HasStaticViewportUnits("calc(1px + 1px)"));
  EXPECT_FALSE(HasStaticViewportUnits("calc(1px + 1em)"));
  EXPECT_FALSE(HasStaticViewportUnits("calc(1px + 1dvh)"));
}

TEST_F(CSSPrimitiveValueTest, HasDynamicViewportUnits) {
  // dv*
  EXPECT_TRUE(HasDynamicViewportUnits("1dvw"));
  EXPECT_TRUE(HasDynamicViewportUnits("1dvh"));
  EXPECT_TRUE(HasDynamicViewportUnits("1dvi"));
  EXPECT_TRUE(HasDynamicViewportUnits("1dvb"));
  EXPECT_TRUE(HasDynamicViewportUnits("1dvmin"));
  EXPECT_TRUE(HasDynamicViewportUnits("1dvmax"));
  EXPECT_TRUE(HasDynamicViewportUnits("calc(1px + 1dvw)"));
  EXPECT_TRUE(HasDynamicViewportUnits("min(1px, 1dvw)"));
  EXPECT_FALSE(HasDynamicViewportUnits("1px"));
  EXPECT_FALSE(HasDynamicViewportUnits("1em"));
  EXPECT_FALSE(HasDynamicViewportUnits("1svh"));
  EXPECT_FALSE(HasDynamicViewportUnits("calc(1px + 1px)"));
  EXPECT_FALSE(HasDynamicViewportUnits("calc(1px + 1em)"));
  EXPECT_FALSE(HasDynamicViewportUnits("calc(1px + 1svh)"));
}

TEST_F(CSSPrimitiveValueTest, ComputeMethodsWithLengthResolver) {
  {
    auto* pxs = CSSMathExpressionNumericLiteral::Create(
        12.0, CSSPrimitiveValue::UnitType::kPixels);
    auto* ems = CSSMathExpressionNumericLiteral::Create(
        1.0, CSSPrimitiveValue::UnitType::kEms);
    auto* subtraction = CSSMathExpressionOperation::CreateArithmeticOperation(
        pxs, ems, CSSMathOperator::kSubtract);
    auto* sign = CSSMathExpressionOperation::CreateSignRelatedFunction(
        {subtraction}, CSSValueID::kSign);
    auto* degs = CSSMathExpressionNumericLiteral::Create(
        10.0, CSSPrimitiveValue::UnitType::kDegrees);
    auto* expression = CSSMathExpressionOperation::CreateArithmeticOperation(
        sign, degs, CSSMathOperator::kMultiply);
    CSSPrimitiveValue* value = CSSMathFunctionValue::Create(expression);

    Font font;
    CSSToLengthConversionData length_resolver = CSSToLengthConversionData();
    length_resolver.SetFontSizes(
        CSSToLengthConversionData::FontSizes(10.0f, 10.0f, &font, 1.0f));
    EXPECT_EQ(10.0, value->ComputeDegrees(length_resolver));
    EXPECT_EQ("calc(sign(-1em + 12px) * 10deg)", value->CustomCSSText());
  }
}

TEST_F(CSSPrimitiveValueTest, ContainerProgressTreeScope) {
  ScopedCSSProgressNotationForTest scoped_feature(true);
  const CSSValue* value = css_test_helpers::ParseValue(
      GetDocument(), "<number>",
      "container-progress(width of my-container from 0px to 1px)");
  ASSERT_TRUE(value);

  const CSSValue& scoped_value = value->EnsureScopedValue(&GetDocument());
  EXPECT_NE(value, &scoped_value);
  EXPECT_TRUE(scoped_value.IsScopedValue());
  // Don't crash:
  const CSSValue& scoped_value2 =
      scoped_value.EnsureScopedValue(&GetDocument());
  EXPECT_TRUE(scoped_value2.IsScopedValue());
  EXPECT_EQ(&scoped_value, &scoped_value2);
}

TEST_F(CSSPrimitiveValueTest, CSSPrimitiveValueOperations) {
  auto* numeric_percentage = CSSNumericLiteralValue::Create(
      10, CSSPrimitiveValue::UnitType::kPercentage);
  auto* numeric_number =
      CSSNumericLiteralValue::Create(10, CSSPrimitiveValue::UnitType::kNumber);
  auto* node_10_px = CSSMathExpressionNumericLiteral::Create(
      10, CSSPrimitiveValue::UnitType::kPixels);
  auto* node_20_em = CSSMathExpressionNumericLiteral::Create(
      20, CSSPrimitiveValue::UnitType::kEms);
  auto* node_subtract = CSSMathExpressionOperation::CreateArithmeticOperation(
      node_10_px, node_20_em, CSSMathOperator::kSubtract);
  auto* node_sign = CSSMathExpressionOperation::CreateSignRelatedFunction(
      {node_subtract}, CSSValueID::kSign);
  auto* function = CSSMathFunctionValue::Create(node_sign);
  EXPECT_EQ(function->Multiply(1, CSSPrimitiveValue::UnitType::kPixels)
                ->Add(10, CSSPrimitiveValue::UnitType::kPixels)
                ->CustomCSSText(),
            "calc(10px + sign(-20em + 10px) * 1px)");
  EXPECT_EQ(function->MultiplyBy(10, CSSPrimitiveValue::UnitType::kNumber)
                ->CustomCSSText(),
            "calc(10 * sign(-20em + 10px))");
  EXPECT_EQ(function->MultiplyBy(1, CSSPrimitiveValue::UnitType::kPixels)
                ->Subtract(*numeric_percentage)
                ->CustomCSSText(),
            "calc(-10% + 1px * sign(-20em + 10px))");
  EXPECT_EQ(function->Divide(20, CSSPrimitiveValue::UnitType::kNumber)
                ->CustomCSSText(),
            "calc(sign(-20em + 10px) / 20)");
  EXPECT_EQ(function->Subtract(*function)->CustomCSSText(),
            "calc(sign(-20em + 10px) - sign(-20em + 10px))");
  EXPECT_EQ(
      numeric_percentage->SubtractFrom(10, CSSPrimitiveValue::UnitType::kPixels)
          ->CustomCSSText(),
      "calc(-10% + 10px)");
  EXPECT_EQ(numeric_number->Subtract(10, CSSPrimitiveValue::UnitType::kNumber)
                ->CustomCSSText(),
            "0");
}

TEST_F(CSSPrimitiveValueTest, ComputeValueToCanonicalUnit) {
  CSSNumericLiteralValue* numeric_percentage = CSSNumericLiteralValue::Create(
      10, CSSPrimitiveValue::UnitType::kPercentage);
  CSSMathExpressionNode* node_20_px = CSSMathExpressionNumericLiteral::Create(
      20, CSSPrimitiveValue::UnitType::kPixels);
  CSSMathExpressionNode* node_2_em = CSSMathExpressionNumericLiteral::Create(
      2, CSSPrimitiveValue::UnitType::kEms);
  CSSMathExpressionNode* node_sub =
      CSSMathExpressionOperation::CreateArithmeticOperation(
          node_20_px, node_2_em, CSSMathOperator::kSubtract);
  auto* function = CSSMathFunctionValue::Create(node_sub);

  Font font;
  CSSToLengthConversionData length_resolver = CSSToLengthConversionData();
  length_resolver.SetFontSizes(
      CSSToLengthConversionData::FontSizes(10.0f, 10.0f, &font, 1.0f));

  EXPECT_EQ(function->ComputeValueInCanonicalUnit(length_resolver), 0);
  EXPECT_EQ(numeric_percentage->ComputeValueInCanonicalUnit(length_resolver),
            10);
}

}  // namespace
}  // namespace blink
