// Copyright 2018 The Chromium Authors. All rights reserved.
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
  size_t underlying_length =
      UnderlyingLengthChecker::GetUnderlyingLength(underlying);
  conversion_checkers.push_back(
      std::make_unique<UnderlyingLengthChecker>(underlying_length));

  if (underlying_length == 0)
    return nullptr;

  InterpolationValue null_underlying(nullptr);
  ConversionCheckers null_checkers;

  auto convert_inner = [this, &null_underlying, &null_checkers](size_t) {
    return this->inner_interpolation_type_->MaybeConvertNeutral(null_underlying,
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
      list->length(), [this, list, state, &null_checkers](size_t index) {
        return this->inner_interpolation_type_->MaybeConvertValue(
            list->Item(index), state, null_checkers);
      });
}

const CSSValue* CSSCustomListInterpolationType::CreateCSSValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    const StyleResolverState& state) const {
  const InterpolableList& interpolable_list =
      ToInterpolableList(interpolable_value);
  const NonInterpolableList* non_interpolable_list =
      non_interpolable_value ? &ToNonInterpolableList(*non_interpolable_value)
                             : nullptr;

  CSSValueList* list = nullptr;

  switch (syntax_repeat_) {
    default:
      NOTREACHED();
      FALLTHROUGH;
    case CSSSyntaxRepeat::kSpaceSeparated:
      list = CSSValueList::CreateSpaceSeparated();
      break;
    case CSSSyntaxRepeat::kCommaSeparated:
      list = CSSValueList::CreateCommaSeparated();
      break;
  }

  DCHECK(!non_interpolable_list ||
         interpolable_list.length() == non_interpolable_list->length());

  for (size_t i = 0; i < interpolable_list.length(); ++i) {
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
  auto composite_callback =
      [](const CSSInterpolationType* interpolation_type,
         double interpolation_fraction, UnderlyingValue& underlying_value,
         double underlying_fraction,
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
      WTF::BindRepeating(
          ListInterpolationFunctions::InterpolableValuesKnownCompatible),
      GetNonInterpolableValuesAreCompatibleCallback(),
      WTF::BindRepeating(composite_callback,
                         WTF::Unretained(inner_interpolation_type_.get()),
                         interpolation_fraction));
}

PairwiseInterpolationValue CSSCustomListInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  return ListInterpolationFunctions::MaybeMergeSingles(
      std::move(start), std::move(end),
      ListInterpolationFunctions::LengthMatchingStrategy::kEqual,
      WTF::BindRepeating(&CSSInterpolationType::MaybeMergeSingles,
                         WTF::Unretained(inner_interpolation_type_.get())));
}

ListInterpolationFunctions::NonInterpolableValuesAreCompatibleCallback
CSSCustomListInterpolationType::GetNonInterpolableValuesAreCompatibleCallback()
    const {
  // TODO(https://crbug.com/981537): Add support for <image> here.
  // TODO(https://crbug.com/981538): Add support for <transform-function> here.
  // TODO(https://crbug.com/981542): Add support for <transform-list> here.
  return WTF::BindRepeating(
      ListInterpolationFunctions::VerifyNoNonInterpolableValues);
}

}  // namespace blink
