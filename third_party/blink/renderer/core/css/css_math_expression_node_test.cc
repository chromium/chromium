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

#include <algorithm>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

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
  EXPECT_TRUE(value->IsPixelsAndPercent());
  EXPECT_EQ(expected_pixels,
            To<CalculationExpressionPixelsAndPercentNode>(*value).Pixels());
  EXPECT_EQ(expected_percent,
            To<CalculationExpressionPixelsAndPercentNode>(*value).Percent());

  absl::optional<PixelsAndPercent> pixels_and_percent =
      expression->ToPixelsAndPercent(conversion_data);
  EXPECT_TRUE(pixels_and_percent.has_value());
  EXPECT_EQ(expected_pixels, pixels_and_percent->pixels);
  EXPECT_EQ(expected_percent, pixels_and_percent->percent);
}

bool AccumulateLengthArray(String text, CSSLengthArray& length_array) {
  auto* property_set =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLQuirksMode);
  property_set->ParseAndSetProperty(CSSPropertyID::kLeft, text,
                                    /* important */ false,
                                    SecureContextMode::kInsecureContext);
  return To<CSSPrimitiveValue>(
             property_set->GetPropertyCSSValue(CSSPropertyID::kLeft))
      ->AccumulateLengthArray(length_array);
}

CSSLengthArray& SetLengthArray(String text, CSSLengthArray& length_array) {
  std::fill(length_array.values.begin(), length_array.values.end(), 0);
  AccumulateLengthArray(text, length_array);
  return length_array;
}

TEST(CSSCalculationValue, AccumulatePixelsAndPercent) {
  ComputedStyleBuilder builder(*ComputedStyle::CreateInitialStyleSingleton());
  builder.SetEffectiveZoom(5);
  scoped_refptr<const ComputedStyle> style = builder.TakeStyle();
  CSSToLengthConversionData conversion_data(
      style.get(), style.get(), style.get(), nullptr,
      CSSToLengthConversionData::ContainerSizes(), style->EffectiveZoom());

  TestAccumulatePixelsAndPercent(
      conversion_data,
      CSSMathExpressionNumericLiteral::Create(CSSNumericLiteralValue::Create(
          10, CSSPrimitiveValue::UnitType::kPixels)),
      50, 0);

  TestAccumulatePixelsAndPercent(
      conversion_data,
      CSSMathExpressionOperation::CreateArithmeticOperation(
          CSSMathExpressionNumericLiteral::Create(
              CSSNumericLiteralValue::Create(
                  10, CSSPrimitiveValue::UnitType::kPixels)),
          CSSMathExpressionNumericLiteral::Create(
              CSSNumericLiteralValue::Create(
                  20, CSSPrimitiveValue::UnitType::kPixels)),
          CSSMathOperator::kAdd),
      150, 0);

  TestAccumulatePixelsAndPercent(
      conversion_data,
      CSSMathExpressionOperation::CreateArithmeticOperation(
          CSSMathExpressionNumericLiteral::Create(
              CSSNumericLiteralValue::Create(
                  1, CSSPrimitiveValue::UnitType::kInches)),
          CSSMathExpressionNumericLiteral::Create(
              CSSNumericLiteralValue::Create(
                  2, CSSPrimitiveValue::UnitType::kNumber)),
          CSSMathOperator::kMultiply),
      960, 0);

  TestAccumulatePixelsAndPercent(
      conversion_data,
      CSSMathExpressionOperation::CreateArithmeticOperation(
          CSSMathExpressionOperation::CreateArithmeticOperation(
              CSSMathExpressionNumericLiteral::Create(
                  CSSNumericLiteralValue::Create(
                      50, CSSPrimitiveValue::UnitType::kPixels)),
              CSSMathExpressionNumericLiteral::Create(
                  CSSNumericLiteralValue::Create(
                      0.25, CSSPrimitiveValue::UnitType::kNumber)),
              CSSMathOperator::kMultiply),
          CSSMathExpressionOperation::CreateArithmeticOperation(
              CSSMathExpressionNumericLiteral::Create(
                  CSSNumericLiteralValue::Create(
                      20, CSSPrimitiveValue::UnitType::kPixels)),
              CSSMathExpressionNumericLiteral::Create(
                  CSSNumericLiteralValue::Create(
                      40, CSSPrimitiveValue::UnitType::kPercentage)),
              CSSMathOperator::kSubtract),
          CSSMathOperator::kSubtract),
      -37.5, 40);
}

TEST(CSSCalculationValue, RefCount) {
  scoped_refptr<const CalculationValue> calc = CalculationValue::Create(
      PixelsAndPercent(1, 2), Length::ValueRange::kAll);

  // FIXME: Test the Length construction without using the ref count value.

  EXPECT_TRUE(calc->HasOneRef());
  {
    Length length_a(calc);
    EXPECT_FALSE(calc->HasOneRef());

    Length length_b;
    length_b = length_a;

    Length length_c(calc);
    length_c = length_a;

    Length length_d(CalculationValue::Create(PixelsAndPercent(1, 2),
                                             Length::ValueRange::kAll));
    length_d = length_a;
  }
  EXPECT_TRUE(calc->HasOneRef());
}

TEST(CSSCalculationValue, AddToLengthUnitValues) {
  CSSLengthArray expectation, actual;
  EXPECT_EQ(expectation.values, SetLengthArray("0", actual).values);

  expectation.values.at(CSSPrimitiveValue::kUnitTypePixels) = 10;
  EXPECT_EQ(expectation.values, SetLengthArray("10px", actual).values);

  expectation.values.at(CSSPrimitiveValue::kUnitTypePixels) = 0;
  expectation.values.at(CSSPrimitiveValue::kUnitTypePercentage) = 20;
  EXPECT_EQ(expectation.values, SetLengthArray("20%", actual).values);

  expectation.values.at(CSSPrimitiveValue::kUnitTypePixels) = 30;
  expectation.values.at(CSSPrimitiveValue::kUnitTypePercentage) = -40;
  EXPECT_EQ(expectation.values,
            SetLengthArray("calc(30px - 40%)", actual).values);

  expectation.values.at(CSSPrimitiveValue::kUnitTypePixels) = 90;
  expectation.values.at(CSSPrimitiveValue::kUnitTypePercentage) = 10;
  EXPECT_EQ(expectation.values,
            SetLengthArray("calc(1in + 10% - 6px)", actual).values);

  expectation.values.at(CSSPrimitiveValue::kUnitTypePixels) = 15;
  expectation.values.at(CSSPrimitiveValue::kUnitTypeFontSize) = 20;
  expectation.values.at(CSSPrimitiveValue::kUnitTypePercentage) = -40;
  EXPECT_EQ(
      expectation.values,
      SetLengthArray("calc((1 * 2) * (5px + 20em / 2) - 80% / (3 - 1) + 5px)",
                     actual)
          .values);
}

TEST(CSSCalculationValue, CSSLengthArrayUnits) {
  ScopedCSSViewportUnits4ForTest scoped_viewport_units(true);

  CSSLengthArray unused;

  // Supported units:
  EXPECT_TRUE(AccumulateLengthArray("1px", unused));
  EXPECT_TRUE(AccumulateLengthArray("1%", unused));
  EXPECT_TRUE(AccumulateLengthArray("1em", unused));
  EXPECT_TRUE(AccumulateLengthArray("1ex", unused));
  EXPECT_TRUE(AccumulateLengthArray("1rem", unused));
  EXPECT_TRUE(AccumulateLengthArray("1ch", unused));
  EXPECT_TRUE(AccumulateLengthArray("1vw", unused));
  EXPECT_TRUE(AccumulateLengthArray("1vh", unused));
  EXPECT_TRUE(AccumulateLengthArray("1vi", unused));
  EXPECT_TRUE(AccumulateLengthArray("1vb", unused));
  EXPECT_TRUE(AccumulateLengthArray("1vmin", unused));
  EXPECT_TRUE(AccumulateLengthArray("1vmax", unused));

  // Unsupported units:
  EXPECT_FALSE(AccumulateLengthArray("1svw", unused));
  EXPECT_FALSE(AccumulateLengthArray("1svh", unused));
  EXPECT_FALSE(AccumulateLengthArray("1svi", unused));
  EXPECT_FALSE(AccumulateLengthArray("1svb", unused));
  EXPECT_FALSE(AccumulateLengthArray("1svmin", unused));
  EXPECT_FALSE(AccumulateLengthArray("1svmax", unused));
  EXPECT_FALSE(AccumulateLengthArray("1lvw", unused));
  EXPECT_FALSE(AccumulateLengthArray("1lvh", unused));
  EXPECT_FALSE(AccumulateLengthArray("1lvi", unused));
  EXPECT_FALSE(AccumulateLengthArray("1lvb", unused));
  EXPECT_FALSE(AccumulateLengthArray("1lvmin", unused));
  EXPECT_FALSE(AccumulateLengthArray("1lvmax", unused));
  EXPECT_FALSE(AccumulateLengthArray("1dvw", unused));
  EXPECT_FALSE(AccumulateLengthArray("1dvh", unused));
  EXPECT_FALSE(AccumulateLengthArray("1dvi", unused));
  EXPECT_FALSE(AccumulateLengthArray("1dvb", unused));
  EXPECT_FALSE(AccumulateLengthArray("1dvmin", unused));
  EXPECT_FALSE(AccumulateLengthArray("1dvmax", unused));
  EXPECT_FALSE(AccumulateLengthArray("1cqw", unused));
  EXPECT_FALSE(AccumulateLengthArray("1cqh", unused));
  EXPECT_FALSE(AccumulateLengthArray("1cqi", unused));
  EXPECT_FALSE(AccumulateLengthArray("1cqb", unused));
  EXPECT_FALSE(AccumulateLengthArray("1cqmin", unused));
  EXPECT_FALSE(AccumulateLengthArray("1cqmax", unused));

  EXPECT_TRUE(AccumulateLengthArray("calc(1em + calc(1ex + 1px))", unused));
  EXPECT_FALSE(AccumulateLengthArray("calc(1dvh + calc(1ex + 1px))", unused));
  EXPECT_FALSE(AccumulateLengthArray("calc(1em + calc(1dvh + 1px))", unused));
  EXPECT_FALSE(AccumulateLengthArray("calc(1em + calc(1ex + 1dvh))", unused));
}

TEST(CSSMathExpressionNode, TestParseDeeplyNestedExpression) {
  enum Kind {
    kCalc,
    kMin,
    kMax,
    kClamp,
  };

  // Ref: https://bugs.chromium.org/p/chromium/issues/detail?id=1211283
  const struct TestCase {
    const Kind kind;
    const int nest_num;
    const bool expected;
  } test_cases[] = {
      {kCalc, 1, true},
      {kCalc, 10, true},
      {kCalc, kMaxExpressionDepth - 1, true},
      {kCalc, kMaxExpressionDepth, false},
      {kCalc, kMaxExpressionDepth + 1, false},
      {kMin, 1, true},
      {kMin, 10, true},
      {kMin, kMaxExpressionDepth - 1, true},
      {kMin, kMaxExpressionDepth, false},
      {kMin, kMaxExpressionDepth + 1, false},
      {kMax, 1, true},
      {kMax, 10, true},
      {kMax, kMaxExpressionDepth - 1, true},
      {kMax, kMaxExpressionDepth, false},
      {kMax, kMaxExpressionDepth + 1, false},
      {kClamp, 1, true},
      {kClamp, 10, true},
      {kClamp, kMaxExpressionDepth - 1, true},
      {kClamp, kMaxExpressionDepth, false},
      {kClamp, kMaxExpressionDepth + 1, false},
  };

  for (const auto& test_case : test_cases) {
    std::stringstream ss;

    // Make nested expression as follows:
    // calc(1px + calc(1px + calc(1px)))
    // min(1px, 1px + min(1px, 1px + min(1px, 1px)))
    // max(1px, 1px + max(1px, 1px + max(1px, 1px)))
    // clamp(1px, 1px, 1px + clamp(1px, 1px, 1px + clamp(1px, 1px, 1px)))
    for (int i = 0; i < test_case.nest_num; i++) {
      if (i)
        ss << " + ";
      switch (test_case.kind) {
        case kCalc:
          ss << "calc(1px";
          break;
        case kMin:
          ss << "min(1px, 1px";
          break;
        case kMax:
          ss << "max(1px, 1px";
          break;
        case kClamp:
          ss << "clamp(1px, 1px, 1px";
          break;
      }
    }
    for (int i = 0; i < test_case.nest_num; i++) {
      ss << ")";
    }

    CSSTokenizer tokenizer(String(ss.str().c_str()));
    const auto tokens = tokenizer.TokenizeToEOF();
    const CSSParserTokenRange range(tokens);
    const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    const CSSMathExpressionNode* res = CSSMathExpressionNode::ParseMathFunction(
        CSSValueID::kCalc, range, *context, kCSSAnchorQueryTypesNone);

    if (test_case.expected) {
      EXPECT_TRUE(res);
    } else {
      EXPECT_FALSE(res);
    }
  }
}

}  // anonymous namespace

}  // namespace blink
