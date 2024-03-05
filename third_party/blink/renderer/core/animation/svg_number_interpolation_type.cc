// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_number_interpolation_type.h"

#include <memory>

#include "third_party/blink/renderer/core/animation/interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/svg/properties/svg_animated_property.h"
#include "third_party/blink/renderer/core/svg/svg_number.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

SVGPropertyBase* SVGNumberInterpolationType::AppliedSVGValueForTesting(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value) const {
  return AppliedSVGValue(interpolable_value, non_interpolable_value);
}

InterpolationValue SVGNumberInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(0));
}

InterpolationValue SVGNumberInterpolationType::MaybeConvertSVGValue(
    const SVGPropertyBase& svg_value) const {
  if (svg_value.GetType() != kAnimatedNumber) {
    return nullptr;
  }
  return InterpolationValue(MakeGarbageCollected<InterpolableNumber>(
      To<SVGNumber>(svg_value).Value()));
}

SVGPropertyBase* SVGNumberInterpolationType::AppliedSVGValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*) const {
  // Note: using default CSSToLengthConversionData here as it's
  // guaranteed to be a double.
  // TODO(crbug.com/325821290): Avoid InterpolableNumber here.
  float value = ClampTo<float>(To<InterpolableNumber>(interpolable_value)
                                   .Value(CSSToLengthConversionData()));
  return MakeGarbageCollected<SVGNumber>(is_non_negative_ && value < 0 ? 0
                                                                       : value);
}

}  // namespace blink
