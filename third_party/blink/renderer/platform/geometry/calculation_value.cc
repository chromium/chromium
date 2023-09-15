// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/calculation_value.h"

#include "third_party/blink/renderer/platform/geometry/blend.h"
#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

CalculationValue::DataUnion::DataUnion(
    scoped_refptr<const CalculationExpressionNode> expression)
    : expression(std::move(expression)) {}

CalculationValue::DataUnion::~DataUnion() {
  // Release of |expression| is left to CalculationValue::~CalculationValue().
}

// static
scoped_refptr<const CalculationValue> CalculationValue::CreateSimplified(
    scoped_refptr<const CalculationExpressionNode> expression,
    Length::ValueRange range) {
  if (expression->IsPixelsAndPercent()) {
    return Create(To<CalculationExpressionPixelsAndPercentNode>(*expression)
                      .GetPixelsAndPercent(),
                  range);
  }
  return base::AdoptRef(new CalculationValue(std::move(expression), range));
}

CalculationValue::CalculationValue(
    scoped_refptr<const CalculationExpressionNode> expression,
    Length::ValueRange range)
    : data_(std::move(expression)),
      is_expression_(true),
      is_non_negative_(range == Length::ValueRange::kNonNegative) {}

CalculationValue::~CalculationValue() {
  if (is_expression_)
    data_.expression.~scoped_refptr<const CalculationExpressionNode>();
  else
    data_.value.~PixelsAndPercent();
}

float CalculationValue::Evaluate(
    float max_value,
    const Length::AnchorEvaluator* anchor_evaluator) const {
  float value = ClampTo<float>(
      is_expression_ ? data_.expression->Evaluate(max_value, anchor_evaluator)
                     : Pixels() + Percent() / 100 * max_value);
  return (IsNonNegative() && value < 0) ? 0 : value;
}

bool CalculationValue::operator==(const CalculationValue& other) const {
  if (IsExpression())
    return other.IsExpression() && *data_.expression == *other.data_.expression;
  return !other.IsExpression() && Pixels() == other.Pixels() &&
         Percent() == other.Percent();
}

scoped_refptr<const CalculationExpressionNode>
CalculationValue::GetOrCreateExpression() const {
  if (IsExpression())
    return data_.expression;
  return base::MakeRefCounted<CalculationExpressionPixelsAndPercentNode>(
      GetPixelsAndPercent());
}

scoped_refptr<const CalculationValue> CalculationValue::Blend(
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
    return Create(PixelsAndPercent(pixels, percent), range);
  }

  auto blended_from = CalculationExpressionOperationNode::CreateSimplified(
      CalculationExpressionOperationNode::Children(
          {from.GetOrCreateExpression(),
           base::MakeRefCounted<CalculationExpressionNumberNode>(1.0 -
                                                                 progress)}),
      CalculationOperator::kMultiply);
  auto blended_to = CalculationExpressionOperationNode::CreateSimplified(
      CalculationExpressionOperationNode::Children(
          {GetOrCreateExpression(),
           base::MakeRefCounted<CalculationExpressionNumberNode>(progress)}),
      CalculationOperator::kMultiply);
  auto result_expression = CalculationExpressionOperationNode::CreateSimplified(
      {std::move(blended_from), std::move(blended_to)},
      CalculationOperator::kAdd);
  return CreateSimplified(result_expression, range);
}

scoped_refptr<const CalculationValue>
CalculationValue::SubtractFromOneHundredPercent() const {
  if (!IsExpression()) {
    PixelsAndPercent result(-Pixels(), 100 - Percent());
    return Create(result, Length::ValueRange::kAll);
  }
  auto hundred_percent =
      base::MakeRefCounted<CalculationExpressionPixelsAndPercentNode>(
          PixelsAndPercent(0, 100));
  auto result_expression = CalculationExpressionOperationNode::CreateSimplified(
      CalculationExpressionOperationNode::Children(
          {std::move(hundred_percent), GetOrCreateExpression()}),
      CalculationOperator::kSubtract);
  return CreateSimplified(std::move(result_expression),
                          Length::ValueRange::kAll);
}

scoped_refptr<const CalculationValue> CalculationValue::Add(
    const CalculationValue& other) const {
  CHECK_EQ(GetValueRange(), other.GetValueRange());
  auto result_expression = CalculationExpressionOperationNode::CreateSimplified(
      {GetOrCreateExpression(), other.GetOrCreateExpression()},
      CalculationOperator::kAdd);
  return CreateSimplified(result_expression, GetValueRange());
}

scoped_refptr<const CalculationValue> CalculationValue::Zoom(
    double factor) const {
  if (!IsExpression()) {
    PixelsAndPercent result(Pixels() * factor, Percent());
    return Create(result, GetValueRange());
  }
  return CreateSimplified(data_.expression->Zoom(factor), GetValueRange());
}

bool CalculationValue::HasAnchorQueries() const {
  return IsExpression() && data_.expression->HasAnchorQueries();
}

bool CalculationValue::HasAutoAnchorPositioning() const {
  return IsExpression() && data_.expression->HasAutoAnchorPositioning();
}

}  // namespace blink
