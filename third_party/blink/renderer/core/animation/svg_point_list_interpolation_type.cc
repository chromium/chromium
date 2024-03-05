// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_point_list_interpolation_type.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/animation/interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/animation/underlying_length_checker.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/svg/svg_point_list.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

InterpolationValue SVGPointListInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  wtf_size_t underlying_length =
      UnderlyingLengthChecker::GetUnderlyingLength(underlying);
  conversion_checkers.push_back(
      MakeGarbageCollected<UnderlyingLengthChecker>(underlying_length));

  if (underlying_length == 0)
    return nullptr;

  auto* result = MakeGarbageCollected<InterpolableList>(underlying_length);
  for (wtf_size_t i = 0; i < underlying_length; i++)
    result->Set(i, MakeGarbageCollected<InterpolableNumber>(0));
  return InterpolationValue(result);
}

InterpolationValue SVGPointListInterpolationType::MaybeConvertSVGValue(
    const SVGPropertyBase& svg_value) const {
  if (svg_value.GetType() != kAnimatedPoints)
    return nullptr;

  const auto& point_list = To<SVGPointList>(svg_value);
  auto* result =
      MakeGarbageCollected<InterpolableList>(point_list.length() * 2);
  for (wtf_size_t i = 0; i < point_list.length(); i++) {
    const SVGPoint& point = *point_list.at(i);
    result->Set(2 * i, MakeGarbageCollected<InterpolableNumber>(point.X()));
    result->Set(2 * i + 1, MakeGarbageCollected<InterpolableNumber>(point.Y()));
  }

  return InterpolationValue(result);
}

PairwiseInterpolationValue SVGPointListInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  wtf_size_t start_length =
      To<InterpolableList>(*start.interpolable_value).length();
  wtf_size_t end_length =
      To<InterpolableList>(*end.interpolable_value).length();
  if (start_length != end_length)
    return nullptr;

  return InterpolationType::MaybeMergeSingles(std::move(start), std::move(end));
}

void SVGPointListInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  wtf_size_t start_length =
      To<InterpolableList>(*underlying_value_owner.Value().interpolable_value)
          .length();
  wtf_size_t end_length =
      To<InterpolableList>(*value.interpolable_value).length();
  if (start_length == end_length)
    InterpolationType::Composite(underlying_value_owner, underlying_fraction,
                                 value, interpolation_fraction);
  else
    underlying_value_owner.Set(*this, value);
}

SVGPropertyBase* SVGPointListInterpolationType::AppliedSVGValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*) const {
  auto* result = MakeGarbageCollected<SVGPointList>();

  const auto& list = To<InterpolableList>(interpolable_value);
  DCHECK_EQ(list.length() % 2, 0U);
  // Note: using default CSSToLengthConversionData here as it's
  // guaranteed to be a double.
  // TODO(crbug.com/325821290): Avoid InterpolableNumber here.
  CSSToLengthConversionData length_resolver;
  for (wtf_size_t i = 0; i < list.length(); i += 2) {
    gfx::PointF point(
        To<InterpolableNumber>(list.Get(i))->Value(length_resolver),
        To<InterpolableNumber>(list.Get(i + 1))->Value(length_resolver));
    result->Append(MakeGarbageCollected<SVGPoint>(point));
  }

  return result;
}

}  // namespace blink
