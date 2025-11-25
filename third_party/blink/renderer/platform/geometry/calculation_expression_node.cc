// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"

#include <algorithm>
#include <cfloat>
#include <numeric>

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/geometry/math_functions.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

// ------ CalculationExpressionNumberNode ------

float CalculationExpressionNumberNode::Evaluate(float max_value,
                                                const EvaluationInput&) const {
  return value_;
}

bool CalculationExpressionNumberNode::Equals(
    const CalculationExpressionNode& other) const {
  auto* other_number = DynamicTo<CalculationExpressionNumberNode>(other);
  if (!other_number) {
    return false;
  }
  return value_ == other_number->Value();
}

const CalculationExpressionNode* CalculationExpressionNumberNode::Zoom(
    double) const {
  return MakeGarbageCollected<CalculationExpressionNumberNode>(value_);
}

// ------ CalculationExpressionSizingKeywordNode ------

CalculationExpressionSizingKeywordNode::CalculationExpressionSizingKeywordNode(
    Keyword keyword)
    : keyword_(keyword) {
  if (keyword != Keyword::kSize && keyword != Keyword::kAny) {
    if (keyword == Keyword::kAuto) {
      has_auto_ = true;
    } else if (keyword == Keyword::kWebkitFillAvailable ||
               keyword == Keyword::kStretch) {
      has_stretch_ = true;
    } else {
      has_content_or_intrinsic_ = true;
    }
  }
}

float CalculationExpressionSizingKeywordNode::Evaluate(
    float max_value,
    const EvaluationInput& input) const {
  Length::Type intrinsic_type = Length::kFixed;
  switch (keyword_) {
    case Keyword::kSize:
      CHECK(input.size_keyword_basis);
      return *input.size_keyword_basis;
    case Keyword::kAny:
      return 0.0f;
    case Keyword::kAuto:
      intrinsic_type = Length::Type::kAuto;
      break;
    case Keyword::kContent:
      intrinsic_type =
          input.calc_size_keyword_behavior == CalcSizeKeywordBehavior::kAsAuto
              ? Length::Type::kAuto
              : Length::Type::kContent;
      break;
    case Keyword::kMinContent:
    case Keyword::kWebkitMinContent:
      CHECK_EQ(input.calc_size_keyword_behavior,
               CalcSizeKeywordBehavior::kAsSpecified);
      intrinsic_type = Length::Type::kMinContent;
      break;
    case Keyword::kMaxContent:
    case Keyword::kWebkitMaxContent:
      CHECK_EQ(input.calc_size_keyword_behavior,
               CalcSizeKeywordBehavior::kAsSpecified);
      intrinsic_type = Length::Type::kMaxContent;
      break;
    case Keyword::kFitContent:
    case Keyword::kWebkitFitContent:
      intrinsic_type =
          input.calc_size_keyword_behavior == CalcSizeKeywordBehavior::kAsAuto
              ? Length::Type::kAuto
              : Length::Type::kFitContent;
      break;
    case Keyword::kStretch:
    case Keyword::kWebkitFillAvailable:
      intrinsic_type =
          input.calc_size_keyword_behavior == CalcSizeKeywordBehavior::kAsAuto
              ? Length::Type::kAuto
              : Length::Type::kStretch;
      break;
  }

  if (!input.intrinsic_evaluator) {
    // TODO(https://crbug.com/313072): I'd like to be able to CHECK() this
    // instead.  However, we hit this code in three cases:
    //  * the code in ContentMinimumInlineSize, which passes max_value of 0
    //  * the (questionable) code in EvaluateValueIfNaNorInfinity(), which
    //    passes max_value of 1 or -1
    //  * the DCHECK()s in
    //    CSSLengthInterpolationType::ApplyStandardPropertyValue pass a max
    //    value of 100
    // So we have to return something.  Return 0 for now, though this may
    // not be ideal.
    CHECK(max_value == 1.0f || max_value == -1.0f || max_value == 0.0f ||
          max_value == 100.0f);
    return 0.0f;
  }
  CHECK(input.intrinsic_evaluator);
  return (*input.intrinsic_evaluator)(Length(intrinsic_type));
}

// ------ CalculationExpressionColorChannelKeywordNode ------

CalculationExpressionColorChannelKeywordNode::
    CalculationExpressionColorChannelKeywordNode(ColorChannelKeyword channel)
    : channel_(channel) {
  has_color_channel_keyword_ = true;
}

float CalculationExpressionColorChannelKeywordNode::Evaluate(
    float max_value,
    const EvaluationInput& evaluation_input) const {
  // If the calling code hasn't set up the input environment, then always
  // return zero.
  if (evaluation_input.color_channel_keyword_values.empty()) {
    return 0;
  }
  return evaluation_input.color_channel_keyword_values.at(channel_);
}

// ------ CalculationExpressionPixelsAndPercentNode ------

float CalculationExpressionPixelsAndPercentNode::Evaluate(
    float max_value,
    const EvaluationInput&) const {
  return value_.pixels + value_.percent / 100 * max_value;
}

bool CalculationExpressionPixelsAndPercentNode::Equals(
    const CalculationExpressionNode& other) const {
  auto* other_pixels_and_percent =
      DynamicTo<CalculationExpressionPixelsAndPercentNode>(other);
  if (!other_pixels_and_percent) {
    return false;
  }
  return value_.pixels == other_pixels_and_percent->value_.pixels &&
         value_.percent == other_pixels_and_percent->value_.percent;
}

const CalculationExpressionNode*
CalculationExpressionPixelsAndPercentNode::Zoom(double factor) const {
  PixelsAndPercent result(value_.pixels * factor, value_.percent,
                          value_.has_explicit_pixels,
                          value_.has_explicit_percent);
  return MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
      result);
}

// ------ CalculationExpressionOperationNode ------

namespace {

bool ShouldConvertRad2DegForOperator(CalculationOperator op) {
  return op == CalculationOperator::kSin || op == CalculationOperator::kCos ||
         op == CalculationOperator::kTan;
}

}  // namespace

// static
const CalculationExpressionNode*
CalculationExpressionOperationNode::CreateSimplified(Children&& children,
                                                     CalculationOperator op) {
  switch (op) {
    case CalculationOperator::kAdd:
    case CalculationOperator::kSubtract: {
      DCHECK_EQ(children.size(), 2u);
      if (!children[0]->IsPixelsAndPercent() ||
          !children[1]->IsPixelsAndPercent()) {
        return MakeGarbageCollected<CalculationExpressionOperationNode>(
            Children({std::move(children[0]), std::move(children[1])}), op);
      }
      const auto& left_pixels_and_percent =
          To<CalculationExpressionPixelsAndPercentNode>(*children[0]);
      PixelsAndPercent right_pixels_and_percent =
          To<CalculationExpressionPixelsAndPercentNode>(*children[1])
              .GetPixelsAndPercent();
      PixelsAndPercent value = left_pixels_and_percent.GetPixelsAndPercent();
      if (op == CalculationOperator::kAdd) {
        value += right_pixels_and_percent;
      } else {
        value -= right_pixels_and_percent;
      }
      return MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
          value);
    }
    case CalculationOperator::kMultiply: {
      DCHECK_EQ(children.size(), 2u);
      const CalculationExpressionNode& lhs = *children[0];
      const CalculationExpressionNode& rhs = *children[1];
      const bool lhs_is_number = lhs.IsNumber();
      const auto* number_node =
          DynamicTo<CalculationExpressionNumberNode>(lhs_is_number ? lhs : rhs);
      const auto* pixels_and_percent_node =
          DynamicTo<CalculationExpressionPixelsAndPercentNode>(
              lhs_is_number ? rhs : lhs);
      if (!number_node || !pixels_and_percent_node) {
        return MakeGarbageCollected<CalculationExpressionOperationNode>(
            std::move(children), op);
      }
      PixelsAndPercent pixels_and_percent =
          pixels_and_percent_node->GetPixelsAndPercent();
      pixels_and_percent *= number_node->Value();
      return MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
          pixels_and_percent);
    }
    case CalculationOperator::kInvert: {
      DCHECK_EQ(children.size(), 1u);
      auto* number = DynamicTo<CalculationExpressionNumberNode>(*children[0]);
      if (number) {
        return MakeGarbageCollected<CalculationExpressionNumberNode>(
            1.0 / number->Value());
      }
      return MakeGarbageCollected<CalculationExpressionOperationNode>(
          Children({std::move(children[0])}), op);
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
        return MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
            PixelsAndPercent(simplified_px));
      }
      return MakeGarbageCollected<CalculationExpressionOperationNode>(
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
        return MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
            PixelsAndPercent(clamped_px));
      }
      return MakeGarbageCollected<CalculationExpressionOperationNode>(
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
        return MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
            PixelsAndPercent(value));
      } else {
        return MakeGarbageCollected<CalculationExpressionOperationNode>(
            std::move(children), op);
      }
    }
    case CalculationOperator::kLog: {
      DCHECK_GE(children.size(), 1u);
      DCHECK_LE(children.size(), 2u);
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
        float value = operand_pixels.size() == 1u
                          ? std::log(operand_pixels.front())
                          : std::log2(operand_pixels.front()) /
                                std::log2(operand_pixels.back());
        return MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
            PixelsAndPercent(value));
      }
      return MakeGarbageCollected<CalculationExpressionOperationNode>(
          std::move(children), op);
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
        return MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
            PixelsAndPercent(value));
      }
      return MakeGarbageCollected<CalculationExpressionOperationNode>(
          std::move(children), op);
    }
    case CalculationOperator::kSin:
    case CalculationOperator::kCos:
    case CalculationOperator::kTan:
    case CalculationOperator::kAsin:
    case CalculationOperator::kAcos:
    case CalculationOperator::kAtan:
    case CalculationOperator::kAbs:
    case CalculationOperator::kExp:
    case CalculationOperator::kSqrt:
    case CalculationOperator::kSign: {
      DCHECK_EQ(children.size(), 1u);
      const auto* pixels_and_percent =
          DynamicTo<CalculationExpressionPixelsAndPercentNode>(
              *children.front());
      if (!pixels_and_percent || pixels_and_percent->Percent()) {
        return MakeGarbageCollected<CalculationExpressionOperationNode>(
            std::move(children), op);
      }
      float value = pixels_and_percent->Pixels();
      if (op == CalculationOperator::kAbs) {
        return MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
            PixelsAndPercent(std::abs(value)));
      } else if (op == CalculationOperator::kSign) {
        return MakeGarbageCollected<CalculationExpressionNumberNode>(
            EvaluateSignFunction(value));
      } else if (op == CalculationOperator::kExp) {
        return MakeGarbageCollected<CalculationExpressionNumberNode>(
            std::exp(value));
      } else if (op == CalculationOperator::kSqrt) {
        return MakeGarbageCollected<CalculationExpressionNumberNode>(
            std::sqrt(value));
      } else {
        if (ShouldConvertRad2DegForOperator(op) &&
            children.front()->IsNumber()) {
          value = Rad2deg(value);
        }
        value = EvaluateTrigonometricFunction(op, value);
        return MakeGarbageCollected<CalculationExpressionNumberNode>(value);
      }
    }
    case CalculationOperator::kProgress:
    case CalculationOperator::kMediaProgress:
    case CalculationOperator::kContainerProgress: {
      DCHECK_EQ(children.size(), 3u);
      Vector<float, 3> operand_pixels;
      bool can_simplify = true;
      for (const Member<const CalculationExpressionNode>& child : children) {
        const auto* pixels_and_percent =
            DynamicTo<CalculationExpressionPixelsAndPercentNode>(*child);
        if (!pixels_and_percent || pixels_and_percent->Percent()) {
          can_simplify = false;
          break;
        }
        operand_pixels.push_back(pixels_and_percent->Pixels());
      }
      if (can_simplify) {
        float progress_px = operand_pixels[0];
        float from_px = operand_pixels[1];
        float to_px = operand_pixels[2];
        float progress_value = (progress_px - from_px) / (to_px - from_px);
        float progress = std::clamp(progress_value, 0.f, 1.f);
        return MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
            PixelsAndPercent(progress));
      }
      return MakeGarbageCollected<CalculationExpressionOperationNode>(
          std::move(children), op);
    }
    case CalculationOperator::kCalcSize: {
      DCHECK_EQ(children.size(), 2u);
      // TODO(https://crbug.com/313072): It may be worth implementing
      // simplification for calc-size(), but it's not likely to be possible to
      // simplify calc-size() in any of its real use cases.
      return MakeGarbageCollected<CalculationExpressionOperationNode>(
          std::move(children), op);
    }
    case CalculationOperator::kPow: {
      DCHECK_EQ(children.size(), 2u);
      if (children.front()->IsNumber() && children.back()->IsNumber()) {
        float value = std::pow(
            To<CalculationExpressionNumberNode>(*children.front()).Value(),
            To<CalculationExpressionNumberNode>(*children.back()).Value());
        return MakeGarbageCollected<CalculationExpressionNumberNode>(value);
      }
      return MakeGarbageCollected<CalculationExpressionOperationNode>(
          std::move(children), op);
    }
    case CalculationOperator::kAtan2: {
      DCHECK_EQ(children.size(), 2u);
      const auto* a =
          DynamicTo<CalculationExpressionPixelsAndPercentNode>(*children[0]);
      const auto* b =
          DynamicTo<CalculationExpressionPixelsAndPercentNode>(*children[1]);
      bool can_simplify = a && !a->Percent() && b && !b->Percent();
      if (can_simplify) {
        float value =
            EvaluateTrigonometricFunction(op, a->Pixels(), {b->Pixels()});
        return MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
            PixelsAndPercent(value));
      } else {
        return MakeGarbageCollected<CalculationExpressionOperationNode>(
            std::move(children), op);
      }
    }
    case CalculationOperator::kRandom: {
      // First value in the array is random base value.
      // https://drafts.csswg.org/css-values-5/#random-base-value
      DCHECK_GE(children.size(), 3u);
      DCHECK_LE(children.size(), 4u);
      return MakeGarbageCollected<CalculationExpressionOperationNode>(
          std::move(children), op);
    }
  }
}

CalculationExpressionOperationNode::CalculationExpressionOperationNode(
    Children&& children,
    CalculationOperator op)
    : children_(std::move(children)), operator_(op) {
  if (op == CalculationOperator::kCalcSize) {
    // "A calc-size() is treated, in all respects, as if it were its
    // calc-size basis."  This is particularly relevant for ignoring the
    // presence of percentages in the calculation.
    CHECK_EQ(children_.size(), 2u);
    const auto& basis = children_[0];
    has_content_or_intrinsic_ = basis->HasContentOrIntrinsicSize();
    has_auto_ = basis->HasAuto();
    has_percent_ = basis->HasPercent();
    has_stretch_ = basis->HasStretch();
#if DCHECK_IS_ON()
    {
      const auto& calculation = children_[1];
      DCHECK(!calculation->HasAuto());
      DCHECK(!calculation->HasContentOrIntrinsicSize());
      DCHECK(!calculation->HasStretch());
    }
#endif
  } else {
    for (const auto& child : children_) {
      DCHECK(!child->HasAuto());
      DCHECK(!child->HasContentOrIntrinsicSize());
      DCHECK(!child->HasStretch());
      if (child->HasPercent()) {
        has_percent_ = true;
      }
      if (child->HasColorChannelKeyword()) {
        has_color_channel_keyword_ = true;
      }
    }
  }
}

float CalculationExpressionOperationNode::Evaluate(
    float max_value,
    const EvaluationInput& input) const {
  switch (operator_) {
    case CalculationOperator::kAdd: {
      DCHECK_EQ(children_.size(), 2u);
      float left = children_[0]->Evaluate(max_value, input);
      float right = children_[1]->Evaluate(max_value, input);
      return left + right;
    }
    case CalculationOperator::kSubtract: {
      DCHECK_EQ(children_.size(), 2u);
      float left = children_[0]->Evaluate(max_value, input);
      float right = children_[1]->Evaluate(max_value, input);
      return left - right;
    }
    case CalculationOperator::kMultiply: {
      DCHECK_EQ(children_.size(), 2u);
      float left = children_[0]->Evaluate(max_value, input);
      float right = children_[1]->Evaluate(max_value, input);
      return left * right;
    }
    case CalculationOperator::kInvert: {
      DCHECK_EQ(children_.size(), 1u);
      float denominator = children_[0]->Evaluate(max_value, input);
      return 1.0 / denominator;
    }
    case CalculationOperator::kMin: {
      DCHECK(!children_.empty());
      float minimum = children_[0]->Evaluate(max_value, input);
      for (auto& child : children_) {
        minimum = std::min(minimum, child->Evaluate(max_value, input));
      }
      return minimum;
    }
    case CalculationOperator::kMax: {
      DCHECK(!children_.empty());
      float maximum = children_[0]->Evaluate(max_value, input);
      for (auto& child : children_) {
        maximum = std::max(maximum, child->Evaluate(max_value, input));
      }
      return maximum;
    }
    case CalculationOperator::kClamp: {
      DCHECK(!children_.empty());
      float min = children_[0]->Evaluate(max_value, input);
      float val = children_[1]->Evaluate(max_value, input);
      float max = children_[2]->Evaluate(max_value, input);
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
      float a = children_[0]->Evaluate(max_value, input);
      float b = children_[1]->Evaluate(max_value, input);
      return EvaluateSteppedValueFunction(operator_, a, b);
    }
    case CalculationOperator::kLog: {
      DCHECK_GE(children_.size(), 1u);
      DCHECK_LE(children_.size(), 2u);
      return children_.size() == 1u
                 ? std::log(children_.front()->Evaluate(max_value, input))
                 : std::log2(children_.front()->Evaluate(max_value, input)) /
                       std::log2(children_.back()->Evaluate(max_value, input));
    }
    case CalculationOperator::kHypot: {
      DCHECK_GE(children_.size(), 1u);
      float value = 0;
      for (const Member<const CalculationExpressionNode>& operand : children_) {
        float a = operand->Evaluate(max_value, input);
        value = std::hypot(value, a);
      }
      return value;
    }
    case CalculationOperator::kAbs:
    case CalculationOperator::kExp:
    case CalculationOperator::kSqrt:
    case CalculationOperator::kSign: {
      DCHECK_EQ(children_.size(), 1u);
      const float value = children_.front()->Evaluate(max_value, input);
      if (operator_ == CalculationOperator::kAbs) {
        return std::abs(value);
      } else if (operator_ == CalculationOperator::kExp) {
        return std::exp(value);
      } else if (operator_ == CalculationOperator::kSign) {
        return EvaluateSignFunction(value);
      } else {
        return std::sqrt(value);
      }
    }
    case CalculationOperator::kCalcSize: {
      DCHECK_EQ(children_.size(), 2u);
      EvaluationInput calculation_input(input);
      calculation_input.size_keyword_basis =
          children_[0]->Evaluate(max_value, input);
      if (max_value == kIndefiniteSize.ToFloat()) {
        // "When evaluating the calc-size calculation, if percentages are not
        // definite in the given context, the resolve to 0px. Otherwise, they
        // resolve as normal."
        //   -- https://drafts.csswg.org/css-values-5/#resolving-calc-size
        max_value = 0.0f;
      }
      return children_[1]->Evaluate(max_value, calculation_input);
    }
    case CalculationOperator::kProgress:
    case CalculationOperator::kMediaProgress:
    case CalculationOperator::kContainerProgress: {
      DCHECK(!children_.empty());
      float progress = children_[0]->Evaluate(max_value, input);
      float from = children_[1]->Evaluate(max_value, input);
      float to = children_[2]->Evaluate(max_value, input);
      float progress_value = (progress - from) / (to - from);
      return std::clamp(progress_value, 0.f, 1.f);
    }
    case CalculationOperator::kPow: {
      DCHECK_EQ(children_.size(), 2u);
      float a = children_[0]->Evaluate(max_value, input);
      float b = children_[1]->Evaluate(max_value, input);
      return std::pow(a, b);
    }
    case CalculationOperator::kSin:
    case CalculationOperator::kCos:
    case CalculationOperator::kTan:
    case CalculationOperator::kAsin:
    case CalculationOperator::kAcos:
    case CalculationOperator::kAtan:
    case CalculationOperator::kAtan2: {
      float a = children_[0]->Evaluate(max_value, input);
      if (ShouldConvertRad2DegForOperator(operator_) &&
          children_.front()->IsNumber()) {
        a = Rad2deg(a);
      }
      std::optional<float> b =
          operator_ == CalculationOperator::kAtan2
              ? std::optional<float>(children_[1]->Evaluate(max_value, input))
              : std::nullopt;
      return EvaluateTrigonometricFunction(operator_, a, b);
    }
    case CalculationOperator::kRandom: {
      DCHECK_GE(children_.size(), 3u);
      DCHECK_LE(children_.size(), 4u);
      float random_base_value = children_[0]->Evaluate(max_value, input);
      float min = children_[1]->Evaluate(max_value, input);
      float max = children_[2]->Evaluate(max_value, input);
      std::optional<float> step = std::nullopt;
      if (children_.size() == 4u) {
        step = children_[3]->Evaluate(max_value, input);
      }
      return ComputeCSSRandomValue(random_base_value, min, max, step);
    }
      // TODO(crbug.com/1284199): Support other math functions.
  }
  NOTREACHED();
}

bool CalculationExpressionOperationNode::Equals(
    const CalculationExpressionNode& other) const {
  auto* other_operation = DynamicTo<CalculationExpressionOperationNode>(other);
  if (!other_operation) {
    return false;
  }
  if (operator_ != other_operation->GetOperator()) {
    return false;
  }
  using ValueType = Children::value_type;
  return std::ranges::equal(
      children_, other_operation->GetChildren(),
      [](const ValueType& a, const ValueType& b) { return *a == *b; });
}

const CalculationExpressionNode* CalculationExpressionOperationNode::Zoom(
    double factor) const {
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
    case CalculationOperator::kInvert: {
      DCHECK_EQ(children_.size(), 1u);
      return CreateSimplified(Children({children_[0]->Zoom(factor)}),
                              operator_);
    }
    case CalculationOperator::kCalcSize: {
      DCHECK_EQ(children_.size(), 2u);
      return CreateSimplified(
          Children({children_[0], children_[1]->Zoom(factor)}), operator_);
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
    case CalculationOperator::kHypot:
    case CalculationOperator::kAbs:
    case CalculationOperator::kLog:
    case CalculationOperator::kExp:
    case CalculationOperator::kSqrt:
    case CalculationOperator::kSign:
    case CalculationOperator::kProgress:
    case CalculationOperator::kMediaProgress:
    case CalculationOperator::kContainerProgress:
    case CalculationOperator::kSin:
    case CalculationOperator::kCos:
    case CalculationOperator::kTan:
    case CalculationOperator::kAsin:
    case CalculationOperator::kAcos:
    case CalculationOperator::kAtan:
    case CalculationOperator::kAtan2:
    case CalculationOperator::kPow:
    case CalculationOperator::kRandom: {
      DCHECK(children_.size());
      HeapVector<Member<const CalculationExpressionNode>> cloned_operands;
      cloned_operands.reserve(children_.size());
      for (const auto& child : children_)
        cloned_operands.push_back(child->Zoom(factor));
      return CreateSimplified(std::move(cloned_operands), operator_);
    }
  }
}

bool CalculationExpressionOperationNode::HasMinContent() const {
  if (operator_ != CalculationOperator::kCalcSize) {
    return false;
  }
  CHECK_EQ(children_.size(), 2u);
  const auto& basis = children_[0];
  return basis->HasMinContent();
}

bool CalculationExpressionOperationNode::HasMaxContent() const {
  if (operator_ != CalculationOperator::kCalcSize) {
    return false;
  }
  CHECK_EQ(children_.size(), 2u);
  const auto& basis = children_[0];
  return basis->HasMaxContent();
}

bool CalculationExpressionOperationNode::HasFitContent() const {
  if (operator_ != CalculationOperator::kCalcSize) {
    return false;
  }
  CHECK_EQ(children_.size(), 2u);
  const auto& basis = children_[0];
  return basis->HasFitContent();
}

}  // namespace blink
