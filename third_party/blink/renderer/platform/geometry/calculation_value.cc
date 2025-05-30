// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/calculation_value.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/platform/geometry/blend.h"
#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

// static
const CalculationValue* CalculationValue::CreateSimplified(
    const CalculationExpressionNode* expression,
    Length::ValueRange range) {
  if (expression->IsPixelsAndPercent()) {
    return MakeGarbageCollected<CalculationValue>(
        To<CalculationExpressionPixelsAndPercentNode>(*expression)
            .GetPixelsAndPercent(),
        range);
  }
  return MakeGarbageCollected<CalculationValue>(PassKey(), expression, range);
}

CalculationValue::CalculationValue(PassKey,
                                   const CalculationExpressionNode* expression,
                                   Length::ValueRange range)
    : expression_(expression),
      is_non_negative_(range == Length::ValueRange::kNonNegative) {
  CHECK(expression);
}

CalculationValue::~CalculationValue() = default;

void CalculationValue::Trace(Visitor* visitor) const {
  visitor->Trace(expression_);
}

float CalculationValue::Evaluate(float max_value,
                                 const EvaluationInput& input) const {
  float value =
      ClampTo<float>(expression_ ? expression_->Evaluate(max_value, input)
                                 : Pixels() + Percent() / 100 * max_value);
  return (IsNonNegative() && value < 0) ? 0 : value;
}

bool CalculationValue::operator==(const CalculationValue& other) const {
  return value_.pixels == other.value_.pixels &&
         value_.percent == other.value_.percent &&
         base::ValuesEquivalent(expression_, other.expression_) &&
         is_non_negative_ == other.is_non_negative_;
}

const CalculationExpressionNode* CalculationValue::GetOrCreateExpression()
    const {
  if (expression_) {
    return expression_.Get();
  }
  return MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
      GetPixelsAndPercent());
}

const CalculationValue* CalculationValue::Blend(
    const CalculationValue& from,
    double progress,
    Length::ValueRange range) const {
  if (!IsExpression() && !from.IsExpression()) {
    PixelsAndPercent from_pixels_and_percent = from.GetPixelsAndPercent();
    PixelsAndPercent to_pixels_and_percent = GetPixelsAndPercent();
    const float pixels = blink::Blend(from_pixels_and_percent.pixels,
                                      to_pixels_and_percent.pixels, progress);
    const float percent = blink::Blend(from_pixels_and_percent.percent,
                                       to_pixels_and_percent.percent, progress);
    bool has_explicit_pixels = from_pixels_and_percent.has_explicit_pixels |
                               to_pixels_and_percent.has_explicit_pixels;
    bool has_explicit_percent = from_pixels_and_percent.has_explicit_percent |
                                to_pixels_and_percent.has_explicit_percent;
    return MakeGarbageCollected<CalculationValue>(
        PixelsAndPercent(pixels, percent, has_explicit_pixels,
                         has_explicit_percent),
        range);
  }

  const auto* blended_from =
      CalculationExpressionOperationNode::CreateSimplified(
          CalculationExpressionOperationNode::Children(
              {from.GetOrCreateExpression(),
               MakeGarbageCollected<CalculationExpressionNumberNode>(
                   1.0 - progress)}),
          CalculationOperator::kMultiply);
  const auto* blended_to = CalculationExpressionOperationNode::CreateSimplified(
      CalculationExpressionOperationNode::Children(
          {GetOrCreateExpression(),
           MakeGarbageCollected<CalculationExpressionNumberNode>(progress)}),
      CalculationOperator::kMultiply);
  const auto* result_expression =
      CalculationExpressionOperationNode::CreateSimplified(
          {blended_from, blended_to}, CalculationOperator::kAdd);
  return CreateSimplified(result_expression, range);
}

const CalculationValue* CalculationValue::SubtractFromOneHundredPercent()
    const {
  if (!IsExpression()) {
    PixelsAndPercent result(-Pixels(), 100 - Percent(), HasExplicitPixels(),
                            /*has_explicit_percent=*/true);
    return MakeGarbageCollected<CalculationValue>(result,
                                                  Length::ValueRange::kAll);
  }
  const auto* hundred_percent =
      MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
          PixelsAndPercent(0, 100, false, true));
  const auto* result_expression =
      CalculationExpressionOperationNode::CreateSimplified(
          CalculationExpressionOperationNode::Children(
              {hundred_percent, GetOrCreateExpression()}),
          CalculationOperator::kSubtract);
  return CreateSimplified(result_expression, Length::ValueRange::kAll);
}

const CalculationValue* CalculationValue::Add(
    const CalculationValue& other) const {
  const auto* result_expression =
      CalculationExpressionOperationNode::CreateSimplified(
          {GetOrCreateExpression(), other.GetOrCreateExpression()},
          CalculationOperator::kAdd);
  return CreateSimplified(result_expression, Length::ValueRange::kAll);
}

const CalculationValue* CalculationValue::Zoom(double factor) const {
  if (expression_) {
    return CreateSimplified(expression_->Zoom(factor), GetValueRange());
  }
  PixelsAndPercent result(Pixels() * factor, Percent(), HasExplicitPixels(),
                          HasExplicitPercent());
  return MakeGarbageCollected<CalculationValue>(result, GetValueRange());
}

bool CalculationValue::HasAuto() const {
  return expression_ && expression_->HasAuto();
}

bool CalculationValue::HasContentOrIntrinsicSize() const {
  return expression_ && expression_->HasContentOrIntrinsicSize();
}

bool CalculationValue::HasAutoOrContentOrIntrinsicSize() const {
  return expression_ && expression_->HasAutoOrContentOrIntrinsicSize();
}

bool CalculationValue::HasPercent() const {
  if (expression_) {
    return expression_->HasPercent();
  }
  return HasExplicitPercent();
}

bool CalculationValue::HasPercentOrStretch() const {
  if (expression_) {
    return expression_->HasPercentOrStretch();
  }
  return HasExplicitPercent();
}

bool CalculationValue::HasStretch() const {
  return expression_ && expression_->HasStretch();
}

bool CalculationValue::HasMinContent() const {
  // `HasContentOrIntrinsicSize` is comparatively faster than `HasMinContent`.
  return expression_ && expression_->HasContentOrIntrinsicSize() &&
         expression_->HasMinContent();
}

bool CalculationValue::HasMaxContent() const {
  // `HasContentOrIntrinsicSize` is comparatively faster than `HasMaxContent`.
  return expression_ && expression_->HasContentOrIntrinsicSize() &&
         expression_->HasMaxContent();
}

bool CalculationValue::HasFitContent() const {
  // `HasContentOrIntrinsicSize` is comparatively faster than `HasFitContent`.
  return expression_ && expression_->HasContentOrIntrinsicSize() &&
         expression_->HasFitContent();
}

bool CalculationValue::HasOnlyFixedAndPercent() const {
  if (expression_) {
    return !expression_->HasAutoOrContentOrIntrinsicSize() &&
           !expression_->HasStretch();
  }
  return true;
}

}  // namespace blink
