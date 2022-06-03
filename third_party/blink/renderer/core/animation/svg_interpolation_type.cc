// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/animation/svg_interpolation_environment.h"
#include "third_party/blink/renderer/core/svg/properties/svg_property.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"

namespace blink {

InterpolationValue SVGInterpolationType::MaybeConvertSingle(
    const PropertySpecificKeyframe& keyframe,
    const InterpolationEnvironment& environment,
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  if (keyframe.IsNeutral())
    return MaybeConvertNeutral(underlying, conversion_checkers);

  auto* svg_value =
      To<SVGInterpolationEnvironment>(environment)
          .SvgBaseValue()
          .CloneForAnimation(To<SVGPropertySpecificKeyframe>(keyframe).Value());
  return MaybeConvertSVGValue(*svg_value);
}

InterpolationValue SVGInterpolationType::MaybeConvertUnderlyingValue(
    const InterpolationEnvironment& environment) const {
  return MaybeConvertSVGValue(
      To<SVGInterpolationEnvironment>(environment).SvgBaseValue());
}

void SVGInterpolationType::Apply(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    InterpolationEnvironment& environment) const {
  To<SVGInterpolationEnvironment>(environment)
      .SvgElement()
      .SetWebAnimatedAttribute(
          Attribute(),
          AppliedSVGValue(interpolable_value, non_interpolable_value));
}

}  // namespace blink
