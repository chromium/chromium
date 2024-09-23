// Copyright 2016 The Chromium Authors
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
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

const FilterOperations& GetFilterList(const CSSProperty& property,
                                      const ComputedStyle& style) {
  switch (property.PropertyID()) {
    default:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case CSSPropertyID::kBackdropFilter:
      return style.BackdropFilter();
    case CSSPropertyID::kFilter:
      return style.Filter();
  }
}

void SetFilterList(const CSSProperty& property,
                   ComputedStyleBuilder& builder,
                   const FilterOperations& filter_operations) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kBackdropFilter:
      builder.SetBackdropFilter(filter_operations);
      break;
    case CSSPropertyID::kFilter:
      builder.SetFilter(filter_operations);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
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
    const auto& underlying_list =
        To<InterpolableList>(*underlying.interpolable_value);
    if (underlying_list.length() != types_.size()) {
      return false;
    }
    for (wtf_size_t i = 0; i < types_.size(); i++) {
      FilterOperation::OperationType other_type =
          To<InterpolableFilter>(underlying_list.Get(i))->GetType();
      if (types_[i] != other_type) {
        return false;
      }
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

  void Trace(Visitor* visitor) const final {
    CSSConversionChecker::Trace(visitor);
    visitor->Trace(filter_operations_wrapper_);
  }

  bool IsValid(const StyleResolverState& state,
               const InterpolationValue&) const final {
    const FilterOperations& filter_operations =
        filter_operations_wrapper_->Operations();
    return filter_operations == GetFilterList(property_, *state.ParentStyle());
  }

 private:
  const CSSProperty& property_;
  Member<FilterOperationsWrapper> filter_operations_wrapper_;
};

InterpolationValue ConvertFilterList(const FilterOperations& filter_operations,
                                     const CSSProperty& property,
                                     double zoom,
                                     mojom::blink::ColorScheme color_scheme,
                                     const ui::ColorProvider* color_provider) {
  wtf_size_t length = filter_operations.size();
  auto* interpolable_list = MakeGarbageCollected<InterpolableList>(length);
  for (wtf_size_t i = 0; i < length; i++) {
    InterpolableFilter* result = InterpolableFilter::MaybeCreate(
        *filter_operations.Operations()[i], property, zoom, color_scheme,
        color_provider);
    if (!result) {
      return nullptr;
    }
    interpolable_list->Set(i, result);
  }
  return InterpolationValue(interpolable_list);
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
  const auto* interpolable_list =
      To<InterpolableList>(underlying.interpolable_value.Get());
  conversion_checkers.push_back(
      MakeGarbageCollected<UnderlyingFilterListChecker>(interpolable_list));
  // The neutral value for composition for a filter list is the empty list, as
  // the additive operator is concatenation, so concat(underlying, []) ==
  // underlying.
  return InterpolationValue(MakeGarbageCollected<InterpolableList>(0));
}

InterpolationValue CSSFilterListInterpolationType::MaybeConvertInitial(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  mojom::blink::ColorScheme color_scheme =
      state.StyleBuilder().UsedColorScheme();
  const ui::ColorProvider* color_provider =
      state.GetDocument().GetColorProviderForPainting(color_scheme);
  return ConvertFilterList(
      GetFilterList(CssProperty(),
                    state.GetDocument().GetStyleResolver().InitialStyle()),
      CssProperty(), 1, color_scheme, color_provider);
}

InterpolationValue CSSFilterListInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  const FilterOperations& inherited_filter_operations =
      GetFilterList(CssProperty(), *state.ParentStyle());
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedFilterListChecker>(
          CssProperty(), inherited_filter_operations));
  mojom::blink::ColorScheme color_scheme =
      state.StyleBuilder().UsedColorScheme();
  const ui::ColorProvider* color_provider =
      state.GetDocument().GetColorProviderForPainting(color_scheme);
  return ConvertFilterList(inherited_filter_operations, CssProperty(),
                           state.StyleBuilder().EffectiveZoom(), color_scheme,
                           color_provider);
}

InterpolationValue CSSFilterListInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState* state,
    ConversionCheckers&) const {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value && identifier_value->GetValueID() == CSSValueID::kNone) {
    return InterpolationValue(MakeGarbageCollected<InterpolableList>(0));
  }

  if (!value.IsBaseValueList()) {
    return nullptr;
  }

  const auto& list = To<CSSValueList>(value);
  wtf_size_t length = list.length();
  auto* interpolable_list = MakeGarbageCollected<InterpolableList>(length);
  for (wtf_size_t i = 0; i < length; i++) {
    mojom::blink::ColorScheme color_scheme =
        state ? state->StyleBuilder().UsedColorScheme()
              : mojom::blink::ColorScheme::kLight;
    const ui::ColorProvider* color_provider =
        state ? state->GetDocument().GetColorProviderForPainting(color_scheme)
              : nullptr;
    InterpolableFilter* result = InterpolableFilter::MaybeConvertCSSValue(
        list.Item(i), color_scheme, color_provider);
    if (!result) {
      return nullptr;
    }
    interpolable_list->Set(i, result);
  }
  return InterpolationValue(interpolable_list);
}

InterpolationValue
CSSFilterListInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  // TODO(crbug.com/1231644): Need to pass an appropriate color provider here.
  return ConvertFilterList(GetFilterList(CssProperty(), style), CssProperty(),
                           style.EffectiveZoom(), style.UsedColorScheme(),
                           /*color_provider=*/nullptr);
}

PairwiseInterpolationValue CSSFilterListInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  auto& start_interpolable_list =
      To<InterpolableList>(*start.interpolable_value);
  auto& end_interpolable_list = To<InterpolableList>(*end.interpolable_value);
  wtf_size_t start_length = start_interpolable_list.length();
  wtf_size_t end_length = end_interpolable_list.length();

  for (wtf_size_t i = 0; i < start_length && i < end_length; i++) {
    if (To<InterpolableFilter>(start_interpolable_list.Get(i))->GetType() !=
        To<InterpolableFilter>(end_interpolable_list.Get(i))->GetType()) {
      return nullptr;
    }
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
  auto* extended_interpolable_list =
      MakeGarbageCollected<InterpolableList>(longer_length);
  for (wtf_size_t i = 0; i < longer_length; i++) {
    if (i < shorter_length) {
      extended_interpolable_list->Set(
          i, std::move(shorter_interpolable_list.GetMutable(i)));
    } else {
      extended_interpolable_list->Set(
          i, InterpolableFilter::CreateInitialValue(
                 To<InterpolableFilter>(longer_interpolable_list.Get(i))
                     ->GetType()));
    }
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
  const auto& interpolable_list = To<InterpolableList>(interpolable_value);
  wtf_size_t length = interpolable_list.length();

  FilterOperations filter_operations;
  filter_operations.Operations().reserve(length);
  for (wtf_size_t i = 0; i < length; i++) {
    filter_operations.Operations().push_back(
        To<InterpolableFilter>(interpolable_list.Get(i))
            ->CreateFilterOperation(state));
  }
  SetFilterList(CssProperty(), state.StyleBuilder(),
                std::move(filter_operations));
}

InterpolationValue
CSSFilterListInterpolationType::PreInterpolationCompositeIfNeeded(
    InterpolationValue value,
    const InterpolationValue& underlying,
    EffectModel::CompositeOperation composite,
    ConversionCheckers& conversion_checkers) const {
  DCHECK(!value.non_interpolable_value);

  // Due to the post-interpolation composite optimization, the interpolation
  // stack aggressively caches interpolated values. When we are doing
  // pre-interpolation compositing, this can cause us to bake-in the composited
  // result even when the underlying value is changing. This checker is a hack
  // to disable that caching in this case.
  // TODO(crbug.com/1009230): Remove this once our interpolation code isn't
  // caching composited values.
  conversion_checkers.push_back(
      MakeGarbageCollected<AlwaysInvalidateChecker>());

  // The non_interpolable_value can be non-null, for example, it contains a
  // single frame url().
  if (underlying.non_interpolable_value) {
    return nullptr;
  }

  // The underlying value can be nullptr, most commonly if it contains a url().
  // TODO(crbug.com/1009229): Properly handle url() in filter composite.
  if (!underlying.interpolable_value) {
    return nullptr;
  }

  auto* interpolable_list =
      To<InterpolableList>(value.interpolable_value.Release());
  const auto& underlying_list =
      To<InterpolableList>(*underlying.interpolable_value);

  if (composite == EffectModel::CompositeOperation::kCompositeAdd) {
    return PerformAdditiveComposition(interpolable_list, underlying_list);
  }
  DCHECK_EQ(composite, EffectModel::CompositeOperation::kCompositeAccumulate);
  return PerformAccumulativeComposition(interpolable_list, underlying_list);
}

InterpolationValue CSSFilterListInterpolationType::PerformAdditiveComposition(
    InterpolableList* interpolable_list,
    const InterpolableList& underlying_list) const {
  // Per the spec, addition of filter lists is defined as concatenation.
  // https://drafts.fxtf.org/filter-effects-1/#addition
  auto* composited_list = MakeGarbageCollected<InterpolableList>(
      underlying_list.length() + interpolable_list->length());
  for (wtf_size_t i = 0; i < composited_list->length(); i++) {
    if (i < underlying_list.length()) {
      composited_list->Set(i, underlying_list.Get(i)->Clone());
    } else {
      composited_list->Set(
          i, interpolable_list->Get(i - underlying_list.length())->Clone());
    }
  }
  return InterpolationValue(composited_list);
}

InterpolationValue
CSSFilterListInterpolationType::PerformAccumulativeComposition(
    InterpolableList* interpolable_list,
    const InterpolableList& underlying_list) const {
  // Per the spec, accumulation of filter lists operates on pairwise addition of
  // the underlying components.
  // https://drafts.fxtf.org/filter-effects-1/#accumulation
  wtf_size_t length = interpolable_list->length();
  wtf_size_t underlying_length = underlying_list.length();

  // If any of the types don't match, fallback to replace behavior.
  for (wtf_size_t i = 0; i < underlying_length && i < length; i++) {
    if (To<InterpolableFilter>(underlying_list.Get(i))->GetType() !=
        To<InterpolableFilter>(interpolable_list->Get(i))->GetType()) {
      return InterpolationValue(interpolable_list);
    }
  }

  // Otherwise, arithmetically combine the matching prefix of the lists then
  // concatenate the remainder of the longer one.
  wtf_size_t max_length = std::max(length, underlying_length);
  auto* composited_list = MakeGarbageCollected<InterpolableList>(max_length);
  for (wtf_size_t i = 0; i < max_length; i++) {
    if (i < underlying_length) {
      composited_list->Set(i, underlying_list.Get(i)->Clone());
      if (i < length) {
        composited_list->GetMutable(i)->Add(*interpolable_list->Get(i));
      }
    } else {
      composited_list->Set(i, interpolable_list->Get(i)->Clone());
    }
  }

  return InterpolationValue(composited_list);
}

}  // namespace blink
