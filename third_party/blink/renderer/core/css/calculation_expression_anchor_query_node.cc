// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/calculation_expression_anchor_query_node.h"
#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/style/anchor_specifier_value.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

// static
scoped_refptr<const CalculationExpressionAnchorQueryNode>
CalculationExpressionAnchorQueryNode::CreateAnchor(
    const AnchorSpecifierValue& anchor_specifier,
    AnchorValue side,
    const Length& fallback) {
  AnchorQueryValue value = {.anchor_side = side};
  return base::MakeRefCounted<CalculationExpressionAnchorQueryNode>(
      AnchorQueryType::kAnchor, anchor_specifier, value, /* percentage */ 0,
      fallback);
}

// static
scoped_refptr<const CalculationExpressionAnchorQueryNode>
CalculationExpressionAnchorQueryNode::CreateAnchorPercentage(
    const AnchorSpecifierValue& anchor_specifier,
    float percentage,
    const Length& fallback) {
  AnchorQueryValue value = {.anchor_side = AnchorValue::kPercentage};
  return base::MakeRefCounted<CalculationExpressionAnchorQueryNode>(
      AnchorQueryType::kAnchor, anchor_specifier, value, percentage, fallback);
}

//  static
scoped_refptr<const CalculationExpressionAnchorQueryNode>
CalculationExpressionAnchorQueryNode::CreateAnchorSize(
    const AnchorSpecifierValue& anchor_specifier,
    AnchorSizeValue size,
    const Length& fallback) {
  AnchorQueryValue value = {.anchor_size = size};
  return base::MakeRefCounted<CalculationExpressionAnchorQueryNode>(
      AnchorQueryType::kAnchorSize, anchor_specifier, value, /* percentage */ 0,
      fallback);
}

bool CalculationExpressionAnchorQueryNode::Equals(
    const CalculationExpressionNode& other) const {
  const auto* other_anchor_query =
      DynamicTo<CalculationExpressionAnchorQueryNode>(other);
  if (!other_anchor_query) {
    return false;
  }
  if (type_ != other_anchor_query->type_) {
    return false;
  }
  if (!base::ValuesEquivalent(anchor_specifier_,
                              other_anchor_query->anchor_specifier_)) {
    return false;
  }
  if (type_ == AnchorQueryType::kAnchor) {
    if (AnchorSide() != other_anchor_query->AnchorSide()) {
      return false;
    }
    if (AnchorSide() == AnchorValue::kPercentage &&
        AnchorSidePercentage() != other_anchor_query->AnchorSidePercentage()) {
      return false;
    }
  } else {
    if (AnchorSize() != other_anchor_query->AnchorSize()) {
      return false;
    }
  }
  if (fallback_ != other_anchor_query->fallback_) {
    return false;
  }
  return true;
}

scoped_refptr<const CalculationExpressionNode>
CalculationExpressionAnchorQueryNode::Zoom(double factor) const {
  return base::MakeRefCounted<CalculationExpressionAnchorQueryNode>(
      type_, *anchor_specifier_, value_, side_percentage_,
      fallback_.Zoom(factor));
}

float CalculationExpressionAnchorQueryNode::Evaluate(
    float max_value,
    const Length::AnchorEvaluator* anchor_evaluator) const {
  if (anchor_evaluator) {
    if (const absl::optional<LayoutUnit> value =
            anchor_evaluator->Evaluate(*this)) {
      return value->ToFloat();
    }
    return FloatValueForLength(fallback_, max_value, anchor_evaluator);
  }
  return 0;
}

}  // namespace blink
