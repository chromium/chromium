// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_length_list_interpolation_type.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/animation/svg_interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/svg_length_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/underlying_length_checker.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_list.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

InterpolationValue SVGLengthListInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  wtf_size_t underlying_length =
      UnderlyingLengthChecker::GetUnderlyingLength(underlying);
  conversion_checkers.push_back(
      std::make_unique<UnderlyingLengthChecker>(underlying_length));

  if (underlying_length == 0)
    return nullptr;

  auto result = std::make_unique<InterpolableList>(underlying_length);
  for (wtf_size_t i = 0; i < underlying_length; i++)
    result->Set(i, SVGLengthInterpolationType::NeutralInterpolableValue());
  return InterpolationValue(std::move(result));
}

InterpolationValue SVGLengthListInterpolationType::MaybeConvertSVGValue(
    const SVGPropertyBase& svg_value) const {
  if (svg_value.GetType() != kAnimatedLengthList)
    return nullptr;

  const SVGLengthList& length_list = ToSVGLengthList(svg_value);
  auto result = std::make_unique<InterpolableList>(length_list.length());
  for (wtf_size_t i = 0; i < length_list.length(); i++) {
    InterpolationValue component =
        SVGLengthInterpolationType::MaybeConvertSVGLength(*length_list.at(i));
    if (!component)
      return nullptr;
    result->Set(i, std::move(component.interpolable_value));
  }
  return InterpolationValue(std::move(result));
}

PairwiseInterpolationValue SVGLengthListInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  wtf_size_t start_length =
      ToInterpolableList(*start.interpolable_value).length();
  wtf_size_t end_length = ToInterpolableList(*end.interpolable_value).length();
  if (start_length != end_length)
    return nullptr;
  return InterpolationType::MaybeMergeSingles(std::move(start), std::move(end));
}

void SVGLengthListInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  wtf_size_t start_length =
      ToInterpolableList(*underlying_value_owner.Value().interpolable_value)
          .length();
  wtf_size_t end_length =
      ToInterpolableList(*value.interpolable_value).length();

  if (start_length == end_length)
    InterpolationType::Composite(underlying_value_owner, underlying_fraction,
                                 value, interpolation_fraction);
  else
    underlying_value_owner.Set(*this, value);
}

SVGPropertyBase* SVGLengthListInterpolationType::AppliedSVGValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*) const {
  NOTREACHED();
  // This function is no longer called, because apply has been overridden.
  return nullptr;
}

void SVGLengthListInterpolationType::Apply(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    InterpolationEnvironment& environment) const {
  SVGElement& element = ToSVGInterpolationEnvironment(environment).SvgElement();
  SVGLengthContext length_context(&element);

  auto* result = MakeGarbageCollected<SVGLengthList>(unit_mode_);
  const InterpolableList& list = ToInterpolableList(interpolable_value);
  for (wtf_size_t i = 0; i < list.length(); i++) {
    result->Append(SVGLengthInterpolationType::ResolveInterpolableSVGLength(
        *list.Get(i), length_context, unit_mode_, negative_values_forbidden_));
  }

  element.SetWebAnimatedAttribute(Attribute(), result);
}

}  // namespace blink
