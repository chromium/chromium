// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"

#include <cfloat>
#include <numeric>

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/geometry/math_functions.h"

namespace blink {

// ------ CalculationExpressionNumberNode ------

float CalculationExpressionNumberNode::Evaluate(
    float max_value,
    const Length::AnchorEvaluator*) const {
  return value_;
}

bool CalculationExpressionNumberNode::Equals(
    const CalculationExpressionNode& other) const {
  if (!other.IsNumber())
    return false;
  const auto& other_number = To<CalculationExpressionNumberNode>(other);
  return value_ == other_number.Value();
}

scoped_refptr<const CalculationExpressionNode>
CalculationExpressionNumberNode::Zoom(double) const {
  return base::MakeRefCounted<CalculationExpressionNumberNode>(value_);
}

#if DCHECK_IS_ON()
CalculationExpressionNode::ResultType
CalculationExpressionNumberNode::ResolvedResultType() const {
  return result_type_;
}
#endif

// ------ CalculationExpressionPixelsAndPercentNode ------

float CalculationExpressionPixelsAndPercentNode::Evaluate(
    float max_value,
    const Length::AnchorEvaluator*) const {
  return value_.pixels + value_.percent / 100 * max_value;
}

bool CalculationExpressionPixelsAndPercentNode::Equals(
    const CalculationExpressionNode& other) const {
  if (!other.IsPixelsAndPercent())
    return false;
  const auto& other_pixels_and_percent =
      To<CalculationExpressionPixelsAndPercentNode>(other);
  return value_.pixels == other_pixels_and_percent.value_.pixels &&
         value_.percent == other_pixels_and_percent.value_.percent;
}

scoped_refptr<const CalculationExpressionNode>
CalculationExpressionPixelsAndPercentNode::Zoom(double factor) const {
  PixelsAndPercent result(value_.pixels * factor, value_.percent);
  return base::MakeRefCounted<CalculationExpressionPixelsAndPercentNode>(
      result);
}

#if DCHECK_IS_ON()
CalculationExpressionNode::ResultType
CalculationExpressionPixelsAndPercentNode::ResolvedResultType() const {
  return result_type_;
}
#endif

// ------ CalculationExpressionOperationNode ------

// static
scoped_refptr<const CalculationExpressionNode>
CalculationExpressionOperationNode::CreateSimplified(Children&& children,
                                                    CalculationOperator op) {
  switch (op) {
    case CalculationOperator::kAdd:
    case CalculationOperator::kSubtract: {
      DCHECK_EQ(children.size(), 2u);
      if (!children[0]->IsPixelsAndPercent() ||
          !children[1]->IsPixelsAndPercent()) {
        return base::MakeRefCounted<CalculationExpressionOperationNode>(
            Children({std::move(children[0]), std::move(children[1])}), op);
      }
      const auto& left_pixels_and_percent =
          To<CalculationExpressionPixelsAndPercentNode>(*children[0]);
      const auto& right_pixels_and_percent =
          To<CalculationExpressionPixelsAndPercentNode>(*children[1]);
      PixelsAndPercent value = left_pixels_and_percent.GetPixelsAndPercent();
      if (op == CalculationOperator::kAdd) {
        value.pixels += right_pixels_and_percent.Pixels();
        value.percent += right_pixels_and_percent.Percent();
      } else {
        value.pixels -= right_pixels_and_percent.Pixels();
        value.percent -= right_pixels_and_percent.Percent();
      }
      return base::MakeRefCounted<CalculationExpressionPixelsAndPercentNode>(
          value);
    }
    case CalculationOperator::kMultiply: {
      DCHECK_EQ(children.size(), 2u);
      auto& maybe_pixels_and_percent_node =
          children[0]->IsNumber() ? children[1] : children[0];
      if (!maybe_pixels_and_percent_node->IsPixelsAndPercent()) {
        return base::MakeRefCounted<CalculationExpressionOperationNode>(
            Children({std::move(children[0]), std::move(children[1])}), op);
      }
      auto& number_node = children[0]->IsNumber() ? children[0] : children[1];
      const auto& number = To<CalculationExpressionNumberNode>(*number_node);
      const auto& pixels_and_percent =
          To<CalculationExpressionPixelsAndPercentNode>(
              *maybe_pixels_and_percent_node);
      PixelsAndPercent value(pixels_and_percent.Pixels() * number.Value(),
                             pixels_and_percent.Percent() * number.Value());
      return base::MakeRefCounted<CalculationExpressionPixelsAndPercentNode>(
          value);
    }
    case CalculationOperator::kMin:
    case CalculationOperator::kMax: {
      DCHECK(children.size());
      float simplified_px;
      bool can_simplify = true;
      for (wtf_size_t i = 0; i < children.size(); ++i) {
        const auto* pixels_and_percent =
            DynamicTo<CalculationExpressionPixelsAndPercentNode>(*children[i]);
        if (!pixels_and_percent || pixels_and_percent->Percent()) {
          can_simplify = false;
          break;
        }
        if (!i) {
          simplified_px = pixels_and_percent->Pixels();
        } else {
          if (op == CalculationOperator::kMin) {
            simplified_px =
                std::min(simplified_px, pixels_and_percent->Pixels());
          } else {
            simplified_px =
                std::max(simplified_px, pixels_and_percent->Pixels());
          }
        }
      }
      if (can_simplify) {
        return base::MakeRefCounted<CalculationExpressionPixelsAndPercentNode>(
            PixelsAndPercent(simplified_px, 0));
      }
      return base::MakeRefCounted<CalculationExpressionOperationNode>(
          std::move(children), op);
    }
    case CalculationOperator::kClamp: {
      DCHECK_EQ(children.size(), 3u);
      Vector<float> operand_pixels;
      operand_pixels.reserve(children.size());
      bool can_simplify = true;
      for (auto& child : children) {
        const auto* pixels_and_percent =
            DynamicTo<CalculationExpressionPixelsAndPercentNode>(*child);
        if (!pixels_and_percent || pixels_and_percent->Percent()) {
          can_simplify = false;
          break;
        }
        operand_pixels.push_back(pixels_and_percent->Pixels());
      }
      if (can_simplify) {
        float min_px = operand_pixels[0];
        float val_px = operand_pixels[1];
        float max_px = operand_pixels[2];
        // clamp(MIN, VAL, MAX) is identical to max(MIN, min(VAL, MAX))
        // according to the spec,
        // https://drafts.csswg.org/css-values-4/#funcdef-clamp.
        float clamped_px = std::max(min_px, std::min(val_px, max_px));
        return base::MakeRefCounted<CalculationExpressionPixelsAndPercentNode>(
            PixelsAndPercent(clamped_px, 0));
      }
      return base::MakeRefCounted<CalculationExpressionOperationNode>(
          std::move(children), op);
    }
    case CalculationOperator::kRoundNearest:
    case CalculationOperator::kRoundUp:
    case CalculationOperator::kRoundDown:
    case CalculationOperator::kRoundToZero:
    case CalculationOperator::kMod:
    case CalculationOperator::kRem: {
      DCHECK_EQ(children.size(), 2u);
      const auto* a =
          DynamicTo<CalculationExpressionPixelsAndPercentNode>(*children[0]);
      const auto* b =
          DynamicTo<CalculationExpressionPixelsAndPercentNode>(*children[1]);
      bool can_simplify = a && !a->Percent() && b && !b->Percent();
      if (can_simplify) {
        float value =
            EvaluateSteppedValueFunction(op, a->Pixels(), b->Pixels());
        return base::MakeRefCounted<CalculationExpressionPixelsAndPercentNode>(
            PixelsAndPercent(value, 0));
      } else {
        return base::MakeRefCounted<CalculationExpressionOperationNode>(
            std::move(children), op);
      }
    }
    case CalculationOperator::kHypot: {
      DCHECK_GE(children.size(), 1u);
      Vector<float> operand_pixels;
      operand_pixels.reserve(children.size());
      bool can_simplify = true;
      for (auto& child : children) {
        const auto* pixels_and_percent =
            DynamicTo<CalculationExpressionPixelsAndPercentNode>(*child);
        if (!pixels_and_percent || pixels_and_percent->Percent()) {
          can_simplify = false;
          break;
        }
        operand_pixels.push_back(pixels_and_percent->Pixels());
      }
      if (can_simplify) {
        float value = 0;
        for (float operand : operand_pixels) {
          value = std::hypot(value, operand);
        }
        return base::MakeRefCounted<CalculationExpressionPixelsAndPercentNode>(
            PixelsAndPercent(value, 0));
      }
      return base::MakeRefCounted<CalculationExpressionOperationNode>(
          std::move(children), op);
    }
    case CalculationOperator::kInvalid:
      NOTREACHED();
      return nullptr;
  }
}

bool CalculationExpressionOperationNode::ComputeHasAnchorQueries() const {
  for (const auto& child : children_) {
    if (child->HasAnchorQueries())
      return true;
  }
  return false;
}

bool CalculationExpressionOperationNode::ComputeHasAutoAnchorPositioning()
    const {
  for (const auto& child : children_) {
    if (child->HasAutoAnchorPositioning()) {
      return true;
    }
  }
  return false;
}

CalculationExpressionOperationNode::CalculationExpressionOperationNode(
    Children&& children,
    CalculationOperator op)
    : children_(std::move(children)), operator_(op) {
#if DCHECK_IS_ON()
  result_type_ = ResolvedResultType();
  DCHECK_NE(result_type_, ResultType::kInvalid);
#endif
  has_anchor_queries_ = ComputeHasAnchorQueries();
  has_auto_anchor_positioning_ = ComputeHasAutoAnchorPositioning();
}

float CalculationExpressionOperationNode::Evaluate(
    float max_value,
    const Length::AnchorEvaluator* anchor_evaluator) const {
  switch (operator_) {
    case CalculationOperator::kAdd: {
      DCHECK_EQ(children_.size(), 2u);
      float left = children_[0]->Evaluate(max_value, anchor_evaluator);
      float right = children_[1]->Evaluate(max_value, anchor_evaluator);
      return left + right;
    }
    case CalculationOperator::kSubtract: {
      DCHECK_EQ(children_.size(), 2u);
      float left = children_[0]->Evaluate(max_value, anchor_evaluator);
      float right = children_[1]->Evaluate(max_value, anchor_evaluator);
      return left - right;
    }
    case CalculationOperator::kMultiply: {
      DCHECK_EQ(children_.size(), 2u);
      float left = children_[0]->Evaluate(max_value, anchor_evaluator);
      float right = children_[1]->Evaluate(max_value, anchor_evaluator);
      return left * right;
    }
    case CalculationOperator::kMin: {
      DCHECK(!children_.empty());
      float minimum = children_[0]->Evaluate(max_value, anchor_evaluator);
      for (auto& child : children_) {
        minimum =
            std::min(minimum, child->Evaluate(max_value, anchor_evaluator));
      }
      return minimum;
    }
    case CalculationOperator::kMax: {
      DCHECK(!children_.empty());
      float maximum = children_[0]->Evaluate(max_value, anchor_evaluator);
      for (auto& child : children_) {
        maximum =
            std::max(maximum, child->Evaluate(max_value, anchor_evaluator));
      }
      return maximum;
    }
    case CalculationOperator::kClamp: {
      DCHECK(!children_.empty());
      float min = children_[0]->Evaluate(max_value, anchor_evaluator);
      float val = children_[1]->Evaluate(max_value, anchor_evaluator);
      float max = children_[2]->Evaluate(max_value, anchor_evaluator);
      // clamp(MIN, VAL, MAX) is identical to max(MIN, min(VAL, MAX))
      return std::max(min, std::min(val, max));
    }
    case CalculationOperator::kRoundNearest:
    case CalculationOperator::kRoundUp:
    case CalculationOperator::kRoundDown:
    case CalculationOperator::kRoundToZero:
    case CalculationOperator::kMod:
    case CalculationOperator::kRem: {
      DCHECK_EQ(children_.size(), 2u);
      float a = children_[0]->Evaluate(max_value, anchor_evaluator);
      float b = children_[1]->Evaluate(max_value, anchor_evaluator);
      return EvaluateSteppedValueFunction(operator_, a, b);
    }
    case CalculationOperator::kHypot: {
      DCHECK_GE(children_.size(), 1u);
      float value = 0;
      for (scoped_refptr<const CalculationExpressionNode> operand : children_) {
        float a = operand->Evaluate(max_value, anchor_evaluator);
        value = std::hypot(value, a);
      }
      return value;
    }
    case CalculationOperator::kInvalid:
      break;
      // TODO(crbug.com/1284199): Support other math functions.
  }
  NOTREACHED();
  return std::numeric_limits<float>::quiet_NaN();
}

bool CalculationExpressionOperationNode::Equals(
    const CalculationExpressionNode& other) const {
  if (!other.IsOperation())
    return false;
  const auto& other_operation = To<CalculationExpressionOperationNode>(other);
  if (operator_ != other_operation.GetOperator())
    return false;
  using ValueType = Children::value_type;
  return base::ranges::equal(
      children_, other_operation.GetChildren(),
      [](const ValueType& a, const ValueType& b) { return *a == *b; });
}

scoped_refptr<const CalculationExpressionNode>
CalculationExpressionOperationNode::Zoom(double factor) const {
  switch (operator_) {
    case CalculationOperator::kAdd:
    case CalculationOperator::kSubtract:
      DCHECK_EQ(children_.size(), 2u);
      return CreateSimplified(
          Children({children_[0]->Zoom(factor), children_[1]->Zoom(factor)}),
          operator_);
    case CalculationOperator::kMultiply: {
      DCHECK_EQ(children_.size(), 2u);
      auto& number = children_[0]->IsNumber() ? children_[0] : children_[1];
      auto& pixels_and_percent =
          children_[0]->IsNumber() ? children_[1] : children_[0];
      return CreateSimplified(
          Children({pixels_and_percent->Zoom(factor), number}), operator_);
    }
    case CalculationOperator::kMin:
    case CalculationOperator::kMax:
    case CalculationOperator::kClamp:
    case CalculationOperator::kRoundNearest:
    case CalculationOperator::kRoundUp:
    case CalculationOperator::kRoundDown:
    case CalculationOperator::kRoundToZero:
    case CalculationOperator::kMod:
    case CalculationOperator::kRem:
    case CalculationOperator::kHypot: {
      DCHECK(children_.size());
      Vector<scoped_refptr<const CalculationExpressionNode>> cloned_operands;
      cloned_operands.reserve(children_.size());
      for (const auto& child : children_)
        cloned_operands.push_back(child->Zoom(factor));
      return CreateSimplified(std::move(cloned_operands), operator_);
    }
    case CalculationOperator::kInvalid:
      NOTREACHED();
      return nullptr;
  }
}

#if DCHECK_IS_ON()
CalculationExpressionNode::ResultType
CalculationExpressionOperationNode::ResolvedResultType() const {
  switch (operator_) {
    case CalculationOperator::kAdd:
    case CalculationOperator::kSubtract: {
      DCHECK_EQ(children_.size(), 2u);
      auto left_type = children_[0]->ResolvedResultType();
      auto right_type = children_[1]->ResolvedResultType();
      if (left_type == ResultType::kInvalid ||
          right_type == ResultType::kInvalid || left_type != right_type)
        return ResultType::kInvalid;

      return left_type;
    }
    case CalculationOperator::kMultiply: {
      DCHECK_EQ(children_.size(), 2u);
      auto left_type = children_[0]->ResolvedResultType();
      auto right_type = children_[1]->ResolvedResultType();
      if (left_type == ResultType::kInvalid ||
          right_type == ResultType::kInvalid ||
          (left_type == ResultType::kPixelsAndPercent &&
           right_type == ResultType::kPixelsAndPercent))
        return ResultType::kInvalid;

      if ((left_type == ResultType::kPixelsAndPercent &&
           right_type == ResultType::kNumber) ||
          (left_type == ResultType::kNumber &&
           right_type == ResultType::kPixelsAndPercent))
        return ResultType::kPixelsAndPercent;

      return ResultType::kNumber;
    }
    case CalculationOperator::kMin:
    case CalculationOperator::kMax:
    case CalculationOperator::kClamp:
    case CalculationOperator::kRoundNearest:
    case CalculationOperator::kRoundUp:
    case CalculationOperator::kRoundDown:
    case CalculationOperator::kRoundToZero:
    case CalculationOperator::kMod:
    case CalculationOperator::kRem:
    case CalculationOperator::kHypot: {
      DCHECK(children_.size());
      auto first_child_type = children_.front()->ResolvedResultType();
      for (const auto& child : children_) {
        if (first_child_type != child->ResolvedResultType())
          return ResultType::kInvalid;
      }

      return first_child_type;
    }
    case CalculationOperator::kInvalid:
      NOTREACHED();
      return result_type_;
  }
}
#endif

}  // namespace blink
