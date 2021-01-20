/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/animation/animation_effect.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_computed_effect_timing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_optional_effect_timing.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/animation_input_helpers.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/timing_calculations.h"
#include "third_party/blink/renderer/core/animation/timing_input.h"

namespace blink {

AnimationEffect::AnimationEffect(const Timing& timing,
                                 EventDelegate* event_delegate)
    : owner_(nullptr),
      timing_(timing),
      event_delegate_(event_delegate),
      needs_update_(true),
      cancel_time_(AnimationTimeDelta()) {
  timing_.AssertValid();
}

void AnimationEffect::UpdateSpecifiedTiming(const Timing& timing) {
  if (!timing_.HasTimingOverrides()) {
    timing_ = timing;
  } else {
    // Style changes that are overridden due to an explicit call to
    // AnimationEffect.updateTiming are not applied.
    if (!timing_.HasTimingOverride(Timing::kOverrideStartDelay))
      timing_.start_delay = timing.start_delay;

    if (!timing_.HasTimingOverride(Timing::kOverrideDirection))
      timing_.direction = timing.direction;

    if (!timing_.HasTimingOverride(Timing::kOverrideDuration))
      timing_.iteration_duration = timing.iteration_duration;

    if (!timing_.HasTimingOverride(Timing::kOverrideEndDelay))
      timing_.end_delay = timing.end_delay;

    if (!timing_.HasTimingOverride(Timing::kOverideFillMode))
      timing_.fill_mode = timing.fill_mode;

    if (!timing_.HasTimingOverride(Timing::kOverrideIterationCount))
      timing_.iteration_count = timing.iteration_count;

    if (!timing_.HasTimingOverride(Timing::kOverrideIterationStart))
      timing_.iteration_start = timing.iteration_start;

    if (!timing_.HasTimingOverride(Timing::kOverrideTimingFunction))
      timing_.timing_function = timing.timing_function;
  }
  InvalidateAndNotifyOwner();
}

void AnimationEffect::SetIgnoreCssTimingProperties() {
  timing_.SetTimingOverride(Timing::kOverrideAll);
}

EffectTiming* AnimationEffect::getTiming() const {
  if (const Animation* animation = GetAnimation())
    animation->FlushPendingUpdates();
  return SpecifiedTiming().ConvertToEffectTiming();
}

ComputedEffectTiming* AnimationEffect::getComputedTiming() const {
  return SpecifiedTiming().getComputedTiming(EnsureCalculated(),
                                             IsA<KeyframeEffect>(this));
}

void AnimationEffect::updateTiming(OptionalEffectTiming* optional_timing,
                                   ExceptionState& exception_state) {
  // TODO(crbug.com/827178): Determine whether we should pass a Document in here
  // (and which) to resolve the CSS secure/insecure context against.
  if (!TimingInput::Update(timing_, optional_timing, nullptr, exception_state))
    return;
  InvalidateAndNotifyOwner();
}

base::Optional<Timing::Phase> TimelinePhaseToTimingPhase(
    base::Optional<TimelinePhase> phase) {
  base::Optional<Timing::Phase> result;
  if (phase) {
    switch (phase.value()) {
      case TimelinePhase::kBefore:
        result = Timing::Phase::kPhaseBefore;
        break;
      case TimelinePhase::kActive:
        result = Timing::Phase::kPhaseActive;
        break;
      case TimelinePhase::kAfter:
        result = Timing::Phase::kPhaseAfter;
        break;
      case TimelinePhase::kInactive:
        // Timing::Phase does not have an inactive phase.
        break;
    }
  }
  return result;
}

void AnimationEffect::UpdateInheritedTime(
    base::Optional<AnimationTimeDelta> inherited_time,
    base::Optional<TimelinePhase> inherited_timeline_phase,
    TimingUpdateReason reason) const {
  base::Optional<double> playback_rate = base::nullopt;
  if (GetAnimation())
    playback_rate = GetAnimation()->playbackRate();
  const Timing::AnimationDirection direction =
      (playback_rate && playback_rate.value() < 0)
          ? Timing::AnimationDirection::kBackwards
          : Timing::AnimationDirection::kForwards;

  base::Optional<Timing::Phase> timeline_phase =
      TimelinePhaseToTimingPhase(inherited_timeline_phase);

  bool needs_update = needs_update_ || last_update_time_ != inherited_time ||
                      (owner_ && owner_->EffectSuppressed()) ||
                      last_update_phase_ != timeline_phase;
  needs_update_ = false;
  last_update_time_ = inherited_time;
  last_update_phase_ = timeline_phase;

  const base::Optional<double> local_time =
      inherited_time ? base::make_optional(inherited_time.value().InSecondsF())
                     : base::nullopt;
  if (needs_update) {
    Timing::CalculatedTiming calculated = SpecifiedTiming().CalculateTimings(
        local_time, timeline_phase, direction, IsA<KeyframeEffect>(this),
        playback_rate);

    const bool was_canceled = calculated.phase != calculated_.phase &&
                              calculated.phase == Timing::kPhaseNone;

    // If the animation was canceled, we need to fire the event condition before
    // updating the calculated timing so that the cancellation time can be
    // determined.
    if (was_canceled && event_delegate_) {
      event_delegate_->OnEventCondition(*this, calculated.phase);
    }

    calculated_ = calculated;
  }

  // Test for events even if timing didn't need an update as the animation may
  // have gained a start time.
  // FIXME: Refactor so that we can DCHECK(owner_) here, this is currently
  // required to be nullable for testing.
  if (reason == kTimingUpdateForAnimationFrame &&
      (!owner_ || owner_->IsEventDispatchAllowed())) {
    if (event_delegate_)
      event_delegate_->OnEventCondition(*this, calculated_.phase);
  }

  if (needs_update) {
    // FIXME: This probably shouldn't be recursive.
    UpdateChildrenAndEffects();
    calculated_.time_to_forwards_effect_change = CalculateTimeToEffectChange(
        true, local_time, calculated_.time_to_next_iteration);
    calculated_.time_to_reverse_effect_change = CalculateTimeToEffectChange(
        false, local_time, calculated_.time_to_next_iteration);
  }
}

void AnimationEffect::InvalidateAndNotifyOwner() const {
  Invalidate();
  if (owner_)
    owner_->EffectInvalidated();
}

const Timing::CalculatedTiming& AnimationEffect::EnsureCalculated() const {
  if (owner_)
    owner_->UpdateIfNecessary();
  return calculated_;
}

Animation* AnimationEffect::GetAnimation() {
  return owner_ ? owner_->GetAnimation() : nullptr;
}
const Animation* AnimationEffect::GetAnimation() const {
  return owner_ ? owner_->GetAnimation() : nullptr;
}

void AnimationEffect::Trace(Visitor* visitor) const {
  visitor->Trace(owner_);
  visitor->Trace(event_delegate_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
