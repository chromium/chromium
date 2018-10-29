// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/css_variable_animator.h"

#include "third_party/blink/renderer/core/animation/css_interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_types_map.h"
#include "third_party/blink/renderer/core/animation/invalidatable_interpolation.h"
#include "third_party/blink/renderer/core/animation/transition_interpolation.h"

namespace blink {

namespace {

HashSet<PropertyHandle> CollectPending(const CSSAnimationUpdate& update) {
  HashSet<PropertyHandle> pending;
  for (const auto& entry : update.ActiveInterpolationsForCustomAnimations())
    pending.insert(entry.key);
  for (const auto& entry : update.ActiveInterpolationsForCustomTransitions())
    pending.insert(entry.key);
  return pending;
}

const ActiveInterpolations& ActiveInterpolationsForCustomProperty(
    const CSSAnimationUpdate& update,
    const PropertyHandle& property) {
  // Interpolations will never be found in both animations_map and
  // transitions_map. This condition is ensured by
  // CSSAnimations::CalculateTransitionUpdateForProperty().
  const ActiveInterpolationsMap& animations_map =
      update.ActiveInterpolationsForCustomAnimations();
  const ActiveInterpolationsMap& transitions_map =
      update.ActiveInterpolationsForCustomTransitions();
  const auto& animation = animations_map.find(property);
  if (animation != animations_map.end()) {
    DCHECK_EQ(transitions_map.find(property), transitions_map.end());
    return animation->value;
  }
  const auto& transition = transitions_map.find(property);
  DCHECK_NE(transition, transitions_map.end());
  return transition->value;
}

}  // namespace

CSSVariableAnimator::CSSVariableAnimator(StyleResolverState& state)
    : CSSVariableResolver(state),
      state_(state),
      update_(state.AnimationUpdate()),
      pending_properties_(CollectPending(update_)) {}

void CSSVariableAnimator::ApplyAll() {
  while (!pending_properties_.IsEmpty()) {
    PropertyHandle property = *pending_properties_.begin();
    Apply(property);
    DCHECK_EQ(pending_properties_.find(property), pending_properties_.end());
  }
}

void CSSVariableAnimator::ApplyAnimation(const AtomicString& name) {
  PropertyHandle property(name);
  if (pending_properties_.Contains(property))
    Apply(property);
}

void CSSVariableAnimator::Apply(const PropertyHandle& property) {
  DCHECK(property.IsCSSCustomProperty());
  DCHECK(pending_properties_.Contains(property));
  const ActiveInterpolations& interpolations =
      ActiveInterpolationsForCustomProperty(update_, property);
  const Interpolation& interpolation = *interpolations.front();
  if (interpolation.IsInvalidatableInterpolation()) {
    CSSInterpolationTypesMap map(state_.GetDocument().GetPropertyRegistry(),
                                 state_.GetDocument());
    CSSInterpolationEnvironment environment(map, state_, this);
    InvalidatableInterpolation::ApplyStack(interpolations, environment);
  } else {
    ToTransitionInterpolation(interpolation).Apply(state_);
  }
  pending_properties_.erase(property);
}

}  // namespace blink
