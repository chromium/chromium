// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"

#include "base/notreached.h"

namespace blink {

// ------ CalculationExpressionNumberNode ------

float CalculationExpressionNumberNode::Evaluate(float max_value) const {
  return value_;
}

bool CalculationExpressionNumberNode::operator==(
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
    float max_value) const {
  return value_.pixels + value_.percent / 100 * max_value;
}

bool CalculationExpressionPixelsAndPercentNode::operator==(
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
      operand_pixels.ReserveCapacity(children.size());
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
    case CalculationOperator::kInvalid:
      NOTREACHED();
      return nullptr;
  }
}

float CalculationExpressionOperationNode::Evaluate(float max_value) const {
  switch (operator_) {
    case CalculationOperator::kAdd: {
      DCHECK_EQ(children_.size(), 2u);
      float left = children_[0]->Evaluate(max_value);
      float right = children_[1]->Evaluate(max_value);
      return left + right;
    }
    case CalculationOperator::kSubtract: {
      DCHECK_EQ(children_.size(), 2u);
      float left = children_[0]->Evaluate(max_value);
      float right = children_[1]->Evaluate(max_value);
      return left - right;
    }
    case CalculationOperator::kMultiply: {
      DCHECK_EQ(children_.size(), 2u);
      float left = children_[0]->Evaluate(max_value);
      float right = children_[1]->Evaluate(max_value);
      return left * right;
    }
    case CalculationOperator::kMin: {
      DCHECK(!children_.IsEmpty());
      float minimum = children_[0]->Evaluate(max_value);
      for (auto& child : children_)
        minimum = std::min(minimum, child->Evaluate(max_value));
      return minimum;
    }
    case CalculationOperator::kMax: {
      DCHECK(!children_.IsEmpty());
      float maximum = children_[0]->Evaluate(max_value);
      for (auto& child : children_)
        maximum = std::max(maximum, child->Evaluate(max_value));
      return maximum;
    }
    case CalculationOperator::kClamp: {
      DCHECK(!children_.IsEmpty());
      float min = children_[0]->Evaluate(max_value);
      float val = children_[1]->Evaluate(max_value);
      float max = children_[2]->Evaluate(max_value);
      // clamp(MIN, VAL, MAX) is identical to max(MIN, min(VAL, MAX))
      return std::max(min, std::min(val, max));
    }
    case CalculationOperator::kInvalid:
      break;
      // TODO(crbug.com/1284199): Support other math functions.
  }
  NOTREACHED();
  return std::numeric_limits<float>::quiet_NaN();
}

bool CalculationExpressionOperationNode::operator==(
    const CalculationExpressionNode& other) const {
  if (!other.IsOperation())
    return false;
  const auto& other_operation = To<CalculationExpressionOperationNode>(other);
  return operator_ == other_operation.GetOperator() &&
         children_ == other_operation.GetChildren();
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
    case CalculationOperator::kClamp: {
      DCHECK(children_.size());
      Vector<scoped_refptr<const CalculationExpressionNode>> cloned_operands;
      cloned_operands.ReserveCapacity(children_.size());
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
    case CalculationOperator::kClamp: {
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

// ------ CalculationExpressionAnchorQueryNode ------

// static
scoped_refptr<const CalculationExpressionAnchorQueryNode>
CalculationExpressionAnchorQueryNode::CreateAnchor(const AtomicString& name,
                                                   AnchorValue side,
                                                   const Length& fallback) {
  AnchorQueryValue value = {.anchor_side = side};
  return base::MakeRefCounted<CalculationExpressionAnchorQueryNode>(
      AnchorQueryType::kAnchor, name, value, /* percentage */ 0, fallback);
}

// static
scoped_refptr<const CalculationExpressionAnchorQueryNode>
CalculationExpressionAnchorQueryNode::CreateAnchorPercentage(
    const AtomicString& name,
    float percentage,
    const Length& fallback) {
  AnchorQueryValue value = {.anchor_side = AnchorValue::kPercentage};
  return base::MakeRefCounted<CalculationExpressionAnchorQueryNode>(
      AnchorQueryType::kAnchor, name, value, percentage, fallback);
}

//  static
scoped_refptr<const CalculationExpressionAnchorQueryNode>
CalculationExpressionAnchorQueryNode::CreateAnchorSize(const AtomicString& name,
                                                       AnchorSizeValue size,
                                                       const Length& fallback) {
  AnchorQueryValue value = {.anchor_size = size};
  return base::MakeRefCounted<CalculationExpressionAnchorQueryNode>(
      AnchorQueryType::kAnchorSize, name, value, /* percentage */ 0, fallback);
}

bool CalculationExpressionAnchorQueryNode::operator==(
    const CalculationExpressionNode& other) const {
  const auto* other_anchor_query =
      DynamicTo<CalculationExpressionAnchorQueryNode>(other);
  if (!other_anchor_query)
    return false;
  if (type_ != other_anchor_query->type_)
    return false;
  if (anchor_name_ != other_anchor_query->anchor_name_)
    return false;
  if (type_ == AnchorQueryType::kAnchor) {
    if (AnchorSide() != other_anchor_query->AnchorSide())
      return false;
    if (AnchorSide() == AnchorValue::kPercentage &&
        AnchorSidePercentage() != other_anchor_query->AnchorSidePercentage()) {
      return false;
    }
  } else {
    if (AnchorSize() != other_anchor_query->AnchorSize())
      return false;
  }
  if (fallback_ != other_anchor_query->fallback_)
    return false;
  return true;
}

scoped_refptr<const CalculationExpressionNode>
CalculationExpressionAnchorQueryNode::Zoom(double factor) const {
  return base::MakeRefCounted<CalculationExpressionAnchorQueryNode>(
      type_, anchor_name_, value_, side_percentage_, fallback_.Zoom(factor));
}

float CalculationExpressionAnchorQueryNode::Evaluate(float) const {
  // TODO(crbug.com/1309178): Implement.
  return 0;
}

}  // namespace blink
