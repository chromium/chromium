// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_value.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/animation_test_helpers.h"
#include "third_party/blink/renderer/core/animation/css_length_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_number_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolation_value.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/animation/transition_interpolation.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class AnimationInterpolableValueTest : public testing::Test {
 protected:
  double InterpolateNumbers(int a, int b, double progress) {
    // We require a property that maps to CSSNumberInterpolationType. 'z-index'
    // suffices for this, and also means we can ignore the AnimatableValues for
    // the compositor (as z-index isn't compositor-compatible).
    PropertyHandle property_handle(GetCSSPropertyZIndex());
    CSSNumberInterpolationType interpolation_type(property_handle);
    InterpolationValue start(MakeGarbageCollected<InterpolableNumber>(a));
    InterpolationValue end(MakeGarbageCollected<InterpolableNumber>(b));
    TransitionInterpolation* i = MakeGarbageCollected<TransitionInterpolation>(
        property_handle, interpolation_type, std::move(start), std::move(end),
        nullptr, nullptr);

    i->Interpolate(0, progress);
    TypedInterpolationValue* interpolated_value = i->GetInterpolatedValue();
    EXPECT_TRUE(interpolated_value);
    CSSToLengthConversionData length_resolver;
    return To<InterpolableNumber>(interpolated_value->GetInterpolableValue())
        .Value(length_resolver);
  }

  void ScaleAndAdd(InterpolableValue& base,
                   double scale,
                   const InterpolableValue& add) {
    base.ScaleAndAdd(scale, add);
  }

  InterpolableValue* InterpolateLists(InterpolableValue* list_a,
                                      InterpolableValue* list_b,
                                      double progress) {
    InterpolableValue* result = list_a->CloneAndZero();
    list_a->Interpolate(*list_b, progress, *result);
    return result;
  }
  test::TaskEnvironment task_environment_;
};

TEST_F(AnimationInterpolableValueTest, InterpolateNumbers) {
  EXPECT_FLOAT_EQ(126, InterpolateNumbers(42, 0, -2));
  EXPECT_FLOAT_EQ(42, InterpolateNumbers(42, 0, 0));
  EXPECT_FLOAT_EQ(29.4f, InterpolateNumbers(42, 0, 0.3));
  EXPECT_FLOAT_EQ(21, InterpolateNumbers(42, 0, 0.5));
  EXPECT_FLOAT_EQ(0, InterpolateNumbers(42, 0, 1));
  EXPECT_FLOAT_EQ(-21, InterpolateNumbers(42, 0, 1.5));
}

TEST_F(AnimationInterpolableValueTest, SimpleList) {
  auto* list_a = MakeGarbageCollected<InterpolableList>(3);
  list_a->Set(0, MakeGarbageCollected<InterpolableNumber>(0));
  list_a->Set(1, MakeGarbageCollected<InterpolableNumber>(42));
  list_a->Set(2, MakeGarbageCollected<InterpolableNumber>(20.5));

  auto* list_b = MakeGarbageCollected<InterpolableList>(3);
  list_b->Set(0, MakeGarbageCollected<InterpolableNumber>(100));
  list_b->Set(1, MakeGarbageCollected<InterpolableNumber>(-200));
  list_b->Set(2, MakeGarbageCollected<InterpolableNumber>(300));

  InterpolableValue* interpolated_value =
      InterpolateLists(std::move(list_a), std::move(list_b), 0.3);
  const auto& out_list = To<InterpolableList>(*interpolated_value);

  CSSToLengthConversionData length_resolver;
  EXPECT_FLOAT_EQ(
      30, To<InterpolableNumber>(out_list.Get(0))->Value(length_resolver));
  EXPECT_FLOAT_EQ(
      -30.6f, To<InterpolableNumber>(out_list.Get(1))->Value(length_resolver));
  EXPECT_FLOAT_EQ(
      104.35f, To<InterpolableNumber>(out_list.Get(2))->Value(length_resolver));
}

TEST_F(AnimationInterpolableValueTest, NestedList) {
  auto* list_a = MakeGarbageCollected<InterpolableList>(3);
  list_a->Set(0, MakeGarbageCollected<InterpolableNumber>(0));
  auto* sub_list_a = MakeGarbageCollected<InterpolableList>(1);
  sub_list_a->Set(0, MakeGarbageCollected<InterpolableNumber>(100));
  list_a->Set(1, sub_list_a);
  list_a->Set(2, MakeGarbageCollected<InterpolableNumber>(0));

  auto* list_b = MakeGarbageCollected<InterpolableList>(3);
  list_b->Set(0, MakeGarbageCollected<InterpolableNumber>(100));
  auto* sub_list_b = MakeGarbageCollected<InterpolableList>(1);
  sub_list_b->Set(0, MakeGarbageCollected<InterpolableNumber>(50));
  list_b->Set(1, sub_list_b);
  list_b->Set(2, MakeGarbageCollected<InterpolableNumber>(1));

  InterpolableValue* interpolated_value = InterpolateLists(list_a, list_b, 0.5);
  const auto& out_list = To<InterpolableList>(*interpolated_value);

  CSSToLengthConversionData length_resolver;
  EXPECT_FLOAT_EQ(
      50, To<InterpolableNumber>(out_list.Get(0))->Value(length_resolver));
  EXPECT_FLOAT_EQ(
      75, To<InterpolableNumber>(To<InterpolableList>(out_list.Get(1))->Get(0))
              ->Value(length_resolver));
  EXPECT_FLOAT_EQ(
      0.5, To<InterpolableNumber>(out_list.Get(2))->Value(length_resolver));
}

TEST_F(AnimationInterpolableValueTest, ScaleAndAddNumbers) {
  CSSToLengthConversionData length_resolver;
  InterpolableNumber* base = MakeGarbageCollected<InterpolableNumber>(10);
  ScaleAndAdd(*base, 2, *MakeGarbageCollected<InterpolableNumber>(1));
  EXPECT_FLOAT_EQ(21, base->Value(length_resolver));

  base = MakeGarbageCollected<InterpolableNumber>(10);
  ScaleAndAdd(*base, 0, *MakeGarbageCollected<InterpolableNumber>(5));
  EXPECT_FLOAT_EQ(5, base->Value(length_resolver));

  base = MakeGarbageCollected<InterpolableNumber>(10);
  ScaleAndAdd(*base, -1, *MakeGarbageCollected<InterpolableNumber>(8));
  EXPECT_FLOAT_EQ(-2, base->Value(length_resolver));
}

TEST_F(AnimationInterpolableValueTest, ScaleAndAddLists) {
  auto* base_list = MakeGarbageCollected<InterpolableList>(3);
  base_list->Set(0, MakeGarbageCollected<InterpolableNumber>(5));
  base_list->Set(1, MakeGarbageCollected<InterpolableNumber>(10));
  base_list->Set(2, MakeGarbageCollected<InterpolableNumber>(15));
  auto* add_list = MakeGarbageCollected<InterpolableList>(3);
  add_list->Set(0, MakeGarbageCollected<InterpolableNumber>(1));
  add_list->Set(1, MakeGarbageCollected<InterpolableNumber>(2));
  add_list->Set(2, MakeGarbageCollected<InterpolableNumber>(3));
  ScaleAndAdd(*base_list, 2, *add_list);
  CSSToLengthConversionData length_resolver;
  EXPECT_FLOAT_EQ(
      11, To<InterpolableNumber>(base_list->Get(0))->Value(length_resolver));
  EXPECT_FLOAT_EQ(
      22, To<InterpolableNumber>(base_list->Get(1))->Value(length_resolver));
  EXPECT_FLOAT_EQ(
      33, To<InterpolableNumber>(base_list->Get(2))->Value(length_resolver));
}

TEST_F(AnimationInterpolableValueTest, InterpolableNumberAsExpression) {
  const struct TestCase {
    String input;
    double output;
    double add_value;
    double scale_value;
    String interpolation_input;
    double interpolation_output;
    double interpolation_fraction;
    double interpolation_result;
  } test_cases[] = {
      {"progress(11em from 1rem to 110px) * 10", 10.0, 10.0, 5.0,
       "progress(11em from 1rem to 110px) * 11", 11.0, 0.5, 10.5},
      {"10deg", 10.0, 10.0, 5.0, "progress(11em from 1rem to 110px) * 11deg",
       11.0, 0.5, 10.5},
      {"progress(11em from 1rem to 110px) * 10deg", 10.0, 10.0, 5.0, "11deg",
       11.0, 0.5, 10.5},
  };

  using enum CSSMathExpressionNode::Flag;
  using Flags = CSSMathExpressionNode::Flags;

  Font font;
  CSSToLengthConversionData length_resolver = CSSToLengthConversionData();
  length_resolver.SetFontSizes(
      CSSToLengthConversionData::FontSizes(10.0f, 10.0f, &font, 1.0f));

  const CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
      kHTMLStandardMode, SecureContextMode::kInsecureContext);

  for (const auto& test_case : test_cases) {
    CSSParserTokenStream stream(test_case.input);

    // Test expression evaluation.
    const CSSMathExpressionNode* expression =
        CSSMathExpressionNode::ParseMathFunction(
            CSSValueID::kCalc, stream, *context, Flags({AllowPercent}),
            kCSSAnchorQueryTypesNone);
    InterpolableNumber* number = nullptr;
    if (auto* numeric_literal =
            DynamicTo<CSSMathExpressionNumericLiteral>(expression)) {
      number = MakeGarbageCollected<InterpolableNumber>(
          numeric_literal->DoubleValue(), numeric_literal->ResolvedUnitType());
    } else {
      number = MakeGarbageCollected<InterpolableNumber>(*expression);
    }
    EXPECT_EQ(number->Value(length_resolver), test_case.output);

    // Test clone, add, scale, scale and add.
    auto* number_copy = number->Clone();
    number_copy->Scale(test_case.scale_value);
    EXPECT_EQ(number_copy->Value(length_resolver),
              test_case.scale_value * test_case.output);
    number_copy->Add(*MakeGarbageCollected<InterpolableNumber>(
        test_case.add_value, expression->ResolvedUnitType()));
    EXPECT_EQ(number_copy->Value(length_resolver),
              test_case.scale_value * test_case.output + test_case.add_value);
    number_copy = number->Clone();
    number_copy->ScaleAndAdd(
        test_case.scale_value,
        *MakeGarbageCollected<InterpolableNumber>(
            test_case.add_value, expression->ResolvedUnitType()));
    EXPECT_EQ(number_copy->Value(length_resolver),
              test_case.scale_value * test_case.output + test_case.add_value);

    // Test interpolation with other expression.
    CSSParserTokenStream target_stream(test_case.interpolation_input);
    const CSSMathExpressionNode* target_expression =
        CSSMathExpressionNode::ParseMathFunction(
            CSSValueID::kCalc, target_stream, *context, Flags({AllowPercent}),
            kCSSAnchorQueryTypesNone);
    InterpolableNumber* target = nullptr;
    if (auto* numeric_literal =
            DynamicTo<CSSMathExpressionNumericLiteral>(target_expression)) {
      target = MakeGarbageCollected<InterpolableNumber>(
          numeric_literal->DoubleValue(), numeric_literal->ResolvedUnitType());
    } else {
      target = MakeGarbageCollected<InterpolableNumber>(*target_expression);
    }
    EXPECT_EQ(target->Value(length_resolver), test_case.interpolation_output);

    auto* interpolation_result = MakeGarbageCollected<InterpolableNumber>();
    number->Interpolate(*target, test_case.interpolation_fraction,
                        *interpolation_result);
    EXPECT_EQ(interpolation_result->Value(length_resolver),
              test_case.interpolation_result);
  }
}

}  // namespace blink
