// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_default_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/css_interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/animation/underlying_value_owner.h"
#include "third_party/blink/renderer/core/css/css_unset_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"

namespace blink {

CSSDefaultNonInterpolableValue::CSSDefaultNonInterpolableValue(
    const CSSValue* css_value)
    : css_value_(css_value) {
  DCHECK(css_value_);
}

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSDefaultNonInterpolableValue);

InterpolationValue CSSDefaultInterpolationType::MaybeConvertSingle(
    const PropertySpecificKeyframe& keyframe,
    const CSSInterpolationEnvironment& environment,
    const InterpolationValue&,
    ConversionCheckers&) const {
  const auto& property_specific = To<CSSPropertySpecificKeyframe>(keyframe);
  const CSSValue* css_value = property_specific.Value();
  const TreeScope* tree_scope = property_specific.GetTreeScope();

  if (!css_value) {
    DCHECK(keyframe.IsNeutral());
    return nullptr;
  }

  css_value = environment.Resolve(GetProperty(), css_value, tree_scope);
  if (!css_value) {
    // Custom property cycle. CSSDefaultInterpolationType *must* succeed
    // at creating a value (for non-neutral keyframes), since this
    // interpolation type is the "last resort". To stay consistent with
    // handling in CSSVarCycleInterpolationType, we use "unset" for cycles.
    // We should likely be using CSSInvalidVariableValue here instead,
    // although the correct behavior isn't actually specified.
    //
    // TODO(crbug.com/40753334): Figure out the correct behavior.
    css_value = cssvalue::CSSUnsetValue::Create();
  }

  return InterpolationValue(
      MakeGarbageCollected<InterpolableList>(0),
      MakeGarbageCollected<CSSDefaultNonInterpolableValue>(css_value));
}

void CSSDefaultInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  underlying_value_owner.Set(this, value);
}

void CSSDefaultInterpolationType::Apply(
    const InterpolableValue&,
    const NonInterpolableValue* non_interpolable_value,
    CSSInterpolationEnvironment& environment) const {
  DCHECK(
      To<CSSDefaultNonInterpolableValue>(non_interpolable_value)->CssValue());
  StyleBuilder::ApplyProperty(
      GetProperty().GetCSSPropertyName(), environment.GetState(),
      *To<CSSDefaultNonInterpolableValue>(non_interpolable_value)->CssValue());
}

}  // namespace blink
