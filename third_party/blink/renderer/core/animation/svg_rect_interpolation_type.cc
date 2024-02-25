// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/animation/svg_rect_interpolation_type.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/animation/interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/svg/svg_rect.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

enum RectComponentIndex : unsigned {
  kRectX,
  kRectY,
  kRectWidth,
  kRectHeight,
  kRectComponentIndexCount,
};

InterpolationValue SVGRectInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  auto* result =
      MakeGarbageCollected<InterpolableList>(kRectComponentIndexCount);
  for (wtf_size_t i = 0; i < kRectComponentIndexCount; i++)
    result->Set(i, MakeGarbageCollected<InterpolableNumber>(0));
  return InterpolationValue(result);
}

InterpolationValue SVGRectInterpolationType::MaybeConvertSVGValue(
    const SVGPropertyBase& svg_value) const {
  if (svg_value.GetType() != kAnimatedRect)
    return nullptr;

  const auto& rect = To<SVGRect>(svg_value);
  auto* result =
      MakeGarbageCollected<InterpolableList>(kRectComponentIndexCount);
  result->Set(kRectX, MakeGarbageCollected<InterpolableNumber>(rect.X()));
  result->Set(kRectY, MakeGarbageCollected<InterpolableNumber>(rect.Y()));
  result->Set(kRectWidth,
              MakeGarbageCollected<InterpolableNumber>(rect.Width()));
  result->Set(kRectHeight,
              MakeGarbageCollected<InterpolableNumber>(rect.Height()));
  return InterpolationValue(result);
}

SVGPropertyBase* SVGRectInterpolationType::AppliedSVGValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*) const {
  const auto& list = To<InterpolableList>(interpolable_value);
  auto* result = MakeGarbageCollected<SVGRect>();
  result->SetX(To<InterpolableNumber>(list.Get(kRectX))->Value());
  result->SetY(To<InterpolableNumber>(list.Get(kRectY))->Value());
  result->SetWidth(To<InterpolableNumber>(list.Get(kRectWidth))->Value());
  result->SetHeight(To<InterpolableNumber>(list.Get(kRectHeight))->Value());
  return result;
}

}  // namespace blink
