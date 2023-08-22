// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_length_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/length_property_functions.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

CSSLengthInterpolationType::CSSLengthInterpolationType(
    PropertyHandle property,
    const PropertyRegistration* registration)
    : CSSInterpolationType(property, registration),
      value_range_(LengthPropertyFunctions::GetValueRange(CssProperty())),
      is_zoomed_length_(
          LengthPropertyFunctions::IsZoomedLength(CssProperty())) {}

class InheritedLengthChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedLengthChecker(const CSSProperty& property, const Length& length)
      : property_(property), length_(length) {}

 private:
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
  return InterpolationValue(InterpolableLength::CreateNeutral());
}

InterpolationValue CSSLengthInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  Length initial_length;
  if (!LengthPropertyFunctions::GetInitialLength(
          CssProperty(), state.GetDocument().GetStyleResolver().InitialStyle(),
          initial_length))
    return nullptr;
  return InterpolationValue(
      InterpolableLength::MaybeConvertLength(initial_length, 1));
}

InterpolationValue CSSLengthInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle())
    return nullptr;
  Length inherited_length;
  LengthPropertyFunctions::GetLength(CssProperty(), *state.ParentStyle(),
                                     inherited_length);
  conversion_checkers.push_back(std::make_unique<InheritedLengthChecker>(
      CssProperty(), inherited_length));
  if (inherited_length.IsAuto()) {
    // If the inherited value changes to a length, the InheritedLengthChecker
    // will invalidate the interpolation's cache.
    return nullptr;
  }
  return InterpolationValue(InterpolableLength::MaybeConvertLength(
      inherited_length, EffectiveZoom(state.ParentStyle()->EffectiveZoom())));
}

InterpolationValue CSSLengthInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers& conversion_checkers) const {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    CSSValueID value_id = identifier_value->GetValueID();
    double pixels;
    if (!LengthPropertyFunctions::GetPixelsForKeyword(CssProperty(), value_id,
                                                      pixels))
      return nullptr;
    return InterpolationValue(InterpolableLength::CreatePixels(pixels));
  }

  return InterpolationValue(InterpolableLength::MaybeConvertCSSValue(value));
}

PairwiseInterpolationValue CSSLengthInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  return InterpolableLength::MergeSingles(std::move(start.interpolable_value),
                                          std::move(end.interpolable_value));
}

InterpolationValue
CSSLengthInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  Length underlying_length;
  if (!LengthPropertyFunctions::GetLength(CssProperty(), style,
                                          underlying_length))
    return nullptr;
  return InterpolationValue(InterpolableLength::MaybeConvertLength(
      underlying_length, EffectiveZoom(style.EffectiveZoom())));
}

const CSSValue* CSSLengthInterpolationType::CreateCSSValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*,
    const StyleResolverState&) const {
  return To<InterpolableLength>(interpolable_value)
      .CreateCSSValue(value_range_);
}

void CSSLengthInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  ComputedStyleBuilder& builder = state.StyleBuilder();
  float zoom = EffectiveZoom(builder.EffectiveZoom());
  CSSToLengthConversionData conversion_data =
      state.CssToLengthConversionData().CopyWithAdjustedZoom(zoom);
  Length length = To<InterpolableLength>(interpolable_value)
                      .CreateLength(conversion_data, value_range_);
  if (LengthPropertyFunctions::SetLength(CssProperty(), builder, length)) {
#if DCHECK_IS_ON()
    const ComputedStyle* before_style = builder.CloneStyle();
    // Assert that setting the length on ComputedStyle directly is identical to
    // the StyleBuilder code path. This check is useful for catching differences
    // in clamping behavior.
    Length before;
    Length after;
    DCHECK(LengthPropertyFunctions::GetLength(CssProperty(), *before_style,
                                              before));
    StyleBuilder::ApplyProperty(GetProperty().GetCSSProperty(), state,
                                *CSSValue::Create(length, zoom));
    const ComputedStyle* after_style = builder.CloneStyle();
    DCHECK(
        LengthPropertyFunctions::GetLength(CssProperty(), *after_style, after));
    DCHECK(before.IsSpecified());
    DCHECK(after.IsSpecified());
    // A relative error of 1/100th of a percent is likely not noticeable.
    // This check can be triggered with a tight tolerance such as 1e-6 for
    // suitably ill-conditioned animations (crbug.com/1204099).
    const float kSlack = 0.0001;
    const float before_length = FloatValueForLength(before, 100);
    const float after_length = FloatValueForLength(after, 100);
    // Length values may be constructed from integers, floating point values, or
    // layout units (64ths of a pixel).  If converted from a layout unit, any
    /// value greater than max_int64 / 64 cannot be precisely expressed
    // (crbug.com/1349686).
    if (std::isfinite(before_length) && std::isfinite(after_length) &&
        std::abs(before_length) < kIntMaxForLayoutUnit) {
      // Test relative difference for large values to avoid floating point
      // inaccuracies tripping the check.
      const float delta = std::abs(before_length) < kSlack
                              ? after_length - before_length
                              : (after_length - before_length) / before_length;
      DCHECK_LT(std::abs(delta), kSlack);
    }
#endif
    return;
  }
  StyleBuilder::ApplyProperty(GetProperty().GetCSSProperty(), state,
                              *CSSValue::Create(length, zoom));
}

}  // namespace blink
