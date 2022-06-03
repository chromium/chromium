// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/svg_number_list_interpolation_type.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/animation/interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/underlying_length_checker.h"
#include "third_party/blink/renderer/core/svg/svg_number_list.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

InterpolationValue SVGNumberListInterpolationType::MaybeConvertNeutral(
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
    result->Set(i, std::make_unique<InterpolableNumber>(0));
  return InterpolationValue(std::move(result));
}

InterpolationValue SVGNumberListInterpolationType::MaybeConvertSVGValue(
    const SVGPropertyBase& svg_value) const {
  if (svg_value.GetType() != kAnimatedNumberList)
    return nullptr;

  const SVGNumberList& number_list = To<SVGNumberList>(svg_value);
  auto result = std::make_unique<InterpolableList>(number_list.length());
  for (wtf_size_t i = 0; i < number_list.length(); i++) {
    result->Set(
        i, std::make_unique<InterpolableNumber>(number_list.at(i)->Value()));
  }
  return InterpolationValue(std::move(result));
}

PairwiseInterpolationValue SVGNumberListInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  size_t start_length =
      To<InterpolableList>(*start.interpolable_value).length();
  size_t end_length = To<InterpolableList>(*end.interpolable_value).length();
  if (start_length != end_length)
    return nullptr;
  return InterpolationType::MaybeMergeSingles(std::move(start), std::move(end));
}

static void PadWithZeroes(std::unique_ptr<InterpolableValue>& list_pointer,
                          wtf_size_t padded_length) {
  auto& list = To<InterpolableList>(*list_pointer);

  if (list.length() >= padded_length)
    return;

  auto result = std::make_unique<InterpolableList>(padded_length);
  wtf_size_t i = 0;
  for (; i < list.length(); i++)
    result->Set(i, std::move(list.GetMutable(i)));
  for (; i < padded_length; i++)
    result->Set(i, std::make_unique<InterpolableNumber>(0));
  list_pointer = std::move(result);
}

void SVGNumberListInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  const auto& list = To<InterpolableList>(*value.interpolable_value);

  if (To<InterpolableList>(*underlying_value_owner.Value().interpolable_value)
          .length() <= list.length())
    PadWithZeroes(underlying_value_owner.MutableValue().interpolable_value,
                  list.length());

  auto& underlying_list = To<InterpolableList>(
      *underlying_value_owner.MutableValue().interpolable_value);

  DCHECK_GE(underlying_list.length(), list.length());
  wtf_size_t i = 0;
  for (; i < list.length(); i++)
    underlying_list.GetMutable(i)->ScaleAndAdd(underlying_fraction,
                                               *list.Get(i));
  for (; i < underlying_list.length(); i++)
    underlying_list.GetMutable(i)->Scale(underlying_fraction);
}

SVGPropertyBase* SVGNumberListInterpolationType::AppliedSVGValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue*) const {
  auto* result = MakeGarbageCollected<SVGNumberList>();
  const auto& list = To<InterpolableList>(interpolable_value);
  for (wtf_size_t i = 0; i < list.length(); i++) {
    result->Append(MakeGarbageCollected<SVGNumber>(
        To<InterpolableNumber>(list.Get(i))->Value()));
  }
  return result;
}

}  // namespace blink
