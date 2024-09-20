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
#include "third_party/blink/renderer/core/css/css_length_resolver.h"
#include "third_party/blink/renderer/core/css/css_math_operator.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

void PrintTo(const CSSLengthArray& length_array, ::std::ostream* os) {
  for (double x : length_array.values) {
    *os << x << ' ';
  }
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

  std::optional<PixelsAndPercent> pixels_and_percent =
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
  ComputedStyleBuilder builder(*ComputedStyle::GetInitialStyleSingleton());
  builder.SetEffectiveZoom(5);
  const ComputedStyle* style = builder.TakeStyle();
  CSSToLengthConversionData::Flags ignored_flags = 0;
  CSSToLengthConversionData conversion_data(
      *style, style, style, CSSToLengthConversionData::ViewportSize(nullptr),
      CSSToLengthConversionData::ContainerSizes(),
      CSSToLengthConversionData::AnchorData(), style->EffectiveZoom(),
      ignored_flags);

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
      PixelsAndPercent(1, 2, /*has_explicit_pixels=*/true,
                       /*has_explicit_percent=*/true),
      Length::ValueRange::kAll);

  // FIXME: Test the Length construction without using the ref count value.

  EXPECT_TRUE(calc->HasOneRef());
  {
    Length length_a(calc);
    EXPECT_FALSE(calc->HasOneRef());

    Length length_b;
    length_b = length_a;

    Length length_c(calc);
    length_c = length_a;

    Length length_d(CalculationValue::Create(
        PixelsAndPercent(1, 2, /*has_explicit_pixels=*/true,
                         /*has_explicit_percent=*/true),
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

using Flag = CSSMathExpressionNode::Flag;
using Flags = CSSMathExpressionNode::Flags;

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
      if (i) {
        ss << " + ";
      }
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

    std::string str = ss.str();
    CSSParserTokenStream stream(str.c_str());
    const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    const CSSMathExpressionNode* res = CSSMathExpressionNode::ParseMathFunction(
        CSSValueID::kCalc, stream, *context, Flags({Flag::AllowPercent}),
        kCSSAnchorQueryTypesNone);

    if (test_case.expected) {
      ASSERT_TRUE(res);
      EXPECT_TRUE(!res->HasPercentage());
    } else {
      EXPECT_FALSE(res);
    }
  }
}

TEST(CSSMathExpressionNode, TestSteppedValueFunctions) {
  const struct TestCase {
    const std::string input;
    const double output;
  } test_cases[] = {
      {"round(10, 10)", 10.0f},
      {"calc(round(up, 101, 10))", 110.0f},
      {"calc(round(down, 106, 10))", 100.0f},
      {"mod(18,5)", 3.0f},
      {"rem(18,5)", 3.0f},
  };

  for (const auto& test_case : test_cases) {
    CSSParserTokenStream stream(test_case.input.c_str());
    const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    const CSSMathExpressionNode* res = CSSMathExpressionNode::ParseMathFunction(
        CSSValueID::kCalc, stream, *context, Flags({Flag::AllowPercent}),
        kCSSAnchorQueryTypesNone);
    EXPECT_EQ(res->DoubleValue(), test_case.output);
    CSSToLengthConversionData resolver{};
    scoped_refptr<const CalculationExpressionNode> node =
        res->ToCalculationExpression(resolver);
    EXPECT_EQ(node->Evaluate(FLT_MAX, {}), test_case.output);
    EXPECT_TRUE(!res->HasPercentage());
  }
}

TEST(CSSMathExpressionNode, TestSteppedValueFunctionsToCalculationExpression) {
  const struct TestCase {
    const CSSMathOperator op;
    const double output;
  } test_cases[] = {
      {CSSMathOperator::kRoundNearest, 10}, {CSSMathOperator::kRoundUp, 10},
      {CSSMathOperator::kRoundDown, 10},    {CSSMathOperator::kRoundToZero, 10},
      {CSSMathOperator::kMod, 0},           {CSSMathOperator::kRem, 0}};

  for (const auto& test_case : test_cases) {
    CSSMathExpressionOperation::Operands operands{
        CSSMathExpressionNumericLiteral::Create(
            10, CSSPrimitiveValue::UnitType::kNumber),
        CSSMathExpressionNumericLiteral::Create(
            10, CSSPrimitiveValue::UnitType::kNumber)};
    const auto* operation = MakeGarbageCollected<CSSMathExpressionOperation>(
        kCalcNumber, std::move(operands), test_case.op);
    CSSToLengthConversionData resolver{};
    scoped_refptr<const CalculationExpressionNode> node =
        operation->ToCalculationExpression(resolver);
    EXPECT_EQ(node->Evaluate(FLT_MAX, {}), test_case.output);
    const CSSMathExpressionNode* css_node =
        CSSMathExpressionOperation::Create(*node);
    EXPECT_NE(css_node, nullptr);
  }
}

TEST(CSSMathExpressionNode, TestSteppedValueFunctionsSerialization) {
  const struct TestCase {
    const String input;
  } test_cases[] = {
      {"round(10%, 10%)"},       {"round(up, 10%, 10%)"},
      {"round(down, 10%, 10%)"}, {"round(to-zero, 10%, 10%)"},
      {"mod(10%, 10%)"},         {"rem(10%, 10%)"},
  };

  for (const auto& test_case : test_cases) {
    CSSParserTokenStream stream(test_case.input);
    const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    const CSSMathExpressionNode* res = CSSMathExpressionNode::ParseMathFunction(
        CSSValueID::kCalc, stream, *context, Flags({Flag::AllowPercent}),
        kCSSAnchorQueryTypesNone);
    EXPECT_EQ(res->CustomCSSText(), test_case.input);
  }
}

TEST(CSSMathExpressionNode, TestExponentialFunctions) {
  const struct TestCase {
    const std::string input;
    const double output;
  } test_cases[] = {
      {"hypot(3, 4)", 5.0f}, {"log(100, 10)", 2.0f}, {"sqrt(144)", 12.0f},
      {"exp(0)", 1.0f},      {"pow(2, 2)", 4.0f},
  };

  for (const auto& test_case : test_cases) {
    CSSParserTokenStream stream(test_case.input.c_str());
    const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    const CSSMathExpressionNode* res = CSSMathExpressionNode::ParseMathFunction(
        CSSValueID::kCalc, stream, *context, Flags({Flag::AllowPercent}),
        kCSSAnchorQueryTypesNone);
    EXPECT_EQ(res->DoubleValue(), test_case.output);
    CSSToLengthConversionData resolver;
    scoped_refptr<const CalculationExpressionNode> node =
        res->ToCalculationExpression(resolver);
    EXPECT_EQ(node->Evaluate(FLT_MAX, {}), test_case.output);
    EXPECT_TRUE(!res->HasPercentage());
  }
}

TEST(CSSMathExpressionNode, TestExponentialFunctionsSerialization) {
  const struct TestCase {
    const String input;
    const bool can_be_simplified_with_conversion_data;
  } test_cases[] = {
      {"hypot(3em, 4rem)", true},
      {"hypot(3%, 4%)", false},
      {"hypot(hypot(3%, 4%), 5em)", false},
  };

  for (const auto& test_case : test_cases) {
    CSSParserTokenStream stream(test_case.input);
    const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    const CSSMathExpressionNode* res = CSSMathExpressionNode::ParseMathFunction(
        CSSValueID::kCalc, stream, *context, Flags({Flag::AllowPercent}),
        kCSSAnchorQueryTypesNone);
    EXPECT_EQ(res->CustomCSSText(), test_case.input);
    EXPECT_EQ(!res->HasPercentage(),
              test_case.can_be_simplified_with_conversion_data);
  }
}

TEST(CSSMathExpressionNode, TestExponentialFunctionsToCalculationExpression) {
  const struct TestCase {
    const CSSMathOperator op;
    const double output;
  } test_cases[] = {{CSSMathOperator::kHypot, 5.0f}};

  for (const auto& test_case : test_cases) {
    CSSMathExpressionOperation::Operands operands{
        CSSMathExpressionNumericLiteral::Create(
            3.0f, CSSPrimitiveValue::UnitType::kNumber),
        CSSMathExpressionNumericLiteral::Create(
            4.0f, CSSPrimitiveValue::UnitType::kNumber)};
    const auto* operation = MakeGarbageCollected<CSSMathExpressionOperation>(
        kCalcNumber, std::move(operands), test_case.op);
    CSSToLengthConversionData resolver{};
    scoped_refptr<const CalculationExpressionNode> node =
        operation->ToCalculationExpression(resolver);
    EXPECT_EQ(node->Evaluate(FLT_MAX, {}), test_case.output);
    const CSSMathExpressionNode* css_node =
        CSSMathExpressionOperation::Create(*node);
    EXPECT_NE(css_node, nullptr);
  }
}

TEST(CSSMathExpressionNode, IdentifierLiteralConversion) {
  const CSSMathExpressionIdentifierLiteral* css_node =
      CSSMathExpressionIdentifierLiteral::Create(AtomicString("test"));
  EXPECT_TRUE(css_node->IsIdentifierLiteral());
  EXPECT_EQ(css_node->Category(), kCalcIdent);
  EXPECT_EQ(css_node->GetValue(), AtomicString("test"));
  scoped_refptr<const CalculationExpressionNode> calc_node =
      css_node->ToCalculationExpression(CSSToLengthConversionData());
  EXPECT_TRUE(calc_node->IsIdentifier());
  EXPECT_EQ(To<CalculationExpressionIdentifierNode>(*calc_node).Value(),
            AtomicString("test"));
  auto* node = CSSMathExpressionNode::Create(*calc_node);
  EXPECT_TRUE(node->IsIdentifierLiteral());
  EXPECT_EQ(To<CSSMathExpressionIdentifierLiteral>(node)->GetValue(),
            AtomicString("test"));
}

TEST(CSSMathExpressionNode, ColorChannelKeywordConversion) {
  const CSSMathExpressionKeywordLiteral* css_node =
      CSSMathExpressionKeywordLiteral::Create(
          CSSValueID::kAlpha,
          CSSMathExpressionKeywordLiteral::Context::kColorChannel);
  EXPECT_TRUE(css_node->IsKeywordLiteral());
  EXPECT_EQ(css_node->Category(), kCalcNumber);
  EXPECT_EQ(css_node->GetValue(), CSSValueID::kAlpha);
  scoped_refptr<const CalculationExpressionNode> calc_node =
      css_node->ToCalculationExpression(CSSToLengthConversionData());
  EXPECT_TRUE(calc_node->IsColorChannelKeyword());
  EXPECT_EQ(
      To<CalculationExpressionColorChannelKeywordNode>(*calc_node).Value(),
      ColorChannelKeyword::kAlpha);
  auto* node = CSSMathExpressionNode::Create(*calc_node);
  EXPECT_TRUE(node->IsKeywordLiteral());
  EXPECT_EQ(To<CSSMathExpressionKeywordLiteral>(node)->GetValue(),
            CSSValueID::kAlpha);
}

TEST(CSSMathExpressionNode, TestProgressNotation) {
  const struct TestCase {
    const std::string input;
    const double output;
  } test_cases[] = {
      {"progress(1px from 0px to 4px)", 0.25f},
      {"progress(10deg from 0deg to 10deg)", 1.0f},
      {"progress(progress(10% from 0% to 40%) * 1px from 0.5px to 1px)", -0.5f},
  };

  for (const auto& test_case : test_cases) {
    CSSParserTokenStream stream(test_case.input.c_str());
    const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    const CSSMathExpressionNode* res = CSSMathExpressionNode::ParseMathFunction(
        CSSValueID::kCalc, stream, *context, Flags({Flag::AllowPercent}),
        kCSSAnchorQueryTypesNone);
    EXPECT_EQ(res->DoubleValue(), test_case.output);
    CSSToLengthConversionData resolver;
    scoped_refptr<const CalculationExpressionNode> node =
        res->ToCalculationExpression(resolver);
    EXPECT_EQ(node->Evaluate(FLT_MAX, {}), test_case.output);
  }
}

TEST(CSSMathExpressionNode, TestProgressNotationComplex) {
  const struct TestCase {
    const std::string input;
    const double output;
  } test_cases[] = {
      {"progress(abs(5%) from hypot(3%, 4%) to 10%)", 0.0f},
  };

  for (const auto& test_case : test_cases) {
    CSSParserTokenStream stream(test_case.input.c_str());
    const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    const CSSMathExpressionNode* res = CSSMathExpressionNode::ParseMathFunction(
        CSSValueID::kCalc, stream, *context, Flags({Flag::AllowPercent}),
        kCSSAnchorQueryTypesNone);
    EXPECT_TRUE(res);
    EXPECT_TRUE(res->IsOperation());
    CSSToLengthConversionData resolver;
    scoped_refptr<const CalculationExpressionNode> node =
        res->ToCalculationExpression(resolver);
    // Very close to 0.0f, but not exactly 0.0f for unknown reason.
    EXPECT_NEAR(node->Evaluate(FLT_MAX, {}), test_case.output, 0.001);
  }
}

TEST(CSSMathExpressionNode, TestInvalidProgressNotation) {
  const std::string test_cases[] = {
      "progress(1% from 0px to 4px)",
      "progress(1px, 0px, 4px)",
      "progress(10deg from 0 to 10deg)",
  };

  for (const auto& test_case : test_cases) {
    CSSParserTokenStream stream(test_case.c_str());
    const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    const CSSMathExpressionNode* res = CSSMathExpressionNode::ParseMathFunction(
        CSSValueID::kCalc, stream, *context, Flags({Flag::AllowPercent}),
        kCSSAnchorQueryTypesNone);
    EXPECT_FALSE(res);
  }
}

TEST(CSSMathExpressionNode, TestFunctionsWithNumberReturn) {
  const struct TestCase {
    const String input;
    const CalculationResultCategory category;
    const double output;
  } test_cases[] = {
      {"10 * sign(10%)", CalculationResultCategory::kCalcNumber, 10.0},
      {"10px * sign(10%)", CalculationResultCategory::kCalcLength, 10.0},
      {"10 + 2 * (1 + sign(10%))", CalculationResultCategory::kCalcNumber,
       14.0},
  };

  for (const auto& test_case : test_cases) {
    CSSParserTokenStream stream(test_case.input);
    const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    const CSSMathExpressionNode* css_node =
        CSSMathExpressionNode::ParseMathFunction(
            CSSValueID::kCalc, stream, *context, Flags({Flag::AllowPercent}),
            kCSSAnchorQueryTypesNone);
    EXPECT_EQ(css_node->CustomCSSText(), test_case.input);
    EXPECT_EQ(css_node->Category(), test_case.category);
    EXPECT_TRUE(css_node->IsOperation());
    scoped_refptr<const CalculationExpressionNode> calc_node =
        css_node->ToCalculationExpression(CSSToLengthConversionData());
    EXPECT_TRUE(calc_node->IsOperation());
    EXPECT_EQ(calc_node->Evaluate(100.0, {}), test_case.output);
    css_node = CSSMathExpressionNode::Create(*calc_node);
    EXPECT_EQ(css_node->CustomCSSText(), test_case.input);
  }
}

TEST(CSSMathExpressionNode, TestColorChannelExpressionWithSubstitution) {
  const struct TestCase {
    const String input;
    const CalculationResultCategory category;
    const double output;
  } test_cases[] = {
      {"h / 2", CalculationResultCategory::kCalcNumber, 120.0f},
  };

  const CSSColorChannelMap color_channel_map = {
      {CSSValueID::kH, 240.0f},
      {CSSValueID::kS, 50.0f},
      {CSSValueID::kL, 75.0f},
      {CSSValueID::kAlpha, 1.0f},
  };

  for (const auto& test_case : test_cases) {
    CSSParserTokenStream stream(test_case.input);
    const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    const CSSMathExpressionNode* css_node =
        CSSMathExpressionNode::ParseMathFunction(
            CSSValueID::kCalc, stream, *context, Flags({Flag::AllowPercent}),
            kCSSAnchorQueryTypesNone, color_channel_map);
    EXPECT_EQ(css_node->Category(), test_case.category);
    EXPECT_TRUE(css_node->IsNumericLiteral());
    scoped_refptr<const CalculationExpressionNode> calc_node =
        css_node->ToCalculationExpression(CSSToLengthConversionData());
    EXPECT_TRUE(calc_node->IsNumber());
    EXPECT_EQ(calc_node->Evaluate(FLT_MAX, {}), test_case.output);
  }
}

TEST(CSSMathExpressionNode, TestColorChannelExpressionWithInvalidChannelName) {
  const String test_cases[] = {
      "r / 2",
  };

  const CSSColorChannelMap color_channel_map = {
      {CSSValueID::kH, 240.0f},
      {CSSValueID::kS, 50.0f},
      {CSSValueID::kL, 75.0f},
      {CSSValueID::kAlpha, 1.0f},
  };

  for (const auto& test_case : test_cases) {
    CSSParserTokenStream stream(test_case);
    const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
        kHTMLStandardMode, SecureContextMode::kInsecureContext);
    const CSSMathExpressionNode* css_node =
        CSSMathExpressionNode::ParseMathFunction(
            CSSValueID::kCalc, stream, *context, Flags({Flag::AllowPercent}),
            kCSSAnchorQueryTypesNone, color_channel_map);
    EXPECT_EQ(css_node, nullptr);
  }
}

TEST(CSSMathExpressionNode, TestColorChannelExpressionWithoutSubstitution) {
  const String input = "(h / 360) * 360deg";

  const CSSColorChannelMap color_channel_map = {
      {CSSValueID::kH, std::nullopt},
      {CSSValueID::kS, std::nullopt},
      {CSSValueID::kL, std::nullopt},
      {CSSValueID::kAlpha, std::nullopt},
  };

  CSSParserTokenStream stream(input);
  const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);
  const CSSMathExpressionNode* css_node =
      CSSMathExpressionNode::ParseMathFunction(
          CSSValueID::kCalc, stream, *context, Flags({Flag::AllowPercent}),
          kCSSAnchorQueryTypesNone, color_channel_map);
  EXPECT_EQ(css_node->Category(), CalculationResultCategory::kCalcAngle);
  EXPECT_TRUE(css_node->IsOperation());
  const CSSMathExpressionOperation* css_op =
      To<CSSMathExpressionOperation>(css_node);
  const CSSMathExpressionNode* operand = css_op->GetOperands()[0];
  EXPECT_TRUE(operand->IsOperation());
  const CSSMathExpressionOperation* inner_css_op =
      To<CSSMathExpressionOperation>(operand);
  const CSSMathExpressionNode* inner_operand = inner_css_op->GetOperands()[0];
  EXPECT_TRUE(inner_operand->IsKeywordLiteral());
  const CSSMathExpressionKeywordLiteral* keyword =
      To<CSSMathExpressionKeywordLiteral>(inner_operand);
  EXPECT_EQ(keyword->GetValue(), CSSValueID::kH);
  EXPECT_EQ(keyword->GetContext(),
            CSSMathExpressionKeywordLiteral::Context::kColorChannel);

  CSSToLengthConversionData resolver{};
  scoped_refptr<const CalculationExpressionNode> node =
      css_node->ToCalculationExpression(resolver);
  EXPECT_TRUE(node->IsOperation());
  const CalculationExpressionOperationNode* operation_node =
      To<CalculationExpressionOperationNode>(node.get());
  EXPECT_EQ(operation_node->GetOperator(), CalculationOperator::kMultiply);
  const CalculationExpressionOperationNode::Children& operands =
      operation_node->GetChildren();
  EXPECT_EQ(operands.size(), 2u);
  EXPECT_TRUE(operands[0]->IsOperation());

  const CalculationExpressionOperationNode* inner_operation_node =
      To<CalculationExpressionOperationNode>(operands[0].get());
  const CalculationExpressionOperationNode::Children& inner_operands =
      inner_operation_node->GetChildren();
  EXPECT_EQ(inner_operation_node->GetOperator(),
            CalculationOperator::kMultiply);
  EXPECT_EQ(inner_operands.size(), 2u);
  EXPECT_TRUE(inner_operands[0]->IsColorChannelKeyword());
  EXPECT_EQ(
      To<CalculationExpressionColorChannelKeywordNode>(inner_operands[0].get())
          ->Value(),
      ColorChannelKeyword::kH);
  EXPECT_TRUE(inner_operands[1]->IsNumber());
  EXPECT_EQ(
      To<CalculationExpressionNumberNode>(inner_operands[1].get())->Value(),
      (1.f / 360.f));

  EXPECT_TRUE(operands[1]->IsPixelsAndPercent());
  EXPECT_EQ(To<CalculationExpressionPixelsAndPercentNode>(operands[1].get())
                ->Pixels(),
            360.f);
}

}  // anonymous namespace

}  // namespace blink
