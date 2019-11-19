// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_filter_list_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/interpolable_filter.h"
#include "third_party/blink/renderer/core/animation/list_interpolation_functions.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

const FilterOperations& GetFilterList(const CSSProperty& property,
                                      const ComputedStyle& style) {
  switch (property.PropertyID()) {
    default:
      NOTREACHED();
      FALLTHROUGH;
    case CSSPropertyID::kBackdropFilter:
      return style.BackdropFilter();
    case CSSPropertyID::kFilter:
      return style.Filter();
  }
}

void SetFilterList(const CSSProperty& property,
                   ComputedStyle& style,
                   const FilterOperations& filter_operations) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kBackdropFilter:
      style.SetBackdropFilter(filter_operations);
      break;
    case CSSPropertyID::kFilter:
      style.SetFilter(filter_operations);
      break;
    default:
      NOTREACHED();
      break;
  }
}

class UnderlyingFilterListChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  UnderlyingFilterListChecker(const InterpolableList* interpolable_list) {
    wtf_size_t length = interpolable_list->length();
    types_.ReserveInitialCapacity(length);
    for (wtf_size_t i = 0; i < length; i++) {
      types_.push_back(
          To<InterpolableFilter>(interpolable_list->Get(i))->GetType());
    }
  }

  bool IsValid(const StyleResolverState&,
               const InterpolationValue& underlying) const final {
    const InterpolableList& underlying_list =
        ToInterpolableList(*underlying.interpolable_value);
    if (underlying_list.length() != types_.size())
      return false;
    for (wtf_size_t i = 0; i < types_.size(); i++) {
      FilterOperation::OperationType other_type =
          To<InterpolableFilter>(underlying_list.Get(i))->GetType();
      if (types_[i] != other_type)
        return false;
    }
    return true;
  }

 private:
  Vector<FilterOperation::OperationType> types_;
};

class InheritedFilterListChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedFilterListChecker(const CSSProperty& property,
                             const FilterOperations& filter_operations)
      : property_(property),
        filter_operations_wrapper_(
            MakeGarbageCollected<FilterOperationsWrapper>(filter_operations)) {}

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    const FilterOperations& filter_operations =
        filter_operations_wrapper_->Operations();
    return filter_operations == GetFilterList(property_, *state.ParentStyle());
  }

 private:
  const CSSProperty& property_;
  Persistent<FilterOperationsWrapper> filter_operations_wrapper_;
};

InterpolationValue ConvertFilterList(const FilterOperations& filter_operations,
                                     double zoom) {
  wtf_size_t length = filter_operations.size();
  auto interpolable_list = std::make_unique<InterpolableList>(length);
  for (wtf_size_t i = 0; i < length; i++) {
    std::unique_ptr<InterpolableFilter> result =
        InterpolableFilter::MaybeCreate(*filter_operations.Operations()[i],
                                        zoom);
    if (!result)
      return nullptr;
    interpolable_list->Set(i, std::move(result));
  }
  return InterpolationValue(std::move(interpolable_list));
}

class AlwaysInvalidateChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return false;
  }
};
}  // namespace

InterpolationValue CSSFilterListInterpolationType::MaybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversion_checkers) const {
  const InterpolableList* interpolable_list =
      ToInterpolableList(underlying.interpolable_value.get());
  conversion_checkers.push_back(
      std::make_unique<UnderlyingFilterListChecker>(interpolable_list));
  // The neutral value for composition for a filter list is the empty list, as
  // the additive operator is concatenation, so concat(underlying, []) ==
  // underlying.
  return InterpolationValue(std::make_unique<InterpolableList>(0));
}

InterpolationValue CSSFilterListInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversion_checkers) const {
  return ConvertFilterList(
      GetFilterList(CssProperty(), ComputedStyle::InitialStyle()), 1);
}

InterpolationValue CSSFilterListInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const FilterOperations& inherited_filter_operations =
      GetFilterList(CssProperty(), *state.ParentStyle());
  conversion_checkers.push_back(std::make_unique<InheritedFilterListChecker>(
      CssProperty(), inherited_filter_operations));
  return ConvertFilterList(inherited_filter_operations,
                           state.Style()->EffectiveZoom());
}

InterpolationValue CSSFilterListInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState*,
    ConversionCheckers&) const {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value && identifier_value->GetValueID() == CSSValueID::kNone)
    return InterpolationValue(std::make_unique<InterpolableList>(0));

  if (!value.IsBaseValueList())
    return nullptr;

  const auto& list = To<CSSValueList>(value);
  wtf_size_t length = list.length();
  auto interpolable_list = std::make_unique<InterpolableList>(length);
  for (wtf_size_t i = 0; i < length; i++) {
    std::unique_ptr<InterpolableFilter> result =
        InterpolableFilter::MaybeConvertCSSValue(list.Item(i));
    if (!result)
      return nullptr;
    interpolable_list->Set(i, std::move(result));
  }
  return InterpolationValue(std::move(interpolable_list));
}

InterpolationValue
CSSFilterListInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  return ConvertFilterList(GetFilterList(CssProperty(), style),
                           style.EffectiveZoom());
}

PairwiseInterpolationValue CSSFilterListInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  InterpolableList& start_interpolable_list =
      ToInterpolableList(*start.interpolable_value);
  InterpolableList& end_interpolable_list =
      ToInterpolableList(*end.interpolable_value);
  wtf_size_t start_length = start_interpolable_list.length();
  wtf_size_t end_length = end_interpolable_list.length();

  for (wtf_size_t i = 0; i < start_length && i < end_length; i++) {
    if (To<InterpolableFilter>(start_interpolable_list.Get(i))->GetType() !=
        To<InterpolableFilter>(end_interpolable_list.Get(i))->GetType())
      return nullptr;
  }

  if (start_length == end_length) {
    return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                      std::move(end.interpolable_value));
  }

  // Extend the shorter InterpolableList with neutral values that are compatible
  // with corresponding filters in the longer list.
  InterpolationValue& shorter = start_length < end_length ? start : end;
  wtf_size_t shorter_length = std::min(start_length, end_length);
  wtf_size_t longer_length = std::max(start_length, end_length);
  InterpolableList& shorter_interpolable_list = start_length < end_length
                                                    ? start_interpolable_list
                                                    : end_interpolable_list;
  const InterpolableList& longer_interpolable_list =
      start_length < end_length ? end_interpolable_list
                                : start_interpolable_list;
  auto extended_interpolable_list =
      std::make_unique<InterpolableList>(longer_length);
  for (wtf_size_t i = 0; i < longer_length; i++) {
    if (i < shorter_length)
      extended_interpolable_list->Set(
          i, std::move(shorter_interpolable_list.GetMutable(i)));
    else
      extended_interpolable_list->Set(
          i, InterpolableFilter::CreateInitialValue(
                 To<InterpolableFilter>(longer_interpolable_list.Get(i))
                     ->GetType()));
  }
  shorter.interpolable_value = std::move(extended_interpolable_list);

  return PairwiseInterpolationValue(std::move(start.interpolable_value),
                                    std::move(end.interpolable_value));
}

void CSSFilterListInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  // We do our compositing behavior in |PreInterpolationCompositeIfNeeded|; see
  // the documentation on that method.
  underlying_value_owner.Set(*this, value);
}

void CSSFilterListInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  const InterpolableList& interpolable_list =
      ToInterpolableList(interpolable_value);
  wtf_size_t length = interpolable_list.length();

  FilterOperations filter_operations;
  filter_operations.Operations().ReserveCapacity(length);
  for (wtf_size_t i = 0; i < length; i++) {
    filter_operations.Operations().push_back(
        To<InterpolableFilter>(interpolable_list.Get(i))
            ->CreateFilterOperation(state));
  }
  SetFilterList(CssProperty(), *state.Style(), std::move(filter_operations));
}

InterpolationValue
CSSFilterListInterpolationType::PreInterpolationCompositeIfNeeded(
    InterpolationValue value,
    const InterpolationValue& underlying,
    EffectModel::CompositeOperation composite,
    ConversionCheckers& conversion_checkers) const {
  DCHECK(!value.non_interpolable_value);
  DCHECK(!underlying.non_interpolable_value);

  // Due to the post-interpolation composite optimization, the interpolation
  // stack aggressively caches interpolated values. When we are doing
  // pre-interpolation compositing, this can cause us to bake-in the composited
  // result even when the underlying value is changing. This checker is a hack
  // to disable that caching in this case.
  // TODO(crbug.com/1009230): Remove this once our interpolation code isn't
  // caching composited values.
  conversion_checkers.push_back(std::make_unique<AlwaysInvalidateChecker>());

  // The underlying value can be nullptr, most commonly if it contains a url().
  // TODO(crbug.com/1009229): Properly handle url() in filter composite.
  if (!underlying.interpolable_value)
    return nullptr;

  auto interpolable_list = std::unique_ptr<InterpolableList>(
      ToInterpolableList(value.interpolable_value.release()));
  const InterpolableList& underlying_list =
      ToInterpolableList(*underlying.interpolable_value);

  if (composite == EffectModel::CompositeOperation::kCompositeAdd) {
    return PerformAdditiveComposition(std::move(interpolable_list),
                                      underlying_list);
  }
  DCHECK_EQ(composite, EffectModel::CompositeOperation::kCompositeAccumulate);
  return PerformAccumulativeComposition(std::move(interpolable_list),
                                        underlying_list);
}

InterpolationValue CSSFilterListInterpolationType::PerformAdditiveComposition(
    std::unique_ptr<InterpolableList> interpolable_list,
    const InterpolableList& underlying_list) const {
  // Per the spec, addition of filter lists is defined as concatenation.
  // https://drafts.fxtf.org/filter-effects-1/#addition
  auto composited_list = std::make_unique<InterpolableList>(
      underlying_list.length() + interpolable_list->length());
  for (wtf_size_t i = 0; i < composited_list->length(); i++) {
    if (i < underlying_list.length()) {
      composited_list->Set(i, underlying_list.Get(i)->Clone());
    } else {
      composited_list->Set(
          i, interpolable_list->Get(i - underlying_list.length())->Clone());
    }
  }
  return InterpolationValue(std::move(composited_list));
}

InterpolationValue
CSSFilterListInterpolationType::PerformAccumulativeComposition(
    std::unique_ptr<InterpolableList> interpolable_list,
    const InterpolableList& underlying_list) const {
  // Per the spec, accumulation of filter lists operates on pairwise addition of
  // the underlying components.
  // https://drafts.fxtf.org/filter-effects-1/#accumulation
  wtf_size_t length = interpolable_list->length();
  wtf_size_t underlying_length = underlying_list.length();

  // If any of the types don't match, fallback to replace behavior.
  for (wtf_size_t i = 0; i < underlying_length && i < length; i++) {
    if (To<InterpolableFilter>(underlying_list.Get(i))->GetType() !=
        To<InterpolableFilter>(interpolable_list->Get(i))->GetType())
      return InterpolationValue(std::move(interpolable_list));
  }

  // Otherwise, arithmetically combine the matching prefix of the lists then
  // concatenate the remainder of the longer one.
  wtf_size_t max_length = std::max(length, underlying_length);
  auto composited_list = std::make_unique<InterpolableList>(max_length);
  for (wtf_size_t i = 0; i < max_length; i++) {
    if (i < underlying_length) {
      composited_list->Set(i, underlying_list.Get(i)->Clone());
      if (i < length)
        composited_list->GetMutable(i)->Add(*interpolable_list->Get(i));
    } else {
      composited_list->Set(i, interpolable_list->Get(i)->Clone());
    }
  }

  return InterpolationValue(std::move(composited_list));
}

}  // namespace blink
