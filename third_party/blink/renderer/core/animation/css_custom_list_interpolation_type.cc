// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_custom_list_interpolation_type.h"

#include "third_party/blink/renderer/core/animation/interpolable_length.h"
#include "third_party/blink/renderer/core/animation/underlying_length_checker.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

InterpolationValue CSSCustomListInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  wtf_size_t underlying_length =
      UnderlyingLengthChecker::GetUnderlyingLength(underlying);
  conversion_checkers.push_back(
      MakeGarbageCollected<UnderlyingLengthChecker>(underlying_length));

  if (underlying_length == 0)
    return nullptr;

  InterpolationValue null_underlying(nullptr);
  ConversionCheckers null_checkers;

  auto convert_inner = [this, &null_underlying, &null_checkers](size_t) {
    return inner_interpolation_type_->MaybeConvertNeutral(null_underlying,
                                                          null_checkers);
  };

  return ListInterpolationFunctions::CreateList(underlying_length,
                                                convert_inner);
}

InterpolationValue CSSCustomListInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState* state,
    ConversionCheckers&) const {
  const auto* list = DynamicTo<CSSValueList>(value);
  if (!list)
    return nullptr;

  ConversionCheckers null_checkers;

  return ListInterpolationFunctions::CreateList(
      list->length(), [this, list, state, &null_checkers](wtf_size_t index) {
        return inner_interpolation_type_->MaybeConvertValue(
            list->Item(index), state, null_checkers);
      });
}

const CSSValue* CSSCustomListInterpolationType::CreateCSSValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    const StyleResolverState& state) const {
  const auto& interpolable_list = To<InterpolableList>(interpolable_value);
  const auto* non_interpolable_list =
      DynamicTo<NonInterpolableList>(*non_interpolable_value);

  CSSValueList* list = nullptr;

  switch (syntax_repeat_) {
    default:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case CSSSyntaxRepeat::kSpaceSeparated:
      list = CSSValueList::CreateSpaceSeparated();
      break;
    case CSSSyntaxRepeat::kCommaSeparated:
      list = CSSValueList::CreateCommaSeparated();
      break;
  }

  DCHECK(!non_interpolable_list ||
         interpolable_list.length() == non_interpolable_list->length());

  for (wtf_size_t i = 0; i < interpolable_list.length(); ++i) {
    const NonInterpolableValue* non_interpolable_single_value =
        non_interpolable_list ? non_interpolable_list->Get(i) : nullptr;
    list->Append(*inner_interpolation_type_->CreateCSSValue(
        *interpolable_list.Get(i), non_interpolable_single_value, state));
  }

  return list;
}

void CSSCustomListInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  // This adapts a ListInterpolationFunctions::CompositeItemCallback function
  // such that we can use the InterpolationType::Composite function of the
  // inner interpolation type to get the answer.
  //
  // TODO(andruud): Make InterpolationType::Composite take an UnderlyingValue
  // rather than an UnderlyingValueOwner.
  const CSSInterpolationType* interpolation_type =
      inner_interpolation_type_.get();
  auto composite_callback =
      [interpolation_type, interpolation_fraction](
          UnderlyingValue& underlying_value, double underlying_fraction,
          const InterpolableValue& interpolable_value,
          const NonInterpolableValue* non_interpolable_value) {
        UnderlyingValueOwner owner;
        owner.Set(*interpolation_type,
                  InterpolationValue(
                      underlying_value.MutableInterpolableValue().Clone(),
                      underlying_value.GetNonInterpolableValue()));

        InterpolationValue interpolation_value(interpolable_value.Clone(),
                                               non_interpolable_value);
        interpolation_type->Composite(owner, underlying_fraction,
                                      interpolation_value,
                                      interpolation_fraction);

        underlying_value.SetInterpolableValue(
            owner.Value().Clone().interpolable_value);
        underlying_value.SetNonInterpolableValue(
            owner.GetNonInterpolableValue());
      };

  ListInterpolationFunctions::Composite(
      underlying_value_owner, underlying_fraction, *this, value,
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      ListInterpolationFunctions::InterpolableValuesKnownCompatible,
      NonInterpolableValuesAreCompatible, composite_callback);
}

PairwiseInterpolationValue CSSCustomListInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  const CSSInterpolationType* interpolation_type =
      inner_interpolation_type_.get();
  return ListInterpolationFunctions::MaybeMergeSingles(
      std::move(start), std::move(end),
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      [interpolation_type](InterpolationValue&& a, InterpolationValue&& b) {
        return interpolation_type->MaybeMergeSingles(std::move(a),
                                                     std::move(b));
      });
}

bool CSSCustomListInterpolationType::NonInterpolableValuesAreCompatible(
    const NonInterpolableValue* a,
    const NonInterpolableValue* b) {
  // TODO(https://crbug.com/981537): Add support for <image> here.
  // TODO(https://crbug.com/981538): Add support for <transform-function> here.
  // TODO(https://crbug.com/981542): Add support for <transform-list> here.
  return ListInterpolationFunctions::VerifyNoNonInterpolableValues(a, b);
}

}  // namespace blink
