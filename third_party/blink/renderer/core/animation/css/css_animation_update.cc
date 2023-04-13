// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_animation_update.h"

#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

// Defined here, to avoid dependencies on ComputedStyle.h in the header file.
CSSAnimationUpdate::CSSAnimationUpdate() = default;
CSSAnimationUpdate::~CSSAnimationUpdate() = default;

void CSSAnimationUpdate::Copy(const CSSAnimationUpdate& update) {
  DCHECK(IsEmpty());
  new_animations_ = update.NewAnimations();
  animations_with_updates_ = update.AnimationsWithUpdates();
  new_transitions_ = update.NewTransitions();
  active_interpolations_for_animations_ =
      update.ActiveInterpolationsForAnimations();
  active_interpolations_for_transitions_ =
      update.ActiveInterpolationsForTransitions();
  cancelled_animation_indices_ = update.CancelledAnimationIndices();
  animation_indices_with_pause_toggled_ =
      update.AnimationIndicesWithPauseToggled();
  cancelled_transitions_ = update.CancelledTransitions();
  finished_transitions_ = update.FinishedTransitions();
  updated_compositor_keyframes_ = update.UpdatedCompositorKeyframes();
  changed_scroll_timelines_ = update.changed_scroll_timelines_;
  changed_view_timelines_ = update.changed_view_timelines_;
  changed_attaching_timelines_ = update.changed_attaching_timelines_;
}

void CSSAnimationUpdate::Clear() {
  new_animations_.clear();
  animations_with_updates_.clear();
  new_transitions_.clear();
  active_interpolations_for_animations_.clear();
  active_interpolations_for_transitions_.clear();
  cancelled_animation_indices_.clear();
  animation_indices_with_pause_toggled_.clear();
  cancelled_transitions_.clear();
  finished_transitions_.clear();
  updated_compositor_keyframes_.clear();
  changed_scroll_timelines_.clear();
  changed_view_timelines_.clear();
  changed_attaching_timelines_.clear();
}

void CSSAnimationUpdate::StartTransition(
    const PropertyHandle& property,
    scoped_refptr<const ComputedStyle> from,
    scoped_refptr<const ComputedStyle> to,
    scoped_refptr<const ComputedStyle> reversing_adjusted_start_value,
    double reversing_shortening_factor,
    const InertEffect& effect) {
  NewTransition* new_transition = MakeGarbageCollected<NewTransition>();
  new_transition->property = property;
  new_transition->from = std::move(from);
  new_transition->to = std::move(to);
  new_transition->reversing_adjusted_start_value =
      std::move(reversing_adjusted_start_value);
  new_transition->reversing_shortening_factor = reversing_shortening_factor;
  new_transition->effect = &effect;
  new_transitions_.Set(property, new_transition);
}

void CSSAnimationUpdate::UnstartTransition(const PropertyHandle& property) {
  new_transitions_.erase(property);
}

CSSAnimationUpdate::NewTransition::NewTransition() = default;
CSSAnimationUpdate::NewTransition::~NewTransition() = default;

}  // namespace blink
