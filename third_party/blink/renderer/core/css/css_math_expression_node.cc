/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/css_math_expression_node.h"

#include <algorithm>
#include <cfloat>
#include <numeric>
#include <tuple>

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/css/css_color_channel_keywords.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_math_operator.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value_clamping_utils.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/try_tactic_transform.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/style/anchor_specifier_value.h"
#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/geometry/math_functions.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/gfx/geometry/sin_cos_degrees.h"

namespace blink {

static CalculationResultCategory UnitCategory(
    CSSPrimitiveValue::UnitType type) {
  switch (type) {
    case CSSPrimitiveValue::UnitType::kNumber:
    case CSSPrimitiveValue::UnitType::kInteger:
      return kCalcNumber;
    case CSSPrimitiveValue::UnitType::kPercentage:
      return kCalcPercent;
    case CSSPrimitiveValue::UnitType::kEms:
    case CSSPrimitiveValue::UnitType::kExs:
    case CSSPrimitiveValue::UnitType::kPixels:
    case CSSPrimitiveValue::UnitType::kCentimeters:
    case CSSPrimitiveValue::UnitType::kMillimeters:
    case CSSPrimitiveValue::UnitType::kQuarterMillimeters:
    case CSSPrimitiveValue::UnitType::kInches:
    case CSSPrimitiveValue::UnitType::kPoints:
    case CSSPrimitiveValue::UnitType::kPicas:
    case CSSPrimitiveValue::UnitType::kUserUnits:
    case CSSPrimitiveValue::UnitType::kRems:
    case CSSPrimitiveValue::UnitType::kChs:
    case CSSPrimitiveValue::UnitType::kViewportWidth:
    case CSSPrimitiveValue::UnitType::kViewportHeight:
    case CSSPrimitiveValue::UnitType::kViewportMin:
    case CSSPrimitiveValue::UnitType::kViewportMax:
    case CSSPrimitiveValue::UnitType::kRexs:
    case CSSPrimitiveValue::UnitType::kRchs:
    case CSSPrimitiveValue::UnitType::kRics:
    case CSSPrimitiveValue::UnitType::kRlhs:
    case CSSPrimitiveValue::UnitType::kIcs:
    case CSSPrimitiveValue::UnitType::kLhs:
    case CSSPrimitiveValue::UnitType::kCaps:
    case CSSPrimitiveValue::UnitType::kRcaps:
    case CSSPrimitiveValue::UnitType::kViewportInlineSize:
    case CSSPrimitiveValue::UnitType::kViewportBlockSize:
    case CSSPrimitiveValue::UnitType::kSmallViewportWidth:
    case CSSPrimitiveValue::UnitType::kSmallViewportHeight:
    case CSSPrimitiveValue::UnitType::kSmallViewportInlineSize:
    case CSSPrimitiveValue::UnitType::kSmallViewportBlockSize:
    case CSSPrimitiveValue::UnitType::kSmallViewportMin:
    case CSSPrimitiveValue::UnitType::kSmallViewportMax:
    case CSSPrimitiveValue::UnitType::kLargeViewportWidth:
    case CSSPrimitiveValue::UnitType::kLargeViewportHeight:
    case CSSPrimitiveValue::UnitType::kLargeViewportInlineSize:
    case CSSPrimitiveValue::UnitType::kLargeViewportBlockSize:
    case CSSPrimitiveValue::UnitType::kLargeViewportMin:
    case CSSPrimitiveValue::UnitType::kLargeViewportMax:
    case CSSPrimitiveValue::UnitType::kDynamicViewportWidth:
    case CSSPrimitiveValue::UnitType::kDynamicViewportHeight:
    case CSSPrimitiveValue::UnitType::kDynamicViewportInlineSize:
    case CSSPrimitiveValue::UnitType::kDynamicViewportBlockSize:
    case CSSPrimitiveValue::UnitType::kDynamicViewportMin:
    case CSSPrimitiveValue::UnitType::kDynamicViewportMax:
    case CSSPrimitiveValue::UnitType::kContainerWidth:
    case CSSPrimitiveValue::UnitType::kContainerHeight:
    case CSSPrimitiveValue::UnitType::kContainerInlineSize:
    case CSSPrimitiveValue::UnitType::kContainerBlockSize:
    case CSSPrimitiveValue::UnitType::kContainerMin:
    case CSSPrimitiveValue::UnitType::kContainerMax:
      return kCalcLength;
    case CSSPrimitiveValue::UnitType::kDegrees:
    case CSSPrimitiveValue::UnitType::kGradians:
    case CSSPrimitiveValue::UnitType::kRadians:
    case CSSPrimitiveValue::UnitType::kTurns:
      return kCalcAngle;
    case CSSPrimitiveValue::UnitType::kMilliseconds:
    case CSSPrimitiveValue::UnitType::kSeconds:
      return kCalcTime;
    case CSSPrimitiveValue::UnitType::kHertz:
    case CSSPrimitiveValue::UnitType::kKilohertz:
      return kCalcFrequency;

    // Resolution units
    case CSSPrimitiveValue::UnitType::kDotsPerPixel:
    case CSSPrimitiveValue::UnitType::kX:
    case CSSPrimitiveValue::UnitType::kDotsPerInch:
    case CSSPrimitiveValue::UnitType::kDotsPerCentimeter:
      return kCalcResolution;

    // Identifier
    case CSSPrimitiveValue::UnitType::kIdent:
      return kCalcIdent;

    default:
      return kCalcOther;
  }
}

CSSMathOperator CSSValueIDToCSSMathOperator(CSSValueID id) {
  switch (id) {
#define CONVERSION_CASE(value_id) \
  case CSSValueID::value_id:      \
    return CSSMathOperator::value_id;

    CONVERSION_CASE(kProgress)
    CONVERSION_CASE(kMediaProgress)
    CONVERSION_CASE(kContainerProgress)

#undef CONVERSION_CASE
    default:
      NOTREACHED();
  }
}

static bool HasDoubleValue(CSSPrimitiveValue::UnitType type) {
  switch (type) {
    case CSSPrimitiveValue::UnitType::kNumber:
    case CSSPrimitiveValue::UnitType::kPercentage:
    case CSSPrimitiveValue::UnitType::kEms:
    case CSSPrimitiveValue::UnitType::kExs:
    case CSSPrimitiveValue::UnitType::kChs:
    case CSSPrimitiveValue::UnitType::kIcs:
    case CSSPrimitiveValue::UnitType::kLhs:
    case CSSPrimitiveValue::UnitType::kCaps:
    case CSSPrimitiveValue::UnitType::kRcaps:
    case CSSPrimitiveValue::UnitType::kRlhs:
    case CSSPrimitiveValue::UnitType::kRems:
    case CSSPrimitiveValue::UnitType::kRexs:
    case CSSPrimitiveValue::UnitType::kRchs:
    case CSSPrimitiveValue::UnitType::kRics:
    case CSSPrimitiveValue::UnitType::kPixels:
    case CSSPrimitiveValue::UnitType::kCentimeters:
    case CSSPrimitiveValue::UnitType::kMillimeters:
    case CSSPrimitiveValue::UnitType::kQuarterMillimeters:
    case CSSPrimitiveValue::UnitType::kInches:
    case CSSPrimitiveValue::UnitType::kPoints:
    case CSSPrimitiveValue::UnitType::kPicas:
    case CSSPrimitiveValue::UnitType::kUserUnits:
    case CSSPrimitiveValue::UnitType::kDegrees:
    case CSSPrimitiveValue::UnitType::kRadians:
    case CSSPrimitiveValue::UnitType::kGradians:
    case CSSPrimitiveValue::UnitType::kTurns:
    case CSSPrimitiveValue::UnitType::kMilliseconds:
    case CSSPrimitiveValue::UnitType::kSeconds:
    case CSSPrimitiveValue::UnitType::kHertz:
    case CSSPrimitiveValue::UnitType::kKilohertz:
    case CSSPrimitiveValue::UnitType::kViewportWidth:
    case CSSPrimitiveValue::UnitType::kViewportHeight:
    case CSSPrimitiveValue::UnitType::kViewportMin:
    case CSSPrimitiveValue::UnitType::kViewportMax:
    case CSSPrimitiveValue::UnitType::kContainerWidth:
    case CSSPrimitiveValue::UnitType::kContainerHeight:
    case CSSPrimitiveValue::UnitType::kContainerInlineSize:
    case CSSPrimitiveValue::UnitType::kContainerBlockSize:
    case CSSPrimitiveValue::UnitType::kContainerMin:
    case CSSPrimitiveValue::UnitType::kContainerMax:
    case CSSPrimitiveValue::UnitType::kDotsPerPixel:
    case CSSPrimitiveValue::UnitType::kX:
    case CSSPrimitiveValue::UnitType::kDotsPerInch:
    case CSSPrimitiveValue::UnitType::kDotsPerCentimeter:
    case CSSPrimitiveValue::UnitType::kFlex:
    case CSSPrimitiveValue::UnitType::kInteger:
      return true;
    default:
      return false;
  }
}

namespace {

double TanDegrees(double degrees) {
  // Use table values for tan() if possible.
  // We pick a pretty arbitrary limit that should be safe.
  if (degrees > -90000000.0 && degrees < 90000000.0) {
    // Make sure 0, 45, 90, 135, 180, 225 and 270 degrees get exact results.
    double n45degrees = degrees / 45.0;
    int octant = static_cast<int>(n45degrees);
    if (octant == n45degrees) {
      constexpr std::array<double, 8> kTanN45 = {
          /* 0deg */ 0.0,
          /* 45deg */ 1.0,
          /* 90deg */ std::numeric_limits<double>::infinity(),
          /* 135deg */ -1.0,
          /* 180deg */ 0.0,
          /* 225deg */ 1.0,
          /* 270deg */ -std::numeric_limits<double>::infinity(),
          /* 315deg */ -1.0,
      };
      return kTanN45[octant & 7];
    }
  }
  // Slow path for non-table cases.
  double x = Deg2rad(degrees);
  return std::tan(x);
}

const PixelsAndPercent CreateClampedSamePixelsAndPercent(float value) {
  return PixelsAndPercent(CSSValueClampingUtils::ClampLength(value),
                          CSSValueClampingUtils::ClampLength(value),
                          /*has_explicit_pixels=*/true,
                          /*has_explicit_percent=*/true);
}

bool IsNaN(PixelsAndPercent value, bool allows_negative_percentage_reference) {
  if (std::isnan(value.pixels + value.percent) ||
      (allows_negative_percentage_reference && std::isinf(value.percent))) {
    return true;
  }
  return false;
}

std::optional<PixelsAndPercent> EvaluateValueIfNaNorInfinity(
    scoped_refptr<const blink::CalculationExpressionNode> value,
    bool allows_negative_percentage_reference) {
  // |input| is not needed because this function is just for handling
  // inf and NaN.
  float evaluated_value = value->Evaluate(1, {});
  if (!std::isfinite(evaluated_value)) {
    return CreateClampedSamePixelsAndPercent(evaluated_value);
  }
  if (allows_negative_percentage_reference) {
    evaluated_value = value->Evaluate(-1, {});
    if (!std::isfinite(evaluated_value)) {
      return CreateClampedSamePixelsAndPercent(evaluated_value);
    }
  }
  return std::nullopt;
}

bool IsAllowedMediaFeature(const CSSValueID& id) {
  return id == CSSValueID::kWidth || id == CSSValueID::kHeight;
}

// TODO(crbug.com/40944203): For now we only support width and height
// size features.
bool IsAllowedContainerFeature(const CSSValueID& id) {
  return id == CSSValueID::kWidth || id == CSSValueID::kHeight;
}

bool CheckProgressFunctionTypes(
    CSSValueID function_id,
    const CSSMathExpressionOperation::Operands& nodes) {
  switch (function_id) {
    case CSSValueID::kProgress: {
      CalculationResultCategory first_category = nodes[0]->Category();
      if (first_category != nodes[1]->Category() ||
          first_category != nodes[2]->Category() ||
          first_category == CalculationResultCategory::kCalcIntrinsicSize) {
        return false;
      }
      break;
    }
    // TODO(crbug.com/40944203): For now we only support kCalcLength media
    // features
    case CSSValueID::kMediaProgress: {
      if (!IsAllowedMediaFeature(
              To<CSSMathExpressionKeywordLiteral>(*nodes[0]).GetValue())) {
        return false;
      }
      if (nodes[1]->Category() != CalculationResultCategory::kCalcLength ||
          nodes[2]->Category() != CalculationResultCategory::kCalcLength) {
        return false;
      }
      break;
    }
    case CSSValueID::kContainerProgress: {
      if (!IsAllowedContainerFeature(
              To<CSSMathExpressionContainerFeature>(*nodes[0]).GetValue())) {
        return false;
      }
      if (nodes[1]->Category() != CalculationResultCategory::kCalcLength ||
          nodes[2]->Category() != CalculationResultCategory::kCalcLength) {
        return false;
      }
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return true;
}

bool CanEagerlySimplify(const CSSMathExpressionNode* operand) {
  if (operand->IsOperation()) {
    return false;
  }

  switch (operand->Category()) {
    case CalculationResultCategory::kCalcNumber:
    case CalculationResultCategory::kCalcAngle:
    case CalculationResultCategory::kCalcTime:
    case CalculationResultCategory::kCalcFrequency:
    case CalculationResultCategory::kCalcResolution:
      return true;
    case CalculationResultCategory::kCalcLength:
      return !CSSPrimitiveValue::IsRelativeUnit(operand->ResolvedUnitType()) &&
             !operand->IsAnchorQuery();
    default:
      return false;
  }
}

bool CanEagerlySimplify(const CSSMathExpressionOperation::Operands& operands) {
  for (const CSSMathExpressionNode* operand : operands) {
    if (!CanEagerlySimplify(operand)) {
      return false;
    }
  }
  return true;
}

enum class ProgressArgsSimplificationStatus {
  kAllArgsResolveToCanonical,
  kAllArgsHaveSameType,
  kCanNotSimplify,
};

// Either all the arguments are numerics and have the same unit type (e.g.
// progress(1em from 0em to 1em)), or they are all numerics and can be resolved
// to the canonical unit (e.g. progress(1deg from 0rad to 1deg)). Note: this
// can't be eagerly simplified - progress(1em from 0px to 1em).
ProgressArgsSimplificationStatus CanEagerlySimplifyProgressArgs(
    const CSSMathExpressionOperation::Operands& operands) {
  if (std::all_of(operands.begin(), operands.end(),
                  [](const CSSMathExpressionNode* node) {
                    return node->IsNumericLiteral() &&
                           node->ComputeValueInCanonicalUnit().has_value();
                  })) {
    return ProgressArgsSimplificationStatus::kAllArgsResolveToCanonical;
  }
  if (std::all_of(operands.begin(), operands.end(),
                  [&](const CSSMathExpressionNode* node) {
                    return node->IsNumericLiteral() &&
                           node->ResolvedUnitType() ==
                               operands.front()->ResolvedUnitType();
                  })) {
    return ProgressArgsSimplificationStatus::kAllArgsHaveSameType;
  }
  return ProgressArgsSimplificationStatus::kCanNotSimplify;
}

using UnitsHashMap = HashMap<CSSPrimitiveValue::UnitType, double>;
struct CSSMathExpressionNodeWithOperator {
  DISALLOW_NEW();

 public:
  CSSMathOperator op;
  Member<const CSSMathExpressionNode> node;

  CSSMathExpressionNodeWithOperator(CSSMathOperator op,
                                    const CSSMathExpressionNode* node)
      : op(op), node(node) {}

  void Trace(Visitor* visitor) const { visitor->Trace(node); }
};
using UnitsVector = HeapVector<CSSMathExpressionNodeWithOperator>;
using UnitsVectorHashMap =
    HeapHashMap<CSSPrimitiveValue::UnitType, Member<UnitsVector>>;

bool IsNumericNodeWithDoubleValue(const CSSMathExpressionNode* node) {
  return node->IsNumericLiteral() && HasDoubleValue(node->ResolvedUnitType());
}

const CSSMathExpressionNode* MaybeNegateFirstNode(
    CSSMathOperator op,
    const CSSMathExpressionNode* node) {
  // If first node's operator is -, negate the value.
  if (IsNumericNodeWithDoubleValue(node) && op == CSSMathOperator::kSubtract) {
    return CSSMathExpressionNumericLiteral::Create(-node->DoubleValue(),
                                                   node->ResolvedUnitType());
  }
  return node;
}

CSSMathOperator MaybeChangeOperatorSignIfNesting(bool is_in_nesting,
                                                 CSSMathOperator outer_op,
                                                 CSSMathOperator current_op) {
  // For the cases like "a - (b + c)" we need to turn + c into - c.
  if (is_in_nesting && outer_op == CSSMathOperator::kSubtract &&
      current_op == CSSMathOperator::kAdd) {
    return CSSMathOperator::kSubtract;
  }
  // For the cases like "a - (b - c)" we need to turn - c into + c.
  if (is_in_nesting && outer_op == CSSMathOperator::kSubtract &&
      current_op == CSSMathOperator::kSubtract) {
    return CSSMathOperator::kAdd;
  }
  // No need to change the sign.
  return current_op;
}

CSSMathExpressionNodeWithOperator MaybeReplaceNodeWithCombined(
    const CSSMathExpressionNode* node,
    CSSMathOperator op,
    const UnitsHashMap& units_map) {
  if (!node->IsNumericLiteral()) {
    return {op, node};
  }
  CSSPrimitiveValue::UnitType unit_type = node->ResolvedUnitType();
  auto it = units_map.find(unit_type);
  if (it != units_map.end()) {
    double value = it->value;
    CSSMathOperator new_op =
        value < 0.0f ? CSSMathOperator::kSubtract : CSSMathOperator::kAdd;
    CSSMathExpressionNode* new_node =
        CSSMathExpressionNumericLiteral::Create(std::abs(value), unit_type);
    return {new_op, new_node};
  }
  return {op, node};
}

// This function combines numeric values that have double value and are of the
// same unit type together in numeric_children and saves all the non add/sub
// operation children and their correct simplified operator in all_children.
void CombineNumericChildrenFromNode(const CSSMathExpressionNode* root,
                                    CSSMathOperator op,
                                    UnitsHashMap& numeric_children,
                                    UnitsVector& all_children,
                                    bool is_in_nesting = false) {
  const CSSPrimitiveValue::UnitType unit_type = root->ResolvedUnitType();
  // Go deeper inside the operation node if possible.
  if (auto* operation = DynamicTo<CSSMathExpressionOperation>(root);
      operation && operation->IsAddOrSubtract()) {
    const CSSMathOperator operation_op = operation->OperatorType();
    is_in_nesting |= operation->IsNestedCalc();
    // Nest from the left (first op) to the right (second op).
    CombineNumericChildrenFromNode(operation->GetOperands().front(), op,
                                   numeric_children, all_children,
                                   is_in_nesting);
    // Change the sign of expression, if we are nesting (inside brackets).
    op = MaybeChangeOperatorSignIfNesting(is_in_nesting, op, operation_op);
    CombineNumericChildrenFromNode(operation->GetOperands().back(), op,
                                   numeric_children, all_children,
                                   is_in_nesting);
    return;
  }
  // If we have numeric with double value - combine under one unit type.
  if (IsNumericNodeWithDoubleValue(root)) {
    double value = op == CSSMathOperator::kAdd ? root->DoubleValue()
                                               : -root->DoubleValue();
    if (auto it = numeric_children.find(unit_type);
        it != numeric_children.end()) {
      it->value += value;
    } else {
      numeric_children.insert(unit_type, value);
    }
  }
  // Save all non add/sub operations.
  all_children.emplace_back(op, root);
}

// This function collects numeric values that have double value
// in the numeric_children vector under the same type and saves all the complex
// children and their correct simplified operator in complex_children.
void CollectNumericChildrenFromNode(const CSSMathExpressionNode* root,
                                    CSSMathOperator op,
                                    UnitsVectorHashMap& numeric_children,
                                    UnitsVector& complex_children,
                                    bool is_in_nesting = false) {
  // Go deeper inside the operation node if possible.
  if (auto* operation = DynamicTo<CSSMathExpressionOperation>(root);
      operation && operation->IsAddOrSubtract()) {
    const CSSMathOperator operation_op = operation->OperatorType();
    is_in_nesting |= operation->IsNestedCalc();
    // Nest from the left (first op) to the right (second op).
    CollectNumericChildrenFromNode(operation->GetOperands().front(), op,
                                   numeric_children, complex_children,
                                   is_in_nesting);
    // Change the sign of expression, if we are nesting (inside brackets).
    op = MaybeChangeOperatorSignIfNesting(is_in_nesting, op, operation_op);
    CollectNumericChildrenFromNode(operation->GetOperands().back(), op,
                                   numeric_children, complex_children,
                                   is_in_nesting);
    return;
  }
  CSSPrimitiveValue::UnitType unit_type = root->ResolvedUnitType();
  // If we have numeric with double value - collect in numeric_children.
  if (IsNumericNodeWithDoubleValue(root)) {
    if (auto it = numeric_children.find(unit_type);
        it != numeric_children.end()) {
      it->value->emplace_back(op, root);
    } else {
      numeric_children.insert(
          unit_type, MakeGarbageCollected<UnitsVector>(
                         1, CSSMathExpressionNodeWithOperator(op, root)));
    }
    return;
  }
  // Save all non add/sub operations.
  complex_children.emplace_back(op, root);
}

CSSMathExpressionNode* AddNodeToSumNode(CSSMathExpressionNode* sum_node,
                                        const CSSMathExpressionNode* node,
                                        CSSMathOperator op) {
  // If the sum node is nullptr, create and return the numeric literal node.
  if (!sum_node) {
    return MaybeNegateFirstNode(op, node)->Copy();
  }
  // If the node is numeric with double values,
  // add the numeric literal node with |value| and
  // operator to match the value's sign.
  if (IsNumericNodeWithDoubleValue(node)) {
    double value = node->DoubleValue();
    CSSMathExpressionNode* new_node = CSSMathExpressionNumericLiteral::Create(
        std::abs(value), node->ResolvedUnitType());
    // Change the operator correctly.
    if (value < 0.0f && op == CSSMathOperator::kAdd) {
      // + -10 -> -10
      op = CSSMathOperator::kSubtract;
    } else if (value < 0.0f && op == CSSMathOperator::kSubtract) {
      // - -10 -> + 10.
      op = CSSMathOperator::kAdd;
    }
    return MakeGarbageCollected<CSSMathExpressionOperation>(
        sum_node, new_node, op, sum_node->Category());
  }
  // Add the node to the sum_node otherwise.
  return MakeGarbageCollected<CSSMathExpressionOperation>(sum_node, node, op,
                                                          sum_node->Category());
}

CSSMathExpressionNode* AddNodesVectorToSumNode(CSSMathExpressionNode* sum_node,
                                               const UnitsVector& vector) {
  for (const auto& [op, node] : vector) {
    sum_node = AddNodeToSumNode(sum_node, node, op);
  }
  return sum_node;
}

// This function follows:
// https://drafts.csswg.org/css-values-4/#sort-a-calculations-children
// As in Blink the math expression tree is binary, we need to collect all the
// elements of this tree together and create a new tree as a result.
CSSMathExpressionNode* MaybeSortSumNode(
    const CSSMathExpressionOperation* root) {
  CHECK(root->IsAddOrSubtract());
  CHECK_EQ(root->GetOperands().size(), 2u);
  // Hash map of vectors of numeric literal values with double value with the
  // same unit type.
  UnitsVectorHashMap numeric_children;
  // Vector of all non add/sub operation children.
  UnitsVector complex_children;
  // Collect all the numeric literal with double value in one vector.
  // Note: using kAdd here as the operator for the first child
  // (e.g. a - b = +a - b, a + b = +a + b)
  CollectNumericChildrenFromNode(root, CSSMathOperator::kAdd, numeric_children,
                                 complex_children, false);
  // Form the final node.
  CSSMathExpressionNode* final_node = nullptr;
  // From spec: If nodes contains a number, remove it from nodes and append it
  // to ret.
  if (auto it = numeric_children.find(CSSPrimitiveValue::UnitType::kNumber);
      it != numeric_children.end()) {
    final_node = AddNodesVectorToSumNode(final_node, *it->value);
    numeric_children.erase(it);
  }
  // From spec: If nodes contains a percentage, remove it from nodes and append
  // it to ret.
  if (auto it = numeric_children.find(CSSPrimitiveValue::UnitType::kPercentage);
      it != numeric_children.end()) {
    final_node = AddNodesVectorToSumNode(final_node, *it->value);
    numeric_children.erase(it);
  }
  // Now, sort the rest numeric values alphabatically.
  // From spec: If nodes contains any dimensions, remove them from nodes, sort
  // them by their units, ordered ASCII case-insensitively, and append them to
  // ret.
  auto comp = [&](const CSSPrimitiveValue::UnitType& key_a,
                  const CSSPrimitiveValue::UnitType& key_b) {
    return strcmp(CSSPrimitiveValue::UnitTypeToString(key_a),
                  CSSPrimitiveValue::UnitTypeToString(key_b)) < 0;
  };
  Vector<CSSPrimitiveValue::UnitType> keys;
  keys.reserve(numeric_children.size());
  for (const CSSPrimitiveValue::UnitType& key : numeric_children.Keys()) {
    keys.push_back(key);
  }
  std::sort(keys.begin(), keys.end(), comp);
  // Now, add those numeric nodes in the sorted order.
  for (const auto& unit_type : keys) {
    final_node =
        AddNodesVectorToSumNode(final_node, *numeric_children.at(unit_type));
  }
  // Now, add all the complex (non-numerics with double value) values.
  final_node = AddNodesVectorToSumNode(final_node, complex_children);
  return final_node;
}

// This function follows:
// https://drafts.csswg.org/css-values-4/#calc-simplification
// As in Blink the math expression tree is binary, we need to collect all the
// elements of this tree together and create a new tree as a result.
CSSMathExpressionNode* MaybeSimplifySumNode(
    const CSSMathExpressionOperation* root) {
  CHECK(root->IsAddOrSubtract());
  CHECK_EQ(root->GetOperands().size(), 2u);
  // Hash map of numeric literal values of the same type, that can be
  // combined together.
  UnitsHashMap numeric_children;
  // Vector of all non add/sub operation children.
  UnitsVector all_children;
  // Collect all the numeric literal values together.
  // Note: using kAdd here as the operator for the first child
  // (e.g. a - b = +a - b, a + b = +a + b)
  CombineNumericChildrenFromNode(root, CSSMathOperator::kAdd, numeric_children,
                                 all_children);
  // Form the final node.
  HashSet<CSSPrimitiveValue::UnitType> used_units;
  CSSMathExpressionNode* final_node = nullptr;
  for (const auto& child : all_children) {
    auto [op, node] =
        MaybeReplaceNodeWithCombined(child.node, child.op, numeric_children);
    CSSPrimitiveValue::UnitType unit_type = node->ResolvedUnitType();
    // Skip already used unit types, as they have been already combined.
    if (IsNumericNodeWithDoubleValue(node)) {
      if (used_units.Contains(unit_type)) {
        continue;
      }
      used_units.insert(unit_type);
    }
    if (!final_node) {
      // First child.
      final_node = MaybeNegateFirstNode(op, node)->Copy();
      continue;
    }
    final_node = MakeGarbageCollected<CSSMathExpressionOperation>(
        final_node, node, op, root->Category());
  }
  return final_node;
}

CSSMathExpressionNode* MaybeDistributeArithmeticOperation(
    const CSSMathExpressionNode* left_side,
    const CSSMathExpressionNode* right_side,
    CSSMathOperator op) {
  if (op != CSSMathOperator::kMultiply && op != CSSMathOperator::kDivide) {
    return nullptr;
  }
  // NOTE: we should not simplify num * (fn + fn), all the operands inside
  // the sum should be numeric.
  // Case (Op1 + Op2) * Num.
  auto* left_operation = DynamicTo<CSSMathExpressionOperation>(left_side);
  auto* right_numeric = DynamicTo<CSSMathExpressionNumericLiteral>(right_side);
  if (left_operation && left_operation->IsAddOrSubtract() &&
      left_operation->AllOperandsAreNumeric() && right_numeric &&
      right_numeric->Category() == CalculationResultCategory::kCalcNumber) {
    auto* new_left_side =
        CSSMathExpressionOperation::CreateArithmeticOperationSimplified(
            left_operation->GetOperands().front(), right_side, op);
    auto* new_right_side =
        CSSMathExpressionOperation::CreateArithmeticOperationSimplified(
            left_operation->GetOperands().back(), right_side, op);
    CSSMathExpressionNode* operation =
        CSSMathExpressionOperation::CreateArithmeticOperationSimplified(
            new_left_side, new_right_side, left_operation->OperatorType());
    // Note: setting SetIsNestedCalc is needed, as we can be in this situation:
    // A - B * (C + D)
    //     /\/\/\/\/\ - we are B * (C + D)
    // and we don't know about the -, as it's another operation,
    // so make the simplified operation nested to end up with:
    // A - (B * C + B * D).
    operation->SetIsNestedCalc();
    return operation;
  }
  // Case Num * (Op1 + Op2). But don't do num / (Op1 + Op2), as it can invert
  // the type.
  auto* right_operation = DynamicTo<CSSMathExpressionOperation>(right_side);
  auto* left_numeric = DynamicTo<CSSMathExpressionNumericLiteral>(left_side);
  if (right_operation && right_operation->IsAddOrSubtract() &&
      right_operation->AllOperandsAreNumeric() && left_numeric &&
      left_numeric->Category() == CalculationResultCategory::kCalcNumber &&
      op != CSSMathOperator::kDivide) {
    auto* new_right_side =
        CSSMathExpressionOperation::CreateArithmeticOperationSimplified(
            left_side, right_operation->GetOperands().front(), op);
    auto* new_left_side =
        CSSMathExpressionOperation::CreateArithmeticOperationSimplified(
            left_side, right_operation->GetOperands().back(), op);
    CSSMathExpressionNode* operation =
        CSSMathExpressionOperation::CreateArithmeticOperationSimplified(
            new_right_side, new_left_side, right_operation->OperatorType());
    // Note: setting SetIsNestedCalc is needed, as we can be in this situation:
    // A - (C + D) * B
    //     /\/\/\/\/\ - we are (C + D) * B
    // and we don't know about the -, as it's another operation,
    // so make the simplified operation nested to end up with:
    // A - (B * C + B * D).
    operation->SetIsNestedCalc();
    return operation;
  }
  return nullptr;
}

}  // namespace

// ------ Start of CSSMathExpressionNumericLiteral member functions ------

// static
CSSMathExpressionNumericLiteral* CSSMathExpressionNumericLiteral::Create(
    const CSSNumericLiteralValue* value) {
  return MakeGarbageCollected<CSSMathExpressionNumericLiteral>(value);
}

// static
CSSMathExpressionNumericLiteral* CSSMathExpressionNumericLiteral::Create(
    double value,
    CSSPrimitiveValue::UnitType type) {
  return MakeGarbageCollected<CSSMathExpressionNumericLiteral>(
      CSSNumericLiteralValue::Create(value, type));
}

CSSMathExpressionNumericLiteral::CSSMathExpressionNumericLiteral(
    const CSSNumericLiteralValue* value)
    : CSSMathExpressionNode(UnitCategory(value->GetType()),
                            false /* has_comparisons*/,
                            false /* has_anchor_functions*/,
                            false /* needs_tree_scope_population*/),
      value_(value) {
  if (!value_->IsNumber() && CanEagerlySimplify(this)) {
    // "If root is a dimension that is not expressed in its canonical unit, and
    // there is enough information available to convert it to the canonical
    // unit, do so, and return the value."
    // https://w3c.github.io/csswg-drafts/css-values/#calc-simplification
    //
    // However, Numbers should not be eagerly simplified here since that would
    // result in converting Integers to Doubles (kNumber, canonical unit for
    // Numbers).

    value_ = value_->CreateCanonicalUnitValue();
  }
}

const CSSMathExpressionNode*
CSSMathExpressionNumericLiteral::ConvertLiteralsFromPercentageToNumber() const {
  if (category_ != kCalcPercent) {
    return this;
  }
  return CSSMathExpressionNumericLiteral::Create(
      value_->DoubleValue() / 100, CSSPrimitiveValue::UnitType::kNumber);
}

CSSPrimitiveValue::BoolStatus CSSMathExpressionNumericLiteral::ResolvesTo(
    double value) const {
  std::optional<double> maybe_value = ComputeValueInCanonicalUnit();
  if (!maybe_value.has_value()) {
    return CSSPrimitiveValue::BoolStatus::kUnresolvable;
  }
  return maybe_value.value() == value ? CSSPrimitiveValue::BoolStatus::kTrue
                                      : CSSPrimitiveValue::BoolStatus::kFalse;
}

CSSPrimitiveValue::BoolStatus CSSMathExpressionNumericLiteral::IsNegative()
    const {
  std::optional<double> maybe_value = ComputeValueInCanonicalUnit();
  if (!maybe_value.has_value()) {
    return CSSPrimitiveValue::BoolStatus::kUnresolvable;
  }
  return maybe_value.value() < 0.0 ? CSSPrimitiveValue::BoolStatus::kTrue
                                   : CSSPrimitiveValue::BoolStatus::kFalse;
}

String CSSMathExpressionNumericLiteral::CustomCSSText() const {
  return value_->CssText();
}

std::optional<PixelsAndPercent>
CSSMathExpressionNumericLiteral::ToPixelsAndPercent(
    const CSSLengthResolver& length_resolver) const {
  switch (category_) {
    case kCalcLength:
      return PixelsAndPercent(value_->ComputeLengthPx(length_resolver), 0.0f,
                              /*has_explicit_pixels=*/true,
                              /*has_explicit_percent=*/false);
    case kCalcPercent:
      DCHECK(value_->IsPercentage());
      return PixelsAndPercent(0.0f, value_->GetDoubleValueWithoutClamping(),
                              /*has_explicit_pixels=*/false,
                              /*has_explicit_percent=*/true);
    case kCalcNumber:
      // TODO(alancutter): Stop treating numbers like pixels unconditionally
      // in calcs to be able to accomodate border-image-width
      // https://drafts.csswg.org/css-backgrounds-3/#the-border-image-width
      return PixelsAndPercent(value_->GetFloatValue() * length_resolver.Zoom(),
                              0.0f, /*has_explicit_pixels=*/true,
                              /*has_explicit_percent=*/false);
    case kCalcAngle:
      // Treat angles as pixels to support calc() expressions on hue angles in
      // relative color syntax. This allows converting such expressions to
      // CalculationValues.
      return PixelsAndPercent(value_->GetFloatValue(), 0.0f,
                              /*has_explicit_pixels=*/true,
                              /*has_explicit_percent=*/false);
    default:
      NOTREACHED_IN_MIGRATION();
      return {};
  }
}

scoped_refptr<const CalculationExpressionNode>
CSSMathExpressionNumericLiteral::ToCalculationExpression(
    const CSSLengthResolver& length_resolver) const {
  if (Category() == kCalcNumber) {
    return base::MakeRefCounted<CalculationExpressionNumberNode>(
        value_->DoubleValue());
  }
  return base::MakeRefCounted<CalculationExpressionPixelsAndPercentNode>(
      *ToPixelsAndPercent(length_resolver));
}

double CSSMathExpressionNumericLiteral::DoubleValue() const {
  if (HasDoubleValue(ResolvedUnitType())) {
    return value_->GetDoubleValueWithoutClamping();
  }
  DUMP_WILL_BE_NOTREACHED();
  return 0;
}

std::optional<double>
CSSMathExpressionNumericLiteral::ComputeValueInCanonicalUnit() const {
  switch (category_) {
    case kCalcNumber:
    case kCalcPercent:
      return value_->DoubleValue();
    case kCalcLength:
      if (CSSPrimitiveValue::IsRelativeUnit(value_->GetType())) {
        return std::nullopt;
      }
      [[fallthrough]];
    case kCalcAngle:
    case kCalcTime:
    case kCalcFrequency:
    case kCalcResolution:
      return value_->DoubleValue() *
             CSSPrimitiveValue::ConversionToCanonicalUnitsScaleFactor(
                 value_->GetType());
    default:
      return std::nullopt;
  }
}

std::optional<double>
CSSMathExpressionNumericLiteral::ComputeValueInCanonicalUnit(
    const CSSLengthResolver& length_resolver) const {
  return value_->ComputeInCanonicalUnit(length_resolver);
}

double CSSMathExpressionNumericLiteral::ComputeDouble(
    const CSSLengthResolver& length_resolver) const {
  switch (category_) {
    case kCalcLength:
      return value_->ComputeLengthPx(length_resolver);
    case kCalcPercent:
    case kCalcNumber:
      return value_->DoubleValue();
    case kCalcAngle:
      return value_->ComputeDegrees();
    case kCalcTime:
      return value_->ComputeSeconds();
    case kCalcResolution:
      return value_->ComputeDotsPerPixel();
    case kCalcFrequency:
      return value_->ComputeInCanonicalUnit();
    case kCalcLengthFunction:
    case kCalcIntrinsicSize:
    case kCalcOther:
    case kCalcIdent:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}

double CSSMathExpressionNumericLiteral::ComputeLengthPx(
    const CSSLengthResolver& length_resolver) const {
  switch (category_) {
    case kCalcLength:
      return value_->ComputeLengthPx(length_resolver);
    case kCalcNumber:
    case kCalcPercent:
    case kCalcAngle:
    case kCalcFrequency:
    case kCalcLengthFunction:
    case kCalcIntrinsicSize:
    case kCalcTime:
    case kCalcResolution:
    case kCalcOther:
    case kCalcIdent:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}

bool CSSMathExpressionNumericLiteral::AccumulateLengthArray(
    CSSLengthArray& length_array,
    double multiplier) const {
  DCHECK_NE(Category(), kCalcNumber);
  return value_->AccumulateLengthArray(length_array, multiplier);
}

void CSSMathExpressionNumericLiteral::AccumulateLengthUnitTypes(
    CSSPrimitiveValue::LengthTypeFlags& types) const {
  value_->AccumulateLengthUnitTypes(types);
}

bool CSSMathExpressionNumericLiteral::operator==(
    const CSSMathExpressionNode& other) const {
  if (!other.IsNumericLiteral()) {
    return false;
  }

  return base::ValuesEquivalent(
      value_, To<CSSMathExpressionNumericLiteral>(other).value_);
}

CSSPrimitiveValue::UnitType CSSMathExpressionNumericLiteral::ResolvedUnitType()
    const {
  return value_->GetType();
}

bool CSSMathExpressionNumericLiteral::IsComputationallyIndependent() const {
  return value_->IsComputationallyIndependent();
}

void CSSMathExpressionNumericLiteral::Trace(Visitor* visitor) const {
  visitor->Trace(value_);
  CSSMathExpressionNode::Trace(visitor);
}

#if DCHECK_IS_ON()
bool CSSMathExpressionNumericLiteral::InvolvesPercentageComparisons() const {
  return false;
}
#endif

// ------ End of CSSMathExpressionNumericLiteral member functions

static constexpr std::array<std::array<CalculationResultCategory, kCalcOther>,
                            kCalcOther>
    kAddSubtractResult = {
        /* CalcNumber */
        {{kCalcNumber, kCalcOther, kCalcOther, kCalcOther, kCalcOther,
          kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther},
         /* CalcLength */
         {kCalcOther, kCalcLength, kCalcLengthFunction, kCalcLengthFunction,
          kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther,
          kCalcOther},
         /* CalcPercent */
         {kCalcOther, kCalcLengthFunction, kCalcPercent, kCalcLengthFunction,
          kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther,
          kCalcOther},
         /* CalcLengthFunction */
         {kCalcOther, kCalcLengthFunction, kCalcLengthFunction,
          kCalcLengthFunction, kCalcOther, kCalcOther, kCalcOther, kCalcOther,
          kCalcOther, kCalcOther},
         /* CalcIntrinsicSize */
         {kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther,
          kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther},
         /* CalcAngle */
         {kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther,
          kCalcAngle, kCalcOther, kCalcOther, kCalcOther, kCalcOther},
         /* CalcTime */
         {kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther,
          kCalcOther, kCalcTime, kCalcOther, kCalcOther, kCalcOther},
         /* CalcFrequency */
         {kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther,
          kCalcOther, kCalcOther, kCalcFrequency, kCalcOther, kCalcOther},
         /* CalcResolution */
         {kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther,
          kCalcOther, kCalcOther, kCalcOther, kCalcResolution, kCalcOther},
         /* CalcIdent */
         {kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther,
          kCalcOther, kCalcOther, kCalcOther, kCalcOther, kCalcOther}}};

static CalculationResultCategory DetermineCategory(
    const CSSMathExpressionNode& left_side,
    const CSSMathExpressionNode& right_side,
    CSSMathOperator op) {
  CalculationResultCategory left_category = left_side.Category();
  CalculationResultCategory right_category = right_side.Category();

  if (left_category == kCalcOther || right_category == kCalcOther) {
    return kCalcOther;
  }

  if (left_category == kCalcIntrinsicSize ||
      right_category == kCalcIntrinsicSize) {
    return kCalcOther;
  }

  switch (op) {
    case CSSMathOperator::kAdd:
    case CSSMathOperator::kSubtract:
      return kAddSubtractResult[left_category][right_category];
    case CSSMathOperator::kMultiply:
      if (left_category != kCalcNumber && right_category != kCalcNumber) {
        return kCalcOther;
      }
      return left_category == kCalcNumber ? right_category : left_category;
    case CSSMathOperator::kDivide:
      if (right_category != kCalcNumber) {
        return kCalcOther;
      }
      return left_category;
    default:
      break;
  }

  NOTREACHED_IN_MIGRATION();
  return kCalcOther;
}

static CalculationResultCategory DetermineComparisonCategory(
    const CSSMathExpressionOperation::Operands& operands) {
  DCHECK(!operands.empty());

  bool is_first = true;
  CalculationResultCategory category = kCalcOther;
  for (const CSSMathExpressionNode* operand : operands) {
    if (is_first) {
      category = operand->Category();
    } else {
      category = kAddSubtractResult[category][operand->Category()];
    }

    is_first = false;
    if (category == kCalcOther) {
      break;
    }
  }

  return category;
}

static CalculationResultCategory DetermineCalcSizeCategory(
    const CSSMathExpressionNode& left_side,
    const CSSMathExpressionNode& right_side,
    CSSMathOperator op) {
  CalculationResultCategory basis_category = left_side.Category();
  CalculationResultCategory calculation_category = right_side.Category();

  if ((basis_category == kCalcLength || basis_category == kCalcPercent ||
       basis_category == kCalcLengthFunction ||
       basis_category == kCalcIntrinsicSize) &&
      (calculation_category == kCalcLength ||
       calculation_category == kCalcPercent ||
       calculation_category == kCalcLengthFunction)) {
    return kCalcIntrinsicSize;
  }
  return kCalcOther;
}

// ------ Start of CSSMathExpressionIdentifierLiteral member functions -

CSSMathExpressionIdentifierLiteral::CSSMathExpressionIdentifierLiteral(
    AtomicString identifier)
    : CSSMathExpressionNode(UnitCategory(CSSPrimitiveValue::UnitType::kIdent),
                            false /* has_comparisons*/,
                            false /* has_anchor_unctions*/,
                            false /* needs_tree_scope_population*/),
      identifier_(std::move(identifier)) {}

scoped_refptr<const CalculationExpressionNode>
CSSMathExpressionIdentifierLiteral::ToCalculationExpression(
    const CSSLengthResolver&) const {
  return base::MakeRefCounted<CalculationExpressionIdentifierNode>(identifier_);
}

// ------ End of CSSMathExpressionIdentifierLiteral member functions ----

// ------ Start of CSSMathExpressionKeywordLiteral member functions -

namespace {

CalculationExpressionSizingKeywordNode::Keyword CSSValueIDToSizingKeyword(
    CSSValueID keyword) {
  // The keywords supported here should be the ones supported in
  // css_parsing_utils::ValidWidthOrHeightKeyword plus 'any', 'auto' and 'size'.

  // This should also match SizingKeywordToCSSValueID below.
  switch (keyword) {
#define KEYWORD_CASE(kw) \
  case CSSValueID::kw:   \
    return CalculationExpressionSizingKeywordNode::Keyword::kw;

    KEYWORD_CASE(kAny)
    KEYWORD_CASE(kSize)
    KEYWORD_CASE(kAuto)
    KEYWORD_CASE(kContent)
    KEYWORD_CASE(kMinContent)
    KEYWORD_CASE(kWebkitMinContent)
    KEYWORD_CASE(kMaxContent)
    KEYWORD_CASE(kWebkitMaxContent)
    KEYWORD_CASE(kFitContent)
    KEYWORD_CASE(kWebkitFitContent)
    KEYWORD_CASE(kWebkitFillAvailable)

#undef KEYWORD_CASE

    default:
      break;
  }

  NOTREACHED();
}

CSSValueID SizingKeywordToCSSValueID(
    CalculationExpressionSizingKeywordNode::Keyword keyword) {
  // This should match CSSValueIDToSizingKeyword above.
  switch (keyword) {
#define KEYWORD_CASE(kw)                                    \
  case CalculationExpressionSizingKeywordNode::Keyword::kw: \
    return CSSValueID::kw;

    KEYWORD_CASE(kAny)
    KEYWORD_CASE(kSize)
    KEYWORD_CASE(kAuto)
    KEYWORD_CASE(kContent)
    KEYWORD_CASE(kMinContent)
    KEYWORD_CASE(kWebkitMinContent)
    KEYWORD_CASE(kMaxContent)
    KEYWORD_CASE(kWebkitMaxContent)
    KEYWORD_CASE(kFitContent)
    KEYWORD_CASE(kWebkitFitContent)
    KEYWORD_CASE(kWebkitFillAvailable)

#undef KEYWORD_CASE
  }

  NOTREACHED();
}

CalculationResultCategory DetermineKeywordCategory(
    CSSValueID keyword,
    CSSMathExpressionKeywordLiteral::Context context) {
  switch (context) {
    case CSSMathExpressionKeywordLiteral::Context::kMediaProgress:
      return kCalcLength;
    case CSSMathExpressionKeywordLiteral::Context::kCalcSize:
      return kCalcLengthFunction;
    case CSSMathExpressionKeywordLiteral::Context::kColorChannel:
      return kCalcNumber;
  };
}

}  // namespace

CSSMathExpressionKeywordLiteral::CSSMathExpressionKeywordLiteral(
    CSSValueID keyword,
    Context context)
    : CSSMathExpressionNode(DetermineKeywordCategory(keyword, context),
                            false /* has_comparisons*/,
                            false /* has_anchor_unctions*/,
                            false /* needs_tree_scope_population*/),
      keyword_(keyword),
      context_(context) {}

scoped_refptr<const CalculationExpressionNode>
CSSMathExpressionKeywordLiteral::ToCalculationExpression(
    const CSSLengthResolver& length_resolver) const {
  switch (context_) {
    case CSSMathExpressionKeywordLiteral::Context::kMediaProgress: {
      switch (keyword_) {
        case CSSValueID::kWidth:
          return base::MakeRefCounted<
              CalculationExpressionPixelsAndPercentNode>(
              PixelsAndPercent(length_resolver.ViewportWidth()));
        case CSSValueID::kHeight:
          return base::MakeRefCounted<
              CalculationExpressionPixelsAndPercentNode>(
              PixelsAndPercent(length_resolver.ViewportHeight()));
        default:
          NOTREACHED();
      }
    }
    case CSSMathExpressionKeywordLiteral::Context::kCalcSize:
      return base::MakeRefCounted<CalculationExpressionSizingKeywordNode>(
          CSSValueIDToSizingKeyword(keyword_));
    case CSSMathExpressionKeywordLiteral::Context::kColorChannel:
      return base::MakeRefCounted<CalculationExpressionColorChannelKeywordNode>(
          CSSValueIDToColorChannelKeyword(keyword_));
  };
}

double CSSMathExpressionKeywordLiteral::ComputeDouble(
    const CSSLengthResolver& length_resolver) const {
  switch (context_) {
    case CSSMathExpressionKeywordLiteral::Context::kMediaProgress: {
      switch (keyword_) {
        case CSSValueID::kWidth:
          return length_resolver.ViewportWidth();
        case CSSValueID::kHeight:
          return length_resolver.ViewportHeight();
        default:
          NOTREACHED();
      }
    }
    case CSSMathExpressionKeywordLiteral::Context::kCalcSize:
    case CSSMathExpressionKeywordLiteral::Context::kColorChannel:
      NOTREACHED();
  };
}

std::optional<PixelsAndPercent>
CSSMathExpressionKeywordLiteral::ToPixelsAndPercent(
    const CSSLengthResolver& length_resolver) const {
  switch (context_) {
    case CSSMathExpressionKeywordLiteral::Context::kMediaProgress:
      switch (keyword_) {
        case CSSValueID::kWidth:
          return PixelsAndPercent(length_resolver.ViewportWidth());
        case CSSValueID::kHeight:
          return PixelsAndPercent(length_resolver.ViewportHeight());
        default:
          NOTREACHED();
      }
    case CSSMathExpressionKeywordLiteral::Context::kCalcSize:
    case CSSMathExpressionKeywordLiteral::Context::kColorChannel:
      return std::nullopt;
  }
}

// ------ End of CSSMathExpressionKeywordLiteral member functions ----

// ------ Start of CSSMathExpressionOperation member functions ------

bool CSSMathExpressionOperation::AllOperandsAreNumeric() const {
  return std::all_of(
      operands_.begin(), operands_.end(),
      [](const CSSMathExpressionNode* op) { return op->IsNumericLiteral(); });
}

// static
CSSMathExpressionNode* CSSMathExpressionOperation::CreateArithmeticOperation(
    const CSSMathExpressionNode* left_side,
    const CSSMathExpressionNode* right_side,
    CSSMathOperator op) {
  DCHECK_NE(left_side->Category(), kCalcOther);
  DCHECK_NE(right_side->Category(), kCalcOther);

  CalculationResultCategory new_category =
      DetermineCategory(*left_side, *right_side, op);
  if (new_category == kCalcOther) {
    return nullptr;
  }

  return MakeGarbageCollected<CSSMathExpressionOperation>(left_side, right_side,
                                                          op, new_category);
}

// static
CSSMathExpressionNode* CSSMathExpressionOperation::CreateCalcSizeOperation(
    const CSSMathExpressionNode* left_side,
    const CSSMathExpressionNode* right_side) {
  DCHECK_NE(left_side->Category(), kCalcOther);
  DCHECK_NE(right_side->Category(), kCalcOther);

  const CSSMathOperator op = CSSMathOperator::kCalcSize;
  CalculationResultCategory new_category =
      DetermineCalcSizeCategory(*left_side, *right_side, op);
  if (new_category == kCalcOther) {
    return nullptr;
  }

  return MakeGarbageCollected<CSSMathExpressionOperation>(left_side, right_side,
                                                          op, new_category);
}

// static
CSSMathExpressionNode* CSSMathExpressionOperation::CreateComparisonFunction(
    Operands&& operands,
    CSSMathOperator op) {
  DCHECK(op == CSSMathOperator::kMin || op == CSSMathOperator::kMax ||
         op == CSSMathOperator::kClamp);

  CalculationResultCategory category = DetermineComparisonCategory(operands);
  if (category == kCalcOther) {
    return nullptr;
  }

  return MakeGarbageCollected<CSSMathExpressionOperation>(
      category, std::move(operands), op);
}

// static
CSSMathExpressionNode*
CSSMathExpressionOperation::CreateComparisonFunctionSimplified(
    Operands&& operands,
    CSSMathOperator op) {
  DCHECK(op == CSSMathOperator::kMin || op == CSSMathOperator::kMax ||
         op == CSSMathOperator::kClamp);

  CalculationResultCategory category = DetermineComparisonCategory(operands);
  if (category == kCalcOther) {
    return nullptr;
  }

  if (CanEagerlySimplify(operands)) {
    Vector<double> canonical_values;
    canonical_values.reserve(operands.size());
    for (const CSSMathExpressionNode* operand : operands) {
      std::optional<double> canonical_value =
          operand->ComputeValueInCanonicalUnit();

      DCHECK(canonical_value.has_value());

      canonical_values.push_back(canonical_value.value());
    }

    CSSPrimitiveValue::UnitType canonical_unit =
        CSSPrimitiveValue::CanonicalUnit(operands.front()->ResolvedUnitType());

    return CSSMathExpressionNumericLiteral::Create(
        EvaluateOperator(canonical_values, op), canonical_unit);
  }

  if (operands.size() == 1) {
    return operands.front()->Copy();
  }

  return MakeGarbageCollected<CSSMathExpressionOperation>(
      category, std::move(operands), op);
}

// Helper function for parsing number value
static double ValueAsNumber(const CSSMathExpressionNode* node, bool& error) {
  if (node->Category() == kCalcNumber) {
    return node->DoubleValue();
  }
  error = true;
  return 0;
}

static bool SupportedCategoryForAtan2(
    const CalculationResultCategory category) {
  switch (category) {
    case kCalcNumber:
    case kCalcLength:
    case kCalcPercent:
    case kCalcTime:
    case kCalcFrequency:
    case kCalcAngle:
      return true;
    default:
      return false;
  }
}

static bool IsRelativeLength(CSSPrimitiveValue::UnitType type) {
  return CSSPrimitiveValue::IsRelativeUnit(type) &&
         CSSPrimitiveValue::IsLength(type);
}

static double ResolveAtan2(const CSSMathExpressionNode* y_node,
                           const CSSMathExpressionNode* x_node,
                           bool& error) {
  const CalculationResultCategory category = y_node->Category();
  if (category != x_node->Category() || !SupportedCategoryForAtan2(category)) {
    error = true;
    return 0;
  }
  CSSPrimitiveValue::UnitType y_type = y_node->ResolvedUnitType();
  CSSPrimitiveValue::UnitType x_type = x_node->ResolvedUnitType();

  // TODO(crbug.com/1392594): We ignore parameters in complex relative units
  // (e.g., 1rem + 1px) until they can be supported.
  if (y_type == CSSPrimitiveValue::UnitType::kUnknown ||
      x_type == CSSPrimitiveValue::UnitType::kUnknown) {
    error = true;
    return 0;
  }

  if (IsRelativeLength(y_type) || IsRelativeLength(x_type)) {
    // TODO(crbug.com/1392594): Relative length units are currently hard
    // to resolve. We ignore the units for now, so that
    // we can at least support the case where both operands have the same unit.
    double y = y_node->DoubleValue();
    double x = x_node->DoubleValue();
    return std::atan2(y, x);
  }
  auto y = y_node->ComputeValueInCanonicalUnit();
  auto x = x_node->ComputeValueInCanonicalUnit();
  return std::atan2(y.value(), x.value());
}

// Helper function for parsing trigonometric functions' parameter
static double ValueAsDegrees(const CSSMathExpressionNode* node, bool& error) {
  if (node->Category() == kCalcAngle) {
    return node->ComputeValueInCanonicalUnit().value();
  }
  return Rad2deg(ValueAsNumber(node, error));
}

static bool CanonicalizeRoundArguments(
    CSSMathExpressionOperation::Operands& nodes) {
  if (nodes.size() == 2) {
    return true;
  }
  // If the type of A matches <number>, then B may be omitted, and defaults to
  // 1; omitting B is otherwise invalid.
  // (https://drafts.csswg.org/css-values-4/#round-func)
  if (nodes.size() == 1 &&
      nodes[0]->Category() == CalculationResultCategory::kCalcNumber) {
    // Add B=1 to get the function on canonical form.
    nodes.push_back(CSSMathExpressionNumericLiteral::Create(
        1, CSSPrimitiveValue::UnitType::kNumber));
    return true;
  }
  return false;
}

static bool ShouldSerializeRoundingStep(
    const CSSMathExpressionOperation::Operands& operands) {
  // Omit the step (B) operand to round(...) if the type of A is <number> and
  // the step is the literal 1.
  if (operands[0]->Category() != CalculationResultCategory::kCalcNumber) {
    return true;
  }
  auto* literal = DynamicTo<CSSMathExpressionNumericLiteral>(*operands[1]);
  if (!literal) {
    return true;
  }
  const CSSNumericLiteralValue& literal_value = literal->GetValue();
  if (!literal_value.IsNumber() || literal_value.DoubleValue() != 1) {
    return true;
  }
  return false;
}

CSSMathExpressionNode*
CSSMathExpressionOperation::CreateTrigonometricFunctionSimplified(
    Operands&& operands,
    CSSValueID function_id) {
  double value;
  auto unit_type = CSSPrimitiveValue::UnitType::kUnknown;
  bool error = false;
  switch (function_id) {
    case CSSValueID::kSin: {
      DCHECK_EQ(operands.size(), 1u);
      unit_type = CSSPrimitiveValue::UnitType::kNumber;
      value = gfx::SinCosDegrees(ValueAsDegrees(operands[0], error)).sin;
      break;
    }
    case CSSValueID::kCos: {
      DCHECK_EQ(operands.size(), 1u);
      unit_type = CSSPrimitiveValue::UnitType::kNumber;
      value = gfx::SinCosDegrees(ValueAsDegrees(operands[0], error)).cos;
      break;
    }
    case CSSValueID::kTan: {
      DCHECK_EQ(operands.size(), 1u);
      unit_type = CSSPrimitiveValue::UnitType::kNumber;
      value = TanDegrees(ValueAsDegrees(operands[0], error));
      break;
    }
    case CSSValueID::kAsin: {
      DCHECK_EQ(operands.size(), 1u);
      unit_type = CSSPrimitiveValue::UnitType::kDegrees;
      value = Rad2deg(std::asin(ValueAsNumber(operands[0], error)));
      DCHECK(value >= -90 && value <= 90 || std::isnan(value));
      break;
    }
    case CSSValueID::kAcos: {
      DCHECK_EQ(operands.size(), 1u);
      unit_type = CSSPrimitiveValue::UnitType::kDegrees;
      value = Rad2deg(std::acos(ValueAsNumber(operands[0], error)));
      DCHECK(value >= 0 && value <= 180 || std::isnan(value));
      break;
    }
    case CSSValueID::kAtan: {
      DCHECK_EQ(operands.size(), 1u);
      unit_type = CSSPrimitiveValue::UnitType::kDegrees;
      value = Rad2deg(std::atan(ValueAsNumber(operands[0], error)));
      DCHECK(value >= -90 && value <= 90 || std::isnan(value));
      break;
    }
    case CSSValueID::kAtan2: {
      DCHECK_EQ(operands.size(), 2u);
      unit_type = CSSPrimitiveValue::UnitType::kDegrees;
      value = Rad2deg(ResolveAtan2(operands[0], operands[1], error));
      DCHECK(value >= -180 && value <= 180 || std::isnan(value));
      break;
    }
    default:
      return nullptr;
  }

  if (error) {
    return nullptr;
  }

  DCHECK_NE(unit_type, CSSPrimitiveValue::UnitType::kUnknown);
  return CSSMathExpressionNumericLiteral::Create(value, unit_type);
}

CSSMathExpressionNode* CSSMathExpressionOperation::CreateSteppedValueFunction(
    Operands&& operands,
    CSSMathOperator op) {
  if (!RuntimeEnabledFeatures::CSSSteppedValueFunctionsEnabled()) {
    return nullptr;
  }
  DCHECK_EQ(operands.size(), 2u);
  if (operands[0]->Category() == kCalcOther ||
      operands[1]->Category() == kCalcOther) {
    return nullptr;
  }
  CalculationResultCategory category =
      kAddSubtractResult[operands[0]->Category()][operands[1]->Category()];
  if (category == kCalcOther) {
    return nullptr;
  }
  if (CanEagerlySimplify(operands)) {
    std::optional<double> a = operands[0]->ComputeValueInCanonicalUnit();
    std::optional<double> b = operands[1]->ComputeValueInCanonicalUnit();
    DCHECK(a.has_value());
    DCHECK(b.has_value());
    double value = EvaluateSteppedValueFunction(op, a.value(), b.value());
    return CSSMathExpressionNumericLiteral::Create(
        value,
        CSSPrimitiveValue::CanonicalUnit(operands.front()->ResolvedUnitType()));
  }
  return MakeGarbageCollected<CSSMathExpressionOperation>(
      category, std::move(operands), op);
}

// static
CSSMathExpressionNode* CSSMathExpressionOperation::CreateExponentialFunction(
    Operands&& operands,
    CSSValueID function_id) {
  if (!RuntimeEnabledFeatures::CSSExponentialFunctionsEnabled()) {
    return nullptr;
  }

  double value = 0;
  bool error = false;
  auto unit_type = CSSPrimitiveValue::UnitType::kNumber;
  switch (function_id) {
    case CSSValueID::kPow: {
      DCHECK_EQ(operands.size(), 2u);
      double a = ValueAsNumber(operands[0], error);
      double b = ValueAsNumber(operands[1], error);
      value = std::pow(a, b);
      break;
    }
    case CSSValueID::kSqrt: {
      DCHECK_EQ(operands.size(), 1u);
      double a = ValueAsNumber(operands[0], error);
      value = std::sqrt(a);
      break;
    }
    case CSSValueID::kHypot: {
      DCHECK_GE(operands.size(), 1u);
      CalculationResultCategory category =
          DetermineComparisonCategory(operands);
      if (category == kCalcOther) {
        return nullptr;
      }
      if (CanEagerlySimplify(operands)) {
        for (const CSSMathExpressionNode* operand : operands) {
          std::optional<double> a = operand->ComputeValueInCanonicalUnit();
          DCHECK(a.has_value());
          value = std::hypot(value, a.value());
        }
        unit_type = CSSPrimitiveValue::CanonicalUnit(
            operands.front()->ResolvedUnitType());
      } else {
        return MakeGarbageCollected<CSSMathExpressionOperation>(
            category, std::move(operands), CSSMathOperator::kHypot);
      }
      break;
    }
    case CSSValueID::kLog: {
      DCHECK_GE(operands.size(), 1u);
      DCHECK_LE(operands.size(), 2u);
      double a = ValueAsNumber(operands[0], error);
      if (operands.size() == 2) {
        double b = ValueAsNumber(operands[1], error);
        value = std::log2(a) / std::log2(b);
      } else {
        value = std::log(a);
      }
      break;
    }
    case CSSValueID::kExp: {
      DCHECK_EQ(operands.size(), 1u);
      double a = ValueAsNumber(operands[0], error);
      value = std::exp(a);
      break;
    }
    default:
      return nullptr;
  }
  if (error) {
    return nullptr;
  }

  DCHECK_NE(unit_type, CSSPrimitiveValue::UnitType::kUnknown);
  return CSSMathExpressionNumericLiteral::Create(value, unit_type);
}

CSSMathExpressionNode* CSSMathExpressionOperation::CreateSignRelatedFunction(
    Operands&& operands,
    CSSValueID function_id) {
  if (!RuntimeEnabledFeatures::CSSSignRelatedFunctionsEnabled()) {
    return nullptr;
  }

  const CSSMathExpressionNode* operand = operands.front();

  if (operand->Category() == kCalcIntrinsicSize) {
    return nullptr;
  }

  switch (function_id) {
    case CSSValueID::kAbs: {
      if (CanEagerlySimplify(operand)) {
        const std::optional<double> opt =
            operand->ComputeValueInCanonicalUnit();
        DCHECK(opt.has_value());
        return CSSMathExpressionNumericLiteral::Create(
            std::abs(opt.value()), operand->ResolvedUnitType());
      }
      return MakeGarbageCollected<CSSMathExpressionOperation>(
          operand->Category(), std::move(operands), CSSMathOperator::kAbs);
    }
    case CSSValueID::kSign: {
      if (CanEagerlySimplify(operand)) {
        const std::optional<double> opt =
            operand->ComputeValueInCanonicalUnit();
        DCHECK(opt.has_value());
        const double value = opt.value();
        const double signum =
            (value == 0 || std::isnan(value)) ? value : ((value > 0) ? 1 : -1);
        return CSSMathExpressionNumericLiteral::Create(
            signum, CSSPrimitiveValue::UnitType::kNumber);
      }
      return MakeGarbageCollected<CSSMathExpressionOperation>(
          kCalcNumber, std::move(operands), CSSMathOperator::kSign);
    }
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

const CSSMathExpressionNode*
CSSMathExpressionOperation::ConvertLiteralsFromPercentageToNumber() const {
  Operands ops;
  ops.reserve(operands_.size());
  for (const CSSMathExpressionNode* op : operands_) {
    ops.push_back(op->ConvertLiteralsFromPercentageToNumber());
  }
  CalculationResultCategory category =
      category_ == kCalcPercent ? kCalcNumber : category_;
  return MakeGarbageCollected<CSSMathExpressionOperation>(
      category, std::move(ops), operator_);
}

namespace {

inline const CSSMathExpressionOperation* DynamicToCalcSize(
    const CSSMathExpressionNode* node) {
  const CSSMathExpressionOperation* operation =
      DynamicTo<CSSMathExpressionOperation>(node);
  if (!operation || !operation->IsCalcSize()) {
    return nullptr;
  }
  return operation;
}

inline bool CanArithmeticOperationBeSimplified(
    const CSSMathExpressionNode* left_side,
    const CSSMathExpressionNode* right_side) {
  return !left_side->IsOperation() && !right_side->IsOperation();
}

}  // namespace

// static
CSSMathExpressionNode*
CSSMathExpressionOperation::CreateArithmeticOperationSimplified(
    const CSSMathExpressionNode* left_side,
    const CSSMathExpressionNode* right_side,
    CSSMathOperator op) {
  DCHECK(op == CSSMathOperator::kAdd || op == CSSMathOperator::kSubtract ||
         op == CSSMathOperator::kMultiply || op == CSSMathOperator::kDivide);

  if (CSSMathExpressionNode* result =
          MaybeDistributeArithmeticOperation(left_side, right_side, op)) {
    return result;
  }

  if (!CanArithmeticOperationBeSimplified(left_side, right_side)) {
    return CreateArithmeticOperation(left_side, right_side, op);
  }

  CalculationResultCategory left_category = left_side->Category();
  CalculationResultCategory right_category = right_side->Category();
  DCHECK_NE(left_category, kCalcOther);
  DCHECK_NE(right_category, kCalcOther);

  // Simplify numbers.
  if (left_category == kCalcNumber && left_side->IsNumericLiteral() &&
      right_category == kCalcNumber && right_side->IsNumericLiteral()) {
    return CSSMathExpressionNumericLiteral::Create(
        EvaluateOperator({left_side->DoubleValue(), right_side->DoubleValue()},
                         op),
        CSSPrimitiveValue::UnitType::kNumber);
  }

  // Simplify addition and subtraction between same types.
  if (op == CSSMathOperator::kAdd || op == CSSMathOperator::kSubtract) {
    if (left_category == right_side->Category()) {
      CSSPrimitiveValue::UnitType left_type = left_side->ResolvedUnitType();
      if (HasDoubleValue(left_type)) {
        CSSPrimitiveValue::UnitType right_type = right_side->ResolvedUnitType();
        if (left_type == right_type) {
          return CSSMathExpressionNumericLiteral::Create(
              EvaluateOperator(
                  {left_side->DoubleValue(), right_side->DoubleValue()}, op),
              left_type);
        }
        CSSPrimitiveValue::UnitCategory left_unit_category =
            CSSPrimitiveValue::UnitTypeToUnitCategory(left_type);
        if (left_unit_category != CSSPrimitiveValue::kUOther &&
            left_unit_category ==
                CSSPrimitiveValue::UnitTypeToUnitCategory(right_type)) {
          CSSPrimitiveValue::UnitType canonical_type =
              CSSPrimitiveValue::CanonicalUnitTypeForCategory(
                  left_unit_category);
          if (canonical_type != CSSPrimitiveValue::UnitType::kUnknown) {
            double left_value =
                left_side->DoubleValue() *
                CSSPrimitiveValue::ConversionToCanonicalUnitsScaleFactor(
                    left_type);
            double right_value =
                right_side->DoubleValue() *
                CSSPrimitiveValue::ConversionToCanonicalUnitsScaleFactor(
                    right_type);
            return CSSMathExpressionNumericLiteral::Create(
                EvaluateOperator({left_value, right_value}, op),
                canonical_type);
          }
        }
      }
    }
  } else {
    // Simplify multiplying or dividing by a number for simplifiable types.
    DCHECK(op == CSSMathOperator::kMultiply || op == CSSMathOperator::kDivide);
    const CSSMathExpressionNode* number_side =
        GetNumericLiteralSide(left_side, right_side);
    if (!number_side) {
      return CreateArithmeticOperation(left_side, right_side, op);
    }
    if (number_side == left_side && op == CSSMathOperator::kDivide) {
      return nullptr;
    }
    const CSSMathExpressionNode* other_side =
        left_side == number_side ? right_side : left_side;

    double number = number_side->DoubleValue();

    CSSPrimitiveValue::UnitType other_type = other_side->ResolvedUnitType();
    if (HasDoubleValue(other_type)) {
      return CSSMathExpressionNumericLiteral::Create(
          EvaluateOperator({other_side->DoubleValue(), number}, op),
          other_type);
    }
  }

  return CreateArithmeticOperation(left_side, right_side, op);
}

namespace {

std::tuple<const CSSMathExpressionNode*, wtf_size_t> SubstituteForSizeKeyword(
    const CSSMathExpressionNode* source,
    const CSSMathExpressionNode* size_substitution,
    wtf_size_t count_in_substitution) {
  CHECK_GT(count_in_substitution, 0u);
  if (const auto* operation = DynamicTo<CSSMathExpressionOperation>(source)) {
    using Operands = CSSMathExpressionOperation::Operands;
    const Operands& source_operands = operation->GetOperands();
    Operands dest_operands;
    dest_operands.reserve(source_operands.size());
    wtf_size_t total_substitution_count = 0;
    for (const CSSMathExpressionNode* source_op : source_operands) {
      const CSSMathExpressionNode* dest_op;
      wtf_size_t substitution_count;
      std::tie(dest_op, substitution_count) = SubstituteForSizeKeyword(
          source_op, size_substitution, count_in_substitution);
      CHECK_EQ(dest_op == source_op, substitution_count == 0);
      total_substitution_count += substitution_count;
      if (!dest_op || total_substitution_count > (1u << 16)) {
        // hit the size limit
        return std::make_tuple(nullptr, total_substitution_count);
      }
      dest_operands.push_back(dest_op);
    }

    if (total_substitution_count == 0) {
      // return the original rather than making a new one
      return std::make_tuple(source, 0);
    }

    return std::make_tuple(MakeGarbageCollected<CSSMathExpressionOperation>(
                               operation->Category(), std::move(dest_operands),
                               operation->OperatorType()),
                           total_substitution_count);
  }

  auto* literal = DynamicTo<CSSMathExpressionKeywordLiteral>(source);
  if (literal &&
      literal->GetContext() ==
          CSSMathExpressionKeywordLiteral::Context::kCalcSize &&
      literal->GetValue() == CSSValueID::kSize) {
    return std::make_tuple(size_substitution, count_in_substitution);
  }
  return std::make_tuple(source, 0);
}

// https://drafts.csswg.org/css-values-5/#de-percentify-a-calc-size-calculation
const CSSMathExpressionNode* SubstituteForPercentages(
    const CSSMathExpressionNode* source) {
  if (const auto* operation = DynamicTo<CSSMathExpressionOperation>(source)) {
    using Operands = CSSMathExpressionOperation::Operands;
    const Operands& source_operands = operation->GetOperands();
    Operands dest_operands;
    dest_operands.reserve(source_operands.size());
    for (const CSSMathExpressionNode* source_op : source_operands) {
      const CSSMathExpressionNode* dest_op =
          SubstituteForPercentages(source_op);
      dest_operands.push_back(dest_op);
    }

    return MakeGarbageCollected<CSSMathExpressionOperation>(
        operation->Category(), std::move(dest_operands),
        operation->OperatorType());
  }

  if (const auto* numeric_literal =
          DynamicTo<CSSMathExpressionNumericLiteral>(source)) {
    const CSSNumericLiteralValue& value = numeric_literal->GetValue();
    if (value.IsPercentage()) {
      return CSSMathExpressionOperation::CreateArithmeticOperation(
          CSSMathExpressionKeywordLiteral::Create(
              CSSValueID::kSize,
              CSSMathExpressionKeywordLiteral::Context::kCalcSize),
          CSSMathExpressionNumericLiteral::Create(
              value.DoubleValue() / 100.0,
              CSSPrimitiveValue::UnitType::kNumber),
          CSSMathOperator::kMultiply);
    }
  }
  return source;
}

bool BasisIsCanonical(const CSSMathExpressionNode* basis) {
  // A basis is canonical if it is a sizing keyword, 'any', or '100%'.
  if (const auto* numeric_literal =
          DynamicTo<CSSMathExpressionNumericLiteral>(basis)) {
    const CSSNumericLiteralValue& value = numeric_literal->GetValue();
    return value.IsPercentage() &&
           value.IsHundred() == CSSMathFunctionValue::BoolStatus::kTrue;
  }

  if (const auto* keyword_literal =
          DynamicTo<CSSMathExpressionKeywordLiteral>(basis)) {
    return keyword_literal->GetContext() ==
           CSSMathExpressionKeywordLiteral::Context::kCalcSize;
  }

  return false;
}

// Do substitution in order to produce a calc-size() whose basis is not
// another calc-size() and is not in non-canonical form.
const CSSMathExpressionOperation* MakeBasisCanonical(
    const CSSMathExpressionOperation* calc_size_input) {
  DCHECK(calc_size_input->IsCalcSize());
  HeapVector<Member<const CSSMathExpressionNode>, 4> calculation_stack;
  const CSSMathExpressionNode* final_basis = nullptr;
  const CSSMathExpressionNode* current_result = nullptr;

  wtf_size_t substitution_count = 1;
  const CSSMathExpressionOperation* current_calc_size = calc_size_input;
  while (true) {
    // If the basis is a calc-size(), push the calculation on the stack, and
    // enter this loop again with its basis.
    const CSSMathExpressionNode* basis = current_calc_size->GetOperands()[0];
    const CSSMathExpressionNode* calculation =
        current_calc_size->GetOperands()[1];
    if (const CSSMathExpressionOperation* basis_calc_size =
            DynamicToCalcSize(basis)) {
      calculation_stack.push_back(calculation);
      current_calc_size = basis_calc_size;
      continue;
    }

    // If the basis is canonical, use it.
    if (BasisIsCanonical(basis)) {
      if (calculation_stack.empty()) {
        // No substitution is needed; return the original.
        return calc_size_input;
      }

      current_result = calculation;
      final_basis = basis;
      break;
    }

    // Otherwise, we have a <calc-sum>, and our canonical basis should be
    // '100%' if we have a percentage and 'any' if we don't.  The percentage
    // case also requires that we substitute (size * (P/100)) for P% in the
    // basis.
    if (basis->HasPercentage()) {
      basis = SubstituteForPercentages(basis);
      final_basis = CSSMathExpressionNumericLiteral::Create(
          100.0, CSSPrimitiveValue::UnitType::kPercentage);
    } else {
      final_basis = CSSMathExpressionKeywordLiteral::Create(
          CSSValueID::kAny,
          CSSMathExpressionKeywordLiteral::Context::kCalcSize);
    }
    CHECK_EQ(substitution_count, 1u);
    std::tie(current_result, substitution_count) =
        SubstituteForSizeKeyword(calculation, basis, 1u);
    break;
  }

  while (!calculation_stack.empty()) {
    std::tie(current_result, substitution_count) =
        SubstituteForSizeKeyword(calculation_stack.back(), current_result,
                                 std::max(substitution_count, 1u));
    if (!current_result) {
      // too much expansion
      return nullptr;
    }
    calculation_stack.pop_back();
  }

  return To<CSSMathExpressionOperation>(
      CSSMathExpressionOperation::CreateCalcSizeOperation(final_basis,
                                                          current_result));
}

}  // namespace

// static
CSSMathExpressionNode*
CSSMathExpressionOperation::CreateArithmeticOperationAndSimplifyCalcSize(
    const CSSMathExpressionNode* left_side,
    const CSSMathExpressionNode* right_side,
    CSSMathOperator op) {
  DCHECK(op == CSSMathOperator::kAdd || op == CSSMathOperator::kSubtract ||
         op == CSSMathOperator::kMultiply || op == CSSMathOperator::kDivide);

  // Merge calc-size() expressions to keep calc-size() always at the top level.
  const CSSMathExpressionOperation* left_calc_size =
      DynamicToCalcSize(left_side);
  const CSSMathExpressionOperation* right_calc_size =
      DynamicToCalcSize(right_side);
  if (left_calc_size) {
    if (right_calc_size) {
      if (op != CSSMathOperator::kAdd && op != CSSMathOperator::kSubtract) {
        return nullptr;
      }
      // In theory we could check for basis equality or for one basis being
      // 'any' before we canonicalize to make some cases faster (and then
      // check again after).  However, the spec doesn't have this
      // optimization, and it is observable.

      // If either value has a non-canonical basis, substitute to produce a
      // canonical basis and try again recursively (with only one level of
      // recursion possible).
      //
      // We need to interpolate between the values *following* substitution of
      // the basis in the calculation, because if we interpolate the two
      // separately we are likely to get nonlinear interpolation behavior
      // (since we would be interpolating two different things linearly and
      // then multiplying them together).
      if (!BasisIsCanonical(left_calc_size->GetOperands()[0])) {
        left_calc_size = MakeBasisCanonical(left_calc_size);
        if (!left_calc_size) {
          return nullptr;  // hit the expansion limit
        }
      }
      if (!BasisIsCanonical(right_calc_size->GetOperands()[0])) {
        right_calc_size = MakeBasisCanonical(right_calc_size);
        if (!right_calc_size) {
          return nullptr;  // hit the expansion limit
        }
      }

      const CSSMathExpressionNode* left_basis =
          left_calc_size->GetOperands()[0];
      const CSSMathExpressionNode* right_basis =
          right_calc_size->GetOperands()[0];

      CHECK(BasisIsCanonical(left_basis));
      CHECK(BasisIsCanonical(right_basis));

      const CSSMathExpressionNode* final_basis = nullptr;
      // If the bases are equal, or one of them is the
      // any keyword, then we can interpolate only the calculations.
      auto is_any_keyword = [](const CSSMathExpressionNode* node) -> bool {
        const auto* literal = DynamicTo<CSSMathExpressionKeywordLiteral>(node);
        return literal && literal->GetValue() == CSSValueID::kAny &&
               literal->GetContext() ==
                   CSSMathExpressionKeywordLiteral::Context::kCalcSize;
      };

      if (*left_basis == *right_basis) {
        final_basis = left_basis;
      } else if (is_any_keyword(left_basis)) {
        final_basis = right_basis;
      } else if (is_any_keyword(right_basis)) {
        final_basis = left_basis;
      }
      if (!final_basis) {
        return nullptr;
      }
      const CSSMathExpressionNode* left_calculation =
          left_calc_size->GetOperands()[1];
      const CSSMathExpressionNode* right_calculation =
          right_calc_size->GetOperands()[1];
      return CreateCalcSizeOperation(
          final_basis, CreateArithmeticOperationSimplified(
                           left_calculation, right_calculation, op));
    } else {
      const CSSMathExpressionNode* left_basis =
          left_calc_size->GetOperands()[0];
      const CSSMathExpressionNode* left_calculation =
          left_calc_size->GetOperands()[1];
      return CreateCalcSizeOperation(
          left_basis, CreateArithmeticOperationSimplified(left_calculation,
                                                          right_side, op));
    }
  } else if (right_calc_size) {
    const CSSMathExpressionNode* right_basis =
        right_calc_size->GetOperands()[0];
    const CSSMathExpressionNode* right_calculation =
        right_calc_size->GetOperands()[1];
    return CreateCalcSizeOperation(
        right_basis,
        CreateArithmeticOperationSimplified(left_side, right_calculation, op));
  }

  return CreateArithmeticOperationSimplified(left_side, right_side, op);
}

CSSMathExpressionOperation::CSSMathExpressionOperation(
    const CSSMathExpressionNode* left_side,
    const CSSMathExpressionNode* right_side,
    CSSMathOperator op,
    CalculationResultCategory category)
    : CSSMathExpressionNode(
          category,
          left_side->HasComparisons() || right_side->HasComparisons(),
          left_side->HasAnchorFunctions() || right_side->HasAnchorFunctions(),
          !left_side->IsScopedValue() || !right_side->IsScopedValue()),
      operands_({left_side, right_side}),
      operator_(op) {}

bool CSSMathExpressionOperation::HasPercentage() const {
  if (Category() == kCalcPercent) {
    return true;
  }
  if (Category() != kCalcLengthFunction && Category() != kCalcIntrinsicSize) {
    return false;
  }
  switch (operator_) {
    case CSSMathOperator::kProgress:
      return false;
    case CSSMathOperator::kCalcSize:
      DCHECK_EQ(operands_.size(), 2u);
      return operands_[0]->HasPercentage();
    default:
      break;
  }
  for (const CSSMathExpressionNode* operand : operands_) {
    if (operand->HasPercentage()) {
      return true;
    }
  }
  return false;
}

bool CSSMathExpressionOperation::InvolvesLayout() const {
  if (Category() == kCalcPercent || Category() == kCalcLengthFunction) {
    return true;
  }
  for (const CSSMathExpressionNode* operand : operands_) {
    if (operand->InvolvesLayout()) {
      return true;
    }
  }
  return false;
}

static bool AnyOperandHasComparisons(
    CSSMathExpressionOperation::Operands& operands) {
  for (const CSSMathExpressionNode* operand : operands) {
    if (operand->HasComparisons()) {
      return true;
    }
  }
  return false;
}

static bool AnyOperandHasAnchorFunctions(
    CSSMathExpressionOperation::Operands& operands) {
  for (const CSSMathExpressionNode* operand : operands) {
    if (operand->HasAnchorFunctions()) {
      return true;
    }
  }
  return false;
}

static bool AnyOperandNeedsTreeScopePopulation(
    CSSMathExpressionOperation::Operands& operands) {
  for (const CSSMathExpressionNode* operand : operands) {
    if (!operand->IsScopedValue()) {
      return true;
    }
  }
  return false;
}

CSSMathExpressionOperation::CSSMathExpressionOperation(
    CalculationResultCategory category,
    Operands&& operands,
    CSSMathOperator op)
    : CSSMathExpressionNode(
          category,
          IsComparison(op) || AnyOperandHasComparisons(operands),
          AnyOperandHasAnchorFunctions(operands),
          AnyOperandNeedsTreeScopePopulation(operands)),
      operands_(std::move(operands)),
      operator_(op) {}

CSSMathExpressionOperation::CSSMathExpressionOperation(
    CalculationResultCategory category,
    CSSMathOperator op)
    : CSSMathExpressionNode(category,
                            IsComparison(op),
                            false /*has_anchor_functions*/,
                            false),
      operator_(op) {}

CSSPrimitiveValue::BoolStatus CSSMathExpressionOperation::ResolvesTo(
    double value) const {
  std::optional<double> maybe_value = ComputeValueInCanonicalUnit();
  if (!maybe_value.has_value()) {
    return CSSPrimitiveValue::BoolStatus::kUnresolvable;
  }
  return maybe_value.value() == value ? CSSPrimitiveValue::BoolStatus::kTrue
                                      : CSSPrimitiveValue::BoolStatus::kFalse;
}

CSSPrimitiveValue::BoolStatus CSSMathExpressionOperation::IsNegative() const {
  std::optional<double> maybe_value = ComputeValueInCanonicalUnit();
  if (!maybe_value.has_value()) {
    return CSSPrimitiveValue::BoolStatus::kUnresolvable;
  }
  return maybe_value.value() < 0.0 ? CSSPrimitiveValue::BoolStatus::kTrue
                                   : CSSPrimitiveValue::BoolStatus::kFalse;
}

std::optional<PixelsAndPercent> CSSMathExpressionOperation::ToPixelsAndPercent(
    const CSSLengthResolver& length_resolver) const {
  std::optional<PixelsAndPercent> result;
  switch (operator_) {
    case CSSMathOperator::kAdd:
    case CSSMathOperator::kSubtract: {
      DCHECK_EQ(operands_.size(), 2u);
      result = operands_[0]->ToPixelsAndPercent(length_resolver);
      if (!result) {
        return std::nullopt;
      }

      std::optional<PixelsAndPercent> other_side =
          operands_[1]->ToPixelsAndPercent(length_resolver);
      if (!other_side) {
        return std::nullopt;
      }
      if (operator_ == CSSMathOperator::kAdd) {
        result.value() += other_side.value();
      } else {
        result.value() -= other_side.value();
      }
      break;
    }
    case CSSMathOperator::kMultiply:
    case CSSMathOperator::kDivide: {
      DCHECK_EQ(operands_.size(), 2u);
      const CSSMathExpressionNode* number_side =
          GetNumericLiteralSide(operands_[0], operands_[1]);
      if (!number_side) {
        return std::nullopt;
      }
      const CSSMathExpressionNode* other_side =
          operands_[0] == number_side ? operands_[1] : operands_[0];
      result = other_side->ToPixelsAndPercent(length_resolver);
      if (!result) {
        return std::nullopt;
      }
      float number = number_side->DoubleValue();
      if (operator_ == CSSMathOperator::kDivide) {
        number = 1.0 / number;
      }
      result.value() *= number;
      break;
    }
    case CSSMathOperator::kCalcSize:
      // While it looks like we might be able to handle some calc-size() cases
      // here, we don't want to do because it would be difficult to avoid a
      // has_explicit_percent state inside the calculation propagating to the
      // result (which should not happen; only the has_explicit_percent state
      // from the basis should do so).
      return std::nullopt;
    case CSSMathOperator::kMin:
    case CSSMathOperator::kMax:
    case CSSMathOperator::kClamp:
    case CSSMathOperator::kRoundNearest:
    case CSSMathOperator::kRoundUp:
    case CSSMathOperator::kRoundDown:
    case CSSMathOperator::kRoundToZero:
    case CSSMathOperator::kMod:
    case CSSMathOperator::kRem:
    case CSSMathOperator::kHypot:
    case CSSMathOperator::kAbs:
    case CSSMathOperator::kSign:
    case CSSMathOperator::kProgress:
    case CSSMathOperator::kMediaProgress:
    case CSSMathOperator::kContainerProgress:
      return std::nullopt;
    case CSSMathOperator::kInvalid:
      NOTREACHED_IN_MIGRATION();
  }
  return result;
}

scoped_refptr<const CalculationExpressionNode>
CSSMathExpressionOperation::ToCalculationExpression(
    const CSSLengthResolver& length_resolver) const {
  switch (operator_) {
    case CSSMathOperator::kAdd:
      DCHECK_EQ(operands_.size(), 2u);
      return CalculationExpressionOperationNode::CreateSimplified(
          CalculationExpressionOperationNode::Children(
              {operands_[0]->ToCalculationExpression(length_resolver),
               operands_[1]->ToCalculationExpression(length_resolver)}),
          CalculationOperator::kAdd);
    case CSSMathOperator::kSubtract:
      DCHECK_EQ(operands_.size(), 2u);
      return CalculationExpressionOperationNode::CreateSimplified(
          CalculationExpressionOperationNode::Children(
              {operands_[0]->ToCalculationExpression(length_resolver),
               operands_[1]->ToCalculationExpression(length_resolver)}),
          CalculationOperator::kSubtract);
    case CSSMathOperator::kMultiply:
      DCHECK_EQ(operands_.size(), 2u);
      return CalculationExpressionOperationNode::CreateSimplified(
          {operands_.front()->ToCalculationExpression(length_resolver),
           operands_.back()->ToCalculationExpression(length_resolver)},
          CalculationOperator::kMultiply);
    case CSSMathOperator::kDivide:
      DCHECK_EQ(operands_.size(), 2u);
      DCHECK_EQ(operands_[1]->Category(), kCalcNumber);
      return CalculationExpressionOperationNode::CreateSimplified(
          CalculationExpressionOperationNode::Children(
              {operands_[0]->ToCalculationExpression(length_resolver),
               base::MakeRefCounted<CalculationExpressionNumberNode>(
                   1.0 / operands_[1]->DoubleValue())}),
          CalculationOperator::kMultiply);
    case CSSMathOperator::kMin:
    case CSSMathOperator::kMax: {
      Vector<scoped_refptr<const CalculationExpressionNode>> operands;
      operands.reserve(operands_.size());
      for (const CSSMathExpressionNode* operand : operands_) {
        operands.push_back(operand->ToCalculationExpression(length_resolver));
      }
      auto expression_operator = operator_ == CSSMathOperator::kMin
                                     ? CalculationOperator::kMin
                                     : CalculationOperator::kMax;
      return CalculationExpressionOperationNode::CreateSimplified(
          std::move(operands), expression_operator);
    }
    case CSSMathOperator::kClamp: {
      Vector<scoped_refptr<const CalculationExpressionNode>> operands;
      operands.reserve(operands_.size());
      for (const CSSMathExpressionNode* operand : operands_) {
        operands.push_back(operand->ToCalculationExpression(length_resolver));
      }
      return CalculationExpressionOperationNode::CreateSimplified(
          std::move(operands), CalculationOperator::kClamp);
    }
    case CSSMathOperator::kRoundNearest:
    case CSSMathOperator::kRoundUp:
    case CSSMathOperator::kRoundDown:
    case CSSMathOperator::kRoundToZero:
    case CSSMathOperator::kMod:
    case CSSMathOperator::kRem:
    case CSSMathOperator::kHypot:
    case CSSMathOperator::kAbs:
    case CSSMathOperator::kSign:
    case CSSMathOperator::kProgress:
    case CSSMathOperator::kMediaProgress:
    case CSSMathOperator::kContainerProgress:
    case CSSMathOperator::kCalcSize: {
      Vector<scoped_refptr<const CalculationExpressionNode>> operands;
      operands.reserve(operands_.size());
      for (const CSSMathExpressionNode* operand : operands_) {
        operands.push_back(operand->ToCalculationExpression(length_resolver));
      }
      CalculationOperator op;
      if (operator_ == CSSMathOperator::kRoundNearest) {
        op = CalculationOperator::kRoundNearest;
      } else if (operator_ == CSSMathOperator::kRoundUp) {
        op = CalculationOperator::kRoundUp;
      } else if (operator_ == CSSMathOperator::kRoundDown) {
        op = CalculationOperator::kRoundDown;
      } else if (operator_ == CSSMathOperator::kRoundToZero) {
        op = CalculationOperator::kRoundToZero;
      } else if (operator_ == CSSMathOperator::kMod) {
        op = CalculationOperator::kMod;
      } else if (operator_ == CSSMathOperator::kRem) {
        op = CalculationOperator::kRem;
      } else if (operator_ == CSSMathOperator::kHypot) {
        op = CalculationOperator::kHypot;
      } else if (operator_ == CSSMathOperator::kAbs) {
        op = CalculationOperator::kAbs;
      } else if (operator_ == CSSMathOperator::kSign) {
        op = CalculationOperator::kSign;
      } else if (operator_ == CSSMathOperator::kProgress) {
        op = CalculationOperator::kProgress;
      } else if (operator_ == CSSMathOperator::kMediaProgress) {
        op = CalculationOperator::kMediaProgress;
      } else if (operator_ == CSSMathOperator::kContainerProgress) {
        op = CalculationOperator::kContainerProgress;
      } else {
        CHECK(operator_ == CSSMathOperator::kCalcSize);
        op = CalculationOperator::kCalcSize;
      }
      return CalculationExpressionOperationNode::CreateSimplified(
          std::move(operands), op);
    }
    case CSSMathOperator::kInvalid:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

double CSSMathExpressionOperation::DoubleValue() const {
  DCHECK(HasDoubleValue(ResolvedUnitType())) << CustomCSSText();
  Vector<double> double_values;
  double_values.reserve(operands_.size());
  for (const CSSMathExpressionNode* operand : operands_) {
    double_values.push_back(operand->DoubleValue());
  }
  return Evaluate(double_values);
}

static bool HasCanonicalUnit(CalculationResultCategory category) {
  return category == kCalcNumber || category == kCalcLength ||
         category == kCalcPercent || category == kCalcAngle ||
         category == kCalcTime || category == kCalcFrequency ||
         category == kCalcResolution;
}

std::optional<double> CSSMathExpressionOperation::ComputeValueInCanonicalUnit()
    const {
  if (!HasCanonicalUnit(category_)) {
    return std::nullopt;
  }

  Vector<double> double_values;
  double_values.reserve(operands_.size());
  for (const CSSMathExpressionNode* operand : operands_) {
    std::optional<double> maybe_value = operand->ComputeValueInCanonicalUnit();
    if (!maybe_value) {
      return std::nullopt;
    }
    double_values.push_back(*maybe_value);
  }
  return Evaluate(double_values);
}

std::optional<double> CSSMathExpressionOperation::ComputeValueInCanonicalUnit(
    const CSSLengthResolver& length_resolver) const {
  if (!HasCanonicalUnit(category_)) {
    return std::nullopt;
  }

  Vector<double> double_values;
  double_values.reserve(operands_.size());
  for (const CSSMathExpressionNode* operand : operands_) {
    std::optional<double> maybe_value =
        operand->ComputeValueInCanonicalUnit(length_resolver);
    if (!maybe_value.has_value()) {
      return std::nullopt;
    }
    double_values.push_back(maybe_value.value());
  }
  return Evaluate(double_values);
}

double CSSMathExpressionOperation::ComputeDouble(
    const CSSLengthResolver& length_resolver) const {
  Vector<double> double_values;
  double_values.reserve(operands_.size());
  for (const CSSMathExpressionNode* operand : operands_) {
    double_values.push_back(
        CSSMathExpressionNode::ComputeDouble(operand, length_resolver));
  }
  return Evaluate(double_values);
}

double CSSMathExpressionOperation::ComputeLengthPx(
    const CSSLengthResolver& length_resolver) const {
  DCHECK(!HasPercentage());
  DCHECK_EQ(Category(), kCalcLength);
  return ComputeDouble(length_resolver);
}

bool CSSMathExpressionOperation::AccumulateLengthArray(
    CSSLengthArray& length_array,
    double multiplier) const {
  switch (operator_) {
    case CSSMathOperator::kAdd:
      DCHECK_EQ(operands_.size(), 2u);
      if (!operands_[0]->AccumulateLengthArray(length_array, multiplier)) {
        return false;
      }
      if (!operands_[1]->AccumulateLengthArray(length_array, multiplier)) {
        return false;
      }
      return true;
    case CSSMathOperator::kSubtract:
      DCHECK_EQ(operands_.size(), 2u);
      if (!operands_[0]->AccumulateLengthArray(length_array, multiplier)) {
        return false;
      }
      if (!operands_[1]->AccumulateLengthArray(length_array, -multiplier)) {
        return false;
      }
      return true;
    case CSSMathOperator::kMultiply:
      DCHECK_EQ(operands_.size(), 2u);
      DCHECK_NE((operands_[0]->Category() == kCalcNumber),
                (operands_[1]->Category() == kCalcNumber));
      if (operands_[0]->Category() == kCalcNumber) {
        return operands_[1]->AccumulateLengthArray(
            length_array, multiplier * operands_[0]->DoubleValue());
      } else {
        return operands_[0]->AccumulateLengthArray(
            length_array, multiplier * operands_[1]->DoubleValue());
      }
    case CSSMathOperator::kDivide:
      DCHECK_EQ(operands_.size(), 2u);
      DCHECK_EQ(operands_[1]->Category(), kCalcNumber);
      return operands_[0]->AccumulateLengthArray(
          length_array, multiplier / operands_[1]->DoubleValue());
    case CSSMathOperator::kMin:
    case CSSMathOperator::kMax:
    case CSSMathOperator::kClamp:
      // When comparison functions are involved, we can't resolve the expression
      // into a length array.
    case CSSMathOperator::kRoundNearest:
    case CSSMathOperator::kRoundUp:
    case CSSMathOperator::kRoundDown:
    case CSSMathOperator::kRoundToZero:
    case CSSMathOperator::kMod:
    case CSSMathOperator::kRem:
    case CSSMathOperator::kHypot:
    case CSSMathOperator::kAbs:
    case CSSMathOperator::kSign:
      // When stepped value functions are involved, we can't resolve the
      // expression into a length array.
    case CSSMathOperator::kProgress:
    case CSSMathOperator::kCalcSize:
    case CSSMathOperator::kMediaProgress:
    case CSSMathOperator::kContainerProgress:
      return false;
    case CSSMathOperator::kInvalid:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

void CSSMathExpressionOperation::AccumulateLengthUnitTypes(
    CSSPrimitiveValue::LengthTypeFlags& types) const {
  for (const CSSMathExpressionNode* operand : operands_) {
    operand->AccumulateLengthUnitTypes(types);
  }
}

bool CSSMathExpressionOperation::IsComputationallyIndependent() const {
  for (const CSSMathExpressionNode* operand : operands_) {
    if (!operand->IsComputationallyIndependent()) {
      return false;
    }
  }
  return true;
}

String CSSMathExpressionOperation::CustomCSSText() const {
  switch (operator_) {
    case CSSMathOperator::kAdd:
    case CSSMathOperator::kSubtract:
    case CSSMathOperator::kMultiply:
    case CSSMathOperator::kDivide: {
      DCHECK_EQ(operands_.size(), 2u);

      // As per
      // https://drafts.csswg.org/css-values-4/#sort-a-calculations-children
      // we should sort the dimensions of the sum node.
      const CSSMathExpressionOperation* operation = this;
      if (IsAddOrSubtract()) {
        const CSSMathExpressionNode* node = MaybeSortSumNode(this);
        // Note: we can hit here, since CSS Typed OM doesn't currently follow
        // the same simplifications as CSS Values spec.
        // https://github.com/w3c/csswg-drafts/issues/9451
        if (!node->IsOperation()) {
          return node->CustomCSSText();
        }
        operation = To<CSSMathExpressionOperation>(node);
      }
      CSSMathOperator op = operation->OperatorType();
      const Operands& operands = operation->GetOperands();

      StringBuilder result;

      // After all the simplifications we only need parentheses here for the
      // cases like: (lhs as unsimplified sum/sub) [* or /] rhs
      const bool left_side_needs_parentheses =
          IsMultiplyOrDivide() && operands.front()->IsOperation() &&
          To<CSSMathExpressionOperation>(operands.front().Get())
              ->IsAddOrSubtract();
      if (left_side_needs_parentheses) {
        result.Append('(');
      }
      result.Append(operands[0]->CustomCSSText());
      if (left_side_needs_parentheses) {
        result.Append(')');
      }

      result.Append(' ');
      result.Append(ToString(op));
      result.Append(' ');

      // After all the simplifications we only need parentheses here for the
      // cases like: lhs [* or /] (rhs as unsimplified sum/sub)
      const bool right_side_needs_parentheses =
          IsMultiplyOrDivide() && operands.back()->IsOperation() &&
          To<CSSMathExpressionOperation>(operands.back().Get())
              ->IsAddOrSubtract();
      if (right_side_needs_parentheses) {
        result.Append('(');
      }
      result.Append(operands[1]->CustomCSSText());
      if (right_side_needs_parentheses) {
        result.Append(')');
      }

      return result.ReleaseString();
    }
    case CSSMathOperator::kMin:
    case CSSMathOperator::kMax:
    case CSSMathOperator::kClamp:
    case CSSMathOperator::kMod:
    case CSSMathOperator::kRem:
    case CSSMathOperator::kHypot:
    case CSSMathOperator::kAbs:
    case CSSMathOperator::kSign:
    case CSSMathOperator::kCalcSize: {
      StringBuilder result;
      result.Append(ToString(operator_));
      result.Append('(');
      result.Append(operands_.front()->CustomCSSText());
      for (const CSSMathExpressionNode* operand : SecondToLastOperands()) {
        result.Append(", ");
        result.Append(operand->CustomCSSText());
      }
      result.Append(')');

      return result.ReleaseString();
    }
    case CSSMathOperator::kRoundNearest:
    case CSSMathOperator::kRoundUp:
    case CSSMathOperator::kRoundDown:
    case CSSMathOperator::kRoundToZero: {
      StringBuilder result;
      result.Append(ToString(operator_));
      result.Append('(');
      if (operator_ != CSSMathOperator::kRoundNearest) {
        result.Append(ToRoundingStrategyString(operator_));
        result.Append(", ");
      }
      result.Append(operands_[0]->CustomCSSText());
      if (ShouldSerializeRoundingStep(operands_)) {
        result.Append(", ");
        result.Append(operands_[1]->CustomCSSText());
      }
      result.Append(')');

      return result.ReleaseString();
    }
    case CSSMathOperator::kProgress:
    case CSSMathOperator::kMediaProgress:
    case CSSMathOperator::kContainerProgress: {
      CHECK_EQ(operands_.size(), 3u);
      StringBuilder result;
      result.Append(ToString(operator_));
      result.Append('(');
      result.Append(operands_.front()->CustomCSSText());
      result.Append(" from ");
      result.Append(operands_[1]->CustomCSSText());
      result.Append(" to ");
      result.Append(operands_.back()->CustomCSSText());
      result.Append(')');

      return result.ReleaseString();
    }
    case CSSMathOperator::kInvalid:
      NOTREACHED_IN_MIGRATION();
      return String();
  }
}

bool CSSMathExpressionOperation::operator==(
    const CSSMathExpressionNode& exp) const {
  if (!exp.IsOperation()) {
    return false;
  }

  const CSSMathExpressionOperation& other = To<CSSMathExpressionOperation>(exp);
  if (operator_ != other.operator_) {
    return false;
  }
  if (operands_.size() != other.operands_.size()) {
    return false;
  }
  for (wtf_size_t i = 0; i < operands_.size(); ++i) {
    if (!base::ValuesEquivalent(operands_[i], other.operands_[i])) {
      return false;
    }
  }
  return true;
}

CSSPrimitiveValue::UnitType CSSMathExpressionOperation::ResolvedUnitType()
    const {
  switch (category_) {
    case kCalcNumber:
      return CSSPrimitiveValue::UnitType::kNumber;
    case kCalcAngle:
    case kCalcTime:
    case kCalcFrequency:
    case kCalcLength:
    case kCalcPercent:
    case kCalcResolution:
      switch (operator_) {
        case CSSMathOperator::kMultiply:
        case CSSMathOperator::kDivide: {
          DCHECK_EQ(operands_.size(), 2u);
          if (operands_[0]->Category() == kCalcNumber) {
            return operands_[1]->ResolvedUnitType();
          }
          if (operands_[1]->Category() == kCalcNumber) {
            return operands_[0]->ResolvedUnitType();
          }
          NOTREACHED_IN_MIGRATION();
          return CSSPrimitiveValue::UnitType::kUnknown;
        }
        case CSSMathOperator::kAdd:
        case CSSMathOperator::kSubtract:
        case CSSMathOperator::kMin:
        case CSSMathOperator::kMax:
        case CSSMathOperator::kClamp:
        case CSSMathOperator::kRoundNearest:
        case CSSMathOperator::kRoundUp:
        case CSSMathOperator::kRoundDown:
        case CSSMathOperator::kRoundToZero:
        case CSSMathOperator::kMod:
        case CSSMathOperator::kRem:
        case CSSMathOperator::kHypot:
        case CSSMathOperator::kAbs: {
          CSSPrimitiveValue::UnitType first_type =
              operands_.front()->ResolvedUnitType();
          if (first_type == CSSPrimitiveValue::UnitType::kUnknown) {
            return CSSPrimitiveValue::UnitType::kUnknown;
          }
          for (const CSSMathExpressionNode* operand : SecondToLastOperands()) {
            CSSPrimitiveValue::UnitType next = operand->ResolvedUnitType();
            if (next == CSSPrimitiveValue::UnitType::kUnknown ||
                next != first_type) {
              return CSSPrimitiveValue::UnitType::kUnknown;
            }
          }
          return first_type;
        }
        case CSSMathOperator::kSign:
        case CSSMathOperator::kProgress:
        case CSSMathOperator::kMediaProgress:
        case CSSMathOperator::kContainerProgress:
          return CSSPrimitiveValue::UnitType::kNumber;
        case CSSMathOperator::kCalcSize: {
          DCHECK_EQ(operands_.size(), 2u);
          CSSPrimitiveValue::UnitType calculation_type =
              operands_[1]->ResolvedUnitType();
          if (calculation_type != CSSPrimitiveValue::UnitType::kIdent) {
            // The basis is not involved.
            return calculation_type;
          }
          // TODO(https://crbug.com/313072): We could in theory resolve the
          // 'size' keyword to produce a correct answer in more cases.
          return CSSPrimitiveValue::UnitType::kUnknown;
        }
        case CSSMathOperator::kInvalid:
          NOTREACHED_IN_MIGRATION();
          return CSSPrimitiveValue::UnitType::kUnknown;
      }
    case kCalcLengthFunction:
    case kCalcIntrinsicSize:
    case kCalcOther:
      return CSSPrimitiveValue::UnitType::kUnknown;
    case kCalcIdent:
      return CSSPrimitiveValue::UnitType::kIdent;
  }

  NOTREACHED_IN_MIGRATION();
  return CSSPrimitiveValue::UnitType::kUnknown;
}

void CSSMathExpressionOperation::Trace(Visitor* visitor) const {
  visitor->Trace(operands_);
  CSSMathExpressionNode::Trace(visitor);
}

// static
const CSSMathExpressionNode* CSSMathExpressionOperation::GetNumericLiteralSide(
    const CSSMathExpressionNode* left_side,
    const CSSMathExpressionNode* right_side) {
  if (left_side->Category() == kCalcNumber && left_side->IsNumericLiteral()) {
    return left_side;
  }
  if (right_side->Category() == kCalcNumber && right_side->IsNumericLiteral()) {
    return right_side;
  }
  return nullptr;
}

// static
double CSSMathExpressionOperation::EvaluateOperator(
    const Vector<double>& operands,
    CSSMathOperator op) {
  // Design doc for infinity and NaN: https://bit.ly/349gXjq

  // Any operation with at least one NaN argument produces NaN
  // https://drafts.csswg.org/css-values/#calc-type-checking
  for (double operand : operands) {
    if (std::isnan(operand)) {
      return operand;
    }
  }

  switch (op) {
    case CSSMathOperator::kAdd:
      DCHECK_EQ(operands.size(), 2u);
      return operands[0] + operands[1];
    case CSSMathOperator::kSubtract:
      DCHECK_EQ(operands.size(), 2u);
      return operands[0] - operands[1];
    case CSSMathOperator::kMultiply:
      DCHECK_EQ(operands.size(), 2u);
      return operands[0] * operands[1];
    case CSSMathOperator::kDivide:
      DCHECK(operands.size() == 1u || operands.size() == 2u);
      return operands[0] / operands[1];
    case CSSMathOperator::kMin: {
      if (operands.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
      }
      double minimum = operands[0];
      for (double operand : operands) {
        // std::min(0.0, -0.0) returns 0.0, manually check for such situation
        // and set result to -0.0.
        if (minimum == 0 && operand == 0 &&
            std::signbit(minimum) != std::signbit(operand)) {
          minimum = -0.0;
          continue;
        }
        minimum = std::min(minimum, operand);
      }
      return minimum;
    }
    case CSSMathOperator::kMax: {
      if (operands.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
      }
      double maximum = operands[0];
      for (double operand : operands) {
        // std::max(-0.0, 0.0) returns -0.0, manually check for such situation
        // and set result to 0.0.
        if (maximum == 0 && operand == 0 &&
            std::signbit(maximum) != std::signbit(operand)) {
          maximum = 0.0;
          continue;
        }
        maximum = std::max(maximum, operand);
      }
      return maximum;
    }
    case CSSMathOperator::kClamp: {
      DCHECK_EQ(operands.size(), 3u);
      double min = operands[0];
      double val = operands[1];
      double max = operands[2];
      // clamp(MIN, VAL, MAX) is identical to max(MIN, min(VAL, MAX))
      // according to the spec,
      // https://drafts.csswg.org/css-values-4/#funcdef-clamp.
      double minimum = std::min(val, max);
      // std::min(0.0, -0.0) returns 0.0, so manually check for this situation
      // to set result to -0.0.
      if (val == 0 && max == 0 && !std::signbit(val) && std::signbit(max)) {
        minimum = -0.0;
      }
      double maximum = std::max(min, minimum);
      // std::max(-0.0, 0.0) returns -0.0, so manually check for this situation
      // to set result to 0.0.
      if (min == 0 && minimum == 0 && std::signbit(min) &&
          !std::signbit(minimum)) {
        maximum = 0.0;
      }
      return maximum;
    }
    case CSSMathOperator::kRoundNearest:
    case CSSMathOperator::kRoundUp:
    case CSSMathOperator::kRoundDown:
    case CSSMathOperator::kRoundToZero:
    case CSSMathOperator::kMod:
    case CSSMathOperator::kRem: {
      DCHECK_EQ(operands.size(), 2u);
      return EvaluateSteppedValueFunction(op, operands[0], operands[1]);
    }
    case CSSMathOperator::kHypot: {
      DCHECK_GE(operands.size(), 1u);
      double value = 0;
      for (double operand : operands) {
        value = std::hypot(value, operand);
      }
      return value;
    }
    case CSSMathOperator::kAbs: {
      DCHECK_EQ(operands.size(), 1u);
      return std::abs(operands.front());
    }
    case CSSMathOperator::kSign: {
      DCHECK_EQ(operands.size(), 1u);
      const double value = operands.front();
      const double signum =
          (value == 0 || std::isnan(value)) ? value : ((value > 0) ? 1 : -1);
      return signum;
    }
    case CSSMathOperator::kProgress:
    case CSSMathOperator::kMediaProgress:
    case CSSMathOperator::kContainerProgress: {
      CHECK_EQ(operands.size(), 3u);
      return (operands[0] - operands[1]) / (operands[2] - operands[1]);
    }
    case CSSMathOperator::kCalcSize: {
      CHECK_EQ(operands.size(), 2u);
      // TODO(https://crbug.com/313072): In theory we could also
      // evaluate (a) cases where the basis (operand 0) is not a double,
      // and (b) cases where the basis (operand 0) is a double and the
      // calculation (operand 1) requires 'size' keyword substitutions.
      // But for now just handle the simplest case.
      return operands[1];
    }
    case CSSMathOperator::kInvalid:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return 0;
}

const CSSMathExpressionNode& CSSMathExpressionOperation::PopulateWithTreeScope(
    const TreeScope* tree_scope) const {
  Operands populated_operands;
  for (const CSSMathExpressionNode* op : operands_) {
    populated_operands.push_back(&op->EnsureScopedValue(tree_scope));
  }
  return *MakeGarbageCollected<CSSMathExpressionOperation>(
      Category(), std::move(populated_operands), operator_);
}

const CSSMathExpressionNode* CSSMathExpressionOperation::TransformAnchors(
    LogicalAxis logical_axis,
    const TryTacticTransform& transform,
    const WritingDirectionMode& writing_direction) const {
  Operands transformed_operands;
  for (const CSSMathExpressionNode* op : operands_) {
    transformed_operands.push_back(
        op->TransformAnchors(logical_axis, transform, writing_direction));
  }
  if (transformed_operands != operands_) {
    return MakeGarbageCollected<CSSMathExpressionOperation>(
        Category(), std::move(transformed_operands), operator_);
  }
  return this;
}

bool CSSMathExpressionOperation::HasInvalidAnchorFunctions(
    const CSSLengthResolver& length_resolver) const {
  for (const CSSMathExpressionNode* op : operands_) {
    if (op->HasInvalidAnchorFunctions(length_resolver)) {
      return true;
    }
  }
  return false;
}

#if DCHECK_IS_ON()
bool CSSMathExpressionOperation::InvolvesPercentageComparisons() const {
  if (IsMinOrMax() && Category() == kCalcPercent && operands_.size() > 1u) {
    return true;
  }
  for (const CSSMathExpressionNode* operand : operands_) {
    if (operand->InvolvesPercentageComparisons()) {
      return true;
    }
  }
  return false;
}
#endif

// ------ End of CSSMathExpressionOperation member functions ------

// ------ Start of CSSMathExpressionContainerProgress member functions ----

namespace {

double EvaluateContainerSize(const CSSIdentifierValue* size_feature,
                             const CSSCustomIdentValue* container_name,
                             const CSSLengthResolver& length_resolver) {
  if (container_name) {
    ScopedCSSName* name = MakeGarbageCollected<ScopedCSSName>(
        container_name->Value(), container_name->GetTreeScope());
    switch (size_feature->GetValueID()) {
      case CSSValueID::kWidth:
        return length_resolver.ContainerWidth(*name);
      case CSSValueID::kHeight:
        return length_resolver.ContainerHeight(*name);
      default:
        NOTREACHED();
    }
  } else {
    switch (size_feature->GetValueID()) {
      case CSSValueID::kWidth:
        return length_resolver.ContainerWidth();
      case CSSValueID::kHeight:
        return length_resolver.ContainerHeight();
      default:
        NOTREACHED();
    }
  }
}

}  // namespace

CSSMathExpressionContainerFeature::CSSMathExpressionContainerFeature(
    const CSSIdentifierValue* size_feature,
    const CSSCustomIdentValue* container_name)
    : CSSMathExpressionNode(
          CalculationResultCategory::kCalcLength,
          /*has_comparisons =*/false,
          /*has_anchor_functions =*/false,
          /*needs_tree_scope_population =*/
          (container_name && !container_name->IsScopedValue())),
      size_feature_(size_feature),
      container_name_(container_name) {
  CHECK(size_feature);
}

String CSSMathExpressionContainerFeature::CustomCSSText() const {
  StringBuilder builder;
  builder.Append(size_feature_->CustomCSSText());
  if (container_name_ && !container_name_->Value().empty()) {
    builder.Append(" of ");
    builder.Append(container_name_->CustomCSSText());
  }
  return builder.ToString();
}

scoped_refptr<const CalculationExpressionNode>
CSSMathExpressionContainerFeature::ToCalculationExpression(
    const CSSLengthResolver& length_resolver) const {
  double progress =
      EvaluateContainerSize(size_feature_, container_name_, length_resolver);
  return base::MakeRefCounted<CalculationExpressionPixelsAndPercentNode>(
      PixelsAndPercent(progress));
}

std::optional<PixelsAndPercent>
CSSMathExpressionContainerFeature::ToPixelsAndPercent(
    const CSSLengthResolver& length_resolver) const {
  return PixelsAndPercent(ComputeDouble(length_resolver));
}

double CSSMathExpressionContainerFeature::ComputeDouble(
    const CSSLengthResolver& length_resolver) const {
  return EvaluateContainerSize(size_feature_, container_name_, length_resolver);
}

// ------ End of CSSMathExpressionContainerProgress member functions ------

// ------ Start of CSSMathExpressionAnchorQuery member functions ------

namespace {

CalculationResultCategory AnchorQueryCategory(
    const CSSPrimitiveValue* fallback) {
  // Note that the main (non-fallback) result of an anchor query is always
  // a kCalcLength, so the only thing that can make our overall result anything
  // else is the fallback.
  if (!fallback || fallback->IsLength()) {
    return kCalcLength;
  }
  // This can happen for e.g. anchor(--a top, 10%). In this case, we can't
  // tell if we're going to return a <length> or a <percentage> without actually
  // evaluating the query.
  //
  // TODO(crbug.com/326088870): Evaluate anchor queries when understanding
  // the CalculationResultCategory for an expression.
  return kCalcLengthFunction;
}

}  // namespace

CSSMathExpressionAnchorQuery::CSSMathExpressionAnchorQuery(
    CSSAnchorQueryType type,
    const CSSValue* anchor_specifier,
    const CSSValue* value,
    const CSSPrimitiveValue* fallback)
    : CSSMathExpressionNode(
          AnchorQueryCategory(fallback),
          false /* has_comparisons */,
          true /* has_anchor_functions */,
          (anchor_specifier && !anchor_specifier->IsScopedValue()) ||
              (fallback && !fallback->IsScopedValue())),
      type_(type),
      anchor_specifier_(anchor_specifier),
      value_(value),
      fallback_(fallback) {}

double CSSMathExpressionAnchorQuery::DoubleValue() const {
  NOTREACHED_IN_MIGRATION();
  return 0;
}

double CSSMathExpressionAnchorQuery::ComputeLengthPx(
    const CSSLengthResolver& length_resolver) const {
  return ComputeDouble(length_resolver);
}

double CSSMathExpressionAnchorQuery::ComputeDouble(
    const CSSLengthResolver& length_resolver) const {
  CHECK_EQ(kCalcLength, Category());
  // Note: The category may also be kCalcLengthFunction (see
  // AnchorQueryCategory), in which case we'll reach ToCalculationExpression
  // instead.

  AnchorQuery query = ToQuery(length_resolver);

  if (std::optional<LayoutUnit> px = EvaluateQuery(query, length_resolver)) {
    return px.value();
  }

  // We should have checked HasInvalidAnchorFunctions() before entering here.
  CHECK(fallback_);
  return fallback_->ComputeLength<double>(length_resolver);
}

String CSSMathExpressionAnchorQuery::CustomCSSText() const {
  StringBuilder result;
  result.Append(IsAnchor() ? "anchor(" : "anchor-size(");
  if (anchor_specifier_) {
    result.Append(anchor_specifier_->CssText());
    if (value_) {
      result.Append(" ");
    }
  }
  if (value_) {
    result.Append(value_->CssText());
  }
  if (fallback_) {
    if (anchor_specifier_ || value_) {
      result.Append(", ");
    }
    result.Append(fallback_->CustomCSSText());
  }
  result.Append(")");
  return result.ToString();
}

bool CSSMathExpressionAnchorQuery::operator==(
    const CSSMathExpressionNode& other) const {
  const auto* other_anchor = DynamicTo<CSSMathExpressionAnchorQuery>(other);
  if (!other_anchor) {
    return false;
  }
  return type_ == other_anchor->type_ &&
         base::ValuesEquivalent(anchor_specifier_,
                                other_anchor->anchor_specifier_) &&
         base::ValuesEquivalent(value_, other_anchor->value_) &&
         base::ValuesEquivalent(fallback_, other_anchor->fallback_);
}

namespace {

CSSAnchorValue CSSValueIDToAnchorValueEnum(CSSValueID value) {
  switch (value) {
    case CSSValueID::kInside:
      return CSSAnchorValue::kInside;
    case CSSValueID::kOutside:
      return CSSAnchorValue::kOutside;
    case CSSValueID::kTop:
      return CSSAnchorValue::kTop;
    case CSSValueID::kLeft:
      return CSSAnchorValue::kLeft;
    case CSSValueID::kRight:
      return CSSAnchorValue::kRight;
    case CSSValueID::kBottom:
      return CSSAnchorValue::kBottom;
    case CSSValueID::kStart:
      return CSSAnchorValue::kStart;
    case CSSValueID::kEnd:
      return CSSAnchorValue::kEnd;
    case CSSValueID::kSelfStart:
      return CSSAnchorValue::kSelfStart;
    case CSSValueID::kSelfEnd:
      return CSSAnchorValue::kSelfEnd;
    case CSSValueID::kCenter:
      return CSSAnchorValue::kCenter;
    default:
      NOTREACHED_IN_MIGRATION();
      return CSSAnchorValue::kCenter;
  }
}

CSSAnchorSizeValue CSSValueIDToAnchorSizeValueEnum(CSSValueID value) {
  switch (value) {
    case CSSValueID::kWidth:
      return CSSAnchorSizeValue::kWidth;
    case CSSValueID::kHeight:
      return CSSAnchorSizeValue::kHeight;
    case CSSValueID::kBlock:
      return CSSAnchorSizeValue::kBlock;
    case CSSValueID::kInline:
      return CSSAnchorSizeValue::kInline;
    case CSSValueID::kSelfBlock:
      return CSSAnchorSizeValue::kSelfBlock;
    case CSSValueID::kSelfInline:
      return CSSAnchorSizeValue::kSelfInline;
    default:
      NOTREACHED_IN_MIGRATION();
      return CSSAnchorSizeValue::kImplicit;
  }
}

}  // namespace

scoped_refptr<const CalculationExpressionNode>
CSSMathExpressionAnchorQuery::ToCalculationExpression(
    const CSSLengthResolver& length_resolver) const {
  AnchorQuery query = ToQuery(length_resolver);

  Length result;

  if (std::optional<LayoutUnit> px = EvaluateQuery(query, length_resolver)) {
    result = Length::Fixed(px.value());
  } else {
    // We should have checked HasInvalidAnchorFunctions() before entering here.
    CHECK(fallback_);
    result = fallback_->ConvertToLength(length_resolver);
  }

  return result.AsCalculationValue()->GetOrCreateExpression();
}

std::optional<LayoutUnit> CSSMathExpressionAnchorQuery::EvaluateQuery(
    const AnchorQuery& query,
    const CSSLengthResolver& length_resolver) const {
  length_resolver.ReferenceAnchor();
  if (AnchorEvaluator* anchor_evaluator =
          length_resolver.GetAnchorEvaluator()) {
    return anchor_evaluator->Evaluate(query,
                                      length_resolver.GetPositionAnchor(),
                                      length_resolver.GetPositionAreaOffsets());
  }
  return std::nullopt;
}

AnchorQuery CSSMathExpressionAnchorQuery::ToQuery(
    const CSSLengthResolver& length_resolver) const {
  DCHECK(IsScopedValue());
  AnchorSpecifierValue* anchor_specifier = AnchorSpecifierValue::Default();
  if (const auto* custom_ident =
          DynamicTo<CSSCustomIdentValue>(anchor_specifier_.Get())) {
    length_resolver.ReferenceTreeScope();
    anchor_specifier = MakeGarbageCollected<AnchorSpecifierValue>(
        *MakeGarbageCollected<ScopedCSSName>(custom_ident->Value(),
                                             custom_ident->GetTreeScope()));
  }
  if (type_ == CSSAnchorQueryType::kAnchor) {
    if (const CSSPrimitiveValue* percentage =
            DynamicTo<CSSPrimitiveValue>(*value_)) {
      DCHECK(percentage->IsPercentage());
      return AnchorQuery(type_, anchor_specifier, percentage->GetFloatValue(),
                         CSSAnchorValue::kPercentage);
    }
    const CSSIdentifierValue& side = To<CSSIdentifierValue>(*value_);
    return AnchorQuery(type_, anchor_specifier, /* percentage */ 0,
                       CSSValueIDToAnchorValueEnum(side.GetValueID()));
  }

  DCHECK_EQ(type_, CSSAnchorQueryType::kAnchorSize);
  CSSAnchorSizeValue size = CSSAnchorSizeValue::kImplicit;
  if (value_) {
    size = CSSValueIDToAnchorSizeValueEnum(
        To<CSSIdentifierValue>(*value_).GetValueID());
  }
  return AnchorQuery(type_, anchor_specifier, /* percentage */ 0, size);
}

const CSSMathExpressionNode&
CSSMathExpressionAnchorQuery::PopulateWithTreeScope(
    const TreeScope* tree_scope) const {
  return *MakeGarbageCollected<CSSMathExpressionAnchorQuery>(
      type_,
      anchor_specifier_ ? &anchor_specifier_->EnsureScopedValue(tree_scope)
                        : nullptr,
      value_,
      fallback_
          ? To<CSSPrimitiveValue>(&fallback_->EnsureScopedValue(tree_scope))
          : nullptr);
}

namespace {

bool FlipLogical(LogicalAxis logical_axis,
                 const TryTacticTransform& transform) {
  return (logical_axis == LogicalAxis::kInline) ? transform.FlippedInline()
                                                : transform.FlippedBlock();
}

CSSValueID TransformAnchorCSSValueID(
    CSSValueID from,
    LogicalAxis logical_axis,
    const TryTacticTransform& transform,
    const WritingDirectionMode& writing_direction) {
  // The value transformation happens on logical insets, so we need to first
  // translate physical to logical, then carry out the transform, and then
  // convert *back* to physical.
  PhysicalToLogical logical_insets(writing_direction, CSSValueID::kTop,
                                   CSSValueID::kRight, CSSValueID::kBottom,
                                   CSSValueID::kLeft);

  LogicalToPhysical<CSSValueID> insets = transform.Transform(
      TryTacticTransform::LogicalSides<CSSValueID>{
          .inline_start = logical_insets.InlineStart(),
          .inline_end = logical_insets.InlineEnd(),
          .block_start = logical_insets.BlockStart(),
          .block_end = logical_insets.BlockEnd()},
      writing_direction);

  bool flip_logical = FlipLogical(logical_axis, transform);

  switch (from) {
    // anchor()
    case CSSValueID::kTop:
      return insets.Top();
    case CSSValueID::kLeft:
      return insets.Left();
    case CSSValueID::kRight:
      return insets.Right();
    case CSSValueID::kBottom:
      return insets.Bottom();
    case CSSValueID::kStart:
      return flip_logical ? CSSValueID::kEnd : from;
    case CSSValueID::kEnd:
      return flip_logical ? CSSValueID::kStart : from;
    case CSSValueID::kSelfStart:
      return flip_logical ? CSSValueID::kSelfEnd : from;
    case CSSValueID::kSelfEnd:
      return flip_logical ? CSSValueID::kSelfStart : from;
    case CSSValueID::kCenter:
      return from;
    // anchor-size()
    case CSSValueID::kWidth:
      return transform.FlippedStart() ? CSSValueID::kHeight : from;
    case CSSValueID::kHeight:
      return transform.FlippedStart() ? CSSValueID::kWidth : from;
    case CSSValueID::kBlock:
      return transform.FlippedStart() ? CSSValueID::kInline : from;
    case CSSValueID::kInline:
      return transform.FlippedStart() ? CSSValueID::kBlock : from;
    case CSSValueID::kSelfBlock:
      return transform.FlippedStart() ? CSSValueID::kSelfInline : from;
    case CSSValueID::kSelfInline:
      return transform.FlippedStart() ? CSSValueID::kSelfBlock : from;
    default:
      NOTREACHED_IN_MIGRATION();
      return from;
  }
}

float TransformAnchorPercentage(float from,
                                LogicalAxis logical_axis,
                                const TryTacticTransform& transform) {
  return FlipLogical(logical_axis, transform) ? (100.0f - from) : from;
}

}  // namespace

const CSSMathExpressionNode* CSSMathExpressionAnchorQuery::TransformAnchors(
    LogicalAxis logical_axis,
    const TryTacticTransform& transform,
    const WritingDirectionMode& writing_direction) const {
  const CSSValue* transformed_value = value_;
  if (const auto* side = DynamicTo<CSSIdentifierValue>(value_.Get())) {
    CSSValueID from = side->GetValueID();
    CSSValueID to = TransformAnchorCSSValueID(from, logical_axis, transform,
                                              writing_direction);
    if (from != to) {
      transformed_value = CSSIdentifierValue::Create(to);
    }
  } else if (const auto* percentage =
                 DynamicTo<CSSPrimitiveValue>(value_.Get())) {
    DCHECK(percentage->IsPercentage());
    float from = percentage->GetFloatValue();
    float to = TransformAnchorPercentage(from, logical_axis, transform);
    if (from != to) {
      transformed_value = CSSNumericLiteralValue::Create(
          to, CSSPrimitiveValue::UnitType::kPercentage);
    }
  }

  // The fallback can contain anchors.
  const CSSPrimitiveValue* transformed_fallback = fallback_.Get();
  if (const auto* math_function =
          DynamicTo<CSSMathFunctionValue>(fallback_.Get())) {
    transformed_fallback = math_function->TransformAnchors(
        logical_axis, transform, writing_direction);
  }

  if (transformed_value != value_ || transformed_fallback != fallback_) {
    // Either the value or the fallback was transformed.
    return MakeGarbageCollected<CSSMathExpressionAnchorQuery>(
        type_, anchor_specifier_, transformed_value, transformed_fallback);
  }

  // No transformation.
  return this;
}

bool CSSMathExpressionAnchorQuery::HasInvalidAnchorFunctions(
    const CSSLengthResolver& length_resolver) const {
  AnchorQuery query = ToQuery(length_resolver);
  std::optional<LayoutUnit> px = EvaluateQuery(query, length_resolver);

  if (px.has_value()) {
    return false;
  }

  // We need to take the fallback. However, if there is no fallback,
  // then we are invalid at computed-value time [1].
  // [1] // https://drafts.csswg.org/css-anchor-position-1/#anchor-valid

  if (fallback_) {
    if (auto* math_fallback =
            DynamicTo<CSSMathFunctionValue>(fallback_.Get())) {
      // The fallback itself may also contain invalid anchor*() functions.
      return math_fallback->HasInvalidAnchorFunctions(length_resolver);
    }
    return false;
  }

  return true;
}

void CSSMathExpressionAnchorQuery::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_specifier_);
  visitor->Trace(value_);
  visitor->Trace(fallback_);
  CSSMathExpressionNode::Trace(visitor);
}

// ------ End of CSSMathExpressionAnchorQuery member functions ------

class CSSMathExpressionNodeParser {
  STACK_ALLOCATED();

 public:
  using Flag = CSSMathExpressionNode::Flag;
  using Flags = CSSMathExpressionNode::Flags;

  // A struct containing parser state that varies within the expression tree.
  struct State {
    STACK_ALLOCATED();

   public:
    uint8_t depth;
    bool allow_size_keyword;

    static_assert(uint8_t(kMaxExpressionDepth + 1) == kMaxExpressionDepth + 1);

    State() : depth(0), allow_size_keyword(false) {}
    State(const State&) = default;
    State& operator=(const State&) = default;
  };

  CSSMathExpressionNodeParser(const CSSParserContext& context,
                              const Flags parsing_flags,
                              CSSAnchorQueryTypes allowed_anchor_queries,
                              const CSSColorChannelMap& color_channel_map)
      : context_(context),
        allowed_anchor_queries_(allowed_anchor_queries),
        parsing_flags_(parsing_flags),
        color_channel_map_(color_channel_map) {}

  bool IsSupportedMathFunction(CSSValueID function_id) {
    switch (function_id) {
      case CSSValueID::kMin:
      case CSSValueID::kMax:
      case CSSValueID::kClamp:
      case CSSValueID::kCalc:
      case CSSValueID::kWebkitCalc:
      case CSSValueID::kSin:
      case CSSValueID::kCos:
      case CSSValueID::kTan:
      case CSSValueID::kAsin:
      case CSSValueID::kAcos:
      case CSSValueID::kAtan:
      case CSSValueID::kAtan2:
      case CSSValueID::kAnchor:
      case CSSValueID::kAnchorSize:
        return true;
      case CSSValueID::kPow:
      case CSSValueID::kSqrt:
      case CSSValueID::kHypot:
      case CSSValueID::kLog:
      case CSSValueID::kExp:
        return RuntimeEnabledFeatures::CSSExponentialFunctionsEnabled();
      case CSSValueID::kRound:
      case CSSValueID::kMod:
      case CSSValueID::kRem:
        return RuntimeEnabledFeatures::CSSSteppedValueFunctionsEnabled();
      case CSSValueID::kAbs:
      case CSSValueID::kSign:
        return RuntimeEnabledFeatures::CSSSignRelatedFunctionsEnabled();
      case CSSValueID::kProgress:
      case CSSValueID::kMediaProgress:
      case CSSValueID::kContainerProgress:
        return RuntimeEnabledFeatures::CSSProgressNotationEnabled();
      case CSSValueID::kCalcSize:
        return RuntimeEnabledFeatures::CSSCalcSizeFunctionEnabled();
      // TODO(crbug.com/1284199): Support other math functions.
      default:
        return false;
    }
  }

  CSSMathExpressionNode* ParseAnchorQuery(CSSValueID function_id,
                                          CSSParserTokenStream& stream) {
    CSSAnchorQueryType anchor_query_type;
    switch (function_id) {
      case CSSValueID::kAnchor:
        anchor_query_type = CSSAnchorQueryType::kAnchor;
        break;
      case CSSValueID::kAnchorSize:
        anchor_query_type = CSSAnchorQueryType::kAnchorSize;
        break;
      default:
        return nullptr;
    }

    if (!(static_cast<CSSAnchorQueryTypes>(anchor_query_type) &
          allowed_anchor_queries_)) {
      return nullptr;
    }

    // |anchor_specifier| may be omitted to represent the default anchor.
    const CSSValue* anchor_specifier =
        css_parsing_utils::ConsumeDashedIdent(stream, context_);

    stream.ConsumeWhitespace();
    const CSSValue* value = nullptr;
    switch (anchor_query_type) {
      case CSSAnchorQueryType::kAnchor:
        value = css_parsing_utils::ConsumeIdent<
            CSSValueID::kInside, CSSValueID::kOutside, CSSValueID::kTop,
            CSSValueID::kLeft, CSSValueID::kRight, CSSValueID::kBottom,
            CSSValueID::kStart, CSSValueID::kEnd, CSSValueID::kSelfStart,
            CSSValueID::kSelfEnd, CSSValueID::kCenter>(stream);
        if (!value) {
          value = css_parsing_utils::ConsumePercent(
              stream, context_, CSSPrimitiveValue::ValueRange::kAll);
        }
        break;
      case CSSAnchorQueryType::kAnchorSize:
        value = css_parsing_utils::ConsumeIdent<
            CSSValueID::kWidth, CSSValueID::kHeight, CSSValueID::kBlock,
            CSSValueID::kInline, CSSValueID::kSelfBlock,
            CSSValueID::kSelfInline>(stream);
        break;
    }
    if (!value && function_id == CSSValueID::kAnchor) {
      return nullptr;
    }

    stream.ConsumeWhitespace();
    // |anchor_specifier| may appear after the <anchor-side> / <anchor-size>.
    if (!anchor_specifier) {
      anchor_specifier =
          css_parsing_utils::ConsumeDashedIdent(stream, context_);
    }

    bool expect_comma = anchor_specifier || value;
    const CSSPrimitiveValue* fallback = nullptr;
    if (!expect_comma ||
        css_parsing_utils::ConsumeCommaIncludingWhitespace(stream)) {
      fallback = css_parsing_utils::ConsumeLengthOrPercent(
          stream, context_, CSSPrimitiveValue::ValueRange::kAll,
          css_parsing_utils::UnitlessQuirk::kForbid, allowed_anchor_queries_);
      if (expect_comma && !fallback) {
        return nullptr;
      }
    }

    stream.ConsumeWhitespace();
    if (!stream.AtEnd()) {
      return nullptr;
    }
    return MakeGarbageCollected<CSSMathExpressionAnchorQuery>(
        anchor_query_type, anchor_specifier, value, fallback);
  }

  bool ParseProgressNotationFromTo(
      CSSParserTokenStream& stream,
      State state,
      CSSMathExpressionOperation::Operands& nodes) {
    if (stream.Peek().Id() != CSSValueID::kFrom) {
      return false;
    }
    stream.ConsumeIncludingWhitespace();
    if (CSSMathExpressionNode* node = ParseValueExpression(stream, state)) {
      nodes.push_back(node);
    }
    if (stream.Peek().Id() != CSSValueID::kTo) {
      return false;
    }
    stream.ConsumeIncludingWhitespace();
    if (CSSMathExpressionNode* node = ParseValueExpression(stream, state)) {
      nodes.push_back(node);
    }
    return true;
  }

  // https://drafts.csswg.org/css-values-5/#progress-func
  // https://drafts.csswg.org/css-values-5/#media-progress-func
  // https://drafts.csswg.org/css-values-5/#container-progress-func
  CSSMathExpressionNode* ParseProgressNotation(CSSValueID function_id,
                                               CSSParserTokenStream& stream,
                                               State state) {
    if (function_id != CSSValueID::kProgress &&
        function_id != CSSValueID::kMediaProgress &&
        function_id != CSSValueID::kContainerProgress) {
      return nullptr;
    }
    // <media-progress()> = media-progress(<media-feature> from <calc-sum> to
    // <calc-sum>)
    CSSMathExpressionOperation::Operands nodes;
    stream.ConsumeWhitespace();
    if (function_id == CSSValueID::kMediaProgress) {
      if (CSSMathExpressionKeywordLiteral* node = ParseKeywordLiteral(
              stream,
              CSSMathExpressionKeywordLiteral::Context::kMediaProgress)) {
        nodes.push_back(node);
      }
    } else if (function_id == CSSValueID::kContainerProgress) {
      // <container-progress()> = container-progress(<size-feature> [ of
      // <container-name> ]? from <calc-sum> to <calc-sum>)
      const CSSIdentifierValue* size_feature =
          css_parsing_utils::ConsumeIdent(stream);
      if (!size_feature) {
        return nullptr;
      }
      if (stream.Peek().Id() == CSSValueID::kOf) {
        stream.ConsumeIncludingWhitespace();
        const CSSCustomIdentValue* container_name =
            css_parsing_utils::ConsumeCustomIdent(stream, context_);
        if (!container_name) {
          return nullptr;
        }
        nodes.push_back(MakeGarbageCollected<CSSMathExpressionContainerFeature>(
            size_feature, container_name));
      } else {
        nodes.push_back(MakeGarbageCollected<CSSMathExpressionContainerFeature>(
            size_feature, nullptr));
      }
    } else if (CSSMathExpressionNode* node =
                   ParseValueExpression(stream, state)) {
      // <progress()> = progress(<calc-sum> from <calc-sum> to <calc-sum>)
      nodes.push_back(node);
    }
    if (!ParseProgressNotationFromTo(stream, state, nodes)) {
      return nullptr;
    }
    if (nodes.size() != 3u || !stream.AtEnd() ||
        !CheckProgressFunctionTypes(function_id, nodes)) {
      return nullptr;
    }
    // Note: we don't need to resolve percents in such case,
    // as all the operands are numeric literals,
    // so p% / (t% - f%) will lose %.
    // Note: we can not simplify media-progress.
    ProgressArgsSimplificationStatus status =
        CanEagerlySimplifyProgressArgs(nodes);
    if (function_id == CSSValueID::kProgress &&
        status != ProgressArgsSimplificationStatus::kCanNotSimplify) {
      Vector<double> double_values;
      double_values.reserve(nodes.size());
      for (const CSSMathExpressionNode* operand : nodes) {
        if (status ==
            ProgressArgsSimplificationStatus::kAllArgsResolveToCanonical) {
          std::optional<double> canonical_value =
              operand->ComputeValueInCanonicalUnit();
          CHECK(canonical_value.has_value());
          double_values.push_back(canonical_value.value());
        } else {
          CHECK(HasDoubleValue(operand->ResolvedUnitType()));
          double_values.push_back(operand->DoubleValue());
        }
      }
      double progress_value = (double_values[0] - double_values[1]) /
                              (double_values[2] - double_values[1]);
      return CSSMathExpressionNumericLiteral::Create(
          progress_value, CSSPrimitiveValue::UnitType::kNumber);
    }
    return MakeGarbageCollected<CSSMathExpressionOperation>(
        CalculationResultCategory::kCalcNumber, std::move(nodes),
        CSSValueIDToCSSMathOperator(function_id));
  }

  CSSMathExpressionNode* ParseCalcSize(CSSValueID function_id,
                                       CSSParserTokenStream& stream,
                                       State state) {
    if (function_id != CSSValueID::kCalcSize ||
        !parsing_flags_.Has(Flag::AllowCalcSize)) {
      return nullptr;
    }

    DCHECK(RuntimeEnabledFeatures::CSSCalcSizeFunctionEnabled());

    stream.ConsumeWhitespace();

    CSSMathExpressionNode* basis = nullptr;

    CSSValueID id = stream.Peek().Id();
    bool basis_is_any = id == CSSValueID::kAny;
    if (id != CSSValueID::kInvalid &&
        (id == CSSValueID::kAny ||
         (id == CSSValueID::kAuto &&
          parsing_flags_.Has(Flag::AllowAutoInCalcSize)) ||
         (id == CSSValueID::kContent &&
          parsing_flags_.Has(Flag::AllowContentInCalcSize)) ||
         css_parsing_utils::ValidWidthOrHeightKeyword(id, context_))) {
      // TODO(https://crbug.com/353538495): Right now 'flex-basis'
      // accepts fewer keywords than other width properties.  So for
      // now specifically exclude the ones that it doesn't accept,
      // based off the flag for accepting 'content'.
      if (parsing_flags_.Has(Flag::AllowContentInCalcSize) &&
          !css_parsing_utils::IdentMatches<
              CSSValueID::kAny, CSSValueID::kAuto, CSSValueID::kContent,
              CSSValueID::kMinContent, CSSValueID::kMaxContent,
              CSSValueID::kFitContent>(id)) {
        return nullptr;
      }

      // Note: We don't want to accept 'none' (for 'max-*' properties) since
      // it's not meaningful for animation, since it's equivalent to infinity.
      stream.ConsumeIncludingWhitespace();
      basis = CSSMathExpressionKeywordLiteral::Create(
          id, CSSMathExpressionKeywordLiteral::Context::kCalcSize);
    } else {
      basis = ParseValueExpression(stream, state);
      if (!basis) {
        return nullptr;
      }
    }

    if (!css_parsing_utils::ConsumeCommaIncludingWhitespace(stream)) {
      return nullptr;
    }

    state.allow_size_keyword = !basis_is_any;
    CSSMathExpressionNode* calculation = ParseValueExpression(stream, state);
    if (!calculation) {
      return nullptr;
    }

    return CSSMathExpressionOperation::CreateCalcSizeOperation(basis,
                                                               calculation);
  }

  CSSMathExpressionNode* ParseMathFunction(CSSValueID function_id,
                                           CSSParserTokenStream& stream,
                                           State state) {
    if (!IsSupportedMathFunction(function_id)) {
      return nullptr;
    }
    if (auto* anchor_query = ParseAnchorQuery(function_id, stream)) {
      context_.Count(WebFeature::kCSSAnchorPositioning);
      return anchor_query;
    }
    if (RuntimeEnabledFeatures::CSSProgressNotationEnabled()) {
      if (CSSMathExpressionNode* progress =
              ParseProgressNotation(function_id, stream, state)) {
        return progress;
      }
    }
    if (RuntimeEnabledFeatures::CSSCalcSizeFunctionEnabled()) {
      if (CSSMathExpressionNode* calc_size =
              ParseCalcSize(function_id, stream, state)) {
        return calc_size;
      }
    }

    // "arguments" refers to comma separated ones.
    wtf_size_t min_argument_count = 1;
    wtf_size_t max_argument_count = std::numeric_limits<wtf_size_t>::max();

    switch (function_id) {
      case CSSValueID::kCalc:
      case CSSValueID::kWebkitCalc:
        max_argument_count = 1;
        break;
      case CSSValueID::kMin:
      case CSSValueID::kMax:
        break;
      case CSSValueID::kClamp:
        min_argument_count = 3;
        max_argument_count = 3;
        break;
      case CSSValueID::kSin:
      case CSSValueID::kCos:
      case CSSValueID::kTan:
      case CSSValueID::kAsin:
      case CSSValueID::kAcos:
      case CSSValueID::kAtan:
        max_argument_count = 1;
        break;
      case CSSValueID::kPow:
        DCHECK(RuntimeEnabledFeatures::CSSExponentialFunctionsEnabled());
        max_argument_count = 2;
        min_argument_count = 2;
        break;
      case CSSValueID::kExp:
      case CSSValueID::kSqrt:
        DCHECK(RuntimeEnabledFeatures::CSSExponentialFunctionsEnabled());
        max_argument_count = 1;
        break;
      case CSSValueID::kHypot:
        DCHECK(RuntimeEnabledFeatures::CSSExponentialFunctionsEnabled());
        max_argument_count = kMaxExpressionDepth;
        break;
      case CSSValueID::kLog:
        DCHECK(RuntimeEnabledFeatures::CSSExponentialFunctionsEnabled());
        max_argument_count = 2;
        break;
      case CSSValueID::kRound:
        DCHECK(RuntimeEnabledFeatures::CSSSteppedValueFunctionsEnabled());
        max_argument_count = 3;
        min_argument_count = 1;
        break;
      case CSSValueID::kMod:
      case CSSValueID::kRem:
        DCHECK(RuntimeEnabledFeatures::CSSSteppedValueFunctionsEnabled());
        max_argument_count = 2;
        min_argument_count = 2;
        break;
      case CSSValueID::kAtan2:
        max_argument_count = 2;
        min_argument_count = 2;
        break;
      case CSSValueID::kAbs:
      case CSSValueID::kSign:
        DCHECK(RuntimeEnabledFeatures::CSSSignRelatedFunctionsEnabled());
        max_argument_count = 1;
        min_argument_count = 1;
        break;
      // TODO(crbug.com/1284199): Support other math functions.
      default:
        break;
    }

    HeapVector<Member<const CSSMathExpressionNode>> nodes;
    // Parse the initial (optional) <rounding-strategy> argument to the round()
    // function.
    if (function_id == CSSValueID::kRound) {
      CSSMathExpressionNode* rounding_strategy = ParseRoundingStrategy(stream);
      if (rounding_strategy) {
        nodes.push_back(rounding_strategy);
      }
    }

    while (!stream.AtEnd() && nodes.size() < max_argument_count) {
      if (nodes.size()) {
        if (!css_parsing_utils::ConsumeCommaIncludingWhitespace(stream)) {
          return nullptr;
        }
      }

      stream.ConsumeWhitespace();
      CSSMathExpressionNode* node = ParseValueExpression(stream, state);
      if (!node) {
        return nullptr;
      }

      nodes.push_back(node);
    }

    if (!stream.AtEnd() || nodes.size() < min_argument_count) {
      return nullptr;
    }

    switch (function_id) {
      case CSSValueID::kCalc:
      case CSSValueID::kWebkitCalc: {
        const CSSMathExpressionNode* node = nodes.front();
        if (node->Category() == kCalcIntrinsicSize) {
          return nullptr;
        }
        return const_cast<CSSMathExpressionNode*>(node);
      }
      case CSSValueID::kMin:
      case CSSValueID::kMax:
      case CSSValueID::kClamp: {
        CSSMathOperator op = CSSMathOperator::kMin;
        if (function_id == CSSValueID::kMax) {
          op = CSSMathOperator::kMax;
        }
        if (function_id == CSSValueID::kClamp) {
          op = CSSMathOperator::kClamp;
        }
        CSSMathExpressionNode* node =
            CSSMathExpressionOperation::CreateComparisonFunctionSimplified(
                std::move(nodes), op);
        if (node) {
          context_.Count(WebFeature::kCSSComparisonFunctions);
        }
        return node;
      }
      case CSSValueID::kSin:
      case CSSValueID::kCos:
      case CSSValueID::kTan:
      case CSSValueID::kAsin:
      case CSSValueID::kAcos:
      case CSSValueID::kAtan:
      case CSSValueID::kAtan2:
        return CSSMathExpressionOperation::
            CreateTrigonometricFunctionSimplified(std::move(nodes),
                                                  function_id);
      case CSSValueID::kPow:
      case CSSValueID::kSqrt:
      case CSSValueID::kHypot:
      case CSSValueID::kLog:
      case CSSValueID::kExp:
        DCHECK(RuntimeEnabledFeatures::CSSExponentialFunctionsEnabled());
        return CSSMathExpressionOperation::CreateExponentialFunction(
            std::move(nodes), function_id);
      case CSSValueID::kRound:
      case CSSValueID::kMod:
      case CSSValueID::kRem: {
        DCHECK(RuntimeEnabledFeatures::CSSSteppedValueFunctionsEnabled());
        CSSMathOperator op;
        if (function_id == CSSValueID::kRound) {
          DCHECK_GE(nodes.size(), 1u);
          DCHECK_LE(nodes.size(), 3u);
          // If the first argument is a rounding strategy, use the specified
          // operation and drop the argument from the list of operands.
          const auto* maybe_rounding_strategy =
              DynamicTo<CSSMathExpressionOperation>(*nodes[0]);
          if (maybe_rounding_strategy &&
              maybe_rounding_strategy->IsRoundingStrategyKeyword()) {
            op = maybe_rounding_strategy->OperatorType();
            nodes.EraseAt(0);
          } else {
            op = CSSMathOperator::kRoundNearest;
          }
          if (!CanonicalizeRoundArguments(nodes)) {
            return nullptr;
          }
        } else if (function_id == CSSValueID::kMod) {
          op = CSSMathOperator::kMod;
        } else {
          op = CSSMathOperator::kRem;
        }
        DCHECK_EQ(nodes.size(), 2u);
        return CSSMathExpressionOperation::CreateSteppedValueFunction(
            std::move(nodes), op);
      }
      case CSSValueID::kAbs:
      case CSSValueID::kSign:
        // TODO(seokho): Relative and Percent values cannot be evaluated at the
        // parsing time. So we should implement cannot be simplified value
        // using CalculationExpressionNode
        DCHECK(RuntimeEnabledFeatures::CSSSignRelatedFunctionsEnabled());
        return CSSMathExpressionOperation::CreateSignRelatedFunction(
            std::move(nodes), function_id);

      // TODO(crbug.com/1284199): Support other math functions.
      default:
        return nullptr;
    }
  }

 private:
  CSSMathExpressionNode* ParseValue(CSSParserTokenStream& stream,
                                    State state,
                                    bool& whitespace_after_token) {
    CSSParserToken token = stream.Consume();
    whitespace_after_token = stream.Peek().GetType() == kWhitespaceToken;
    stream.ConsumeWhitespace();
    if (token.Id() == CSSValueID::kInfinity) {
      return CSSMathExpressionNumericLiteral::Create(
          std::numeric_limits<double>::infinity(),
          CSSPrimitiveValue::UnitType::kNumber);
    }
    if (token.Id() == CSSValueID::kNegativeInfinity) {
      return CSSMathExpressionNumericLiteral::Create(
          -std::numeric_limits<double>::infinity(),
          CSSPrimitiveValue::UnitType::kNumber);
    }
    if (token.Id() == CSSValueID::kNan) {
      return CSSMathExpressionNumericLiteral::Create(
          std::numeric_limits<double>::quiet_NaN(),
          CSSPrimitiveValue::UnitType::kNumber);
    }
    if (token.Id() == CSSValueID::kPi) {
      return CSSMathExpressionNumericLiteral::Create(
          M_PI, CSSPrimitiveValue::UnitType::kNumber);
    }
    if (token.Id() == CSSValueID::kE) {
      return CSSMathExpressionNumericLiteral::Create(
          M_E, CSSPrimitiveValue::UnitType::kNumber);
    }
    if (state.allow_size_keyword && token.Id() == CSSValueID::kSize) {
      return CSSMathExpressionKeywordLiteral::Create(
          CSSValueID::kSize,
          CSSMathExpressionKeywordLiteral::Context::kCalcSize);
    }
    if (!(token.GetType() == kNumberToken ||
          (token.GetType() == kPercentageToken &&
           parsing_flags_.Has(Flag::AllowPercent)) ||
          token.GetType() == kDimensionToken)) {
      // For relative color syntax.
      // If the associated values of color channels are known, swap them in
      // here. e.g. color(from color(srgb 1 0 0) calc(r * 2) 0 0) should
      // swap in "1" for the value of "r" in the calc expression.
      // If channel values are not known, create keyword literals for valid
      // channel names instead.
      if (auto it = color_channel_map_.find(token.Id());
          it != color_channel_map_.end()) {
        const std::optional<double>& channel = it->value;
        if (channel.has_value()) {
          return CSSMathExpressionNumericLiteral::Create(
              channel.value(), CSSPrimitiveValue::UnitType::kNumber);
        } else {
          return CSSMathExpressionKeywordLiteral::Create(
              token.Id(),
              CSSMathExpressionKeywordLiteral::Context::kColorChannel);
        }
      }
      return nullptr;
    }

    CSSPrimitiveValue::UnitType type = token.GetUnitType();
    if (UnitCategory(type) == kCalcOther) {
      return nullptr;
    }

    return CSSMathExpressionNumericLiteral::Create(
        CSSNumericLiteralValue::Create(token.NumericValue(), type));
  }

  CSSMathExpressionNode* ParseRoundingStrategy(CSSParserTokenStream& stream) {
    CSSMathOperator rounding_op = CSSMathOperator::kInvalid;
    switch (stream.Peek().Id()) {
      case CSSValueID::kNearest:
        rounding_op = CSSMathOperator::kRoundNearest;
        break;
      case CSSValueID::kUp:
        rounding_op = CSSMathOperator::kRoundUp;
        break;
      case CSSValueID::kDown:
        rounding_op = CSSMathOperator::kRoundDown;
        break;
      case CSSValueID::kToZero:
        rounding_op = CSSMathOperator::kRoundToZero;
        break;
      default:
        return nullptr;
    }
    stream.ConsumeIncludingWhitespace();
    return MakeGarbageCollected<CSSMathExpressionOperation>(
        CalculationResultCategory::kCalcNumber, rounding_op);
  }

  CSSMathExpressionNode* ParseValueTerm(CSSParserTokenStream& stream,
                                        State state,
                                        bool& whitespace_after_token) {
    if (stream.AtEnd()) {
      return nullptr;
    }

    if (stream.Peek().GetType() == kLeftParenthesisToken ||
        stream.Peek().FunctionId() == CSSValueID::kCalc) {
      CSSMathExpressionNode* result;
      {
        CSSParserTokenStream::BlockGuard guard(stream);
        stream.ConsumeWhitespace();
        result = ParseValueExpression(stream, state);
        if (!result || !stream.AtEnd()) {
          return nullptr;
        }
        result->SetIsNestedCalc();
      }
      whitespace_after_token = stream.Peek().GetType() == kWhitespaceToken;
      stream.ConsumeWhitespace();
      return result;
    }

    if (stream.Peek().GetType() == kFunctionToken) {
      CSSMathExpressionNode* result;
      CSSValueID function_id = stream.Peek().FunctionId();
      {
        CSSParserTokenStream::BlockGuard guard(stream);
        stream.ConsumeWhitespace();
        result = ParseMathFunction(function_id, stream, state);
      }
      whitespace_after_token = stream.Peek().GetType() == kWhitespaceToken;
      stream.ConsumeWhitespace();
      return result;
    }

    if (stream.Peek().GetBlockType() != CSSParserToken::kNotBlock) {
      return nullptr;
    }

    return ParseValue(stream, state, whitespace_after_token);
  }

  CSSMathExpressionNode* ParseValueMultiplicativeExpression(
      CSSParserTokenStream& stream,
      State state,
      bool& whitespace_after_last) {
    if (stream.AtEnd()) {
      return nullptr;
    }

    CSSMathExpressionNode* result =
        ParseValueTerm(stream, state, whitespace_after_last);
    if (!result) {
      return nullptr;
    }

    while (!stream.AtEnd()) {
      CSSMathOperator math_operator = ParseCSSArithmeticOperator(stream.Peek());
      if (math_operator != CSSMathOperator::kMultiply &&
          math_operator != CSSMathOperator::kDivide) {
        break;
      }
      stream.ConsumeIncludingWhitespace();

      CSSMathExpressionNode* rhs =
          ParseValueTerm(stream, state, whitespace_after_last);
      if (!rhs) {
        return nullptr;
      }

      result = CSSMathExpressionOperation::CreateArithmeticOperationSimplified(
          result, rhs, math_operator);

      if (!result) {
        return nullptr;
      }
    }

    return result;
  }

  CSSMathExpressionNode* ParseAdditiveValueExpression(
      CSSParserTokenStream& stream,
      State state) {
    if (stream.AtEnd()) {
      return nullptr;
    }

    bool whitespace_after_expr = false;  // Initialized only as paranoia.
    CSSMathExpressionNode* result = ParseValueMultiplicativeExpression(
        stream, state, whitespace_after_expr);
    if (!result) {
      return nullptr;
    }

    while (!stream.AtEnd()) {
      CSSMathOperator math_operator = ParseCSSArithmeticOperator(stream.Peek());
      if (math_operator != CSSMathOperator::kAdd &&
          math_operator != CSSMathOperator::kSubtract) {
        break;
      }
      if (!whitespace_after_expr) {
        return nullptr;  // calc(1px+ 2px) is invalid
      }
      stream.Consume();
      if (stream.Peek().GetType() != kWhitespaceToken) {
        return nullptr;  // calc(1px +2px) is invalid
      }
      stream.ConsumeIncludingWhitespace();

      CSSMathExpressionNode* rhs = ParseValueMultiplicativeExpression(
          stream, state, whitespace_after_expr);
      if (!rhs) {
        return nullptr;
      }

      result = CSSMathExpressionOperation::CreateArithmeticOperationSimplified(
          result, rhs, math_operator);

      if (!result) {
        return nullptr;
      }
    }

    if (auto* operation = DynamicTo<CSSMathExpressionOperation>(result)) {
      if (operation->IsAddOrSubtract()) {
        result = MaybeSimplifySumNode(operation);
      }
    }

    return result;
  }

  CSSMathExpressionKeywordLiteral* ParseKeywordLiteral(
      CSSParserTokenStream& stream,
      CSSMathExpressionKeywordLiteral::Context context) {
    const CSSParserToken token = stream.Peek();
    if (token.GetType() == kIdentToken) {
      stream.ConsumeIncludingWhitespace();
      return CSSMathExpressionKeywordLiteral::Create(token.Id(), context);
    }
    return nullptr;
  }

  CSSMathExpressionNode* ParseValueExpression(CSSParserTokenStream& stream,
                                              State state) {
    if (++state.depth > kMaxExpressionDepth) {
      return nullptr;
    }
    return ParseAdditiveValueExpression(stream, state);
  }

  const CSSParserContext& context_;
  const CSSAnchorQueryTypes allowed_anchor_queries_;
  const Flags parsing_flags_;
  const CSSColorChannelMap& color_channel_map_;
};

scoped_refptr<const CalculationValue> CSSMathExpressionNode::ToCalcValue(
    const CSSLengthResolver& length_resolver,
    Length::ValueRange range,
    bool allows_negative_percentage_reference) const {
  if (auto maybe_pixels_and_percent = ToPixelsAndPercent(length_resolver)) {
    // Clamping if pixels + percent could result in NaN. In special case,
    // inf px + inf % could evaluate to nan when
    // allows_negative_percentage_reference is true.
    if (IsNaN(*maybe_pixels_and_percent,
              allows_negative_percentage_reference)) {
      maybe_pixels_and_percent = CreateClampedSamePixelsAndPercent(
          std::numeric_limits<float>::quiet_NaN());
    } else {
      maybe_pixels_and_percent->pixels =
          CSSValueClampingUtils::ClampLength(maybe_pixels_and_percent->pixels);
      maybe_pixels_and_percent->percent =
          CSSValueClampingUtils::ClampLength(maybe_pixels_and_percent->percent);
    }
    return CalculationValue::Create(*maybe_pixels_and_percent, range);
  }

  auto value = ToCalculationExpression(length_resolver);
  std::optional<PixelsAndPercent> evaluated_value =
      EvaluateValueIfNaNorInfinity(value, allows_negative_percentage_reference);
  if (evaluated_value.has_value()) {
    return CalculationValue::Create(evaluated_value.value(), range);
  }
  return CalculationValue::CreateSimplified(value, range);
}

// static
CSSMathExpressionNode* CSSMathExpressionNode::Create(
    const CalculationValue& calc) {
  if (calc.IsExpression()) {
    return Create(*calc.GetOrCreateExpression());
  }
  return Create(calc.GetPixelsAndPercent());
}

// static
CSSMathExpressionNode* CSSMathExpressionNode::Create(PixelsAndPercent value) {
  double percent = value.percent;
  double pixels = value.pixels;
  if (!value.has_explicit_pixels) {
    CHECK(!pixels);
    return CSSMathExpressionNumericLiteral::Create(
        percent, CSSPrimitiveValue::UnitType::kPercentage);
  }
  if (!value.has_explicit_percent) {
    CHECK(!percent);
    return CSSMathExpressionNumericLiteral::Create(
        pixels, CSSPrimitiveValue::UnitType::kPixels);
  }
  CSSMathOperator op = CSSMathOperator::kAdd;
  if (pixels < 0) {
    pixels = -pixels;
    op = CSSMathOperator::kSubtract;
  }
  return CSSMathExpressionOperation::CreateArithmeticOperation(
      CSSMathExpressionNumericLiteral::Create(CSSNumericLiteralValue::Create(
          percent, CSSPrimitiveValue::UnitType::kPercentage)),
      CSSMathExpressionNumericLiteral::Create(CSSNumericLiteralValue::Create(
          pixels, CSSPrimitiveValue::UnitType::kPixels)),
      op);
}

// static
CSSMathExpressionNode* CSSMathExpressionNode::Create(
    const CalculationExpressionNode& node) {
  if (const auto* pixels_and_percent =
          DynamicTo<CalculationExpressionPixelsAndPercentNode>(node)) {
    return Create(pixels_and_percent->GetPixelsAndPercent());
  }

  if (const auto* identifier =
          DynamicTo<CalculationExpressionIdentifierNode>(node)) {
    return CSSMathExpressionIdentifierLiteral::Create(identifier->Value());
  }

  if (const auto* sizing_keyword =
          DynamicTo<CalculationExpressionSizingKeywordNode>(node)) {
    return CSSMathExpressionKeywordLiteral::Create(
        SizingKeywordToCSSValueID(sizing_keyword->Value()),
        CSSMathExpressionKeywordLiteral::Context::kCalcSize);
  }

  if (const auto* color_channel_keyword =
          DynamicTo<CalculationExpressionColorChannelKeywordNode>(node)) {
    return CSSMathExpressionKeywordLiteral::Create(
        ColorChannelKeywordToCSSValueID(color_channel_keyword->Value()),
        CSSMathExpressionKeywordLiteral::Context::kColorChannel);
  }

  if (const auto* number = DynamicTo<CalculationExpressionNumberNode>(node)) {
    return CSSMathExpressionNumericLiteral::Create(
        number->Value(), CSSPrimitiveValue::UnitType::kNumber);
  }

  DCHECK(node.IsOperation());

  const auto& operation = To<CalculationExpressionOperationNode>(node);
  const auto& children = operation.GetChildren();
  const auto calc_op = operation.GetOperator();
  switch (calc_op) {
    case CalculationOperator::kMultiply: {
      DCHECK_EQ(children.size(), 2u);
      return CSSMathExpressionOperation::CreateArithmeticOperation(
          Create(*children.front()), Create(*children.back()),
          CSSMathOperator::kMultiply);
    }
    case CalculationOperator::kAdd:
    case CalculationOperator::kSubtract: {
      DCHECK_EQ(children.size(), 2u);
      auto* lhs = Create(*children[0]);
      auto* rhs = Create(*children[1]);
      CSSMathOperator op = (calc_op == CalculationOperator::kAdd)
                               ? CSSMathOperator::kAdd
                               : CSSMathOperator::kSubtract;
      return CSSMathExpressionOperation::CreateArithmeticOperation(lhs, rhs,
                                                                   op);
    }
    case CalculationOperator::kMin:
    case CalculationOperator::kMax: {
      DCHECK(children.size());
      CSSMathExpressionOperation::Operands operands;
      for (const auto& child : children) {
        operands.push_back(Create(*child));
      }
      CSSMathOperator op = (calc_op == CalculationOperator::kMin)
                               ? CSSMathOperator::kMin
                               : CSSMathOperator::kMax;
      return CSSMathExpressionOperation::CreateComparisonFunction(
          std::move(operands), op);
    }
    case CalculationOperator::kClamp: {
      DCHECK_EQ(children.size(), 3u);
      CSSMathExpressionOperation::Operands operands;
      for (const auto& child : children) {
        operands.push_back(Create(*child));
      }
      return CSSMathExpressionOperation::CreateComparisonFunction(
          std::move(operands), CSSMathOperator::kClamp);
    }
    case CalculationOperator::kRoundNearest:
    case CalculationOperator::kRoundUp:
    case CalculationOperator::kRoundDown:
    case CalculationOperator::kRoundToZero:
    case CalculationOperator::kMod:
    case CalculationOperator::kRem: {
      DCHECK_EQ(children.size(), 2u);
      CSSMathExpressionOperation::Operands operands;
      for (const auto& child : children) {
        operands.push_back(Create(*child));
      }
      CSSMathOperator op;
      if (calc_op == CalculationOperator::kRoundNearest) {
        op = CSSMathOperator::kRoundNearest;
      } else if (calc_op == CalculationOperator::kRoundUp) {
        op = CSSMathOperator::kRoundUp;
      } else if (calc_op == CalculationOperator::kRoundDown) {
        op = CSSMathOperator::kRoundDown;
      } else if (calc_op == CalculationOperator::kRoundToZero) {
        op = CSSMathOperator::kRoundToZero;
      } else if (calc_op == CalculationOperator::kMod) {
        op = CSSMathOperator::kMod;
      } else {
        op = CSSMathOperator::kRem;
      }
      return CSSMathExpressionOperation::CreateSteppedValueFunction(
          std::move(operands), op);
    }
    case CalculationOperator::kHypot: {
      DCHECK_GE(children.size(), 1u);
      CSSMathExpressionOperation::Operands operands;
      for (const auto& child : children) {
        operands.push_back(Create(*child));
      }
      return CSSMathExpressionOperation::CreateExponentialFunction(
          std::move(operands), CSSValueID::kHypot);
    }
    case CalculationOperator::kAbs:
    case CalculationOperator::kSign: {
      DCHECK_EQ(children.size(), 1u);
      CSSMathExpressionOperation::Operands operands;
      operands.push_back(Create(*children.front()));
      CSSValueID op = calc_op == CalculationOperator::kAbs ? CSSValueID::kAbs
                                                           : CSSValueID::kSign;
      return CSSMathExpressionOperation::CreateSignRelatedFunction(
          std::move(operands), op);
    }
    case CalculationOperator::kProgress:
    case CalculationOperator::kMediaProgress:
    case CalculationOperator::kContainerProgress: {
      CHECK_EQ(children.size(), 3u);
      CSSMathExpressionOperation::Operands operands;
      operands.push_back(Create(*children.front()));
      operands.push_back(Create(*children[1]));
      operands.push_back(Create(*children.back()));
      CSSMathOperator op = calc_op == CalculationOperator::kProgress
                               ? CSSMathOperator::kProgress
                               : CSSMathOperator::kMediaProgress;
      return MakeGarbageCollected<CSSMathExpressionOperation>(
          CalculationResultCategory::kCalcNumber, std::move(operands), op);
    }
    case CalculationOperator::kCalcSize: {
      CHECK_EQ(children.size(), 2u);
      return CSSMathExpressionOperation::CreateCalcSizeOperation(
          Create(*children.front()), Create(*children.back()));
    }
    case CalculationOperator::kInvalid:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

// static
CSSMathExpressionNode* CSSMathExpressionNode::ParseMathFunction(
    CSSValueID function_id,
    CSSParserTokenStream& stream,
    const CSSParserContext& context,
    const Flags parsing_flags,
    CSSAnchorQueryTypes allowed_anchor_queries,
    const CSSColorChannelMap& color_channel_map) {
  CSSMathExpressionNodeParser parser(context, parsing_flags,
                                     allowed_anchor_queries, color_channel_map);
  CSSMathExpressionNodeParser::State state;
  CSSMathExpressionNode* result =
      parser.ParseMathFunction(function_id, stream, state);

  // TODO(pjh0718): Do simplificiation for result above.
  return result;
}

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::CSSMathExpressionNodeWithOperator)
