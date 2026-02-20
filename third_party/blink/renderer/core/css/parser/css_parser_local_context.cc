// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"

#include "third_party/blink/renderer/core/css/properties/css_property.h"

namespace blink {

bool CSSParserLocalContext::PercentagesDependOnUsedValue() const {
  if (!unresolved_property_name_.has_value() ||
      unresolved_property_name_->Id() == CSSPropertyID::kInvalid ||
      unresolved_property_name_->Id() == CSSPropertyID::kVariable) {
    return false;
  }
  if (InFunctionContext()) {
    CSSValueID current_function_id = functions_stack_.back();
    return current_function_id == CSSValueID::kCrossFade ||
           current_function_id == CSSValueID::kConicGradient ||
           current_function_id == CSSValueID::kRadialGradient ||
           current_function_id == CSSValueID::kInset ||
           current_function_id == CSSValueID::kXywh ||
           current_function_id == CSSValueID::kRect ||
           current_function_id == CSSValueID::kCircle ||
           current_function_id == CSSValueID::kEllipse ||
           current_function_id == CSSValueID::kPolygon ||
           current_function_id == CSSValueID::kShape ||
           current_function_id == CSSValueID::kScale3d ||
           current_function_id == CSSValueID::kScaleZ ||
           current_function_id == CSSValueID::kTranslate ||
           current_function_id == CSSValueID::kTranslateX ||
           current_function_id == CSSValueID::kTranslateY ||
           current_function_id == CSSValueID::kTranslate3d ||
           current_function_id == CSSValueID::kRepeat;
  }
  CSSProperty property =
      CSSProperty::Get(ResolveCSSPropertyID(unresolved_property_name_->Id()));
  return property.PercentagesDependOnUsedValue();
}

#if DCHECK_IS_ON()
void CSSParserLocalContext::CheckPercentagesFlagSetOnProperty() const {
  if (InFunctionContext() || !unresolved_property_name_.has_value() ||
      unresolved_property_name_->IsCustomProperty() ||
      unresolved_property_name_->Id() != CSSPropertyID::kInvalid) {
    return;
  }
  CSSProperty property =
      CSSProperty::Get(ResolveCSSPropertyID(unresolved_property_name_->Id()));
  DCHECK(property.PercentagesDependOnUsedValue() ||
         property.PercentagesDoNotDependOnUsedValue());
}
#endif

const AtomicString CSSParserLocalContext::PropertyNameAndRandomCount() const {
  StringBuilder str;
  if (unresolved_property_name_.has_value() &&
      unresolved_property_name_->Id() != CSSPropertyID::kInvalid) {
    // Use string of form "PROPERTY {property_name} {property_value_index}"
    // as name, this is later used for caching random values [0]. The prefix
    // "PROPERTY" is needed since we need to make distinguish between custom
    // property name and random value identifier, i.e. <dashed-ident> value in
    // <random-value-sharing> [1]
    // [0] https://drafts.csswg.org/css-values-5/#random-caching-key
    // [1] https://drafts.csswg.org/css-values-5/#typedef-random-value-sharing
    str.Append("PROPERTY ");
    if (custom_function_name_) {
      str.Append(custom_function_name_);
      str.Append(" ");
    }
    CSSPropertyName resolved_property_name = *unresolved_property_name_;
    if (current_shorthand_ != CSSPropertyID::kInvalid) {
      resolved_property_name = CSSPropertyName(current_shorthand_);
    } else if (!unresolved_property_name_->IsCustomProperty()) {
      resolved_property_name = CSSPropertyName(
          ResolveCSSPropertyID(unresolved_property_name_->Id()));
    }
    str.Append(resolved_property_name.ToAtomicString());
    str.Append(" ");
    str.AppendNumber(random_value_count_);
  }
  return str.ToAtomicString();
}

}  // namespace blink
