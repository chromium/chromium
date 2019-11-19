// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"

namespace blink {

// ------ CalculationExpressionLeafNode ------

float CalculationExpressionLeafNode::Evaluate(float max_value) const {
  return value_.pixels + value_.percent / 100 * max_value;
}

bool CalculationExpressionLeafNode::operator==(
    const CalculationExpressionNode& other) const {
  if (!other.IsLeaf())
    return false;
  const auto& other_leaf = To<CalculationExpressionLeafNode>(other);
  return value_.pixels == other_leaf.value_.pixels &&
         value_.percent == other_leaf.value_.percent;
}

scoped_refptr<const CalculationExpressionNode>
CalculationExpressionLeafNode::Zoom(double factor) const {
  PixelsAndPercent result(value_.pixels * factor, value_.percent);
  return base::MakeRefCounted<CalculationExpressionLeafNode>(result);
}

// ------ CalculationExpressionMultiplicationNode ------

// static
scoped_refptr<const CalculationExpressionNode>
CalculationExpressionMultiplicationNode::CreateSimplified(
    scoped_refptr<const CalculationExpressionNode> node,
    float factor) {
  if (!node->IsLeaf()) {
    return base::MakeRefCounted<CalculationExpressionMultiplicationNode>(
        std::move(node), factor);
  }
  const auto& leaf = To<CalculationExpressionLeafNode>(*node);
  PixelsAndPercent value(leaf.Pixels() * factor, leaf.Percent() * factor);
  return base::MakeRefCounted<CalculationExpressionLeafNode>(value);
}

float CalculationExpressionMultiplicationNode::Evaluate(float max_value) const {
  return child_->Evaluate(max_value) * factor_;
}

bool CalculationExpressionMultiplicationNode::operator==(
    const CalculationExpressionNode& other) const {
  if (!other.IsMultiplication())
    return false;
  const auto& other_multiply =
      To<CalculationExpressionMultiplicationNode>(other);
  return factor_ == other_multiply.factor_ && *child_ == *other_multiply.child_;
}

scoped_refptr<const CalculationExpressionNode>
CalculationExpressionMultiplicationNode::Zoom(double factor) const {
  return CreateSimplified(child_->Zoom(factor), factor_);
}

// ------ CalculationExpressionAdditiveNode ------

// static
scoped_refptr<const CalculationExpressionNode>
CalculationExpressionAdditiveNode::CreateSimplified(
    scoped_refptr<const CalculationExpressionNode> lhs,
    scoped_refptr<const CalculationExpressionNode> rhs,
    Type type) {
  if (!lhs->IsLeaf() || !rhs->IsLeaf()) {
    return base::MakeRefCounted<CalculationExpressionAdditiveNode>(
        std::move(lhs), std::move(rhs), type);
  }
  const auto& left_leaf = To<CalculationExpressionLeafNode>(*lhs);
  const auto& right_leaf = To<CalculationExpressionLeafNode>(*rhs);
  PixelsAndPercent value = left_leaf.GetPixelsAndPercent();
  if (type == Type::kAdd) {
    value.pixels += right_leaf.Pixels();
    value.percent += right_leaf.Percent();
  } else {
    value.pixels -= right_leaf.Pixels();
    value.percent -= right_leaf.Percent();
  }
  return base::MakeRefCounted<CalculationExpressionLeafNode>(value);
}

float CalculationExpressionAdditiveNode::Evaluate(float max_value) const {
  if (IsAdd())
    return lhs_->Evaluate(max_value) + rhs_->Evaluate(max_value);
  if (IsSubtract())
    return lhs_->Evaluate(max_value) - rhs_->Evaluate(max_value);
  NOTREACHED();
  return 0;
}

bool CalculationExpressionAdditiveNode::operator==(
    const CalculationExpressionNode& other) const {
  if (!other.IsAdditive())
    return false;
  const auto& other_add_subtract = To<CalculationExpressionAdditiveNode>(other);
  // Do we need to consider add as commutative?
  return type_ == other_add_subtract.type_ &&
         *lhs_ == *other_add_subtract.lhs_ && *rhs_ == *other_add_subtract.rhs_;
}

scoped_refptr<const CalculationExpressionNode>
CalculationExpressionAdditiveNode::Zoom(double factor) const {
  return CreateSimplified(lhs_->Zoom(factor), rhs_->Zoom(factor), type_);
}

// ------ CalculationExpressionComparisonNode ------

// static
scoped_refptr<const CalculationExpressionNode>
CalculationExpressionComparisonNode::CreateSimplified(
    Vector<scoped_refptr<const CalculationExpressionNode>>&& operands,
    Type type) {
  DCHECK(operands.size());
  float simplified_px;
  bool can_simplify = true;
  for (wtf_size_t i = 0; i < operands.size(); ++i) {
    const auto* leaf = DynamicTo<CalculationExpressionLeafNode>(*operands[i]);
    if (!leaf || leaf->Percent()) {
      can_simplify = false;
      break;
    }
    if (!i) {
      simplified_px = leaf->Pixels();
    } else {
      if (type == Type::kMin)
        simplified_px = std::min(simplified_px, leaf->Pixels());
      else
        simplified_px = std::max(simplified_px, leaf->Pixels());
    }
  }
  if (can_simplify) {
    return base::MakeRefCounted<CalculationExpressionLeafNode>(
        PixelsAndPercent(simplified_px, 0));
  }
  return base::MakeRefCounted<CalculationExpressionComparisonNode>(
      std::move(operands), type);
}

float CalculationExpressionComparisonNode::Evaluate(float max_value) const {
  float result = operands_.front()->Evaluate(max_value);
  if (IsMin()) {
    for (wtf_size_t i = 1; i < operands_.size(); ++i)
      result = std::min(result, operands_[i]->Evaluate(max_value));
  } else if (IsMax()) {
    for (wtf_size_t i = 1; i < operands_.size(); ++i)
      result = std::max(result, operands_[i]->Evaluate(max_value));
  } else {
    NOTREACHED();
  }
  return result;
}

bool CalculationExpressionComparisonNode::operator==(
    const CalculationExpressionNode& other) const {
  if (!other.IsComparison())
    return false;
  const auto& other_comparison = To<CalculationExpressionComparisonNode>(other);
  if (type_ != other_comparison.type_)
    return false;
  if (operands_.size() != other_comparison.operands_.size())
    return false;
  // We may consider ignoring operand ordering to allow better memory
  // optimization. The code complexity might not pay off, though.
  for (wtf_size_t i = 0; i < operands_.size(); ++i) {
    if (*operands_[i] != *other_comparison.operands_[i])
      return false;
  }
  return true;
}

scoped_refptr<const CalculationExpressionNode>
CalculationExpressionComparisonNode::Zoom(double factor) const {
  Vector<scoped_refptr<const CalculationExpressionNode>> cloned_operands;
  cloned_operands.ReserveCapacity(operands_.size());
  for (const auto& operand : operands_)
    cloned_operands.push_back(operand->Zoom(factor));
  return CreateSimplified(std::move(cloned_operands), type_);
}

}  // namespace blink
