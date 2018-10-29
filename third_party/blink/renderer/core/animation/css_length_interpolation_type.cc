// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_length_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/css/css_animatable_value_factory.h"
#include "third_party/blink/renderer/core/animation/length_interpolation_functions.h"
#include "third_party/blink/renderer/core/animation/length_property_functions.h"
#include "third_party/blink/renderer/core/css/css_calculation_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

CSSLengthInterpolationType::CSSLengthInterpolationType(
    PropertyHandle property,
    const PropertyRegistration* registration)
    : CSSInterpolationType(property, registration),
      value_range_(LengthPropertyFunctions::GetValueRange(CssProperty())) {}

float CSSLengthInterpolationType::EffectiveZoom(
    const ComputedStyle& style) const {
  return LengthPropertyFunctions::IsZoomedLength(CssProperty())
             ? style.EffectiveZoom()
             : 1;
}

class InheritedLengthChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  static std::unique_ptr<InheritedLengthChecker> Create(
      const CSSProperty& property,
      const Length& length) {
    return base::WrapUnique(new InheritedLengthChecker(property, length));
  }

 private:
  InheritedLengthChecker(const CSSProperty& property, const Length& length)
      : property_(property), length_(length) {}

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    Length parent_length;
    LengthPropertyFunctions::GetLength(property_, *state.ParentStyle(),
                                       parent_length);
    return parent_length == length_;
  }

  const CSSProperty& property_;
  const Length length_;
};

InterpolationValue CSSLengthInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(
      LengthInterpolationFunctions::CreateNeutralInterpolableValue());
}

InterpolationValue CSSLengthInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversion_checkers) const {
  Length initial_length;
  if (!LengthPropertyFunctions::GetInitialLength(CssProperty(), initial_length))
    return nullptr;
  return LengthInterpolationFunctions::MaybeConvertLength(initial_length, 1);
}

InterpolationValue CSSLengthInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle())
    return nullptr;
  Length inherited_length;
  LengthPropertyFunctions::GetLength(CssProperty(), *state.ParentStyle(),
                                     inherited_length);
  conversion_checkers.push_back(
      InheritedLengthChecker::Create(CssProperty(), inherited_length));
  if (inherited_length.IsAuto()) {
    // If the inherited value changes to a length, the InheritedLengthChecker
    // will invalidate the interpolation's cache.
    return nullptr;
  }
  return LengthInterpolationFunctions::MaybeConvertLength(
      inherited_length, EffectiveZoom(*state.ParentStyle()));
}

InterpolationValue CSSLengthInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers& conversion_checkers) const {
  if (value.IsIdentifierValue()) {
    CSSValueID value_id = ToCSSIdentifierValue(value).GetValueID();
    double pixels;
    if (!LengthPropertyFunctions::GetPixelsForKeyword(CssProperty(), value_id,
                                                      pixels))
      return nullptr;
    return InterpolationValue(
        LengthInterpolationFunctions::CreateInterpolablePixels(pixels));
  }

  return LengthInterpolationFunctions::MaybeConvertCSSValue(value);
}

PairwiseInterpolationValue CSSLengthInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  return LengthInterpolationFunctions::MergeSingles(std::move(start),
                                                    std::move(end));
}

InterpolationValue
CSSLengthInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  Length underlying_length;
  if (!LengthPropertyFunctions::GetLength(CssProperty(), style,
                                          underlying_length))
    return nullptr;
  return LengthInterpolationFunctions::MaybeConvertLength(underlying_length,
                                                          EffectiveZoom(style));
}

const CSSValue* CSSLengthInterpolationType::CreateCSSValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    const StyleResolverState&) const {
  return LengthInterpolationFunctions::CreateCSSValue(
      interpolable_value, non_interpolable_value, value_range_);
}

void CSSLengthInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  InterpolationValue& underlying = underlying_value_owner.MutableValue();
  LengthInterpolationFunctions::Composite(
      underlying.interpolable_value, underlying.non_interpolable_value,
      underlying_fraction, *value.interpolable_value,
      value.non_interpolable_value.get());
}

void CSSLengthInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  ComputedStyle& style = *state.Style();
  float zoom = EffectiveZoom(style);
  CSSToLengthConversionData conversion_data = state.CssToLengthConversionData();
  conversion_data.SetZoom(zoom);
  Length length = LengthInterpolationFunctions::CreateLength(
      interpolable_value, non_interpolable_value, conversion_data,
      value_range_);
  if (LengthPropertyFunctions::SetLength(CssProperty(), style, length)) {
#if DCHECK_IS_ON()
    // Assert that setting the length on ComputedStyle directly is identical to
    // the StyleBuilder code path. This check is useful for catching differences
    // in clamping behaviour.
    Length before;
    Length after;
    DCHECK(LengthPropertyFunctions::GetLength(CssProperty(), style, before));
    StyleBuilder::ApplyProperty(GetProperty().GetCSSProperty(), state,
                                *CSSValue::Create(length, zoom));
    DCHECK(LengthPropertyFunctions::GetLength(CssProperty(), style, after));
    DCHECK(before.IsSpecified());
    DCHECK(after.IsSpecified());
    const float kSlack = 0.1;
    float delta =
        FloatValueForLength(after, 100) - FloatValueForLength(before, 100);
    DCHECK_LT(std::abs(delta), kSlack);
#endif
    return;
  }
  StyleBuilder::ApplyProperty(GetProperty().GetCSSProperty(), state,
                              *CSSValue::Create(length, zoom));
}

}  // namespace blink
