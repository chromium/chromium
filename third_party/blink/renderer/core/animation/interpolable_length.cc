// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_length.h"

#include "third_party/blink/renderer/core/animation/underlying_value.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/platform/geometry/blend.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"

namespace blink {

using UnitType = CSSPrimitiveValue::UnitType;

namespace {

CSSMathExpressionNode* NumberNode(double number) {
  return CSSMathExpressionNumericLiteral::Create(
      CSSNumericLiteralValue::Create(number, UnitType::kNumber));
}

CSSMathExpressionNode* PercentageNode(double number) {
  return CSSMathExpressionNumericLiteral::Create(
      CSSNumericLiteralValue::Create(number, UnitType::kPercentage));
}

}  // namespace

// static
std::unique_ptr<InterpolableLength> InterpolableLength::CreatePixels(
    double pixels) {
  CSSLengthArray length_array;
  length_array.values[CSSPrimitiveValue::kUnitTypePixels] = pixels;
  length_array.type_flags.set(CSSPrimitiveValue::kUnitTypePixels);
  return std::make_unique<InterpolableLength>(std::move(length_array));
}

// static
std::unique_ptr<InterpolableLength> InterpolableLength::CreatePercent(
    double percent) {
  CSSLengthArray length_array;
  length_array.values[CSSPrimitiveValue::kUnitTypePercentage] = percent;
  length_array.type_flags.set(CSSPrimitiveValue::kUnitTypePercentage);
  return std::make_unique<InterpolableLength>(std::move(length_array));
}

// static
std::unique_ptr<InterpolableLength> InterpolableLength::CreateNeutral() {
  return std::make_unique<InterpolableLength>(CSSLengthArray());
}

// static
std::unique_ptr<InterpolableLength> InterpolableLength::MaybeConvertCSSValue(
    const CSSValue& value) {
  const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value);
  if (!primitive_value)
    return nullptr;

  if (!primitive_value->IsLength() && !primitive_value->IsPercentage() &&
      !primitive_value->IsCalculatedPercentageWithLength())
    return nullptr;

  CSSLengthArray length_array;
  if (primitive_value->AccumulateLengthArray(length_array))
    return std::make_unique<InterpolableLength>(std::move(length_array));

  DCHECK(primitive_value->IsMathFunctionValue());
  return std::make_unique<InterpolableLength>(
      *To<CSSMathFunctionValue>(primitive_value)->ExpressionNode());
}

// static
std::unique_ptr<InterpolableLength> InterpolableLength::MaybeConvertLength(
    const Length& length,
    float zoom) {
  if (!length.IsSpecified())
    return nullptr;

  if (length.IsCalculated() && length.GetCalculationValue().IsExpression()) {
    auto unzoomed_calc = length.GetCalculationValue().Zoom(1.0 / zoom);
    return std::make_unique<InterpolableLength>(
        *CSSMathExpressionNode::Create(*unzoomed_calc));
  }

  PixelsAndPercent pixels_and_percent = length.GetPixelsAndPercent();
  CSSLengthArray length_array;

  length_array.values[CSSPrimitiveValue::kUnitTypePixels] =
      pixels_and_percent.pixels / zoom;
  length_array.type_flags[CSSPrimitiveValue::kUnitTypePixels] =
      pixels_and_percent.pixels != 0;

  length_array.values[CSSPrimitiveValue::kUnitTypePercentage] =
      pixels_and_percent.percent;
  length_array.type_flags[CSSPrimitiveValue::kUnitTypePercentage] =
      length.IsPercentOrCalc();
  return std::make_unique<InterpolableLength>(std::move(length_array));
}

// static
PairwiseInterpolationValue InterpolableLength::MergeSingles(
    std::unique_ptr<InterpolableValue> start,
    std::unique_ptr<InterpolableValue> end) {
  // TODO(crbug.com/991672): We currently have a lot of "fast paths" that do not
  // go through here, and hence, do not merge the percentage info of two
  // lengths. We should stop doing that.
  auto& start_length = To<InterpolableLength>(*start);
  auto& end_length = To<InterpolableLength>(*end);
  if (start_length.HasPercentage() || end_length.HasPercentage()) {
    start_length.SetHasPercentage();
    end_length.SetHasPercentage();
  }
  if (start_length.IsExpression() || end_length.IsExpression()) {
    start_length.SetExpression(start_length.AsExpression());
    end_length.SetExpression(end_length.AsExpression());
  }
  return PairwiseInterpolationValue(std::move(start), std::move(end));
}

InterpolableLength::InterpolableLength(CSSLengthArray&& length_array) {
  SetLengthArray(std::move(length_array));
}

void InterpolableLength::SetLengthArray(CSSLengthArray&& length_array) {
  type_ = Type::kLengthArray;
  length_array_ = std::move(length_array);
  expression_.Clear();
}

InterpolableLength::InterpolableLength(
    const CSSMathExpressionNode& expression) {
  SetExpression(expression);
}

void InterpolableLength::SetExpression(
    const CSSMathExpressionNode& expression) {
  type_ = Type::kExpression;
  expression_ = &expression;
}

InterpolableLength* InterpolableLength::RawClone() const {
  return new InterpolableLength(*this);
}

bool InterpolableLength::HasPercentage() const {
  if (IsLengthArray()) {
    return length_array_.type_flags.test(
        CSSPrimitiveValue::kUnitTypePercentage);
  }
  return expression_->HasPercentage();
}

void InterpolableLength::SetHasPercentage() {
  if (HasPercentage())
    return;

  if (IsLengthArray()) {
    length_array_.type_flags.set(CSSPrimitiveValue::kUnitTypePercentage);
    return;
  }

  DEFINE_STATIC_LOCAL(Persistent<CSSMathExpressionNode>, zero_percent,
                      {PercentageNode(0)});
  SetExpression(*CSSMathExpressionBinaryOperation::Create(
      expression_, zero_percent, CSSMathOperator::kAdd));
}

void InterpolableLength::SubtractFromOneHundredPercent() {
  if (IsLengthArray()) {
    for (double& value : length_array_.values)
      value *= -1;
    length_array_.values[CSSPrimitiveValue::kUnitTypePercentage] += 100;
    length_array_.type_flags.set(CSSPrimitiveValue::kUnitTypePercentage);
    return;
  }

  DEFINE_STATIC_LOCAL(Persistent<CSSMathExpressionNode>, hundred_percent,
                      {PercentageNode(100)});
  SetExpression(*CSSMathExpressionBinaryOperation::CreateSimplified(
      hundred_percent, expression_, CSSMathOperator::kSubtract));
}

static double ClampToRange(double x, ValueRange range) {
  return (range == kValueRangeNonNegative && x < 0) ? 0 : x;
}

static const CSSNumericLiteralValue& ClampNumericLiteralValueToRange(
    const CSSNumericLiteralValue& value,
    ValueRange range) {
  if (range == kValueRangeAll || value.DoubleValue() >= 0)
    return value;
  return *CSSNumericLiteralValue::Create(0, value.GetType());
}

static UnitType IndexToUnitType(wtf_size_t index) {
  return CSSPrimitiveValue::LengthUnitTypeToUnitType(
      static_cast<CSSPrimitiveValue::LengthUnitType>(index));
}

Length InterpolableLength::CreateLength(
    const CSSToLengthConversionData& conversion_data,
    ValueRange range) const {
  if (IsExpression())
    return Length(expression_->ToCalcValue(conversion_data, range));

  bool has_percentage = HasPercentage();
  double pixels = 0;
  double percentage = 0;
  for (wtf_size_t i = 0; i < length_array_.values.size(); ++i) {
    double value = length_array_.values[i];
    if (value == 0)
      continue;
    if (i == CSSPrimitiveValue::kUnitTypePercentage) {
      percentage = value;
    } else {
      pixels += conversion_data.ZoomedComputedPixels(value, IndexToUnitType(i));
    }
  }

  if (percentage != 0)
    has_percentage = true;
  if (pixels != 0 && has_percentage) {
    return Length(CalculationValue::Create(
        PixelsAndPercent(clampTo<float>(pixels), clampTo<float>(percentage)),
        range));
  }
  if (has_percentage)
    return Length::Percent(ClampToRange(percentage, range));
  return Length::Fixed(
      CSSPrimitiveValue::ClampToCSSLengthRange(ClampToRange(pixels, range)));
}

const CSSPrimitiveValue* InterpolableLength::CreateCSSValue(
    ValueRange range) const {
  if (IsExpression())
    return CSSMathFunctionValue::Create(expression_, range);

  DCHECK(IsLengthArray());
  if (length_array_.type_flags.count() > 1u) {
    const CSSMathExpressionNode& expression = AsExpression();
    if (!expression.IsNumericLiteral())
      return CSSMathFunctionValue::Create(&AsExpression(), range);

    // This creates a temporary CSSMathExpressionNode. Eliminate it if this
    // results in significant performance regression.
    return &ClampNumericLiteralValueToRange(
        To<CSSMathExpressionNumericLiteral>(expression).GetValue(), range);
  }

  for (wtf_size_t i = 0; i < length_array_.values.size(); ++i) {
    if (length_array_.type_flags.test(i)) {
      double value = ClampToRange(length_array_.values[i], range);
      UnitType unit_type = IndexToUnitType(i);
      return CSSNumericLiteralValue::Create(value, unit_type);
    }
  }

  return CSSNumericLiteralValue::Create(0, UnitType::kPixels);
}

const CSSMathExpressionNode& InterpolableLength::AsExpression() const {
  if (IsExpression())
    return *expression_;

  DCHECK(IsLengthArray());
  bool has_percentage = HasPercentage();

  CSSMathExpressionNode* root_node = nullptr;
  for (wtf_size_t i = 0; i < length_array_.values.size(); ++i) {
    double value = length_array_.values[i];
    if (value == 0 &&
        (i != CSSPrimitiveValue::kUnitTypePercentage || !has_percentage)) {
      continue;
    }
    CSSNumericLiteralValue* current_value =
        CSSNumericLiteralValue::Create(value, IndexToUnitType(i));
    CSSMathExpressionNode* current_node =
        CSSMathExpressionNumericLiteral::Create(current_value);
    if (!root_node) {
      root_node = current_node;
    } else {
      root_node = CSSMathExpressionBinaryOperation::Create(
          root_node, current_node, CSSMathOperator::kAdd);
    }
  }

  if (root_node)
    return *root_node;
  return *CSSMathExpressionNumericLiteral::Create(
      CSSNumericLiteralValue::Create(0, UnitType::kPixels));
}

void InterpolableLength::Scale(double scale) {
  if (IsLengthArray()) {
    for (auto& value : length_array_.values)
      value *= scale;
    return;
  }

  DCHECK(IsExpression());
  SetExpression(*CSSMathExpressionBinaryOperation::CreateSimplified(
      expression_, NumberNode(scale), CSSMathOperator::kMultiply));
}

void InterpolableLength::Add(const InterpolableValue& other) {
  const InterpolableLength& other_length = To<InterpolableLength>(other);
  if (IsLengthArray() && other_length.IsLengthArray()) {
    for (wtf_size_t i = 0; i < length_array_.values.size(); ++i) {
      length_array_.values[i] =
          length_array_.values[i] + other_length.length_array_.values[i];
    }
    length_array_.type_flags |= other_length.length_array_.type_flags;
    return;
  }

  CSSMathExpressionNode* result =
      CSSMathExpressionBinaryOperation::CreateSimplified(
          &AsExpression(), &other_length.AsExpression(), CSSMathOperator::kAdd);
  SetExpression(*result);
}

void InterpolableLength::ScaleAndAdd(double scale,
                                     const InterpolableValue& other) {
  const InterpolableLength& other_length = To<InterpolableLength>(other);
  if (IsLengthArray() && other_length.IsLengthArray()) {
    for (wtf_size_t i = 0; i < length_array_.values.size(); ++i) {
      length_array_.values[i] = length_array_.values[i] * scale +
                                other_length.length_array_.values[i];
    }
    length_array_.type_flags |= other_length.length_array_.type_flags;
    return;
  }

  CSSMathExpressionNode* scaled =
      CSSMathExpressionBinaryOperation::CreateSimplified(
          &AsExpression(), NumberNode(scale), CSSMathOperator::kMultiply);
  CSSMathExpressionNode* result =
      CSSMathExpressionBinaryOperation::CreateSimplified(
          scaled, &other_length.AsExpression(), CSSMathOperator::kAdd);
  SetExpression(*result);
}

void InterpolableLength::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  DCHECK(other.IsLength());
  // TODO(crbug.com/991672): Ensure that all |MergeSingles| variants that merge
  // two |InterpolableLength| objects should also assign them the same shape
  // (i.e. type flags) after merging into a |PairwiseInterpolationValue|. We
  // currently fail to do that, and hit the following DCHECK:
  // DCHECK_EQ(HasPercentage(),
  //           To<InterpolableLength>(other).HasPercentage());
}

void InterpolableLength::Interpolate(const InterpolableValue& to,
                                     const double progress,
                                     InterpolableValue& result) const {
  const auto& to_length = To<InterpolableLength>(to);
  auto& result_length = To<InterpolableLength>(result);
  if (IsLengthArray() && to_length.IsLengthArray()) {
    if (!result_length.IsLengthArray())
      result_length.SetLengthArray(CSSLengthArray());
    const CSSLengthArray& to_length_array = to_length.length_array_;
    CSSLengthArray& result_length_array =
        To<InterpolableLength>(result).length_array_;
    for (wtf_size_t i = 0; i < length_array_.values.size(); ++i) {
      result_length_array.values[i] =
          Blend(length_array_.values[i], to_length_array.values[i], progress);
    }
    result_length_array.type_flags =
        length_array_.type_flags | to_length_array.type_flags;
    return;
  }

  CSSMathExpressionNode* blended_from =
      CSSMathExpressionBinaryOperation::CreateSimplified(
          &AsExpression(), NumberNode(1 - progress),
          CSSMathOperator::kMultiply);
  CSSMathExpressionNode* blended_to =
      CSSMathExpressionBinaryOperation::CreateSimplified(
          &to_length.AsExpression(), NumberNode(progress),
          CSSMathOperator::kMultiply);
  CSSMathExpressionNode* result_expression =
      CSSMathExpressionBinaryOperation::CreateSimplified(
          blended_from, blended_to, CSSMathOperator::kAdd);
  result_length.SetExpression(*result_expression);

  DCHECK_EQ(result_length.HasPercentage(),
            HasPercentage() || to_length.HasPercentage());
}

}  // namespace blink
