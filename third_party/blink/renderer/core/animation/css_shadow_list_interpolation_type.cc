// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css_shadow_list_interpolation_type.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/interpolable_shadow.h"
#include "third_party/blink/renderer/core/animation/list_interpolation_functions.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {
const ShadowList* GetShadowList(const CSSProperty& property,
                                const ComputedStyle& style) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kBoxShadow:
      return style.BoxShadow();
    case CSSPropertyID::kTextShadow:
      return style.TextShadow();
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}
}  // namespace

InterpolationValue CSSShadowListInterpolationType::ConvertShadowList(
    const ShadowList* shadow_list,
    double zoom,
    mojom::blink::ColorScheme color_scheme,
    const ui::ColorProvider* color_provider) const {
  if (!shadow_list)
    return CreateNeutralValue();
  const ShadowDataVector& shadows = shadow_list->Shadows();
  return ListInterpolationFunctions::CreateList(
      shadows.size(),
      [&shadows, zoom, color_scheme, color_provider](wtf_size_t index) {
        return InterpolationValue(InterpolableShadow::Create(
            shadows[index], zoom, color_scheme, color_provider));
      });
}

InterpolationValue CSSShadowListInterpolationType::CreateNeutralValue() const {
  return ListInterpolationFunctions::CreateEmptyList();
}

InterpolationValue CSSShadowListInterpolationType::MaybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return CreateNeutralValue();
}

InterpolationValue CSSShadowListInterpolationType::MaybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return CreateNeutralValue();
}

class InheritedShadowListChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  InheritedShadowListChecker(const CSSProperty& property,
                             const ShadowList* shadow_list)
      : property_(property), shadow_list_(shadow_list) {}

  void Trace(Visitor* visitor) const final {
    visitor->Trace(shadow_list_);
    CSSInterpolationType::CSSConversionChecker::Trace(visitor);
  }

 private:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    const ShadowList* inherited_shadow_list =
        GetShadowList(property_, *state.ParentStyle());
    if (!inherited_shadow_list && !shadow_list_)
      return true;
    if (!inherited_shadow_list || !shadow_list_)
      return false;
    return *inherited_shadow_list == *shadow_list_;
  }

  const CSSProperty& property_;
  Member<const ShadowList> shadow_list_;
};

InterpolationValue CSSShadowListInterpolationType::MaybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversion_checkers) const {
  if (!state.ParentStyle())
    return nullptr;
  const ShadowList* inherited_shadow_list =
      GetShadowList(CssProperty(), *state.ParentStyle());
  conversion_checkers.push_back(
      MakeGarbageCollected<InheritedShadowListChecker>(CssProperty(),
                                                       inherited_shadow_list));
  mojom::blink::ColorScheme color_scheme =
      state.StyleBuilder().UsedColorScheme();
  const ui::ColorProvider* color_provider =
      state.GetDocument().GetColorProviderForPainting(color_scheme);
  return ConvertShadowList(inherited_shadow_list,
                           state.ParentStyle()->EffectiveZoom(), color_scheme,
                           color_provider);
}

class AlwaysInvalidateChecker
    : public CSSInterpolationType::CSSConversionChecker {
 public:
  bool IsValid(const StyleResolverState& state,
               const InterpolationValue& underlying) const final {
    return false;
  }
};

InterpolationValue CSSShadowListInterpolationType::MaybeConvertValue(
    const CSSValue& value,
    const StyleResolverState* state,
    ConversionCheckers&) const {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  if (identifier_value && identifier_value->GetValueID() == CSSValueID::kNone)
    return CreateNeutralValue();

  if (!value.IsBaseValueList())
    return nullptr;

  const auto& value_list = To<CSSValueList>(value);
  return ListInterpolationFunctions::CreateList(
      value_list.length(), [&value_list, state](wtf_size_t index) {
        mojom::blink::ColorScheme color_scheme =
            state ? state->StyleBuilder().UsedColorScheme()
                  : mojom::blink::ColorScheme::kLight;
        const ui::ColorProvider* color_provider =
            state
                ? state->GetDocument().GetColorProviderForPainting(color_scheme)
                : nullptr;
        return InterpolationValue(InterpolableShadow::MaybeConvertCSSValue(
            value_list.Item(index), color_scheme, color_provider));
      });
}

PairwiseInterpolationValue CSSShadowListInterpolationType::MaybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  return ListInterpolationFunctions::MaybeMergeSingles(
      std::move(start), std::move(end),
      ListInterpolationFunctions::LengthMatchingStrategy::kPadToLargest,
      [](InterpolationValue&& start_item, InterpolationValue&& end_item) {
        return InterpolableShadow::MaybeMergeSingles(
            std::move(start_item.interpolable_value),
            std::move(end_item.interpolable_value));
      });
}

InterpolationValue
CSSShadowListInterpolationType::MaybeConvertStandardPropertyUnderlyingValue(
    const ComputedStyle& style) const {
  // TODO(crbug.com/1231644): Need to pass an appropriate color provider here.
  return ConvertShadowList(GetShadowList(CssProperty(), style),
                           style.EffectiveZoom(), style.UsedColorScheme(),
                           /*color_provider=*/nullptr);
}

void CSSShadowListInterpolationType::Composite(
    UnderlyingValueOwner& underlying_value_owner,
    double underlying_fraction,
    const InterpolationValue& value,
    double interpolation_fraction) const {
  // We do our compositing behavior in |PreInterpolationCompositeIfNeeded|; see
  // the documentation on that method.
  underlying_value_owner.Set(*this, value);
}

static ShadowList* CreateShadowList(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    const StyleResolverState& state) {
  const auto& interpolable_list = To<InterpolableList>(interpolable_value);
  wtf_size_t length = interpolable_list.length();
  if (length == 0)
    return nullptr;
  ShadowDataVector shadows;
  shadows.ReserveInitialCapacity(length);
  for (wtf_size_t i = 0; i < length; i++) {
    shadows.push_back(To<InterpolableShadow>(interpolable_list.Get(i))
                          ->CreateShadowData(state));
  }
  return MakeGarbageCollected<ShadowList>(std::move(shadows));
}

void CSSShadowListInterpolationType::ApplyStandardPropertyValue(
    const InterpolableValue& interpolable_value,
    const NonInterpolableValue* non_interpolable_value,
    StyleResolverState& state) const {
  ShadowList* shadow_list =
      CreateShadowList(interpolable_value, non_interpolable_value, state);
  switch (CssProperty().PropertyID()) {
    case CSSPropertyID::kBoxShadow:
      state.StyleBuilder().SetBoxShadow(shadow_list);
      return;
    case CSSPropertyID::kTextShadow:
      state.StyleBuilder().SetTextShadow(shadow_list);
      return;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

InterpolationValue
CSSShadowListInterpolationType::PreInterpolationCompositeIfNeeded(
    InterpolationValue value,
    const InterpolationValue& underlying,
    EffectModel::CompositeOperation composite,
    ConversionCheckers& conversion_checkers) const {
  // Due to the post-interpolation composite optimization, the interpolation
  // stack aggressively caches interpolated values. When we are doing
  // pre-interpolation compositing, this can cause us to bake-in the composited
  // result even when the underlying value is changing. This checker is a hack
  // to disable that caching in this case.
  // TODO(crbug.com/1009230): Remove this once our interpolation code isn't
  // caching composited values.
  conversion_checkers.push_back(
      MakeGarbageCollected<AlwaysInvalidateChecker>());
  auto* interpolable_list =
      To<InterpolableList>(value.interpolable_value.Release());
  if (composite == EffectModel::CompositeOperation::kCompositeAdd) {
    return PerformAdditiveComposition(interpolable_list, underlying);
  }
  DCHECK_EQ(composite, EffectModel::CompositeOperation::kCompositeAccumulate);
  return PerformAccumulativeComposition(interpolable_list,
                                        std::move(underlying));
}

InterpolationValue CSSShadowListInterpolationType::PerformAdditiveComposition(
    InterpolableList* interpolable_list,
    const InterpolationValue& underlying) const {
  // Per the spec, addition of shadow lists is defined as concatenation.
  // https://w3.org/TR/web-animations-1/#combining-shadow-lists
  const InterpolableList& underlying_list =
      To<InterpolableList>(*underlying.interpolable_value);
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
  return InterpolationValue(composited_list, underlying.non_interpolable_value);
}

InterpolationValue
CSSShadowListInterpolationType::PerformAccumulativeComposition(
    InterpolableList* interpolable_list,
    const InterpolationValue& underlying) const {
  // Per the spec, accumulation of shadow lists operates on pairwise addition of
  // the underlying components.
  // https://w3.org/TR/web-animations-1/#combining-shadow-lists
  const InterpolableList& underlying_list =
      To<InterpolableList>(*underlying.interpolable_value);
  wtf_size_t length = interpolable_list->length();
  wtf_size_t underlying_length = underlying_list.length();
  // If any of the shadow style(inset or normal) value don't match, fallback to
  // replace behavior.
  for (wtf_size_t i = 0; i < underlying_length && i < length; i++) {
    if (To<InterpolableShadow>(underlying_list.Get(i))->GetShadowStyle() !=
        To<InterpolableShadow>(interpolable_list->Get(i))->GetShadowStyle()) {
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
      if (i < length)
        composited_list->GetMutable(i)->Add(*interpolable_list->Get(i));
    } else {
      composited_list->Set(i, interpolable_list->Get(i)->Clone());
    }
  }
  return InterpolationValue(composited_list, underlying.non_interpolable_value);
}

}  // namespace blink
