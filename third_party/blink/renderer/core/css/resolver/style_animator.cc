// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/style_animator.h"

#include "third_party/blink/renderer/core/animation/css_interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_types_map.h"
#include "third_party/blink/renderer/core/animation/invalidatable_interpolation.h"
#include "third_party/blink/renderer/core/animation/transition_interpolation.h"
#include "third_party/blink/renderer/core/css/css_pending_interpolation_value.h"
#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"

namespace blink {

namespace {

PropertyHandle ToPropertyHandle(
    const CSSProperty& property,
    const cssvalue::CSSPendingInterpolationValue& value) {
  if (IsA<CustomProperty>(property))
    return PropertyHandle(property.GetPropertyNameAtomicString());
  return PropertyHandle(property, value.IsPresentationAttribute());
}

const ActiveInterpolations& GetActiveInterpolations(
    const ActiveInterpolationsMap& animations_map,
    const ActiveInterpolationsMap& transitions_map,
    const PropertyHandle& property) {
  // Interpolations will never be found in both animations_map and
  // transitions_map. This condition is ensured by
  // CSSAnimations::CalculateTransitionUpdateForProperty().
  const auto& animation = animations_map.find(property);
  if (animation != animations_map.end()) {
    DCHECK_EQ(transitions_map.find(property), transitions_map.end());
    return animation->value;
  }
  const auto& transition = transitions_map.find(property);
  DCHECK_NE(transition, transitions_map.end());
  return transition->value;
}

const ActiveInterpolations& GetActiveInterpolations(
    const CSSAnimationUpdate& update,
    const PropertyHandle& property) {
  if (property.IsCSSCustomProperty()) {
    return GetActiveInterpolations(
        update.ActiveInterpolationsForCustomAnimations(),
        update.ActiveInterpolationsForCustomTransitions(), property);
  }
  return GetActiveInterpolations(
      update.ActiveInterpolationsForStandardAnimations(),
      update.ActiveInterpolationsForStandardTransitions(), property);
}

}  // namespace

StyleAnimator::StyleAnimator(StyleResolverState& state, StyleCascade& cascade)
    : state_(state), cascade_(cascade) {}

void StyleAnimator::Apply(const CSSProperty& property,
                          const cssvalue::CSSPendingInterpolationValue& value,
                          StyleCascade::Resolver& resolver) {
  PropertyHandle property_handle = ToPropertyHandle(property, value);
  const ActiveInterpolations& interpolations =
      GetActiveInterpolations(state_.AnimationUpdate(), property_handle);
  const Interpolation& interpolation = *interpolations.front();
  if (interpolation.IsInvalidatableInterpolation()) {
    CSSInterpolationTypesMap map(state_.GetDocument().GetPropertyRegistry(),
                                 state_.GetDocument());
    CSSInterpolationEnvironment environment(map, state_, &cascade_, &resolver);
    InvalidatableInterpolation::ApplyStack(interpolations, environment);
  } else {
    ToTransitionInterpolation(interpolation).Apply(state_);
  }
}

}  // namespace blink
