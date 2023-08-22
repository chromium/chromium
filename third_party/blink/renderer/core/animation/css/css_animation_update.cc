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
  changed_deferred_timelines_ = update.changed_deferred_timelines_;
  changed_timeline_attachments_ = update.changed_timeline_attachments_;
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
  changed_deferred_timelines_.clear();
  changed_timeline_attachments_.clear();
}

void CSSAnimationUpdate::StartTransition(
    const PropertyHandle& property,
    const ComputedStyle* from,
    const ComputedStyle* to,
    const ComputedStyle* reversing_adjusted_start_value,
    double reversing_shortening_factor,
    const InertEffect& effect) {
  NewTransition* new_transition = MakeGarbageCollected<NewTransition>(
      property, from, to, reversing_adjusted_start_value,
      reversing_shortening_factor, &effect);
  new_transitions_.Set(property, new_transition);
}

void CSSAnimationUpdate::UnstartTransition(const PropertyHandle& property) {
  new_transitions_.erase(property);
}

}  // namespace blink
