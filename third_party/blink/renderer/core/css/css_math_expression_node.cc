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

#include "base/compiler_specific.h"
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
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/try_tactic_transform.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/nth_index_cache.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/style/anchor_specifier_value.h"
#include "third_party/blink/renderer/platform/geometry/calculation_expression_node.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/geometry/math_functions.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector_traits.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
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

CSSMathType DetermineType(const CSSMathExpressionNode& left_side,
                          const CSSMathExpressionNode& right_side,
                          CSSMathOperator op) {
  if (left_side.IsCalcSize() || right_side.IsCalcSize()) {
    return CSSMathType::InvalidType();
  }
  CSSMathType left_type(left_side);
  CSSMathType right_type(right_side);
  switch (op) {
    case CSSMathOperator::kAdd:
    case CSSMathOperator::kSubtract:
      return left_type + right_type;
    case CSSMathOperator::kMultiply:
      return left_type * right_type;
    case CSSMathOperator::kDivide:
      return left_type / right_type;
    default:
      NOTREACHED();
  }
}

bool NodeHasNestedIntermediateResult(const CSSMathExpressionNode* node) {
  return node->IsOperation() &&
         To<CSSMathExpressionOperation>(node)->HasNestedIntermediateResult();
}

}  // namespace

CSSMathType::CSSMathType(CalculationResultCategory category) {
  if (category != kCalcNumber) {
    base_type_powers_[CalculationCategoryToBaseType(category)] = 1;
  }
  if (category == kCalcLengthFunction) {
    percentage_hint_ = kLength;
  }
}

CSSMathType::CSSMathType(const CSSMathExpressionNode& node) {
  const CSSMathExpressionOperation* operation =
      DynamicTo<CSSMathExpressionOperation>(node);
  if (operation && operation->IsArithmeticOperation()) {
    *this = operation->Type();
  } else {
    *this = CSSMathType(node.Category());
  }
}

CSSMathType::CSSMathType(bool is_valid) : is_valid_(is_valid) {}

CSSMathType::CSSMathType(BaseTypePowers types_map,
                         PercentageHint percentage_hint)
    : base_type_powers_(std::move(types_map)),
      percentage_hint_(std::move(percentage_hint)) {}

CSSMathType CSSMathType::InvalidType() {
  return CSSMathType(/*is_valid=*/false);
}

CalculationResultCategory CSSMathType::BaseTypeToCalculationCategory(
    BaseType base_type) {
  using enum BaseType;
  switch (base_type) {
    case kLength:
      return kCalcLength;
    case BaseType::kPercent:
      return kCalcPercent;
    case BaseType::kAngle:
      return kCalcAngle;
    case BaseType::kTime:
      return kCalcTime;
    case BaseType::kFrequency:
      return kCalcFrequency;
    case BaseType::kResolution:
      return kCalcResolution;
    case BaseType::kFlex:
    case BaseType::kNumTypes:
      NOTREACHED();
  }
}

CSSMathType::BaseType CSSMathType::CalculationCategoryToBaseType(
    CalculationResultCategory category) {
  using enum BaseType;
  switch (category) {
    case kCalcLength:
    case kCalcLengthFunction:
      return kLength;
    case kCalcPercent:
      return kPercent;
    case kCalcAngle:
      return kAngle;
    case kCalcTime:
      return kTime;
    case kCalcFrequency:
      return kFrequency;
    case kCalcResolution:
      return kResolution;
    case kCalcNumber:
    case kCalcIdent:
    case kCalcOther:
    case kCalcIntermediate:
      NOTREACHED();
  }
}

bool CSSMathType::IsValid() const {
  return is_valid_;
}

CalculationResultCategory CSSMathType::Category() const {
  if (!IsValid()) {
    return kCalcOther;
  }
  BaseType type;
  int16_t types_sum = 0;
  for (uint8_t type_index = 0u; type_index < BaseType::kNumTypes;
       ++type_index) {
    int8_t type_power = base_type_powers_[type_index];
    if (!type_power) {
      continue;
    }
    // If more than one base type has a non-zero power, it's intermediate.
    if (types_sum != 0) {
      if (type_index != kPercent) {
        return kCalcIntermediate;
      }
      DCHECK(!percentage_hint_.has_value());
      // Because we don't provide the percent hint from the "outside"
      // as css-values expects [1], it's possible for the percentage
      // hint to be unset even for expressions that involve percentages
      // as well some other type, e.g.:
      //
      //  width: (1% * 1%) / 1px;
      //
      // The CSSMathType for the above expression would contain
      // [percent -> 2, length -> -1] with a null percent hint.
      // However, the expression is clearly valid for 'width', since
      // the percentages resolve against lengths. To address this,
      // we effectively deduce what the percent hint should have been
      // from the other (non-percent) base type powers:
      //
      // If there is more than one non-percent base type with non-zero power,
      // it's intermediate (handled by early-out above).
      //
      // Otherwise, if there is exactly *one* other (non-percent) base type
      // with a non-zero power, then we assume that percentages were intended
      // to resolve against that base type, and basically move the powers from
      // "percent" to the other base type. Continuing the example from before:
      //
      //   [percent -> 2, length -> -1] => [percent -> 0, length -> 1]
      //
      // However, we do not actually need to modify `base_type_powers_`
      // according to the above; the remainder of this function only
      // checks `types_sum`, so it's enough to "pretend" that these numbers
      // were combined all along.
      //
      // Note: This relies on kPercent appearing *last* in the enum.
      //
      // Note: Whether or not this deduced category is valid for
      //       the relevant context will be determined by the call
      //       site. For example, "width:(1% * 1%) / 1deg" would produce
      //       kAngle for this algorithm, but ultimately be rejected.
      //
      // [1]
      // https://drafts.csswg.org/css-values-4/#determine-the-type-of-a-calculation
      types_sum += type_power;
      break;
    }
    type = BaseType(type_index);
    types_sum += type_power;
  }
  if (types_sum == 0) {
    return kCalcNumber;
  }
  if (types_sum != 1) {
    return kCalcIntermediate;
  }
  if (percentage_hint_) {
    if (base_type_powers_[kLength]) {
      return kCalcLengthFunction;
    } else {
      return kCalcOther;
    }
  }
  return BaseTypeToCalculationCategory(type);
}

bool CSSMathType::IsIntermediateResult() const {
  return IsValid() && Category() == kCalcIntermediate;
}

void CSSMathType::ApplyHint(BaseType hint) {
  // To apply the percent hint `hint` to a type without a percent hint, perform
  // the following steps:
  if (percentage_hint_.has_value()) {
    return;
  }
  // Set type’s percent hint to hint.
  percentage_hint_ = hint;
  // If hint is anything other than "percent".
  if (hint != kPercent) {
    // Add type["percent"] to type[hint].
    base_type_powers_[hint] += base_type_powers_[kPercent];
    // Then set type["percent"] to 0.
    base_type_powers_[kPercent] = 0;
  }
}

CSSMathType operator+(CSSMathType type1, CSSMathType type2) {
  DCHECK(type1.IsValid() && type2.IsValid());
  // If both type1 and type2 have non-null percent hints with different values
  // The types can’t be added. Return failure.
  if (type1.percentage_hint_.has_value() &&
      type2.percentage_hint_.has_value() &&
      type1.percentage_hint_ != type2.percentage_hint_) {
    return CSSMathType::InvalidType();
  }
  // If type1 has a non-null percent hint hint and type2 doesn’t
  // Apply the percent hint hint to type2.
  if (type1.percentage_hint_.has_value() &&
      !type2.percentage_hint_.has_value()) {
    type2.ApplyHint(type1.percentage_hint_.value());
  }
  // Vice versa if type2 has a non-null percent hint and type1 doesn’t.
  if (!type1.percentage_hint_.has_value() &&
      type2.percentage_hint_.has_value()) {
    type1.ApplyHint(type2.percentage_hint_.value());
  }
  // Otherwise continue to the next step.
  // If all the entries of type1 with non-zero values are contained in type2
  // with the same value, and vice-versa.
  if (type1 == type2) {
    // Copy all of type1’s entries to finalType, and then copy all of type2’s
    // entries to finalType that finalType doesn’t already contain. Set
    // finalType’s percent hint to type1’s percent hint. Return finalType.
    return CSSMathType(std::move(type1.base_type_powers_),
                       std::move(type1.percentage_hint_));
  }
  // If type1 and/or type2 contain "percent" with a non-zero value, and type1
  // and/or type2 contain a key other than "percent" with a non-zero value.
  using enum CSSMathType::BaseType;
  bool type1_contains_percent = type1.base_type_powers_[kPercent] != 0;
  bool type2_contains_percent = type2.base_type_powers_[kPercent] != 0;
  bool type1_or_type2_contain_percent =
      type1_contains_percent || type2_contains_percent;
  bool type1_contains_key_other_than_percent =
      type1.base_type_powers_[kPercent] !=
      std::accumulate(type1.base_type_powers_.begin(),
                      type1.base_type_powers_.end(), 0);
  bool type2_contains_key_other_than_percent =
      type2.base_type_powers_[kPercent] !=
      std::accumulate(type2.base_type_powers_.begin(),
                      type2.base_type_powers_.end(), 0);
  bool type1_or_type2_contain_key_other_than_percent =
      type1_contains_key_other_than_percent ||
      type2_contains_key_other_than_percent;
  if (type1_or_type2_contain_percent &&
      type1_or_type2_contain_key_other_than_percent) {
    // For each base type other than "percent" hint:
    for (uint8_t type_index = 0; type_index < CSSMathType::BaseType::kNumTypes;
         ++type_index) {
      auto type = static_cast<CSSMathType::BaseType>(type_index);
      if (type == kPercent) {
        continue;
      }
      // 1. Provisionally apply the percent hint hint to both type1 and type2.
      CSSMathType temp_type1(type1);
      CSSMathType temp_type2(type2);
      temp_type1.ApplyHint(type);
      temp_type2.ApplyHint(type);
      // 2. If, afterwards, all the entries of type1 with non-zero values are
      // contained in type2 with the same value, and vice versa, then copy all
      // of type1’s entries to finalType, and then copy all of type2’s entries
      // to finalType that finalType doesn’t already contain. Set finalType’s
      // percent hint to hint. Return finalType.
      if (temp_type1 == temp_type2) {
        return CSSMathType(std::move(temp_type1.base_type_powers_),
                           std::move(temp_type1.percentage_hint_));
      }
      // 3. Otherwise, revert type1 and type2 to their state at the start of
      // this loop.
    }
    // If the loop finishes without returning finalType, then the types can’t
    // be added. Return failure.
    return CSSMathType::InvalidType();
  }
  return CSSMathType::InvalidType();
}

CSSMathType operator*(CSSMathType type1, CSSMathType type2) {
  DCHECK(type1.IsValid() && type2.IsValid());
  // If both type1 and type2 have non-null percent hints with different values
  // The types can’t be added. Return failure.
  if (type1.percentage_hint_.has_value() &&
      type2.percentage_hint_.has_value() &&
      type1.percentage_hint_ != type2.percentage_hint_) {
    return CSSMathType::InvalidType();
  }
  // If type1 has a non-null percent hint hint and type2 doesn’t
  // Apply the percent hint hint to type2.
  if (type1.percentage_hint_.has_value() &&
      !type2.percentage_hint_.has_value()) {
    type2.ApplyHint(type1.percentage_hint_.value());
  }
  // Vice versa if type2 has a non-null percent hint and type1 doesn’t.
  if (!type1.percentage_hint_.has_value() &&
      type2.percentage_hint_.has_value()) {
    type1.ApplyHint(type2.percentage_hint_.value());
  }
  // Set finalType’s percent hint to type1’s percent hint (we can just use
  // type1). Copy all of type1’s entries to finalType, then for each baseType →
  // power of type2:
  for (uint8_t type_index = 0; type_index < CSSMathType::BaseType::kNumTypes;
       ++type_index) {
    type1.base_type_powers_[type_index] += type2.base_type_powers_[type_index];
  }
  return type1;
}

CSSMathType operator/(CSSMathType type1, CSSMathType type2) {
  return type1 * -type2;
}

CSSMathType CSSMathType::operator-() const {
  CSSMathType type(*this);
  for (uint8_t type_index = 0; type_index < CSSMathType::BaseType::kNumTypes;
       ++type_index) {
    type.base_type_powers_[type_index] = -type.base_type_powers_[type_index];
  }
  return type;
}

#if DCHECK_IS_ON()
std::ostream& operator<<(std::ostream& os, const CSSMathType& type) {
  if (!type.IsValid()) {
    os << "InvalidType ";
  }
  os << "CSSMathType(";
  bool first = true;
  for (uint8_t type_index = 0; type_index < CSSMathType::BaseType::kNumTypes;
       ++type_index) {
    int8_t power = type.base_type_powers_[type_index];
    if (power != 0) {
      if (!first) {
        os << ", ";
      }
      first = false;
      os << static_cast<int>(type_index) << "^" << static_cast<int>(power);
    }
  }
  if (type.percentage_hint_) {
    if (!first) {
      os << ", ";
    }
    os << "percent_hint=" << static_cast<int>(type.percentage_hint_.value());
  }
  os << ")";
  return os;
}
#endif

namespace {

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
    const blink::CalculationExpressionNode* value,
    bool allows_negative_percentage_reference) {
  if (value->HasColorChannelKeyword()) {
    // We cannot correctly evaluate for NaN or infinity until we know the
    // color channel values to substitute in.
    return std::nullopt;
  }
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
      // calc-size() is not allowed as a parameter to progress(),
      // since it can only be a base of any calculation.
      // Also, intermediate calculations are not allowed as parameters to
      // progress(), since only values with canonical units
      // can be used.
      if (nodes[0]->IsCalcSize() || first_category == kCalcIntermediate) {
        return false;
      }
      if (first_category != nodes[1]->Category() ||
          first_category != nodes[2]->Category()) {
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
      NOTREACHED();
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
      return operand->ComputeValueInCanonicalUnit().has_value();
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
// progress(1em, 0em, 1em)), or they are all numerics and can be resolved
// to the canonical unit (e.g. progress(1deg, 0rad, 1deg)). Note: this
// can't be eagerly simplified - progress(1em, 0px, 1em).
ProgressArgsSimplificationStatus CanEagerlySimplifyProgressArgs(
    const CSSMathExpressionOperation::Operands& operands) {
  if (std::all_of(operands.begin(), operands.end(),
                  [](const CSSMathExpressionNode* node) {
                    return node->IsNumericLiteral() &&
                           node->ComputeValueInCanonicalUnit().has_value();
                  })) {
    return ProgressArgsSimplificationStatus::kAllArgsResolveToCanonical;
  }
  if (std::all_of(
          operands.begin(), operands.end(),
          [&](const CSSMathExpressionNode* node) {
            return node->IsNumericLiteral() &&
                   node->ResolvedUnitTypeForSimplification() ==
                       operands.front()->ResolvedUnitTypeForSimplification();
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
using GCedUnitsVector = GCedHeapVector<CSSMathExpressionNodeWithOperator>;
using UnitsVectorHashMap =
    HeapHashMap<CSSPrimitiveValue::UnitType, Member<GCedUnitsVector>>;

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
    const UnitsHashMap& units_map,
    bool is_multiply) {
  if (!node->IsNumericLiteral()) {
    return {op, node};
  }
  CSSPrimitiveValue::UnitType unit_type =
      node->ResolvedUnitTypeForSimplification();
  auto it = units_map.find(unit_type);
  if (it != units_map.end()) {
    double value = it->value;
    CSSMathOperator new_op = op;
    if (!is_multiply) {
      new_op =
          value < 0.0f ? CSSMathOperator::kSubtract : CSSMathOperator::kAdd;
      value = std::abs(value);
    }
    CSSMathExpressionNode* new_node =
        CSSMathExpressionNumericLiteral::Create(value, unit_type);
    return {new_op, new_node};
  }
  return {op, node};
}

// Contains an operation node with arguments for processing (i.e.
// collecting numeric children or combining numeric children from the
// given CSSMathExpressionNode).
struct NumericChildrenTraversalNode {
  DISALLOW_NEW();

  void Trace(Visitor* visitor) const { visitor->Trace(node); }

  Member<const CSSMathExpressionNode> node;
  CSSMathOperator op;
  bool is_in_nesting;
};

template <typename T>
void TraverseNumericChildrenFromNode(const CSSMathExpressionNode* root,
                                     CSSMathOperator op,
                                     T&& process_node,
                                     bool is_in_nesting = false) {
  HeapVector<NumericChildrenTraversalNode> operation_nodes;

  auto should_traverse = [](const CSSMathExpressionNode* root,
                            CSSMathOperator op) {
    // Go deeper inside the operation node if possible.
    // But don't try to go inside complex-typed operations such as 1rem * 1px /
    // 1px, as those should not be sorted, because we don't know their unit
    // type.
    auto* operation = DynamicTo<CSSMathExpressionOperation>(root);
    return operation && !operation->HasNestedIntermediateResult() &&
           (op == CSSMathOperator::kMultiply ? operation->IsMultiplyOrDivide()
                                             : operation->IsAddOrSubtract());
  };
  auto push_into_operation_nodes = [&](const CSSMathExpressionNode* node,
                                       CSSMathOperator op, bool is_in_nesting) {
    auto* operation = DynamicTo<CSSMathExpressionOperation>(node);
    DCHECK(operation);
    const CSSMathOperator operation_op = operation->OperatorType();
    is_in_nesting |= operation->IsNestedCalc();

    // Change the sign of expression, if we are nesting (inside brackets).
    CSSMathOperator back_op =
        MaybeChangeOperatorSignIfNesting(is_in_nesting, op, operation_op);

    // Nest from the left (first op) to the right (second op).
    // Since Traverse() pops the last element from `operation_nodes`
    // and processes it, push `GetOperands().back()` first.
    operation_nodes.emplace_back(operation->GetOperands().back(), back_op,
                                 is_in_nesting);
    operation_nodes.emplace_back(operation->GetOperands().front(), op,
                                 is_in_nesting);
  };

  if (!should_traverse(root, op)) {
    process_node(root, op);
    return;
  }
  push_into_operation_nodes(root, op, is_in_nesting);

  while (!operation_nodes.empty()) {
    NumericChildrenTraversalNode last = operation_nodes.back();
    operation_nodes.pop_back();
    if (!should_traverse(last.node, last.op)) {
      process_node(last.node, last.op);
      continue;
    }
    push_into_operation_nodes(last.node, last.op, last.is_in_nesting);
  }
}

// This function combines numeric values that have double value and are of the
// same unit type together in numeric_children and saves all the non add/sub
// (or mul, if op is kMultiply) operation children and their correct simplified
// operator in all_children.
void CombineNumericChildrenFromNode(const CSSMathExpressionNode* root,
                                    CSSMathOperator op,
                                    UnitsHashMap& numeric_children,
                                    UnitsVector& all_children,
                                    bool is_in_nesting = false) {
  auto process_node = [&](const CSSMathExpressionNode* root,
                          CSSMathOperator op) {
    // If we have numeric with double value - combine under one unit type.
    if (IsNumericNodeWithDoubleValue(root)) {
      const CSSPrimitiveValue::UnitType unit_type =
          root->ResolvedUnitTypeForSimplification();
      double value = op == CSSMathOperator::kSubtract ? -root->DoubleValue()
                                                      : root->DoubleValue();
      if (auto it = numeric_children.find(unit_type);
          it != numeric_children.end()) {
        if (op == CSSMathOperator::kMultiply) {
          it->value *= value;
        } else {
          it->value += value;
        }
      } else {
        numeric_children.insert(unit_type, value);
      }
    }
    // Save all non add/sub (or non-mul, respectively) operations.
    all_children.emplace_back(op, root);
  };
  TraverseNumericChildrenFromNode(root, op, process_node, is_in_nesting);
}

// This function collects numeric values that have double value
// in the numeric_children vector under the same type and saves all the complex
// children and their correct simplified operator in complex_children.
void CollectNumericChildrenFromNode(const CSSMathExpressionNode* root,
                                    CSSMathOperator op,
                                    UnitsVectorHashMap& numeric_children,
                                    UnitsVector& complex_children,
                                    bool is_in_nesting = false) {
  auto process_node = [&](const CSSMathExpressionNode* root,
                          CSSMathOperator op) {
    // If we have numeric with double value - collect in numeric_children.
    if (IsNumericNodeWithDoubleValue(root)) {
      CSSPrimitiveValue::UnitType unit_type =
          root->ResolvedUnitTypeForSimplification();
      if (auto it = numeric_children.find(unit_type);
          it != numeric_children.end()) {
        it->value->emplace_back(op, root);
      } else {
        numeric_children.insert(
            unit_type, MakeGarbageCollected<GCedUnitsVector>(
                           1, CSSMathExpressionNodeWithOperator(op, root)));
      }
      return;
    }
    // Save all non add/sub operations.
    complex_children.emplace_back(op, root);
  };
  TraverseNumericChildrenFromNode(root, op, process_node, is_in_nesting);
}

// This function follows:
// https://drafts.csswg.org/css-values-4/#sort-a-calculations-children
// As in Blink the math expression tree is binary, we need to collect all the
// elements of this tree together and return them in the right order.
// (We don't need a new node, since we're only using it for serialization.)
UnitsVector CollectSumOrProductInOrder(const CSSMathExpressionOperation* root) {
  CHECK(root->IsAddOrSubtract() || root->IsMultiplyOrDivide());
  CHECK_EQ(root->GetOperands().size(), 2u);
  // Hash map of vectors of numeric literal values with double value with the
  // same unit type.
  UnitsVectorHashMap numeric_children;
  // Vector of all non add/sub (or non-mul, as appropriate) children.
  UnitsVector complex_children;
  // Collect all the numeric literal with double value in one vector.
  // Note: using kAdd/kMultiply here as the operator for the first child
  // (e.g. a - b = +a - b, a + b = +a + b)
  CollectNumericChildrenFromNode(root,
                                 root->IsAddOrSubtract()
                                     ? CSSMathOperator::kAdd
                                     : CSSMathOperator::kMultiply,
                                 numeric_children, complex_children, false);
  // Form the final vector.
  UnitsVector ret;
  // From spec: If nodes contains a number, remove it from nodes and append it
  // to ret.
  if (auto it = numeric_children.find(CSSPrimitiveValue::UnitType::kNumber);
      it != numeric_children.end()) {
    ret.AppendVector(*it->value);
    numeric_children.erase(it);
  }
  // From spec: If nodes contains a percentage, remove it from nodes and append
  // it to ret.
  if (auto it = numeric_children.find(CSSPrimitiveValue::UnitType::kPercentage);
      it != numeric_children.end()) {
    ret.AppendVector(*it->value);
    numeric_children.erase(it);
  }
  // Now, sort the rest numeric values alphabatically.
  // From spec: If nodes contains any dimensions, remove them from nodes, sort
  // them by their units, ordered ASCII case-insensitively, and append them to
  // ret.
  auto comp = [&](const CSSPrimitiveValue::UnitType& key_a,
                  const CSSPrimitiveValue::UnitType& key_b) {
    StringView a = CSSPrimitiveValue::UnitTypeToString(key_a);
    StringView b = CSSPrimitiveValue::UnitTypeToString(key_b);
    return CodeUnitCompareIgnoringAsciiCaseLessThan(a, b);
  };
  Vector<CSSPrimitiveValue::UnitType> keys;
  keys.reserve(numeric_children.size());
  for (const CSSPrimitiveValue::UnitType& key : numeric_children.Keys()) {
    keys.push_back(key);
  }
  std::sort(keys.begin(), keys.end(), comp);
  // Now, add those numeric nodes in the sorted order.
  for (const auto& unit_type : keys) {
    ret.AppendVector(*numeric_children.at(unit_type));
  }
  // Now, add all the complex (non-numerics with double value) values.
  ret.AppendVector(complex_children);
  return ret;
}

// This function prevents some cases of typed arithmetic from being simplified.
// We shouldn't simplify cases like 1px * 1px (which is kCalcIntermediate) and
// anything on another side of the operation, or if 1px * 1px is operation
// itself. As well as we shouldn't simplify e.g. [1px * (1 / 1px)] * 1px,
// because the part inside [] is kCalcNumber, but in reality we shouldn't
// combine 1px from [] and 1px from the right part of operation.
bool ArithmeticOperationIsAllowedToBeSimplified(
    const CSSMathExpressionOperation& operation) {
  DCHECK(operation.IsArithmeticOperation());
  if (!RuntimeEnabledFeatures::CSSTypedArithmeticEnabled()) {
    return true;
  }
  return !operation.HasNestedIntermediateResult();
}

// This function follows:
// https://drafts.csswg.org/css-values-4/#calc-simplification
// As in Blink the math expression tree is binary, we need to collect all the
// elements of this tree together and create a new tree as a result.
CSSMathExpressionNode* MaybeSimplifySumOrProductNode(
    const CSSMathExpressionOperation* root) {
  CHECK(root->IsAddOrSubtract() || root->IsMultiplyOrDivide());
  CHECK_EQ(root->GetOperands().size(), 2u);
  CHECK_NE(root->GetOperands().front()->Category(), kCalcIntermediate);
  CHECK_NE(root->GetOperands().back()->Category(), kCalcIntermediate);
  // Hash map of numeric literal values of the same type, that can be
  // combined together.
  UnitsHashMap numeric_children;
  // Vector of all non add/sub operation children.
  UnitsVector all_children;
  // Collect all the numeric literal values together.
  // Note: using kAdd/kMultiply here as the operator for the first child
  // (e.g. a - b = +a - b, a + b = +a + b)
  const bool is_multiply = root->IsMultiplyOrDivide();
  CombineNumericChildrenFromNode(
      root, is_multiply ? CSSMathOperator::kMultiply : CSSMathOperator::kAdd,
      numeric_children, all_children);
  // Form the final node.
  HashSet<CSSPrimitiveValue::UnitType> used_units;
  CSSMathExpressionNode* final_node = nullptr;
  for (const auto& child : all_children) {
    auto [op, node] = MaybeReplaceNodeWithCombined(
        child.node, child.op, numeric_children, is_multiply);

    if (IsNumericNodeWithDoubleValue(node)) {
      CSSPrimitiveValue::UnitType unit_type =
          node->ResolvedUnitTypeForSimplification();
      // Skip already used unit types, as they have been already combined.
      if (used_units.Contains(unit_type)) {
        continue;
      }
      used_units.insert(unit_type);

      // Skip a constant factor of unity, unless it is the only factor.
      if (is_multiply && unit_type == CSSPrimitiveValue::UnitType::kNumber &&
          node->DoubleValue() == 1.0 &&
          (numeric_children.size() + all_children.size()) > 1) {
        continue;
      }
    }

    if (!final_node) {
      // First child.
      final_node = MaybeNegateFirstNode(op, node)->Copy();
      continue;
    }
    final_node =
        CSSMathExpressionOperation::CreateArithmeticOperationSimplified(
            final_node, node, op);
  }
  return final_node;
}

const CSSMathExpressionNode* MaybeSimplifyIfSumOrProductNode(
    const CSSMathExpressionNode* node) {
  if (const auto* op = DynamicTo<CSSMathExpressionOperation>(node)) {
    if (op->IsAddOrSubtract() || op->IsMultiplyOrDivide()) {
      if (ArithmeticOperationIsAllowedToBeSimplified(*op)) {
        return MaybeSimplifySumOrProductNode(op);
      }
    }
  }
  return node;
}

CSSMathExpressionNode* MaybeDistributeArithmeticOperation(
    const CSSMathExpressionNode* left_side,
    const CSSMathExpressionNode* right_side,
    CSSMathOperator op) {
  if (op != CSSMathOperator::kMultiply && op != CSSMathOperator::kDivide) {
    return nullptr;
  }
  if (RuntimeEnabledFeatures::CSSTypedArithmeticEnabled() &&
      (left_side->Category() == kCalcIntermediate ||
       right_side->Category() == kCalcIntermediate)) {
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

template <>
struct VectorTraits<NumericChildrenTraversalNode>
    : VectorTraitsBase<NumericChildrenTraversalNode> {
  static const bool kCanClearUnusedSlotsWithMemset = true;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanMoveWithMemcpy = true;
  static const bool kCanTraceConcurrently = true;
};

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
      return PixelsAndPercent(0.0f, value_->DoubleValue(),
                              /*has_explicit_pixels=*/false,
                              /*has_explicit_percent=*/true);
    case kCalcNumber:
      // TODO(alancutter): Stop treating numbers like pixels unconditionally
      // in calcs to be able to accomodate border-image-width
      // https://drafts.csswg.org/css-backgrounds-3/#the-border-image-width
      return PixelsAndPercent(
          ClampTo<float>(value_->ClampedDoubleValue()) * length_resolver.Zoom(),
          0.0f, /*has_explicit_pixels=*/true,
          /*has_explicit_percent=*/false);
    case kCalcAngle:
      // Treat angles as pixels to support calc() expressions on hue angles in
      // relative color syntax. This allows converting such expressions to
      // CalculationValues.
      return PixelsAndPercent(ClampTo<float>(value_->ClampedDoubleValue()),
                              0.0f,
                              /*has_explicit_pixels=*/true,
                              /*has_explicit_percent=*/false);
    default:
      NOTREACHED();
  }
}

const CalculationExpressionNode*
CSSMathExpressionNumericLiteral::ToCalculationExpression(
    const CSSLengthResolver& length_resolver) const {
  if (Category() == kCalcNumber) {
    return MakeGarbageCollected<CalculationExpressionNumberNode>(
        value_->DoubleValue());
  }
  return MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
      *ToPixelsAndPercent(length_resolver));
}

double CSSMathExpressionNumericLiteral::DoubleValue() const {
  if (HasDoubleValue(ResolvedUnitType())) {
    return value_->DoubleValue();
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
    case kCalcIntermediate:
    case kCalcOther:
    case kCalcIdent:
      NOTREACHED();
  }
  NOTREACHED();
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
    case kCalcIntermediate:
    case kCalcTime:
    case kCalcResolution:
    case kCalcOther:
    case kCalcIdent:
      NOTREACHED();
  }
  NOTREACHED();
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

bool CSSMathExpressionNumericLiteral::MayHaveRelativeUnit() const {
  return CSSPrimitiveValue::IsRelativeUnit(value_->GetType());
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
         /* CalcIntermediate */
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

  if (left_side.IsCalcSize() || right_side.IsCalcSize()) {
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

  NOTREACHED();
}

static CalculationResultCategory DetermineComparisonCategory(
    const CSSMathExpressionOperation::Operands& operands) {
  DCHECK(!operands.empty());

  bool is_first = true;
  CalculationResultCategory category = kCalcOther;
  for (const CSSMathExpressionNode* operand : operands) {
    if (operand->IsCalcSize()) {
      return kCalcOther;
    }
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
       basis_category == kCalcLengthFunction || left_side.IsCalcSize()) &&
      (calculation_category == kCalcLength ||
       calculation_category == kCalcPercent ||
       calculation_category == kCalcLengthFunction) &&
      !right_side.IsCalcSize()) {
    return kCalcLengthFunction;
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

const CalculationExpressionNode*
CSSMathExpressionIdentifierLiteral::ToCalculationExpression(
    const CSSLengthResolver&) const {
  return MakeGarbageCollected<CalculationExpressionIdentifierNode>(identifier_);
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
    KEYWORD_CASE(kStretch)
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
    KEYWORD_CASE(kStretch)
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

const CalculationExpressionNode*
CSSMathExpressionKeywordLiteral::ToCalculationExpression(
    const CSSLengthResolver& length_resolver) const {
  switch (context_) {
    case CSSMathExpressionKeywordLiteral::Context::kMediaProgress: {
      switch (keyword_) {
        case CSSValueID::kWidth:
          return MakeGarbageCollected<
              CalculationExpressionPixelsAndPercentNode>(
              PixelsAndPercent(length_resolver.ViewportWidth()));
        case CSSValueID::kHeight:
          return MakeGarbageCollected<
              CalculationExpressionPixelsAndPercentNode>(
              PixelsAndPercent(length_resolver.ViewportHeight()));
        default:
          NOTREACHED();
      }
    }
    case CSSMathExpressionKeywordLiteral::Context::kCalcSize:
      return MakeGarbageCollected<CalculationExpressionSizingKeywordNode>(
          CSSValueIDToSizingKeyword(keyword_));
    case CSSMathExpressionKeywordLiteral::Context::kColorChannel:
      return MakeGarbageCollected<CalculationExpressionColorChannelKeywordNode>(
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
  CSSMathType type = DetermineType(*left_side, *right_side, op);
  if (RuntimeEnabledFeatures::CSSTypedArithmeticEnabled()) {
    if (!type.IsValid()) {
      return nullptr;
    }
    new_category = type.Category();
  }
  if (new_category == kCalcOther) {
    return nullptr;
  }

  // Convert (a / b) to (a * b') (where b' is an Invert node, i.e., 1/b),
  // except (1 / b) which becomes b'.
  if (op == CSSMathOperator::kDivide) {
    CSSMathExpressionNode* inverted_right_side =
        CreateInvertFunction(right_side);

    if (left_side->Category() == kCalcNumber && left_side->IsNumericLiteral() &&
        left_side->DoubleValue() == 1.0) {
      return inverted_right_side;
    } else {
      op = CSSMathOperator::kMultiply;
      right_side = inverted_right_side;
    }
  }

  return MakeGarbageCollected<CSSMathExpressionOperation>(
      left_side, right_side, op, new_category, std::move(type));
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

  return MakeGarbageCollected<CSSMathExpressionOperation>(
      left_side, right_side, op, new_category, CSSMathType());
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
      category, std::move(operands), op, CSSMathType());
}

const CSSMathExpressionNode*
CSSMathExpressionOperation::CopyRandomWithPropertyNameAndValueIndexIfNeeded(
    const CSSPropertyName& property_name,
    wtf_size_t property_value_index) const {
  DCHECK(NeedsPropertyNameAndValueIndexForRandom());
  Operands operands(operands_);
  for (wtf_size_t i = 0; i < operands_.size(); i++) {
    operands[i] = operands_[i]->CopyRandomWithPropertyNameAndValueIndexIfNeeded(
        property_name, property_value_index);
  }
  return MakeGarbageCollected<CSSMathExpressionOperation>(
      category_, std::move(operands), operator_, type_);
}

// Helper function for parsing number value
static double ValueAsNumber(const CSSMathExpressionNode* node, bool& error) {
  if (node->Category() == kCalcNumber) {
    return node->DoubleValue();
  }
  error = true;
  return 0;
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

namespace {

bool ShouldConvertRad2DegForOperator(CSSMathOperator op) {
  return op == CSSMathOperator::kSin || op == CSSMathOperator::kCos ||
         op == CSSMathOperator::kTan;
}

CSSValueID TrigonometricCalculationOperatorToCSSValueID(
    const CalculationOperator& op) {
  switch (op) {
    case CalculationOperator::kSin:
      return CSSValueID::kSin;
    case CalculationOperator::kCos:
      return CSSValueID::kCos;
    case CalculationOperator::kTan:
      return CSSValueID::kTan;
    case CalculationOperator::kAsin:
      return CSSValueID::kAsin;
    case CalculationOperator::kAcos:
      return CSSValueID::kAcos;
    case CalculationOperator::kAtan:
      return CSSValueID::kAtan;
    case CalculationOperator::kAtan2:
      return CSSValueID::kAtan2;
    default:
      return CSSValueID::kInvalid;
  }
}

CSSMathOperator TrigonometricFunctionIdToOperator(
    const CSSValueID& function_id) {
  switch (function_id) {
    case CSSValueID::kSin:
      return CSSMathOperator::kSin;
    case CSSValueID::kCos:
      return CSSMathOperator::kCos;
    case CSSValueID::kTan:
      return CSSMathOperator::kTan;
    case CSSValueID::kAsin:
      return CSSMathOperator::kAsin;
    case CSSValueID::kAcos:
      return CSSMathOperator::kAcos;
    case CSSValueID::kAtan:
      return CSSMathOperator::kAtan;
    case CSSValueID::kAtan2:
      return CSSMathOperator::kAtan2;
    default:
      return CSSMathOperator::kInvalid;
  }
}

}  // namespace

CSSMathExpressionNode* CSSMathExpressionOperation::CreateTrigonometricFunction(
    Operands&& operands,
    CSSValueID function_id) {
  DCHECK(operands.size() == 1u && function_id != CSSValueID::kAtan2 ||
         operands.size() == 2u);
  CSSMathOperator op = TrigonometricFunctionIdToOperator(function_id);
  if (op == CSSMathOperator::kInvalid) {
    return nullptr;
  }
  bool is_number_output = ShouldConvertRad2DegForOperator(op);
  if (operands.size() == 1u) {
    bool sin_cos_tan_category_check =
        is_number_output && (operands.front()->Category() == kCalcNumber ||
                             operands.front()->Category() == kCalcAngle);
    bool asin_acos_atan_check =
        !is_number_output && operands.front()->Category() == kCalcNumber;
    if (!sin_cos_tan_category_check && !asin_acos_atan_check) {
      return nullptr;
    }
  } else if (operands.front()->Category() != operands.back()->Category()) {
    return nullptr;
  }
  if (!CanEagerlySimplify(operands)) {
    CalculationResultCategory category =
        is_number_output ? CalculationResultCategory::kCalcNumber
                         : CalculationResultCategory::kCalcAngle;
    return MakeGarbageCollected<CSSMathExpressionOperation>(
        category, std::move(operands), op, CSSMathType());
  }
  CSSPrimitiveValue::UnitType unit_type =
      is_number_output ? CSSPrimitiveValue::UnitType::kNumber
                       : CSSPrimitiveValue::UnitType::kDegrees;
  double a = operands.front()->ComputeValueInCanonicalUnit().value();
  if (is_number_output && operands.front()->Category() == kCalcNumber) {
    a = Rad2deg(a);
  }
  std::optional<double> b = op == CSSMathOperator::kAtan2
                                ? operands.back()->ComputeValueInCanonicalUnit()
                                : std::nullopt;
  double value = EvaluateTrigonometricFunction(op, a, b);
  return CSSMathExpressionNumericLiteral::Create(value, unit_type);
}

CSSMathExpressionNode* CSSMathExpressionOperation::CreateSteppedValueFunction(
    Operands&& operands,
    CSSMathOperator op) {
  DCHECK_EQ(operands.size(), 2u);
  if (operands[0]->Category() == kCalcOther ||
      operands[1]->Category() == kCalcOther) {
    return nullptr;
  }
  if (operands.front()->IsCalcSize() || operands.back()->IsCalcSize()) {
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
      category, std::move(operands), op, CSSMathType());
}

// static
CSSMathExpressionNode* CSSMathExpressionOperation::CreateExponentialFunction(
    Operands&& operands,
    CSSValueID function_id) {
  // calc-size() is not allowed as a parameter to exponential functions,
  // since it can only be a base of any calculation.
  // Also, intermediate calculations are not allowed as parameters to
  // exponential functions, since only values with canonical units
  // can be used as parameters to exponential functions.
  if (operands.front()->IsCalcSize() ||
      operands.front()->Category() == kCalcIntermediate) {
    return nullptr;
  }

  double value = 0;
  bool error = false;
  auto unit_type = CSSPrimitiveValue::UnitType::kNumber;
  switch (function_id) {
    case CSSValueID::kPow: {
      DCHECK_EQ(operands.size(), 2u);
      CalculationResultCategory category =
          DetermineComparisonCategory(operands);
      if (category != kCalcNumber) {
        return nullptr;
      }
      if (CanEagerlySimplify(operands)) {
        std::optional<double> a = operands[0]->ComputeValueInCanonicalUnit();
        std::optional<double> b = operands[1]->ComputeValueInCanonicalUnit();
        CHECK(a.has_value());
        CHECK(b.has_value());
        value = std::pow(a.value(), b.value());
      } else {
        return MakeGarbageCollected<CSSMathExpressionOperation>(
            category, std::move(operands), CSSMathOperator::kPow,
            CSSMathType());
      }
      break;
    }
    case CSSValueID::kSqrt: {
      DCHECK_EQ(operands.size(), 1u);
      if (!CanEagerlySimplify(operands.front())) {
        return MakeGarbageCollected<CSSMathExpressionOperation>(
            operands.front()->Category(), std::move(operands),
            CSSMathOperator::kSqrt, CSSMathType());
      }
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
            category, std::move(operands), CSSMathOperator::kHypot,
            CSSMathType());
      }
      break;
    }
    case CSSValueID::kLog: {
      DCHECK_GE(operands.size(), 1u);
      DCHECK_LE(operands.size(), 2u);
      if (operands.front()->Category() != kCalcNumber ||
          operands.back()->Category() != kCalcNumber) {
        return nullptr;
      }
      if (!CanEagerlySimplify(operands)) {
        return MakeGarbageCollected<CSSMathExpressionOperation>(
            kCalcNumber, std::move(operands), CSSMathOperator::kLog,
            CSSMathType());
      }
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
      if (!CanEagerlySimplify(operands.front())) {
        return MakeGarbageCollected<CSSMathExpressionOperation>(
            kCalcNumber, std::move(operands), CSSMathOperator::kExp,
            CSSMathType());
      }
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
  const CSSMathExpressionNode* operand = operands.front();

  if (operand->IsCalcSize()) {
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
          operand->Category(), std::move(operands), CSSMathOperator::kAbs,
          CSSMathType());
    }
    case CSSValueID::kSign: {
      if (CanEagerlySimplify(operand)) {
        const std::optional<double> opt =
            operand->ComputeValueInCanonicalUnit();
        DCHECK(opt.has_value());
        const double signum = EvaluateSignFunction(opt.value());
        return CSSMathExpressionNumericLiteral::Create(
            signum, CSSPrimitiveValue::UnitType::kNumber);
      }
      return MakeGarbageCollected<CSSMathExpressionOperation>(
          kCalcNumber, std::move(operands), CSSMathOperator::kSign,
          CSSMathType());
    }
    default:
      NOTREACHED();
  }
}

CSSMathExpressionNode* CSSMathExpressionOperation::CreateInvertFunction(
    const CSSMathExpressionNode* operand) {
  // https://drafts.csswg.org/css-values-4/#serialize-a-calculation-tree
  //
  // - If root’s child is a number (not a percentage or dimension)
  //   return the reciprocal of the child’s value.
  if (!operand->IsOperation() &&
      operand->Category() == CalculationResultCategory::kCalcNumber) {
    const std::optional<double> opt = operand->ComputeValueInCanonicalUnit();
    if (opt.has_value() && opt.value() != 0) {
      return CSSMathExpressionNumericLiteral::Create(
          1.0 / opt.value(), CSSPrimitiveValue::UnitType::kNumber);
    }
  }

  // - If root’s child is an Invert node, return the child’s child.
  if (operand->IsOperation() &&
      To<CSSMathExpressionOperation>(operand)->IsInvert()) {
    return To<CSSMathExpressionOperation>(operand)->operands_[0]->Copy();
  }

  // E.g. 1 / 1px.
  if (operand->Category() != kCalcNumber) {
    return MakeGarbageCollected<CSSMathExpressionOperation>(
        kCalcIntermediate, Operands{operand}, CSSMathOperator::kInvert,
        -CSSMathType(*operand));
  }
  return MakeGarbageCollected<CSSMathExpressionOperation>(
      kCalcNumber, Operands{operand}, CSSMathOperator::kInvert, CSSMathType());
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
      category, std::move(ops), operator_, type_);
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
    const CSSMathExpressionNode* right_side,
    CSSMathOperator op) {
  if (!left_side->IsNumericLiteral() || !right_side->IsNumericLiteral()) {
    return false;
  }
  // Can't simplify 1px * 1px.
  if ((op == CSSMathOperator::kMultiply || op == CSSMathOperator::kDivide) &&
      left_side->Category() != kCalcNumber &&
      right_side->Category() != kCalcNumber) {
    return false;
  }
  if (!RuntimeEnabledFeatures::CSSTypedArithmeticEnabled()) {
    return true;
  }
  // Don't simplify invert(1 / 1px) or intermedite result.
  return !(left_side->IsOperation() &&
           To<CSSMathExpressionOperation>(left_side)->IsInvert()) &&
         !(right_side->IsOperation() &&
           To<CSSMathExpressionOperation>(right_side)->IsInvert()) &&
         !DetermineType(*left_side, *right_side, op).IsIntermediateResult();
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

  if (!CanArithmeticOperationBeSimplified(left_side, right_side, op)) {
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
      CSSPrimitiveValue::UnitType left_type =
          left_side->ResolvedUnitTypeForSimplification();
      if (HasDoubleValue(left_type)) {
        CSSPrimitiveValue::UnitType right_type =
            right_side->ResolvedUnitTypeForSimplification();
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
    if (right_category != kCalcNumber && op == CSSMathOperator::kDivide) {
      return nullptr;
    }
    const CSSMathExpressionNode* number_side =
        GetNumericLiteralSide(left_side, right_side);
    if (!number_side) {
      return CreateArithmeticOperation(left_side, right_side, op);
    }
    const CSSMathExpressionNode* other_side =
        left_side == number_side ? right_side : left_side;

    double number = number_side->DoubleValue();

    CSSPrimitiveValue::UnitType other_type =
        other_side->ResolvedUnitTypeForSimplification();
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

    CSSMathExpressionOperation* new_op =
        MakeGarbageCollected<CSSMathExpressionOperation>(
            operation->Category(), std::move(dest_operands),
            operation->OperatorType(), operation->Type());
    return std::make_tuple(MaybeSimplifyIfSumOrProductNode(new_op),
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
        operation->OperatorType(), operation->Type());
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
    return value.IsPercentage() && value.GetValueIfKnown() == 100.0;
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
const CSSMathExpressionNode*
CSSMathExpressionOperation::CreateArithmeticOperationAndSimplifyCalcSize(
    const CSSMathExpressionNode* left_side,
    const CSSMathExpressionNode* right_side,
    CSSMathOperator op) {
  DCHECK(op == CSSMathOperator::kAdd || op == CSSMathOperator::kSubtract ||
         op == CSSMathOperator::kMultiply);

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
          final_basis,
          MaybeSimplifyIfSumOrProductNode(CreateArithmeticOperationSimplified(
              left_calculation, right_calculation, op)));
    } else {
      const CSSMathExpressionNode* left_basis =
          left_calc_size->GetOperands()[0];
      const CSSMathExpressionNode* left_calculation =
          left_calc_size->GetOperands()[1];
      return CreateCalcSizeOperation(
          left_basis,
          MaybeSimplifyIfSumOrProductNode(CreateArithmeticOperationSimplified(
              left_calculation, right_side, op)));
    }
  } else if (right_calc_size) {
    const CSSMathExpressionNode* right_basis =
        right_calc_size->GetOperands()[0];
    const CSSMathExpressionNode* right_calculation =
        right_calc_size->GetOperands()[1];
    return CreateCalcSizeOperation(
        right_basis,
        MaybeSimplifyIfSumOrProductNode(CreateArithmeticOperationSimplified(
            left_side, right_calculation, op)));
  }

  return MaybeSimplifyIfSumOrProductNode(
      CreateArithmeticOperationSimplified(left_side, right_side, op));
}

CSSMathExpressionOperation::CSSMathExpressionOperation(
    const CSSMathExpressionNode* left_side,
    const CSSMathExpressionNode* right_side,
    CSSMathOperator op,
    CalculationResultCategory category,
    CSSMathType type)
    : CSSMathExpressionNode(
          category,
          left_side->HasComparisons() || right_side->HasComparisons(),
          left_side->HasAnchorFunctions() || right_side->HasAnchorFunctions(),
          !left_side->IsScopedValue() || !right_side->IsScopedValue()),
      operands_({left_side, right_side}),
      operator_(op),
      type_(std::move(type)) {
  DCHECK_NE(CSSMathOperator::kDivide, op);
  has_nested_intermediate_result_ = type_.IsIntermediateResult();
  has_nested_intermediate_result_ |= NodeHasNestedIntermediateResult(left_side);
  has_nested_intermediate_result_ |=
      NodeHasNestedIntermediateResult(right_side);
  needs_property_name_and_value_index_for_random_ |=
      (left_side && left_side->NeedsPropertyNameAndValueIndexForRandom()) ||
      (right_side && right_side->NeedsPropertyNameAndValueIndexForRandom());
}

bool CSSMathExpressionOperation::HasPercentage() const {
  if (Category() == kCalcPercent) {
    return true;
  }
  if (Category() != kCalcLengthFunction) {
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
    CSSMathOperator op,
    CSSMathType type)
    : CSSMathExpressionNode(
          category,
          IsComparison(op) || AnyOperandHasComparisons(operands),
          AnyOperandHasAnchorFunctions(operands),
          AnyOperandNeedsTreeScopePopulation(operands)),
      operands_(std::move(operands)),
      operator_(op),
      type_(std::move(type)) {
  DCHECK_NE(CSSMathOperator::kDivide, op);
  has_nested_intermediate_result_ = type_.IsIntermediateResult();
  if (IsArithmeticOperation()) {
    has_nested_intermediate_result_ |=
        NodeHasNestedIntermediateResult(operands_.front());
    has_nested_intermediate_result_ |=
        NodeHasNestedIntermediateResult(operands_.back());
  }
  for (const CSSMathExpressionNode* operand : operands_) {
    needs_property_name_and_value_index_for_random_ |=
        operand && operand->NeedsPropertyNameAndValueIndexForRandom();
  }
}

CSSMathExpressionOperation::CSSMathExpressionOperation(
    CalculationResultCategory category,
    CSSMathOperator op,
    CSSMathType type)
    : CSSMathExpressionNode(category,
                            IsComparison(op),
                            false /*has_anchor_functions*/,
                            false),
      operator_(op),
      type_(std::move(type)) {
  DCHECK_NE(CSSMathOperator::kDivide, op);
  has_nested_intermediate_result_ = type_.IsIntermediateResult();
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
    case CSSMathOperator::kMultiply: {
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
    case CSSMathOperator::kInvert:
      // 1/x can never give pixels.
      return std::nullopt;
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
    case CSSMathOperator::kSqrt:
    case CSSMathOperator::kLog:
    case CSSMathOperator::kExp:
    case CSSMathOperator::kHypot:
    case CSSMathOperator::kAbs:
    case CSSMathOperator::kSign:
    case CSSMathOperator::kProgress:
    case CSSMathOperator::kMediaProgress:
    case CSSMathOperator::kContainerProgress:
    case CSSMathOperator::kPow:
    case CSSMathOperator::kSin:
    case CSSMathOperator::kCos:
    case CSSMathOperator::kTan:
    case CSSMathOperator::kAsin:
    case CSSMathOperator::kAcos:
    case CSSMathOperator::kAtan:
    case CSSMathOperator::kAtan2:
      return std::nullopt;
    case CSSMathOperator::kDivide:
    case CSSMathOperator::kInvalid:
      NOTREACHED();
  }
  return result;
}

namespace {

CalculationOperator ConvertOperator(CSSMathOperator op) {
  switch (op) {
    case CSSMathOperator::kAdd:
      return CalculationOperator::kAdd;
    case CSSMathOperator::kSubtract:
      return CalculationOperator::kSubtract;
    case CSSMathOperator::kMultiply:
      return CalculationOperator::kMultiply;
    case CSSMathOperator::kInvert:
      return CalculationOperator::kInvert;
    case CSSMathOperator::kMin:
      return CalculationOperator::kMin;
    case CSSMathOperator::kMax:
      return CalculationOperator::kMax;
    case CSSMathOperator::kClamp:
      return CalculationOperator::kClamp;
    case CSSMathOperator::kRoundNearest:
      return CalculationOperator::kRoundNearest;
    case CSSMathOperator::kRoundUp:
      return CalculationOperator::kRoundUp;
    case CSSMathOperator::kRoundDown:
      return CalculationOperator::kRoundDown;
    case CSSMathOperator::kRoundToZero:
      return CalculationOperator::kRoundToZero;
    case CSSMathOperator::kMod:
      return CalculationOperator::kMod;
    case CSSMathOperator::kRem:
      return CalculationOperator::kRem;
    case CSSMathOperator::kLog:
      return CalculationOperator::kLog;
    case CSSMathOperator::kExp:
      return CalculationOperator::kExp;
    case CSSMathOperator::kSqrt:
      return CalculationOperator::kSqrt;
    case CSSMathOperator::kHypot:
      return CalculationOperator::kHypot;
    case CSSMathOperator::kAbs:
      return CalculationOperator::kAbs;
    case CSSMathOperator::kSign:
      return CalculationOperator::kSign;
    case CSSMathOperator::kProgress:
      return CalculationOperator::kProgress;
    case CSSMathOperator::kMediaProgress:
      return CalculationOperator::kMediaProgress;
    case CSSMathOperator::kContainerProgress:
      return CalculationOperator::kContainerProgress;
    case CSSMathOperator::kCalcSize:
      return CalculationOperator::kCalcSize;
    case CSSMathOperator::kSin:
      return CalculationOperator::kSin;
    case CSSMathOperator::kCos:
      return CalculationOperator::kCos;
    case CSSMathOperator::kTan:
      return CalculationOperator::kTan;
    case CSSMathOperator::kAsin:
      return CalculationOperator::kAsin;
    case CSSMathOperator::kAcos:
      return CalculationOperator::kAcos;
    case CSSMathOperator::kAtan:
      return CalculationOperator::kAtan;
    case CSSMathOperator::kAtan2:
      return CalculationOperator::kAtan2;
    case CSSMathOperator::kPow:
      return CalculationOperator::kPow;
    case CSSMathOperator::kDivide:
    case CSSMathOperator::kInvalid:
      NOTREACHED();
  }
}

}  // namespace

const CalculationExpressionNode*
CSSMathExpressionOperation::ToCalculationExpression(
    const CSSLengthResolver& length_resolver) const {
  CalculationOperator op = ConvertOperator(operator_);
  HeapVector<Member<const CalculationExpressionNode>> operands;
  operands.reserve(operands_.size());
  for (const CSSMathExpressionNode* operand : operands_) {
    operands.push_back(operand->ToCalculationExpression(length_resolver));
  }
  if (IsArithmeticOperation() &&
      !ArithmeticOperationIsAllowedToBeSimplified(*this)) {
    return MakeGarbageCollected<CalculationExpressionOperationNode>(
        std::move(operands), op);
  }
  return CalculationExpressionOperationNode::CreateSimplified(
      std::move(operands), op);
}

double CSSMathExpressionOperation::DoubleValue() const {
  DCHECK(HasDoubleValue(ResolvedUnitType())) << CustomCSSText();
  Vector<double> double_values;
  double_values.reserve(operands_.size());
  for (const CSSMathExpressionNode* operand : operands_) {
    double value = operand->DoubleValue();
    if (ShouldConvertRad2DegForOperator(operator_) &&
        operand->Category() == kCalcNumber) {
      value = Rad2deg(value);
    }
    double_values.push_back(value);
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
  if (category_ != kCalcIntermediate && !HasCanonicalUnit(category_)) {
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
  if (category_ != kCalcIntermediate && !HasCanonicalUnit(category_)) {
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
    double value = maybe_value.value();
    if (ShouldConvertRad2DegForOperator(operator_) &&
        operand->Category() == kCalcNumber) {
      value = Rad2deg(maybe_value.value());
    }
    double_values.push_back(value);
  }
  return Evaluate(double_values);
}

double CSSMathExpressionOperation::ComputeDouble(
    const CSSLengthResolver& length_resolver) const {
  Vector<double> double_values;
  double_values.reserve(operands_.size());
  for (const CSSMathExpressionNode* operand : operands_) {
    double value =
        CSSMathExpressionNode::ComputeDouble(operand, length_resolver);
    if (ShouldConvertRad2DegForOperator(operator_) &&
        operand->Category() == kCalcNumber) {
      value = Rad2deg(value);
    }
    double_values.push_back(value);
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
        if (IsNumericNodeWithDoubleValue(operands_[0])) {
          return operands_[1]->AccumulateLengthArray(
              length_array, multiplier * operands_[0]->DoubleValue());
        }
        return false;
      } else if (IsNumericNodeWithDoubleValue(operands_[1])) {
        return operands_[0]->AccumulateLengthArray(
            length_array, multiplier * operands_[1]->DoubleValue());
      } else {
        return false;
      }
    case CSSMathOperator::kInvert:
      // We don't support this yet.
      return false;
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
    case CSSMathOperator::kLog:
    case CSSMathOperator::kExp:
    case CSSMathOperator::kSqrt:
    case CSSMathOperator::kHypot:
    case CSSMathOperator::kAbs:
    case CSSMathOperator::kSign:
      // When stepped value functions are involved, we can't resolve the
      // expression into a length array.
    case CSSMathOperator::kProgress:
    case CSSMathOperator::kCalcSize:
    case CSSMathOperator::kMediaProgress:
    case CSSMathOperator::kContainerProgress:
    case CSSMathOperator::kPow:
    case CSSMathOperator::kSin:
    case CSSMathOperator::kCos:
    case CSSMathOperator::kTan:
    case CSSMathOperator::kAsin:
    case CSSMathOperator::kAcos:
    case CSSMathOperator::kAtan:
    case CSSMathOperator::kAtan2:
      return false;
    case CSSMathOperator::kInvalid:
    case CSSMathOperator::kDivide:
      NOTREACHED();
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

bool CSSMathExpressionOperation::IsElementDependent() const {
  for (const CSSMathExpressionNode* operand : operands_) {
    if (operand->IsElementDependent()) {
      return true;
    }
  }
  return false;
}

bool CSSMathExpressionOperation::MayHaveRelativeUnit() const {
  for (const CSSMathExpressionNode* operand : operands_) {
    if (operand->MayHaveRelativeUnit()) {
      return true;
    }
  }
  return false;
}

// https://drafts.csswg.org/css-values-4/#serialize-a-math-function
// “If a result of this serialization starts with a "(" (open parenthesis) and
// ends with a ")" (close parenthesis), remove those characters from the
// result.”
static void SerializeTopLevelNode(const CSSMathExpressionNode* node,
                                  StringBuilder& result) {
  String text = node->CustomCSSText();
  if (text.StartsWith('(')) {
    DCHECK(text.EndsWith(')'));
    result.Append(StringView(text, 1, text.length() - 2));
  } else {
    result.Append(text);
  }
}

String CSSMathExpressionOperation::CustomCSSText() const {
  switch (operator_) {
    case CSSMathOperator::kAdd:
    case CSSMathOperator::kSubtract:
    case CSSMathOperator::kMultiply: {
      DCHECK_EQ(operands_.size(), 2u);

      // As per
      // https://drafts.csswg.org/css-values-4/#sort-a-calculations-children
      // we should sort the dimensions of the sum node.
      UnitsVector terms = CollectSumOrProductInOrder(this);
      // During sorting, we might have ended up with just one term for the case
      // of complex-typed operation in form of Xpx * Ypx. That term would be the
      // same as the original operation, so in this case we need to unwrap it
      // into a product of two operands. This is a temporary workaround until we
      // fix the sorting to properly work with complex-typed units.
      if (terms.size() == 1) {
        terms = {
            {CSSMathOperator::kMultiply, operands_.front()},
            {operator_, operands_.back()},
        };
      }

      // https://drafts.csswg.org/css-values-4/#serialize-a-calculation-tree
      //
      // The parens will be removed by the caller if needed
      // (#serialize-a-math-function).
      StringBuilder result;
      result.Append('(');

      // The first node doesn't have an operator before it, so we need to make
      // sure it's always suitable as an additive value.
      const CSSMathExpressionNode* first_node =
          MaybeNegateFirstNode(terms[0].op, terms[0].node);
      result.Append(first_node->CustomCSSText());

      for (wtf_size_t i = 1; i < terms.size(); ++i) {
        CSSMathOperator op = terms[i].op;
        const CSSMathExpressionNode* node = terms[i].node;

        // For negative literals in sums, we flip the operator instead of
        // outputting the sign (e.g., a + -b => a - b).
        if (IsNumericNodeWithDoubleValue(node) &&
            (op == CSSMathOperator::kAdd || op == CSSMathOperator::kSubtract)) {
          double value = node->DoubleValue();
          if (value < 0.0) {
            op = op == CSSMathOperator::kAdd ? CSSMathOperator::kSubtract
                                             : CSSMathOperator::kAdd;
            node = CSSMathExpressionNumericLiteral::Create(
                -value, node->ResolvedUnitType());
          }
        }
        result.Append(' ');
        result.Append(ToString(op));
        result.Append(' ');
        result.Append(node->CustomCSSText());
      }

      result.Append(')');
      return result.ReleaseString();
    }
    case CSSMathOperator::kMin:
    case CSSMathOperator::kMax:
    case CSSMathOperator::kClamp:
    case CSSMathOperator::kMod:
    case CSSMathOperator::kRem:
    case CSSMathOperator::kLog:
    case CSSMathOperator::kExp:
    case CSSMathOperator::kSqrt:
    case CSSMathOperator::kHypot:
    case CSSMathOperator::kAbs:
    case CSSMathOperator::kSign:
    case CSSMathOperator::kCalcSize:
    case CSSMathOperator::kSin:
    case CSSMathOperator::kCos:
    case CSSMathOperator::kTan:
    case CSSMathOperator::kAsin:
    case CSSMathOperator::kAcos:
    case CSSMathOperator::kAtan:
    case CSSMathOperator::kAtan2:
    case CSSMathOperator::kPow: {
      StringBuilder result;
      result.Append(ToString(operator_));
      result.Append('(');
      SerializeTopLevelNode(operands_.front(), result);
      for (const CSSMathExpressionNode* operand : SecondToLastOperands()) {
        result.Append(", ");
        SerializeTopLevelNode(operand, result);
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
      SerializeTopLevelNode(operands_[0], result);
      if (ShouldSerializeRoundingStep(operands_)) {
        result.Append(", ");
        SerializeTopLevelNode(operands_[1], result);
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
      SerializeTopLevelNode(operands_.front(), result);
      result.Append(", ");
      SerializeTopLevelNode(operands_[1], result);
      result.Append(", ");
      SerializeTopLevelNode(operands_.back(), result);
      result.Append(')');

      return result.ReleaseString();
    }
    // https://drafts.csswg.org/css-values-4/#serialize-a-calculation-tree
    case CSSMathOperator::kInvert: {
      CHECK_EQ(operands_.size(), 1u);
      StringBuilder result;
      result.Append("(1 / ");
      result.Append(operands_[0]->CustomCSSText());
      result.Append(')');
      return result.ReleaseString();
    }
    case CSSMathOperator::kInvalid:
    case CSSMathOperator::kDivide:
      NOTREACHED();
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
        case CSSMathOperator::kMultiply: {
          DCHECK_EQ(operands_.size(), 2u);
          if (operands_[0]->Category() == kCalcNumber) {
            return operands_[1]->ResolvedUnitType();
          }
          if (operands_[1]->Category() == kCalcNumber) {
            return operands_[0]->ResolvedUnitType();
          }
          NOTREACHED();
        }
        case CSSMathOperator::kAdd:
        case CSSMathOperator::kSubtract:
        case CSSMathOperator::kMin:
        case CSSMathOperator::kMax:
        case CSSMathOperator::kClamp:
        case CSSMathOperator::kSqrt:
        case CSSMathOperator::kRoundNearest:
        case CSSMathOperator::kRoundUp:
        case CSSMathOperator::kRoundDown:
        case CSSMathOperator::kRoundToZero:
        case CSSMathOperator::kMod:
        case CSSMathOperator::kRem:
        case CSSMathOperator::kHypot:
        case CSSMathOperator::kAbs:
        case CSSMathOperator::kInvert: {
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
        case CSSMathOperator::kPow:
        case CSSMathOperator::kSin:
        case CSSMathOperator::kCos:
        case CSSMathOperator::kLog:
        case CSSMathOperator::kExp:
        case CSSMathOperator::kTan:
          return CSSPrimitiveValue::UnitType::kNumber;
        case CSSMathOperator::kAsin:
        case CSSMathOperator::kAcos:
        case CSSMathOperator::kAtan:
        case CSSMathOperator::kAtan2:
          return CSSPrimitiveValue::UnitType::kDegrees;
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
        case CSSMathOperator::kDivide:
        case CSSMathOperator::kInvalid:
          NOTREACHED();
      }
    case kCalcLengthFunction:
    case kCalcIntermediate:
    case kCalcOther:
      return CSSPrimitiveValue::UnitType::kUnknown;
    case kCalcIdent:
      return CSSPrimitiveValue::UnitType::kIdent;
  }

  NOTREACHED();
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
      // While kDivide should not happen during normal evaluation
      // (it is rewritten to kInvert during parsing), this can happen
      // during eager simplification.
      DCHECK(operands.size() == 1u || operands.size() == 2u);
      return operands[0] / operands[1];
    case CSSMathOperator::kInvert:
      DCHECK_EQ(operands.size(), 1u);
      return 1.0 / operands[0];
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
    case CSSMathOperator::kExp: {
      DCHECK_EQ(operands.size(), 1u);
      return std::exp(operands.front());
    }
    case CSSMathOperator::kLog: {
      DCHECK_GE(operands.size(), 1u);
      DCHECK_LE(operands.size(), 2u);
      if (operands.size() == 2) {
        return std::log2(operands.front()) / std::log2(operands.back());
      }
      return std::log(operands.front());
    }
    case CSSMathOperator::kSqrt: {
      DCHECK_EQ(operands.size(), 1u);
      return std::sqrt(operands.front());
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
      return EvaluateSignFunction(operands.front());
    }
    case CSSMathOperator::kProgress:
    case CSSMathOperator::kMediaProgress:
    case CSSMathOperator::kContainerProgress: {
      CHECK_EQ(operands.size(), 3u);
      double progress_value =
          (operands[0] - operands[1]) / (operands[2] - operands[1]);
      return std::clamp(progress_value, 0., 1.);
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
    case CSSMathOperator::kPow: {
      DCHECK_EQ(operands.size(), 2u);
      return std::pow(operands[0], operands[1]);
    }
    case CSSMathOperator::kSin:
    case CSSMathOperator::kCos:
    case CSSMathOperator::kTan:
    case CSSMathOperator::kAsin:
    case CSSMathOperator::kAcos:
    case CSSMathOperator::kAtan:
      DCHECK_EQ(operands.size(), 1u);
      return EvaluateTrigonometricFunction(op, operands.front());
    case CSSMathOperator::kAtan2:
      DCHECK_EQ(operands.size(), 2u);
      return EvaluateTrigonometricFunction(op, operands.front(),
                                           {operands.back()});
    case CSSMathOperator::kInvalid:
      NOTREACHED();
  }
}

const CSSMathExpressionNode& CSSMathExpressionOperation::PopulateWithTreeScope(
    const TreeScope* tree_scope) const {
  Operands populated_operands;
  for (const CSSMathExpressionNode* op : operands_) {
    populated_operands.push_back(&op->EnsureScopedValue(tree_scope));
  }
  return *MakeGarbageCollected<CSSMathExpressionOperation>(
      Category(), std::move(populated_operands), operator_, type_);
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
        Category(), std::move(transformed_operands), operator_, type_);
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

const CalculationExpressionNode*
CSSMathExpressionContainerFeature::ToCalculationExpression(
    const CSSLengthResolver& length_resolver) const {
  double progress =
      EvaluateContainerSize(size_feature_, container_name_, length_resolver);
  return MakeGarbageCollected<CalculationExpressionPixelsAndPercentNode>(
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
  NOTREACHED();
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
      NOTREACHED();
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
      NOTREACHED();
  }
}

}  // namespace

const CalculationExpressionNode*
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
      return AnchorQuery(type_, anchor_specifier,
                         percentage->ComputePercentage(length_resolver),
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
    case CSSValueID::kOutside:
    case CSSValueID::kInside:
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
      NOTREACHED();
  }
}

const CSSPrimitiveValue* TransformAnchorPercentage(
    const CSSPrimitiveValue* from,
    LogicalAxis logical_axis,
    const TryTacticTransform& transform) {
  if (FlipLogical(logical_axis, transform)) {
    return from->SubtractFrom(100.0, CSSPrimitiveValue::UnitType::kPercentage);
  }
  return from;
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
  } else if (const auto* from = DynamicTo<CSSPrimitiveValue>(value_.Get())) {
    DCHECK(from->IsPercentage());
    transformed_value =
        TransformAnchorPercentage(from, logical_axis, transform);
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
      case CSSValueID::kCalcSize:
      case CSSValueID::kRound:
      case CSSValueID::kMod:
      case CSSValueID::kRem:
      case CSSValueID::kPow:
      case CSSValueID::kSqrt:
      case CSSValueID::kHypot:
      case CSSValueID::kLog:
      case CSSValueID::kExp:
      case CSSValueID::kSiblingCount:
      case CSSValueID::kSiblingIndex:
      case CSSValueID::kAbs:
      case CSSValueID::kSign:
        return true;
      case CSSValueID::kProgress:
        return RuntimeEnabledFeatures::CSSProgressNotationEnabled();
      case CSSValueID::kMediaProgress:
        return RuntimeEnabledFeatures::CSSMediaProgressNotationEnabled();
      case CSSValueID::kContainerProgress:
        return RuntimeEnabledFeatures::CSSContainerProgressNotationEnabled();
      case CSSValueID::kRandom:
        return RuntimeEnabledFeatures::CSSRandomFunctionEnabled();
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

  bool ParseProgressNotationStartAndEndValues(
      CSSParserTokenStream& stream,
      State state,
      CSSMathExpressionOperation::Operands& nodes) {
    if (!css_parsing_utils::ConsumeCommaIncludingWhitespace(stream)) {
      return false;
    }
    if (CSSMathExpressionNode* node = ParseValueExpression(stream, state)) {
      nodes.push_back(node);
    }
    if (!css_parsing_utils::ConsumeCommaIncludingWhitespace(stream)) {
      return false;
    }
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
    // <media-progress()> = media-progress(<media-feature>, <calc-sum>,
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
      // <container-name> ]?, <calc-sum>, <calc-sum>)
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
      // <progress()> = progress(<calc-sum>, <calc-sum>, <calc-sum>)
      nodes.push_back(node);
    }
    if (!ParseProgressNotationStartAndEndValues(stream, state, nodes)) {
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
      progress_value = std::clamp(progress_value, 0., 1.);
      return CSSMathExpressionNumericLiteral::Create(
          progress_value, CSSPrimitiveValue::UnitType::kNumber);
    }
    return MakeGarbageCollected<CSSMathExpressionOperation>(
        CalculationResultCategory::kCalcNumber, std::move(nodes),
        CSSValueIDToCSSMathOperator(function_id), CSSMathType());
  }

  CSSMathExpressionNode* ParseCalcSize(CSSValueID function_id,
                                       CSSParserTokenStream& stream,
                                       State state) {
    if (function_id != CSSValueID::kCalcSize ||
        !parsing_flags_.Has(Flag::AllowCalcSize)) {
      return nullptr;
    }

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
              CSSValueID::kFitContent, CSSValueID::kStretch>(id)) {
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

  CSSMathExpressionNode* ParseSiblingIndexOrCount(CSSValueID function_id,
                                                  CSSParserTokenStream& stream,
                                                  State state) {
    if (function_id != CSSValueID::kSiblingCount &&
        function_id != CSSValueID::kSiblingIndex) {
      return nullptr;
    }
    if (!context_.InElementContext()) {
      return nullptr;
    }
    if (!stream.AtEnd()) {
      // These do not take any arguments.
      return nullptr;
    }
    cssvalue::CSSScopedKeywordValue* scoped_function =
        MakeGarbageCollected<cssvalue::CSSScopedKeywordValue>(function_id);
    return MakeGarbageCollected<CSSMathExpressionSiblingFunction>(
        scoped_function);
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
    if (CSSMathExpressionNode* calc_size =
            ParseCalcSize(function_id, stream, state)) {
      context_.Count(WebFeature::kCSSCalcSizeFunction);
      return calc_size;
    }
    if (CSSMathExpressionNode* sibling_function =
            ParseSiblingIndexOrCount(function_id, stream, state)) {
      return sibling_function;
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
        max_argument_count = 2;
        min_argument_count = 2;
        break;
      case CSSValueID::kExp:
      case CSSValueID::kSqrt:
        max_argument_count = 1;
        break;
      case CSSValueID::kHypot:
        max_argument_count = kMaxExpressionDepth;
        break;
      case CSSValueID::kLog:
        max_argument_count = 2;
        break;
      case CSSValueID::kRound:
        max_argument_count = 2;
        min_argument_count = 1;
        break;
      case CSSValueID::kMod:
      case CSSValueID::kRem:
        max_argument_count = 2;
        min_argument_count = 2;
        break;
      case CSSValueID::kAtan2:
        max_argument_count = 2;
        min_argument_count = 2;
        break;
      case CSSValueID::kAbs:
      case CSSValueID::kSign:
        max_argument_count = 1;
        min_argument_count = 1;
        break;
      case CSSValueID::kRandom:
        DCHECK(RuntimeEnabledFeatures::CSSRandomFunctionEnabled());
        max_argument_count = 3;
        min_argument_count = 2;
        break;
      // TODO(crbug.com/1284199): Support other math functions.
      default:
        break;
    }

    HeapVector<Member<const CSSMathExpressionNode>> nodes;

    // Parse any non-expression argument(s).
    std::variant<CSSMathOperator, const RandomValueSharing*> non_expr_argument;
    switch (function_id) {
      case CSSValueID::kRound: {
        // Parse the initial (optional) <rounding-strategy> argument to the
        // round() function.
        std::optional<CSSMathOperator> rounding_strategy =
            ParseRoundingStrategy(stream);
        if (rounding_strategy) {
          if (!css_parsing_utils::ConsumeCommaIncludingWhitespace(stream)) {
            return nullptr;
          }
        }
        // If no rounding strategy, was specified operation, use "nearest".
        non_expr_argument =
            rounding_strategy.value_or(CSSMathOperator::kRoundNearest);
        break;
      }
      case CSSValueID::kRandom: {
        DCHECK(RuntimeEnabledFeatures::CSSRandomFunctionEnabled());
        // Parse the (optional) <random-value-sharing> argument of the random()
        // function.
        const RandomValueSharing* random_value_sharing =
            RandomValueSharing::Parse(stream, context_);
        if (random_value_sharing) {
          if (!css_parsing_utils::ConsumeCommaIncludingWhitespace(stream)) {
            return nullptr;
          }
          non_expr_argument = random_value_sharing;
        } else {
          non_expr_argument = RandomValueSharing::Auto();
        }
        break;
      }
      default:
        break;
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
        if (node->IsCalcSize()) {
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
            CSSMathExpressionOperation::CreateComparisonFunction(
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
      case CSSValueID::kAtan2: {
        CSSMathExpressionNode* node =
            CSSMathExpressionOperation::CreateTrigonometricFunction(
                std::move(nodes), function_id);
        if (node) {
          context_.Count(WebFeature::kCSSTrigFunctions);
        }
        return node;
      }
      case CSSValueID::kPow:
      case CSSValueID::kSqrt:
      case CSSValueID::kHypot:
      case CSSValueID::kLog:
      case CSSValueID::kExp: {
        CSSMathExpressionNode* node =
            CSSMathExpressionOperation::CreateExponentialFunction(
                std::move(nodes), function_id);
        if (node) {
          context_.Count(WebFeature::kCSSExponentialFunctions);
        }
        return node;
      }
      case CSSValueID::kRound:
      case CSSValueID::kMod:
      case CSSValueID::kRem: {
        CSSMathOperator op;
        if (function_id == CSSValueID::kRound) {
          DCHECK_GE(nodes.size(), 1u);
          DCHECK_LE(nodes.size(), 2u);
          const auto& rounding_strategy =
              std::get<CSSMathOperator>(non_expr_argument);
          op = rounding_strategy;
          if (!CanonicalizeRoundArguments(nodes)) {
            return nullptr;
          }
        } else if (function_id == CSSValueID::kMod) {
          op = CSSMathOperator::kMod;
        } else {
          op = CSSMathOperator::kRem;
        }
        DCHECK_EQ(nodes.size(), 2u);
        context_.Count(WebFeature::kCSSRoundModRemFunctions);
        return CSSMathExpressionOperation::CreateSteppedValueFunction(
            std::move(nodes), op);
      }
      case CSSValueID::kAbs:
      case CSSValueID::kSign:
        return CSSMathExpressionOperation::CreateSignRelatedFunction(
            std::move(nodes), function_id);

      case CSSValueID::kSiblingIndex:
      case CSSValueID::kSiblingCount:
        // Handled above.
        return nullptr;
      case CSSValueID::kRandom: {
        DCHECK(RuntimeEnabledFeatures::CSSRandomFunctionEnabled());
        DCHECK_GE(nodes.size(), 2u);
        DCHECK_LE(nodes.size(), 3u);
        const auto& random_value_sharing =
            std::get<const RandomValueSharing*>(non_expr_argument);
        return CSSMathExpressionRandomFunction::Create(random_value_sharing,
                                                       std::move(nodes));
      }
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
      context_.Count(WebFeature::kCSSCalcConstants);
      return CSSMathExpressionNumericLiteral::Create(
          std::numeric_limits<double>::infinity(),
          CSSPrimitiveValue::UnitType::kNumber);
    }
    if (token.Id() == CSSValueID::kNegativeInfinity) {
      context_.Count(WebFeature::kCSSCalcConstants);
      return CSSMathExpressionNumericLiteral::Create(
          -std::numeric_limits<double>::infinity(),
          CSSPrimitiveValue::UnitType::kNumber);
    }
    if (token.Id() == CSSValueID::kNan) {
      context_.Count(WebFeature::kCSSCalcConstants);
      return CSSMathExpressionNumericLiteral::Create(
          std::numeric_limits<double>::quiet_NaN(),
          CSSPrimitiveValue::UnitType::kNumber);
    }
    if (token.Id() == CSSValueID::kPi) {
      context_.Count(WebFeature::kCSSCalcConstants);
      return CSSMathExpressionNumericLiteral::Create(
          M_PI, CSSPrimitiveValue::UnitType::kNumber);
    }
    if (token.Id() == CSSValueID::kE) {
      context_.Count(WebFeature::kCSSCalcConstants);
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

  std::optional<CSSMathOperator> ParseRoundingStrategy(
      CSSParserTokenStream& stream) {
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
        return std::nullopt;
    }
    stream.ConsumeIncludingWhitespace();
    return rounding_op;
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

    if (auto* operation = DynamicTo<CSSMathExpressionOperation>(result)) {
      if (operation->IsMultiplyOrDivide() &&
          ArithmeticOperationIsAllowedToBeSimplified(*operation)) {
        result = MaybeSimplifySumOrProductNode(operation);
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
      if (operation->IsAddOrSubtract() &&
          ArithmeticOperationIsAllowedToBeSimplified(*operation)) {
        result = MaybeSimplifySumOrProductNode(operation);
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

const CalculationValue* CSSMathExpressionNode::ToCalcValue(
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
    return MakeGarbageCollected<CalculationValue>(*maybe_pixels_and_percent,
                                                  range);
  }

  const auto* value = ToCalculationExpression(length_resolver);
  std::optional<PixelsAndPercent> evaluated_value =
      EvaluateValueIfNaNorInfinity(value, allows_negative_percentage_reference);
  if (evaluated_value.has_value()) {
    return MakeGarbageCollected<CalculationValue>(evaluated_value.value(),
                                                  range);
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
    case CalculationOperator::kInvert: {
      DCHECK_EQ(children.size(), 1u);
      return CSSMathExpressionOperation::CreateInvertFunction(
          Create(*children.front()));
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
    case CalculationOperator::kLog: {
      DCHECK_GE(children.size(), 1u);
      DCHECK_LE(children.size(), 2u);
      CSSMathExpressionOperation::Operands operands;
      for (const auto& child : children) {
        operands.push_back(Create(*child));
      }
      return CSSMathExpressionOperation::CreateExponentialFunction(
          std::move(operands), CSSValueID::kLog);
    }
    case CalculationOperator::kExp: {
      DCHECK_EQ(children.size(), 1u);
      CSSMathExpressionOperation::Operands operands;
      operands.push_back(Create(*children.front()));
      return CSSMathExpressionOperation::CreateExponentialFunction(
          std::move(operands), CSSValueID::kExp);
    }
    case CalculationOperator::kSqrt: {
      DCHECK_EQ(children.size(), 1u);
      CSSMathExpressionOperation::Operands operands;
      operands.push_back(Create(*children.front()));
      return CSSMathExpressionOperation::CreateExponentialFunction(
          std::move(operands), CSSValueID::kSqrt);
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
          CalculationResultCategory::kCalcNumber, std::move(operands), op,
          CSSMathType());
    }
    case CalculationOperator::kCalcSize: {
      CHECK_EQ(children.size(), 2u);
      return CSSMathExpressionOperation::CreateCalcSizeOperation(
          Create(*children.front()), Create(*children.back()));
    }
    case CalculationOperator::kPow: {
      DCHECK_EQ(children.size(), 2u);
      CSSMathExpressionOperation::Operands operands;
      operands.push_back(Create(*children.front()));
      operands.push_back(Create(*children.back()));
      return CSSMathExpressionOperation::CreateSignRelatedFunction(
          std::move(operands), CSSValueID::kPow);
    }
    case CalculationOperator::kSin:
    case CalculationOperator::kCos:
    case CalculationOperator::kTan:
    case CalculationOperator::kAsin:
    case CalculationOperator::kAcos:
    case CalculationOperator::kAtan: {
      DCHECK_EQ(children.size(), 1u);
      CSSValueID funtion_id =
          TrigonometricCalculationOperatorToCSSValueID(calc_op);
      CSSMathExpressionOperation::Operands operands;
      operands.push_back(Create(*children.front()));
      return CSSMathExpressionOperation::CreateTrigonometricFunction(
          std::move(operands), funtion_id);
    }
    case CalculationOperator::kAtan2: {
      DCHECK_EQ(children.size(), 2u);
      CSSMathExpressionOperation::Operands operands;
      operands.push_back(Create(*children.front()));
      operands.push_back(Create(*children.back()));
      return CSSMathExpressionOperation::CreateSignRelatedFunction(
          std::move(operands), CSSValueID::kAtan2);
    }
    case CalculationOperator::kRandom: {
      DCHECK_GE(children.size(), 3u);
      DCHECK_LE(children.size(), 4u);
      const RandomValueSharing* random_value_sharing =
          RandomValueSharing::Fixed(
              DynamicTo<CalculationExpressionNumberNode>(*children[0])
                  ->Value());
      CSSMathExpressionOperation::Operands operands;
      for (wtf_size_t i = 1; i < children.size(); ++i) {
        operands.push_back(Create(*children[i]));
      }
      return CSSMathExpressionRandomFunction::Create(random_value_sharing,
                                                     std::move(operands));
    }
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

String CSSMathExpressionSiblingFunction::CustomCSSText() const {
  return function_->GetValueID() == CSSValueID::kSiblingIndex
             ? "sibling-index()"
             : "sibling-count()";
}

const CalculationExpressionNode*
CSSMathExpressionSiblingFunction::ToCalculationExpression(
    const CSSLengthResolver& length_resolver) const {
  return MakeGarbageCollected<CalculationExpressionNumberNode>(
      ComputeDouble(length_resolver));
}

bool CSSMathExpressionSiblingFunction::operator==(
    const CSSMathExpressionNode& other) const {
  return other.IsSiblingFunction() &&
         *function_ == *To<CSSMathExpressionSiblingFunction>(other).function_;
}

double CSSMathExpressionSiblingFunction::ComputeDouble(
    const CSSLengthResolver& length_resolver) const {
  length_resolver.ReferenceSibling();
  const Element* element = length_resolver.GetElement();
  if (const TreeScope* value_scope = function_->GetTreeScope()) {
    if (!element->GetTreeScope().IsInclusiveAncestorTreeScopeOf(*value_scope)) {
      return 0;
    }
  }
  NthIndexCache* nth_index_cache = element->ownerDocument()->GetNthIndexCache();
  if (function_->GetValueID() == CSSValueID::kSiblingIndex) {
    return nth_index_cache->NthChildIndex(const_cast<Element&>(*element),
                                          /*filter=*/nullptr,
                                          /*selector_checker=*/nullptr,
                                          /*context=*/nullptr);
  } else {
    return nth_index_cache->NthChildIndex(const_cast<Element&>(*element),
                                          /*filter=*/nullptr,
                                          /*selector_checker=*/nullptr,
                                          /*context=*/nullptr) +
           nth_index_cache->NthLastChildIndex(const_cast<Element&>(*element),
                                              /*filter=*/nullptr,
                                              /*selector_checker=*/nullptr,
                                              /*context=*/nullptr) -
           1;
  }
}

const CSSMathExpressionNode&
CSSMathExpressionSiblingFunction::PopulateWithTreeScope(
    const TreeScope* tree_scope) const {
  return *MakeGarbageCollected<CSSMathExpressionSiblingFunction>(
      &To<cssvalue::CSSScopedKeywordValue>(
          function_->EnsureScopedValue(tree_scope)));
}

std::optional<double>
CSSMathExpressionSiblingFunction::ComputeValueInCanonicalUnit(
    const CSSLengthResolver& length_resolver) const {
  if (length_resolver.GetElement()) {
    return ComputeDouble(length_resolver);
  }
  return std::nullopt;
}

void CSSMathExpressionSiblingFunction::Trace(Visitor* visitor) const {
  visitor->Trace(function_);
  CSSMathExpressionNode::Trace(visitor);
}

bool RandomValueSharing::IsFixed() const {
  return std::holds_alternative<Member<const CSSPrimitiveValue>>(value_);
}
const CSSPrimitiveValue* RandomValueSharing::GetFixed() const {
  DCHECK(std::holds_alternative<Member<const CSSPrimitiveValue>>(value_));
  return std::get<Member<const CSSPrimitiveValue>>(value_);
}
bool RandomValueSharing::IsAuto() const {
  return !std::holds_alternative<NameAndElementShared>(value_) ||
         !std::get<NameAndElementShared>(value_).name.StartsWith("--");
}
AtomicString RandomValueSharing::Name() const {
  if (!std::holds_alternative<NameAndElementShared>(value_)) {
    return g_null_atom;
  }
  return std::get<NameAndElementShared>(value_).name;
}
bool RandomValueSharing::IsElementShared() const {
  return std::holds_alternative<NameAndElementShared>(value_) &&
         std::get<NameAndElementShared>(value_).element_shared;
}

const RandomValueSharing*
RandomValueSharing::CopyWithPropertyValueIndexNameIfNeeded(
    const CSSPropertyName& property_name,
    wtf_size_t property_value_index) const {
  if (IsFixed()) {
    const CSSPrimitiveValue* fixed_with_property = To<CSSPrimitiveValue>(
        GetFixed()->CopyRandomValueWithPropertyNameAndValueIndexIfNeeded(
            property_name, property_value_index));
    return MakeGarbageCollected<RandomValueSharing>(fixed_with_property);
  }
  NameAndElementShared name_and_element_shared =
      std::get<NameAndElementShared>(value_);
  if (name_and_element_shared.name.IsNull()) {
    StringBuilder str;
    // Use string of form "PROPERTY {property_name} {property_value_index}"
    // as name, this is later used for caching random values [0]. The prefix
    // "PROPERTY" is needed since we need to make distinguish between custom
    // property name and random value identifier, i.e. <dashed-ident> value in
    // <random-value-sharing> [1]
    // [0] https://drafts.csswg.org/css-values-5/#random-caching-key
    // [1] https://drafts.csswg.org/css-values-5/#typedef-random-value-sharing
    str.Append("PROPERTY ");
    str.Append(property_name.ToAtomicString());
    str.Append(" ");
    str.AppendNumber(property_value_index);
    return MakeGarbageCollected<RandomValueSharing>(
        str.ToAtomicString(), name_and_element_shared.element_shared);
  }
  return this;
}

const RandomValueSharing* RandomValueSharing::Parse(
    CSSParserTokenStream& stream,
    const CSSParserContext& context) {
  if (stream.Peek().GetType() != kIdentToken) {
    return nullptr;
  }

  CSSParserToken token = stream.Peek();
  if (token.Value() == "fixed") {
    CSSParserTokenStream::State savepoint = stream.Save();
    stream.ConsumeIncludingWhitespace();

    CSSPrimitiveValue* fixed_value = css_parsing_utils::ConsumeNumber(
        stream, context, CSSPrimitiveValue::ValueRange::kAll);
    if (!fixed_value) {
      stream.Restore(savepoint);
      return nullptr;
    }
    if (const CSSNumericLiteralValue* numeric_value =
            DynamicTo<CSSNumericLiteralValue>(fixed_value)) {
      DCHECK(numeric_value->IsNumber());
      double double_value = numeric_value->DoubleValue();
      if (double_value < 0.0 || double_value > 1.0) {
        stream.Restore(savepoint);
        return nullptr;
      }
      if (double_value == 1.0) {
        fixed_value = CSSNumericLiteralValue::Create(
            std::nextafter(1.0f, 0.0f), CSSPrimitiveValue::UnitType::kNumber);
      }
    }
    return MakeGarbageCollected<RandomValueSharing>(fixed_value);
  }

  wtf_size_t offset = stream.Offset();

  ElementShared element_shared(false);
  if (token.Value() == "element-shared") {
    element_shared = ElementShared(true);
    stream.ConsumeIncludingWhitespace();
  }

  token = stream.Peek();
  if (stream.Peek().GetType() != kIdentToken) {
    return MakeGarbageCollected<RandomValueSharing>(element_shared);
  }

  AtomicString name = g_null_atom;
  if (token.Value() == "auto") {
    stream.ConsumeIncludingWhitespace();
  }

  if (token.Value().ToString().StartsWith("--")) {
    name = stream.ConsumeIncludingWhitespace().Value().ToAtomicString();
  }

  token = stream.Peek();
  if (!element_shared && stream.Peek().GetType() == kIdentToken &&
      token.Value() == "element-shared") {
    element_shared = ElementShared(true);
    stream.ConsumeIncludingWhitespace();
  }

  if (stream.Offset() == offset) {
    return nullptr;
  }
  return MakeGarbageCollected<RandomValueSharing>(name, element_shared);
}

const RandomValueSharing* RandomValueSharing::Fixed(double fixed_value) {
  return MakeGarbageCollected<RandomValueSharing>(
      CSSNumericLiteralValue::Create(fixed_value,
                                     CSSPrimitiveValue::UnitType::kNumber));
}

void RandomValueSharing::Trace(Visitor* visitor) const {
  if (IsFixed()) {
    visitor->Trace(std::get<Member<const CSSPrimitiveValue>>(value_));
  }
}

String RandomValueSharing::CssText() const {
  StringBuilder result;
  if (IsFixed()) {
    result.Append("fixed ");
    result.Append(GetFixed()->CustomCSSText());
  }
  if (!IsAuto()) {
    result.Append(Name());
  }
  if (IsElementShared()) {
    if (!result.empty()) {
      result.Append(" ");
    }
    result.Append("element-shared");
  }
  return result.ToString();
}

bool RandomValueSharing::operator==(const RandomValueSharing& other) const {
  if (std::holds_alternative<NameAndElementShared>(value_) &&
      std::holds_alternative<NameAndElementShared>(other.value_)) {
    return std::get<NameAndElementShared>(value_) ==
           std::get<NameAndElementShared>(other.value_);
  }
  if (std::holds_alternative<Member<const CSSPrimitiveValue>>(value_) &&
      std::holds_alternative<Member<const CSSPrimitiveValue>>(other.value_)) {
    return base::ValuesEquivalent(
        std::get<Member<const CSSPrimitiveValue>>(value_),
        std::get<Member<const CSSPrimitiveValue>>(other.value_));
  }
  return false;
}

CSSMathExpressionRandomFunction::CSSMathExpressionRandomFunction(
    base::PassKey<CSSMathExpressionRandomFunction>,
    CalculationResultCategory category,
    const RandomValueSharing* random_value_sharing,
    const CSSMathExpressionNode* min,
    const CSSMathExpressionNode* max,
    const CSSMathExpressionNode* step)
    : CSSMathExpressionNode(category,
                            /*has_comparisons=*/false,
                            /*has_anchor_functions=*/false,
                            /*needs_tree_scope_population=*/false),
      random_value_sharing_(random_value_sharing),
      min_(min),
      max_(max),
      step_(step) {
  needs_property_name_and_value_index_for_random_ =
      random_value_sharing->Name().IsNull();
}

CSSMathExpressionRandomFunction* CSSMathExpressionRandomFunction::Create(
    const RandomValueSharing* random_value_sharing,
    HeapVector<Member<const CSSMathExpressionNode>>&& nodes) {
  CalculationResultCategory category = DetermineComparisonCategory(nodes);
  // Currently the computed value for calc() expressions with category
  // `kCalcPercent`, i.e. calc() with only percentages: min(10%, 30%)
  // would be simplified to 10%. This is not correct, since percentages
  // here can represent negative values. Same issue will happen with
  // random() if `min`, `max` (and optionally `step`) parameters have only
  // percentages values. To avoid that we will use `category
  // `kCalcLengthPercent` for these expressions for now.
  // TODO(crbug.com/463635948): Remove the following if check.
  if (category == kCalcPercent) {
    category = kCalcLengthFunction;
  }
  if (category == CalculationResultCategory::kCalcOther) {
    return nullptr;
  }
  const CSSMathExpressionNode* step = (nodes.size() == 3) ? nodes[2] : nullptr;
  return MakeGarbageCollected<CSSMathExpressionRandomFunction>(
      base::PassKey<CSSMathExpressionRandomFunction>(), category,
      random_value_sharing,
      /* min= */ nodes[0], /* max= */ nodes[1], /* step= */ step);
}

CSSMathExpressionNode* CSSMathExpressionRandomFunction::Copy() const {
  return MakeGarbageCollected<CSSMathExpressionRandomFunction>(
      base::PassKey<CSSMathExpressionRandomFunction>(), category_,
      random_value_sharing_, min_, max_, step_);
}

const CSSMathExpressionNode* CSSMathExpressionRandomFunction::
    CopyRandomWithPropertyNameAndValueIndexIfNeeded(
        const CSSPropertyName& property_name,
        wtf_size_t property_value_index) const {
  const RandomValueSharing* random_value_sharing =
      random_value_sharing_->CopyWithPropertyValueIndexNameIfNeeded(
          property_name, property_value_index);
  return MakeGarbageCollected<CSSMathExpressionRandomFunction>(
      base::PassKey<CSSMathExpressionRandomFunction>(), category_,
      random_value_sharing, min_, max_, step_);
}

bool CSSMathExpressionRandomFunction::IsComputationallyIndependent() const {
  return min_->IsComputationallyIndependent() &&
         max_->IsComputationallyIndependent() &&
         (step_ && step_->IsComputationallyIndependent());
}

bool CSSMathExpressionRandomFunction::IsElementDependent() const {
  return !random_value_sharing_->IsElementShared();
}

bool CSSMathExpressionRandomFunction::HasInvalidAnchorFunctions(
    const CSSLengthResolver& length_resolver) const {
  return min_->HasInvalidAnchorFunctions(length_resolver) ||
         max_->HasInvalidAnchorFunctions(length_resolver) ||
         (step_ && step_->HasInvalidAnchorFunctions(length_resolver));
}

bool CSSMathExpressionRandomFunction::MayHaveRelativeUnit() const {
  return min_->MayHaveRelativeUnit() || max_->MayHaveRelativeUnit() ||
         (step_ && step_->MayHaveRelativeUnit());
}

void CSSMathExpressionRandomFunction::AccumulateLengthUnitTypes(
    CSSPrimitiveValue::LengthTypeFlags& types) const {
  min_->AccumulateLengthUnitTypes(types);
  max_->AccumulateLengthUnitTypes(types);
  if (step_) {
    step_->AccumulateLengthUnitTypes(types);
  }
}

namespace {

double GetRandomBaseValue(const RandomValueSharing* random_value_sharing,
                          const CSSLengthResolver& length_resolver) {
  DCHECK(random_value_sharing);
  const Element* element = length_resolver.GetElement();
  CHECK(element);
  if (random_value_sharing->IsFixed()) {
    double random_base_value = std::clamp(
        random_value_sharing->GetFixed()->ComputeNumber(length_resolver), 0.,
        1.);
    if (random_base_value == 1.0) {
      random_base_value = std::nextafter(1.0f, 0.0f);
    }
    return random_base_value;
  }
  return element->GetDocument().GetStyleEngine().GetCachedRandomBaseValue(
      *random_value_sharing, element);
}

}  // namespace

const CalculationExpressionNode*
CSSMathExpressionRandomFunction::ToCalculationExpression(
    const CSSLengthResolver& length_resolver) const {
  double random_base_value =
      GetRandomBaseValue(random_value_sharing_, length_resolver);

  HeapVector<Member<const CalculationExpressionNode>> operands;
  operands.push_back(
      MakeGarbageCollected<CalculationExpressionNumberNode>(random_base_value));
  operands.push_back(min_->ToCalculationExpression(length_resolver));
  operands.push_back(max_->ToCalculationExpression(length_resolver));
  if (step_) {
    operands.push_back(step_->ToCalculationExpression(length_resolver));
  }
  return CalculationExpressionOperationNode::CreateSimplified(
      std::move(operands), CalculationOperator::kRandom);
}

#if DCHECK_IS_ON()
bool CSSMathExpressionRandomFunction::InvolvesPercentageComparisons() const {
  return min_->InvolvesPercentageComparisons() ||
         max_->InvolvesPercentageComparisons() ||
         (step_ && step_->InvolvesPercentageComparisons());
}
#endif

double CSSMathExpressionRandomFunction::ComputeDouble(
    const CSSLengthResolver& length_resolver) const {
  if (!random_value_sharing_->IsElementShared()) {
    length_resolver.ReferenceElementDependentRandom();
  }
  double random_base_value =
      GetRandomBaseValue(random_value_sharing_, length_resolver);
  double min = min_->ComputeNumber(length_resolver);
  double max = max_->ComputeNumber(length_resolver);
  std::optional<double> step = std::nullopt;
  if (step_) {
    step = step_->ComputeNumber(length_resolver);
  }
  return ComputeCSSRandomValue(random_base_value, min, max, step);
}

CSSPrimitiveValue::UnitType CSSMathExpressionRandomFunction::ResolvedUnitType()
    const {
  CSSPrimitiveValue::UnitType min_type = min_->ResolvedUnitType();
  CSSPrimitiveValue::UnitType max_type = max_->ResolvedUnitType();
  if (min_type == CSSPrimitiveValue::UnitType::kUnknown ||
      max_type == CSSPrimitiveValue::UnitType::kUnknown ||
      min_type != max_type) {
    return CSSPrimitiveValue::UnitType::kUnknown;
  }
  if (!step_) {
    return min_type;
  }
  CSSPrimitiveValue::UnitType step_type = step_->ResolvedUnitType();

  if (step_type == CSSPrimitiveValue::UnitType::kUnknown ||
      min_type != step_type) {
    return CSSPrimitiveValue::UnitType::kUnknown;
  }
  return min_type;
}

double CSSMathExpressionRandomFunction::ComputeLengthPx(
    const CSSLengthResolver& length_resolver) const {
  DCHECK(!HasPercentage());
  DCHECK_EQ(Category(), kCalcLength);
  return ComputeDouble(length_resolver);
}

String CSSMathExpressionRandomFunction::CustomCSSText() const {
  StringBuilder result;
  result.Append("random(");
  String random_value_sharing_str = random_value_sharing_->CssText();
  if (!random_value_sharing_str.empty()) {
    result.Append(random_value_sharing_str);
    result.Append(", ");
  }
  result.Append(min_->CustomCSSText());
  result.Append(", ");
  result.Append(max_->CustomCSSText());
  if (step_) {
    result.Append(", ");
    result.Append(step_->CustomCSSText());
  }
  result.Append(')');
  return result.ToString();
}

bool CSSMathExpressionRandomFunction::operator==(
    const CSSMathExpressionNode& exp) const {
  if (!exp.IsRandomFunction()) {
    return false;
  }
  const CSSMathExpressionRandomFunction& other =
      To<CSSMathExpressionRandomFunction>(exp);
  return random_value_sharing_ == other.random_value_sharing_ &&
         min_ == other.min_ && max_ == other.max_ && step_ == other.step_;
}

void CSSMathExpressionRandomFunction::Trace(Visitor* visitor) const {
  visitor->Trace(random_value_sharing_);
  visitor->Trace(min_);
  visitor->Trace(max_);
  visitor->Trace(step_);
  CSSMathExpressionNode::Trace(visitor);
}

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::CSSMathExpressionNodeWithOperator)
