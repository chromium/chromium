// Copyright 2019 The Chromium Authors. All rights reserved.
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
scoped_refptr<CalculationValue> CalculationValue::CreateSimplified(
    scoped_refptr<const CalculationExpressionNode> expression,
    ValueRange range) {
  if (expression->IsLeaf()) {
    return Create(
        To<CalculationExpressionLeafNode>(*expression).GetPixelsAndPercent(),
        range);
  }
  return base::AdoptRef(new CalculationValue(std::move(expression), range));
}

CalculationValue::CalculationValue(
    scoped_refptr<const CalculationExpressionNode> expression,
    ValueRange range)
    : data_(std::move(expression)),
      is_expression_(true),
      is_non_negative_(range == kValueRangeNonNegative) {}

CalculationValue::~CalculationValue() {
  if (is_expression_)
    data_.expression.~scoped_refptr<const CalculationExpressionNode>();
  else
    data_.value.~PixelsAndPercent();
}

float CalculationValue::Evaluate(float max_value) const {
  float value = is_expression_ ? value = data_.expression->Evaluate(max_value)
                               : value = Pixels() + Percent() / 100 * max_value;
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
  return base::MakeRefCounted<CalculationExpressionLeafNode>(
      GetPixelsAndPercent());
}

scoped_refptr<CalculationValue> CalculationValue::Blend(
    const CalculationValue& from,
    double progress,
    ValueRange range) const {
  if (!IsExpression() && !from.IsExpression()) {
    PixelsAndPercent from_pixels_and_percent = from.GetPixelsAndPercent();
    PixelsAndPercent to_pixels_and_percent = GetPixelsAndPercent();
    const float pixels = blink::Blend(from_pixels_and_percent.pixels,
                                      to_pixels_and_percent.pixels, progress);
    const float percent = blink::Blend(from_pixels_and_percent.percent,
                                       to_pixels_and_percent.percent, progress);
    return Create(PixelsAndPercent(pixels, percent), range);
  }

  auto blended_from = CalculationExpressionMultiplicationNode::CreateSimplified(
      from.GetOrCreateExpression(), 1.0 - progress);
  auto blended_to = CalculationExpressionMultiplicationNode::CreateSimplified(
      GetOrCreateExpression(), progress);
  auto result_expression = CalculationExpressionAdditiveNode::CreateSimplified(
      std::move(blended_from), std::move(blended_to),
      CalculationExpressionAdditiveNode::Type::kAdd);
  return CreateSimplified(std::move(result_expression), range);
}

scoped_refptr<CalculationValue>
CalculationValue::SubtractFromOneHundredPercent() const {
  if (!IsExpression()) {
    PixelsAndPercent result(-Pixels(), 100 - Percent());
    return Create(result, kValueRangeAll);
  }
  auto hundred_percent = base::MakeRefCounted<CalculationExpressionLeafNode>(
      PixelsAndPercent(0, 100));
  auto result_expression = CalculationExpressionAdditiveNode::CreateSimplified(
      std::move(hundred_percent), GetOrCreateExpression(),
      CalculationExpressionAdditiveNode::Type::kSubtract);
  return CreateSimplified(std::move(result_expression), kValueRangeAll);
}

scoped_refptr<CalculationValue> CalculationValue::Zoom(double factor) const {
  if (!IsExpression()) {
    PixelsAndPercent result(Pixels() * factor, Percent());
    return Create(result, GetValueRange());
  }
  return CreateSimplified(data_.expression->Zoom(factor), GetValueRange());
}

}  // namespace blink
