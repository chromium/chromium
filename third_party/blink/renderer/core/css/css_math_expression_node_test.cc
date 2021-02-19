/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/css_math_expression_node.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

void PrintTo(const CSSLengthArray& length_array, ::std::ostream* os) {
  for (double x : length_array.values)
    *os << x << ' ';
}

namespace {

void TestAccumulatePixelsAndPercent(
    const CSSToLengthConversionData& conversion_data,
    CSSMathExpressionNode* expression,
    float expected_pixels,
    float expected_percent) {
  scoped_refptr<const CalculationExpressionNode> value =
      expression->ToCalculationExpression(conversion_data);
  EXPECT_TRUE(value->IsLeaf());
  EXPECT_EQ(expected_pixels,
            To<CalculationExpressionLeafNode>(*value).Pixels());
  EXPECT_EQ(expected_percent,
            To<CalculationExpressionLeafNode>(*value).Percent());

  base::Optional<PixelsAndPercent> pixels_and_percent =
      expression->ToPixelsAndPercent(conversion_data);
  EXPECT_TRUE(pixels_and_percent.has_value());
  EXPECT_EQ(expected_pixels, pixels_and_percent->pixels);
  EXPECT_EQ(expected_percent, pixels_and_percent->percent);
}

CSSLengthArray& SetLengthArray(CSSLengthArray& length_array, String text) {
  for (double& x : length_array.values)
    x = 0;
  auto* property_set =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode);
  property_set->SetProperty(CSSPropertyID::kLeft, text, /* important */ false,
                            SecureContextMode::kInsecureContext);
  To<CSSPrimitiveValue>(property_set->GetPropertyCSSValue(CSSPropertyID::kLeft))
      ->AccumulateLengthArray(length_array);
  return length_array;
}

bool LengthArraysEqual(CSSLengthArray& a, CSSLengthArray& b) {
  for (wtf_size_t i = 0; i < CSSPrimitiveValue::kLengthUnitTypeCount; ++i) {
    if (a.values.at(i) != b.values.at(i))
      return false;
  }
  return true;
}

TEST(CSSCalculationValue, AccumulatePixelsAndPercent) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetEffectiveZoom(5);
  CSSToLengthConversionData conversion_data(style.get(), style.get(), nullptr,
                                            style->EffectiveZoom());

  TestAccumulatePixelsAndPercent(
      conversion_data,
      CSSMathExpressionNumericLiteral::Create(
          CSSNumericLiteralValue::Create(10,
                                         CSSPrimitiveValue::UnitType::kPixels),
          true),
      50, 0);

  TestAccumulatePixelsAndPercent(
      conversion_data,
      CSSMathExpressionBinaryOperation::Create(
          CSSMathExpressionNumericLiteral::Create(
              CSSNumericLiteralValue::Create(
                  10, CSSPrimitiveValue::UnitType::kPixels),
              true),
          CSSMathExpressionNumericLiteral::Create(
              CSSNumericLiteralValue::Create(
                  20, CSSPrimitiveValue::UnitType::kPixels),
              true),
          CSSMathOperator::kAdd),
      150, 0);

  TestAccumulatePixelsAndPercent(
      conversion_data,
      CSSMathExpressionBinaryOperation::Create(
          CSSMathExpressionNumericLiteral::Create(
              CSSNumericLiteralValue::Create(
                  1, CSSPrimitiveValue::UnitType::kInches),
              true),
          CSSMathExpressionNumericLiteral::Create(
              CSSNumericLiteralValue::Create(
                  2, CSSPrimitiveValue::UnitType::kNumber),
              true),
          CSSMathOperator::kMultiply),
      960, 0);

  TestAccumulatePixelsAndPercent(
      conversion_data,
      CSSMathExpressionBinaryOperation::Create(
          CSSMathExpressionBinaryOperation::Create(
              CSSMathExpressionNumericLiteral::Create(
                  CSSNumericLiteralValue::Create(
                      50, CSSPrimitiveValue::UnitType::kPixels),
                  true),
              CSSMathExpressionNumericLiteral::Create(
                  CSSNumericLiteralValue::Create(
                      0.25, CSSPrimitiveValue::UnitType::kNumber),
                  false),
              CSSMathOperator::kMultiply),
          CSSMathExpressionBinaryOperation::Create(
              CSSMathExpressionNumericLiteral::Create(
                  CSSNumericLiteralValue::Create(
                      20, CSSPrimitiveValue::UnitType::kPixels),
                  true),
              CSSMathExpressionNumericLiteral::Create(
                  CSSNumericLiteralValue::Create(
                      40, CSSPrimitiveValue::UnitType::kPercentage),
                  false),
              CSSMathOperator::kSubtract),
          CSSMathOperator::kSubtract),
      -37.5, 40);
}

TEST(CSSCalculationValue, RefCount) {
  scoped_refptr<CalculationValue> calc =
      CalculationValue::Create(PixelsAndPercent(1, 2), kValueRangeAll);

  // FIXME: Test the Length construction without using the ref count value.

  EXPECT_TRUE(calc->HasOneRef());
  {
    Length length_a(calc);
    EXPECT_FALSE(calc->HasOneRef());

    Length length_b;
    length_b = length_a;

    Length length_c(calc);
    length_c = length_a;

    Length length_d(
        CalculationValue::Create(PixelsAndPercent(1, 2), kValueRangeAll));
    length_d = length_a;
  }
  EXPECT_TRUE(calc->HasOneRef());
}

TEST(CSSCalculationValue, AddToLengthUnitValues) {
  CSSLengthArray expectation, actual;
  EXPECT_TRUE(LengthArraysEqual(expectation, SetLengthArray(actual, "0")));

  expectation.values.at(CSSPrimitiveValue::kUnitTypePixels) = 10;
  EXPECT_TRUE(LengthArraysEqual(expectation, SetLengthArray(actual, "10px")));

  expectation.values.at(CSSPrimitiveValue::kUnitTypePixels) = 0;
  expectation.values.at(CSSPrimitiveValue::kUnitTypePercentage) = 20;
  EXPECT_TRUE(LengthArraysEqual(expectation, SetLengthArray(actual, "20%")));

  expectation.values.at(CSSPrimitiveValue::kUnitTypePixels) = 30;
  expectation.values.at(CSSPrimitiveValue::kUnitTypePercentage) = -40;
  EXPECT_TRUE(LengthArraysEqual(expectation,
                                SetLengthArray(actual, "calc(30px - 40%)")));

  expectation.values.at(CSSPrimitiveValue::kUnitTypePixels) = 90;
  expectation.values.at(CSSPrimitiveValue::kUnitTypePercentage) = 10;
  EXPECT_TRUE(LengthArraysEqual(
      expectation, SetLengthArray(actual, "calc(1in + 10% - 6px)")));

  expectation.values.at(CSSPrimitiveValue::kUnitTypePixels) = 15;
  expectation.values.at(CSSPrimitiveValue::kUnitTypeFontSize) = 20;
  expectation.values.at(CSSPrimitiveValue::kUnitTypePercentage) = -40;
  EXPECT_TRUE(LengthArraysEqual(
      expectation,
      SetLengthArray(
          actual, "calc((1 * 2) * (5px + 20em / 2) - 80% / (3 - 1) + 5px)")));
}

}  // anonymous namespace

}  // namespace blink
