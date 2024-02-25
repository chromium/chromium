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
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_string_unrestricteddouble.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/animation_input_helpers.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/timing_calculations.h"
#include "third_party/blink/renderer/core/animation/timing_input.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"

namespace blink {

namespace {

void UseCountEffectTimingDelayZero(Document& document, const Timing& timing) {
  if (timing.iteration_duration == AnimationTimeDelta()) {
    UseCounter::Count(document, WebFeature::kGetEffectTimingDelayZero);
  }
}

}  // namespace

AnimationEffect::AnimationEffect(const Timing& timing,
                                 EventDelegate* event_delegate)
    : owner_(nullptr),
      timing_(timing),
      event_delegate_(event_delegate),
      needs_update_(true),
      cancel_time_(AnimationTimeDelta()) {
  timing_.AssertValid();
  InvalidateNormalizedTiming();
}

AnimationTimeDelta AnimationEffect::IntrinsicIterationDuration() const {
  if (auto* animation = GetAnimation()) {
    auto* timeline = animation->TimelineInternal();
    if (timeline) {
      return timeline->CalculateIntrinsicIterationDuration(animation, timing_);
    }
  }
  return AnimationTimeDelta();
}

// Scales all timing values so that end_time == timeline_duration
// https://drafts.csswg.org/web-animations-2/#time-based-animation-to-a-proportional-animation
void AnimationEffect::EnsureNormalizedTiming() const {
  // Only run the normalization process if needed
  if (normalized_)
    return;

  normalized_ = Timing::NormalizedTiming();
  // A valid timeline duration signifies use of a progress based timeline.
  if (TimelineDuration()) {
    // Normalize timings for progress based timelines
    normalized_->timeline_duration = TimelineDuration();

    // TODO(crbug.com/1216527): Refactor for animation-range + delays. Still
    // some details to sort out in the spec when mixing delays and range
    // offsets. What happens if you have an animation range and time based
    // delays?
    if (timing_.iteration_duration) {
      // TODO(kevers): We can probably get rid of this branch and just
      // ignore all timing that is not % based.  A fair number of tests still
      // rely on this branch though, so will need to update the tests
      // accordingly to see if they are still relevant.

      // Scaling up iteration_duration allows animation effect to be able to
      // handle values produced by progress based timelines. At this point it
      // can be assumed that EndTimeInternal() will give us a good value.

      const AnimationTimeDelta active_duration =
          TimingCalculations::MultiplyZeroAlwaysGivesZero(
              timing_.iteration_duration.value(), timing_.iteration_count);
      DCHECK_GE(active_duration, AnimationTimeDelta());

      // Per the spec, the end time has a lower bound of 0.0:
      // https://w3.org/TR/web-animations-1/#end-time
      const AnimationTimeDelta end_time =
          std::max(timing_.start_delay.AsTimeValue() + active_duration +
                       timing_.end_delay.AsTimeValue(),
                   AnimationTimeDelta());

      // Negative start_delay that is >= iteration_duration or iteration_count
      // of 0 will cause end_time to be 0 or negative.
      if (end_time.is_zero()) {
        // end_time of zero causes division by zero so we handle it here
        normalized_->start_delay = AnimationTimeDelta();
        normalized_->end_delay = AnimationTimeDelta();
        normalized_->iteration_duration = AnimationTimeDelta();
      } else if (end_time.is_inf()) {
        // The iteration count or duration may be infinite; however, start and
        // end delays are strictly finite. Thus, in the limit when end time
        // approaches infinity:
        //    start delay / end time = finite / infinite = 0
        //    end delay / end time = finite / infinite = 0
        //    iteration duration / end time = 1 / iteration count
        // This condition can be reached by switching to a scroll timeline on
        // an existing infinite duration animation.
        // Note that base::TimeDelta::operator/() DCHECKS that the numerator and
        // denominator cannot both be zero or both be infinite since both cases
        // are undefined. Fortunately, we can evaluate the limit in the infinite
        // end time case based on the definition of end time
        normalized_->start_delay = AnimationTimeDelta();
        normalized_->end_delay = AnimationTimeDelta();
        normalized_->iteration_duration =
            (1.0 / timing_.iteration_count) *
            normalized_->timeline_duration.value();
      } else {
        // End time is not 0 or infinite.
        // Convert to percentages then multiply by the timeline_duration

        // TODO(kevers): Revisit once % delays are supported. At present,
        // % delays are zero and the following product aligns with the animation
        // range. Note the range duration will need to be plumbed through to
        // InertEffect via CSSAnimationProxy. One more reason to try and get rid
        // of InertEffect.
        AnimationTimeDelta range_duration =
            IntrinsicIterationDuration() * timing_.iteration_count;

        normalized_->start_delay =
            (timing_.start_delay.AsTimeValue() / end_time) * range_duration;

        normalized_->end_delay =
            (timing_.end_delay.AsTimeValue() / end_time) * range_duration;

        normalized_->iteration_duration =
            (timing_.iteration_duration.value() / end_time) * range_duration;
      }
    } else {
      // Default (auto) duration with a non-monotonic timeline case.
      // TODO(crbug.com/1216527): Update timing once ratified in the spec.
      // Normalized timing is purely used internally in order to keep the bulk
      // of the animation code time-based.
      normalized_->iteration_duration = IntrinsicIterationDuration();
      AnimationTimeDelta active_duration =
          normalized_->iteration_duration * timing_.iteration_count;
      double start_delay = timing_.start_delay.relative_delay.value_or(0);
      double end_delay = timing_.end_delay.relative_delay.value_or(0);

      if (active_duration > AnimationTimeDelta()) {
        double active_percent = (1 - start_delay - end_delay);
        AnimationTimeDelta end_time = active_duration / active_percent;
        normalized_->start_delay = end_time * start_delay;
        normalized_->end_delay = end_time * end_delay;
      } else {
        // TODO(kevers): This is not quite correct as the delays should probably
        // be divided proportionately.
        normalized_->start_delay = AnimationTimeDelta();
        normalized_->end_delay = TimelineDuration().value();
      }
    }
  } else {
    // Monotonic timeline case.
    // Populates normalized values for use with time based timelines.
    normalized_->start_delay = timing_.start_delay.AsTimeValue();
    normalized_->end_delay = timing_.end_delay.AsTimeValue();
    normalized_->iteration_duration =
        timing_.iteration_duration.value_or(AnimationTimeDelta());
  }

  normalized_->active_duration =
      TimingCalculations::MultiplyZeroAlwaysGivesZero(
          normalized_->iteration_duration, timing_.iteration_count);

  // Per the spec, the end time has a lower bound of 0.0:
  // https://w3.org/TR/web-animations-1/#end-time#end-time
  normalized_->end_time =
      std::max(normalized_->start_delay + normalized_->active_duration +
                   normalized_->end_delay,
               AnimationTimeDelta());

  // Determine if boundary aligned to indicate if the active-(before|after)
  // phase boundary is inclusive or exclusive.
  if (GetAnimation()) {
    GetAnimation()->UpdateBoundaryAlignment(normalized_.value());
  }
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

  InvalidateNormalizedTiming();
  InvalidateAndNotifyOwner();
}

void AnimationEffect::SetIgnoreCssTimingProperties() {
  timing_.SetTimingOverride(Timing::kOverrideAll);
}

EffectTiming* AnimationEffect::getTiming() const {
  if (const Animation* animation = GetAnimation()) {
    animation->FlushPendingUpdates();
    UseCountEffectTimingDelayZero(*animation->GetDocument(), SpecifiedTiming());
  }
  return SpecifiedTiming().ConvertToEffectTiming();
}

ComputedEffectTiming* AnimationEffect::getComputedTiming() {
  // A composited animation does not need to tick main frame updates, and
  // the cached state for localTime can become stale.
  if (Animation* animation = GetAnimation()) {
    std::optional<AnimationTimeDelta> current_time =
        animation->CurrentTimeInternal();
    if (current_time != last_update_time_ || animation->Outdated()) {
      animation->Update(kTimingUpdateOnDemand);
    }
  }

  return SpecifiedTiming().getComputedTiming(
      EnsureCalculated(), NormalizedTiming(), IsA<KeyframeEffect>(this));
}

void AnimationEffect::updateTiming(OptionalEffectTiming* optional_timing,
                                   ExceptionState& exception_state) {
  if (GetAnimation() && GetAnimation()->TimelineInternal() &&
      GetAnimation()->TimelineInternal()->IsProgressBased()) {
    if (optional_timing->hasDuration()) {
      if (optional_timing->duration()->IsUnrestrictedDouble()) {
        double duration =
            optional_timing->duration()->GetAsUnrestrictedDouble();
        if (duration == std::numeric_limits<double>::infinity()) {
          exception_state.ThrowTypeError(
              "Effect duration cannot be Infinity when used with Scroll "
              "Timelines");
          return;
        }
      } else if (optional_timing->duration()->GetAsString() == "auto") {
        // TODO(crbug.com/1216527)
        // Eventually we hope to be able to be more flexible with
        // iteration_duration "auto" and its interaction with start_delay and
        // end_delay. For now we will throw an exception if either delay is set.
        // Once delays are changed to CSSNumberish, we will need to adjust logic
        // here to allow for percentage values but not time values.

        // If either delay or end_delay are non-zero, we can't handle "auto"
        if (SpecifiedTiming().start_delay.IsNonzeroTimeBasedDelay() ||
            SpecifiedTiming().end_delay.IsNonzeroTimeBasedDelay()) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kNotSupportedError,
              "Effect duration \"auto\" with time delays is not yet "
              "implemented when used with Scroll Timelines");
          return;
        }
      }
    }

    if (optional_timing->hasIterations() &&
        optional_timing->iterations() ==
            std::numeric_limits<double>::infinity()) {
      // iteration count of infinity makes no sense for scroll timelines
      exception_state.ThrowTypeError(
          "Effect iterations cannot be Infinity when used with Scroll "
          "Timelines");
      return;
    }
  }

  // TODO(crbug.com/827178): Determine whether we should pass a Document in here
  // (and which) to resolve the CSS secure/insecure context against.
  if (!TimingInput::Update(timing_, optional_timing, nullptr, exception_state))
    return;

  InvalidateNormalizedTiming();
  InvalidateAndNotifyOwner();
}

void AnimationEffect::UpdateInheritedTime(
    std::optional<AnimationTimeDelta> inherited_time,
    bool is_idle,
    double inherited_playback_rate,
    TimingUpdateReason reason) const {
  const Timing::AnimationDirection direction =
      (inherited_playback_rate < 0) ? Timing::AnimationDirection::kBackwards
                                    : Timing::AnimationDirection::kForwards;

  bool needs_update = needs_update_ || last_update_time_ != inherited_time ||
                      last_is_idle_ != is_idle ||
                      (owner_ && owner_->EffectSuppressed());
  needs_update_ = false;
  last_update_time_ = inherited_time;
  last_is_idle_ = is_idle;

  if (needs_update) {
    Timing::CalculatedTiming calculated = SpecifiedTiming().CalculateTimings(
        inherited_time, is_idle, NormalizedTiming(), direction,
        IsA<KeyframeEffect>(this), inherited_playback_rate);

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
        true, inherited_time, calculated_.time_to_next_iteration);
    calculated_.time_to_reverse_effect_change = CalculateTimeToEffectChange(
        false, inherited_time, calculated_.time_to_next_iteration);
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
