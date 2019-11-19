// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_default_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/css_interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
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
    const InterpolationEnvironment& environment,
    const InterpolationValue&,
    ConversionCheckers&) const {
  const CSSValue* css_value = ToCSSPropertySpecificKeyframe(keyframe).Value();

  if (!css_value) {
    DCHECK(keyframe.IsNeutral());
    return nullptr;
  }

  if (RuntimeEnabledFeatures::CSSCascadeEnabled()) {
    css_value = ToCSSInterpolationEnvironment(environment)
                    .Resolve(GetProperty(), css_value);
    if (!css_value)
      return nullptr;
  }

  return InterpolationValue(std::make_unique<InterpolableList>(0),
                            CSSDefaultNonInterpolableValue::Create(css_value));
}

void CSSDefaultInterpolationType::Apply(
    const InterpolableValue&,
    const NonInterpolableValue* non_interpolable_value,
    InterpolationEnvironment& environment) const {
  DCHECK(ToCSSDefaultNonInterpolableValue(non_interpolable_value)->CssValue());
  StyleBuilder::ApplyProperty(
      GetProperty().GetCSSPropertyName(),
      ToCSSInterpolationEnvironment(environment).GetState(),
      *ToCSSDefaultNonInterpolableValue(non_interpolable_value)->CssValue());
}

}  // namespace blink
