// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_permission_element_utils.h"

#include "third_party/blink/renderer/platform/geometry/calculation_value.h"
#include "third_party/blink/renderer/platform/geometry/length.h"

namespace blink {

namespace {

const CalculationExpressionNode* BuildFitContentExpr(float factor) {
  const auto* constant_expr =
      MakeGarbageCollected<CalculationExpressionNumberNode>(factor);
  const auto* size_expr =
      MakeGarbageCollected<CalculationExpressionSizingKeywordNode>(
          CalculationExpressionSizingKeywordNode::Keyword::kSize);
  return CalculationExpressionOperationNode::CreateSimplified(
      CalculationExpressionOperationNode::Children({constant_expr, size_expr}),
      CalculationOperator::kMultiply);
}
}  // namespace

const CalculationExpressionNode*
HTMLPermissionElementUtils::BuildLengthBoundExpr(
    const Length& length,
    const CalculationExpressionNode* lower_bound_expr,
    const CalculationExpressionNode* upper_bound_expr) {
  if (lower_bound_expr && upper_bound_expr) {
    return CalculationExpressionOperationNode::CreateSimplified(
        CalculationExpressionOperationNode::Children(
            {lower_bound_expr,
             length.AsCalculationValue()->GetOrCreateExpression(),
             upper_bound_expr}),
        CalculationOperator::kClamp);
  }

  if (lower_bound_expr) {
    return CalculationExpressionOperationNode::CreateSimplified(
        CalculationExpressionOperationNode::Children(
            {lower_bound_expr,
             length.AsCalculationValue()->GetOrCreateExpression()}),
        CalculationOperator::kMax);
  }

  if (upper_bound_expr) {
    return CalculationExpressionOperationNode::CreateSimplified(
        CalculationExpressionOperationNode::Children(
            {upper_bound_expr,
             length.AsCalculationValue()->GetOrCreateExpression()}),
        CalculationOperator::kMin);
  }

  NOTREACHED();
}

// static
Length HTMLPermissionElementUtils::AdjustedBoundedLength(
    const Length& length,
    std::optional<float> lower_bound,
    std::optional<float> upper_bound,
    bool should_multiply_by_content_size) {
  bool is_content_or_stretch =
      length.HasContentOrIntrinsic() || length.HasStretch();

  const Length& length_to_use =
      is_content_or_stretch || length.IsNone() ? Length::Auto() : length;

  // If the |length| is not supported and the |bound| is static, return a simple
  // fixed length.
  if (length_to_use.IsAuto() && !should_multiply_by_content_size) {
    return Length(
        lower_bound.has_value() ? lower_bound.value() : upper_bound.value(),
        Length::Type::kFixed);
  }

  // If the |length| is supported and the |bound| is static, return a
  // min|max|clamp expression-type length.
  if (!should_multiply_by_content_size) {
    const auto* lower_bound_expr =
        lower_bound.has_value()
            ? MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
                  PixelsAndPercent(lower_bound.value()))
            : nullptr;

    const auto* upper_bound_expr =
        upper_bound.has_value()
            ? MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
                  PixelsAndPercent(upper_bound.value()))
            : nullptr;

    // expr = min|max|clamp(bound, length, [bound2])
    const auto* expr =
        BuildLengthBoundExpr(length_to_use, lower_bound_expr, upper_bound_expr);
    return Length(CalculationValue::CreateSimplified(
        expr, Length::ValueRange::kNonNegative));
  }

  // bound_expr = size * bound.
  const auto* lower_bound_expr = lower_bound.has_value()
                                     ? BuildFitContentExpr(lower_bound.value())
                                     : nullptr;
  const auto* upper_bound_expr = upper_bound.has_value()
                                     ? BuildFitContentExpr(upper_bound.value())
                                     : nullptr;

  const CalculationExpressionNode* bound_expr = nullptr;

  if (!length_to_use.IsAuto()) {
    // bound_expr = min|max|clamp(size * bound, length, [size * bound2])
    bound_expr =
        BuildLengthBoundExpr(length_to_use, lower_bound_expr, upper_bound_expr);
  } else {
    bound_expr = lower_bound_expr ? lower_bound_expr : upper_bound_expr;
  }

  // This uses internally the CalculationExpressionSizingKeywordNode to create
  // an expression that depends on the size of the contents of the permission
  // element, in order to set necessary min/max bounds on width and height. If
  // https://drafts.csswg.org/css-values-5/#calc-size is ever abandoned,
  // the functionality should still be kept around in some way that can
  // facilitate this use case.

  const auto* fit_content_expr =
      MakeGarbageCollected<CalculationExpressionSizingKeywordNode>(
          CalculationExpressionSizingKeywordNode::Keyword::kFitContent);

  // expr = calc-size(fit-content, bound_expr)
  const auto* expr = CalculationExpressionOperationNode::CreateSimplified(
      CalculationExpressionOperationNode::Children(
          {fit_content_expr, bound_expr}),
      CalculationOperator::kCalcSize);

  return Length(CalculationValue::CreateSimplified(
      expr, Length::ValueRange::kNonNegative));
}

}  // namespace blink
