// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/invalidatable_interpolation.h"

#include <memory>
#include "third_party/blink/renderer/core/animation/css_interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/string_keyframe.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

void InvalidatableInterpolation::Interpolate(int, double fraction) {
  if (fraction == current_fraction_) {
    return;
  }

  current_fraction_ = fraction;
  if (is_conversion_cached_ && cached_pair_conversion_) {
    cached_pair_conversion_->InterpolateValue(fraction, cached_value_);
  }
  // We defer the interpolation to ensureValidConversion() if
  // |cached_pair_conversion_| is null.
}

PairwisePrimitiveInterpolation*
InvalidatableInterpolation::MaybeConvertPairwise(
    const InterpolationEnvironment& environment,
    const UnderlyingValueOwner& underlying_value_owner) const {
  for (const auto& interpolation_type : *interpolation_types_) {
    if ((start_keyframe_->IsNeutral() || end_keyframe_->IsNeutral()) &&
        (!underlying_value_owner ||
         underlying_value_owner.GetType() != *interpolation_type)) {
      continue;
    }
    ConversionCheckers conversion_checkers;
    PairwiseInterpolationValue result =
        interpolation_type->MaybeConvertPairwise(
            *start_keyframe_, *end_keyframe_, environment,
            underlying_value_owner.Value(), conversion_checkers);
    AddConversionCheckers(*interpolation_type, conversion_checkers);
    if (result) {
      return MakeGarbageCollected<PairwisePrimitiveInterpolation>(
          *interpolation_type, std::move(result.start_interpolable_value),
          std::move(result.end_interpolable_value),
          std::move(result.non_interpolable_value));
    }
  }
  return nullptr;
}

TypedInterpolationValue* InvalidatableInterpolation::ConvertSingleKeyframe(
    const PropertySpecificKeyframe& keyframe,
    const InterpolationEnvironment& environment,
    const UnderlyingValueOwner& underlying_value_owner) const {
  if (keyframe.IsNeutral() && !underlying_value_owner) {
    return nullptr;
  }
  for (const auto& interpolation_type : *interpolation_types_) {
    if (keyframe.IsNeutral() &&
        underlying_value_owner.GetType() != *interpolation_type) {
      continue;
    }
    ConversionCheckers conversion_checkers;
    InterpolationValue result = interpolation_type->MaybeConvertSingle(
        keyframe, environment, underlying_value_owner.Value(),
        conversion_checkers);
    AddConversionCheckers(*interpolation_type, conversion_checkers);
    if (result) {
      return MakeGarbageCollected<TypedInterpolationValue>(
          *interpolation_type, std::move(result.interpolable_value),
          std::move(result.non_interpolable_value));
    }
  }
  DCHECK(keyframe.IsNeutral());
  return nullptr;
}

void InvalidatableInterpolation::AddConversionCheckers(
    const InterpolationType& type,
    ConversionCheckers& conversion_checkers) const {
  for (wtf_size_t i = 0; i < conversion_checkers.size(); i++) {
    conversion_checkers[i]->SetType(type);
    conversion_checkers_.push_back(std::move(conversion_checkers[i]));
  }
}

TypedInterpolationValue*
InvalidatableInterpolation::MaybeConvertUnderlyingValue(
    const InterpolationEnvironment& environment) const {
  for (const auto& interpolation_type : *interpolation_types_) {
    InterpolationValue result =
        interpolation_type->MaybeConvertUnderlyingValue(environment);
    if (result) {
      return MakeGarbageCollected<TypedInterpolationValue>(
          *interpolation_type, std::move(result.interpolable_value),
          std::move(result.non_interpolable_value));
    }
  }
  return nullptr;
}

bool InvalidatableInterpolation::DependsOnUnderlyingValue() const {
  return start_keyframe_->UnderlyingFraction() != 0 ||
         end_keyframe_->UnderlyingFraction() != 0;
}

bool InvalidatableInterpolation::IsNeutralKeyframeActive() const {
  return start_keyframe_->IsNeutral() || end_keyframe_->IsNeutral();
}

void InvalidatableInterpolation::ClearConversionCache(
    InterpolationEnvironment& environment) const {
  if (auto* css_environment =
          DynamicTo<CSSInterpolationEnvironment>(environment)) {
    css_environment->GetState().SetAffectsCompositorSnapshots();
  }

  is_conversion_cached_ = false;
  cached_pair_conversion_.Clear();
  conversion_checkers_.clear();
  cached_value_.Clear();
}

bool InvalidatableInterpolation::IsConversionCacheValid(
    const InterpolationEnvironment& environment,
    const UnderlyingValueOwner& underlying_value_owner) const {
  if (!is_conversion_cached_) {
    return false;
  }
  if (IsNeutralKeyframeActive()) {
    if (cached_pair_conversion_ && cached_pair_conversion_->IsFlip()) {
      return false;
    }
    // Pairwise interpolation can never happen between different
    // InterpolationTypes, neutral values always represent the underlying value.
    if (!underlying_value_owner || !cached_value_ ||
        cached_value_->GetType() != underlying_value_owner.GetType()) {
      return false;
    }
  }
  for (const auto& checker : conversion_checkers_) {
    if (!checker->IsValid(environment, underlying_value_owner.Value())) {
      return false;
    }
  }
  return true;
}

const TypedInterpolationValue*
InvalidatableInterpolation::EnsureValidConversion(
    InterpolationEnvironment& environment,
    const UnderlyingValueOwner& underlying_value_owner) const {
  DCHECK(!std::isnan(current_fraction_));
  DCHECK(interpolation_types_ &&
         interpolation_types_version_ ==
             environment.GetInterpolationTypesMap().Version());
  if (IsConversionCacheValid(environment, underlying_value_owner)) {
    return cached_value_.Get();
  }
  ClearConversionCache(environment);

  PairwisePrimitiveInterpolation* pairwise_conversion =
      MaybeConvertPairwise(environment, underlying_value_owner);
  if (pairwise_conversion) {
    cached_value_ = pairwise_conversion->InitialValue();
    cached_pair_conversion_ = std::move(pairwise_conversion);
  } else {
    cached_pair_conversion_ = MakeGarbageCollected<FlipPrimitiveInterpolation>(
        ConvertSingleKeyframe(*start_keyframe_, environment,
                              underlying_value_owner),
        ConvertSingleKeyframe(*end_keyframe_, environment,
                              underlying_value_owner));
  }
  cached_pair_conversion_->InterpolateValue(current_fraction_, cached_value_);
  is_conversion_cached_ = true;
  return cached_value_.Get();
}

void InvalidatableInterpolation::EnsureValidInterpolationTypes(
    InterpolationEnvironment& environment) const {
  const InterpolationTypesMap& map = environment.GetInterpolationTypesMap();
  size_t latest_version = map.Version();
  if (interpolation_types_ && interpolation_types_version_ == latest_version) {
    return;
  }
  const InterpolationTypes* latest_interpolation_types = &map.Get(property_);
  DCHECK(latest_interpolation_types);
  if (interpolation_types_ != latest_interpolation_types) {
    ClearConversionCache(environment);
  }
  interpolation_types_ = latest_interpolation_types;
  interpolation_types_version_ = latest_version;
}

void InvalidatableInterpolation::SetFlagIfInheritUsed(
    InterpolationEnvironment& environment) const {
  if (!property_.IsCSSProperty() && !property_.IsPresentationAttribute()) {
    return;
  }
  StyleResolverState& state =
      To<CSSInterpolationEnvironment>(environment).GetState();
  if (!state.ParentStyle()) {
    return;
  }
  const CSSValue* start_value =
      To<CSSPropertySpecificKeyframe>(*start_keyframe_).Value();
  const CSSValue* end_value =
      To<CSSPropertySpecificKeyframe>(*end_keyframe_).Value();
  if ((start_value && start_value->IsInheritedValue()) ||
      (end_value && end_value->IsInheritedValue())) {
    state.ParentStyle()->SetChildHasExplicitInheritance();
  }
}

double InvalidatableInterpolation::UnderlyingFraction() const {
  if (current_fraction_ == 0) {
    return start_keyframe_->UnderlyingFraction();
  }
  if (current_fraction_ == 1) {
    return end_keyframe_->UnderlyingFraction();
  }
  return cached_pair_conversion_->InterpolateUnderlyingFraction(
      start_keyframe_->UnderlyingFraction(),
      end_keyframe_->UnderlyingFraction(), current_fraction_);
}

void InvalidatableInterpolation::ApplyStack(
    const ActiveInterpolations& interpolations,
    InterpolationEnvironment& environment) {
  DCHECK(!interpolations.empty());
  wtf_size_t starting_index = 0;

  // Compute the underlying value to composite onto.
  UnderlyingValueOwner underlying_value_owner;
  const auto& first_interpolation =
      To<InvalidatableInterpolation>(*interpolations.at(starting_index));
  first_interpolation.EnsureValidInterpolationTypes(environment);
  if (first_interpolation.DependsOnUnderlyingValue()) {
    underlying_value_owner.Set(
        first_interpolation.MaybeConvertUnderlyingValue(environment));
  } else {
    const TypedInterpolationValue* first_value =
        first_interpolation.EnsureValidConversion(environment,
                                                  underlying_value_owner);

    // Fast path for replace interpolations that are the only one to apply.
    if (interpolations.size() == 1) {
      if (first_value) {
        first_interpolation.SetFlagIfInheritUsed(environment);
        first_value->GetType().Apply(first_value->GetInterpolableValue(),
                                     first_value->GetNonInterpolableValue(),
                                     environment);
      }
      return;
    }
    underlying_value_owner.Set(first_value);
    starting_index++;
  }

  // Composite interpolations onto the underlying value.
  bool should_apply = false;
  for (wtf_size_t i = starting_index; i < interpolations.size(); i++) {
    const auto& current_interpolation =
        To<InvalidatableInterpolation>(*interpolations.at(i));
    DCHECK(current_interpolation.DependsOnUnderlyingValue());
    current_interpolation.EnsureValidInterpolationTypes(environment);
    const TypedInterpolationValue* current_value =
        current_interpolation.EnsureValidConversion(environment,
                                                    underlying_value_owner);
    if (!current_value) {
      continue;
    }

    should_apply = true;
    current_interpolation.SetFlagIfInheritUsed(environment);
    if (!current_interpolation.DependsOnUnderlyingValue() ||
        !underlying_value_owner ||
        underlying_value_owner.GetType() != current_value->GetType()) {
      underlying_value_owner.Set(current_value);
    } else {
      current_value->GetType().Composite(
          underlying_value_owner, current_interpolation.UnderlyingFraction(),
          current_value->Value(), current_interpolation.current_fraction_);
    }
  }

  if (should_apply && underlying_value_owner) {
    underlying_value_owner.GetType().Apply(
        *underlying_value_owner.Value().interpolable_value,
        underlying_value_owner.Value().non_interpolable_value.get(),
        environment);
  }
}

}  // namespace blink
