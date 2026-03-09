// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"

#include "base/notreached.h"
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
    switch (current_function_id) {
      case CSSValueID::kCrossFade:
      case CSSValueID::kWebkitCrossFade:
      case CSSValueID::kConicGradient:
      case CSSValueID::kRadialGradient:
      case CSSValueID::kWebkitRadialGradient:
      case CSSValueID::kWebkitGradient:
      case CSSValueID::kInset:
      case CSSValueID::kXywh:
      case CSSValueID::kRect:
      case CSSValueID::kCircle:
      case CSSValueID::kEllipse:
      case CSSValueID::kPolygon:
      case CSSValueID::kShape:
      case CSSValueID::kScale3d:
      case CSSValueID::kScaleZ:
      case CSSValueID::kTranslate:
      case CSSValueID::kTranslateX:
      case CSSValueID::kTranslateY:
      case CSSValueID::kTranslate3d:
      case CSSValueID::kRepeat:
      case CSSValueID::kRay:
      case CSSValueID::kView:
        return true;
      case CSSValueID::kBlur:
      case CSSValueID::kBrightness:
      case CSSValueID::kColor:
      case CSSValueID::kColorMix:
      case CSSValueID::kColorStop:
      case CSSValueID::kContrast:
      case CSSValueID::kDropShadow:
      case CSSValueID::kDynamicRangeLimitMix:
      case CSSValueID::kGrayscale:
      case CSSValueID::kHsl:
      case CSSValueID::kHsla:
      case CSSValueID::kHueRotate:
      case CSSValueID::kHwb:
      case CSSValueID::kInvert:
      case CSSValueID::kLab:
      case CSSValueID::kLch:
      case CSSValueID::kLinear:
      case CSSValueID::kMatrix:
      case CSSValueID::kMatrix3d:
      case CSSValueID::kOklab:
      case CSSValueID::kOklch:
      case CSSValueID::kOpacity:
      case CSSValueID::kPaletteMix:
      case CSSValueID::kPath:
      case CSSValueID::kPerspective:
      case CSSValueID::kRgb:
      case CSSValueID::kRgba:
      case CSSValueID::kRotate:
      case CSSValueID::kRotate3d:
      case CSSValueID::kRotateX:
      case CSSValueID::kRotateY:
      case CSSValueID::kRotateZ:
      case CSSValueID::kSaturate:
      case CSSValueID::kScale:
      case CSSValueID::kScaleX:
      case CSSValueID::kScaleY:
      case CSSValueID::kSepia:
      case CSSValueID::kSkew:
      case CSSValueID::kSkewX:
      case CSSValueID::kSkewY:
      case CSSValueID::kTranslateZ:
        return false;
      default:
        NOTREACHED();
    }
  }
  CSSProperty property =
      CSSProperty::Get(ResolveCSSPropertyID(unresolved_property_name_->Id()));
  return property.PercentagesDependOnUsedValue();
}

#if DCHECK_IS_ON()
void CSSParserLocalContext::CheckPercentagesFlagSetOnProperty() const {
  // Early exit if the property context is a shorthand. While this context
  // should ideally be a longhand, some shorthands with custom expansion logic
  // skip generic helpers that update the context. Since percentage dependency
  // flags are only defined on longhands, we skip the check in this case.
  if (InFunctionContext() || !unresolved_property_name_.has_value() ||
      unresolved_property_name_->IsCustomProperty() ||
      unresolved_property_name_->Id() == CSSPropertyID::kInvalid ||
      ResolveCSSPropertyID(unresolved_property_name_->Id()) ==
          current_shorthand_) {
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
