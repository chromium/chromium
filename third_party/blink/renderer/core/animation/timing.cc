// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_computed_effect_timing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_effect_timing.h"
#include "third_party/blink/renderer/core/animation/timing_calculations.h"

namespace blink {

String Timing::FillModeString(FillMode fill_mode) {
  switch (fill_mode) {
    case FillMode::NONE:
      return "none";
    case FillMode::FORWARDS:
      return "forwards";
    case FillMode::BACKWARDS:
      return "backwards";
    case FillMode::BOTH:
      return "both";
    case FillMode::AUTO:
      return "auto";
  }
  NOTREACHED();
  return "none";
}

Timing::FillMode Timing::StringToFillMode(const String& fill_mode) {
  if (fill_mode == "none")
    return Timing::FillMode::NONE;
  if (fill_mode == "backwards")
    return Timing::FillMode::BACKWARDS;
  if (fill_mode == "both")
    return Timing::FillMode::BOTH;
  if (fill_mode == "forwards")
    return Timing::FillMode::FORWARDS;
  DCHECK_EQ(fill_mode, "auto");
  return Timing::FillMode::AUTO;
}

String Timing::PlaybackDirectionString(PlaybackDirection playback_direction) {
  switch (playback_direction) {
    case PlaybackDirection::NORMAL:
      return "normal";
    case PlaybackDirection::REVERSE:
      return "reverse";
    case PlaybackDirection::ALTERNATE_NORMAL:
      return "alternate";
    case PlaybackDirection::ALTERNATE_REVERSE:
      return "alternate-reverse";
  }
  NOTREACHED();
  return "normal";
}

Timing::FillMode Timing::ResolvedFillMode(bool is_keyframe_effect) const {
  if (fill_mode != Timing::FillMode::AUTO)
    return fill_mode;

  // https://drafts.csswg.org/web-animations/#the-effecttiming-dictionaries
  if (is_keyframe_effect)
    return Timing::FillMode::NONE;
  return Timing::FillMode::BOTH;
}

AnimationTimeDelta Timing::IterationDuration() const {
  AnimationTimeDelta result = iteration_duration.value_or(AnimationTimeDelta());
  DCHECK_GE(result, AnimationTimeDelta());
  return result;
}

double Timing::ActiveDuration() const {
  const double result =
      MultiplyZeroAlwaysGivesZero(IterationDuration(), iteration_count);
  DCHECK_GE(result, 0);
  return result;
}

double Timing::EndTimeInternal() const {
  // Per the spec, the end time has a lower bound of 0.0:
  // https://drafts.csswg.org/web-animations-1/#end-time
  return std::max(start_delay + ActiveDuration() + end_delay, 0.0);
}

EffectTiming* Timing::ConvertToEffectTiming() const {
  EffectTiming* effect_timing = EffectTiming::Create();

  effect_timing->setDelay(start_delay * 1000);
  effect_timing->setEndDelay(end_delay * 1000);
  effect_timing->setFill(FillModeString(fill_mode));
  effect_timing->setIterationStart(iteration_start);
  effect_timing->setIterations(iteration_count);
  UnrestrictedDoubleOrString duration;
  if (iteration_duration) {
    duration.SetUnrestrictedDouble(iteration_duration->InMillisecondsF());
  } else {
    duration.SetString("auto");
  }
  effect_timing->setDuration(duration);
  effect_timing->setDirection(PlaybackDirectionString(direction));
  effect_timing->setEasing(timing_function->ToString());

  return effect_timing;
}

ComputedEffectTiming* Timing::getComputedTiming(
    const CalculatedTiming& calculated_timing,
    bool is_keyframe_effect) const {
  ComputedEffectTiming* computed_timing = ComputedEffectTiming::Create();

  // ComputedEffectTiming members.
  computed_timing->setEndTime(EndTimeInternal() * 1000);
  computed_timing->setActiveDuration(ActiveDuration() * 1000);

  if (calculated_timing.local_time)
    computed_timing->setLocalTime(calculated_timing.local_time.value() * 1000);
  else
    computed_timing->setLocalTimeToNull();

  if (calculated_timing.is_in_effect) {
    DCHECK(calculated_timing.current_iteration);
    DCHECK(calculated_timing.progress);
    computed_timing->setProgress(calculated_timing.progress.value());
    computed_timing->setCurrentIteration(
        calculated_timing.current_iteration.value());
  } else {
    computed_timing->setProgressToNull();
    computed_timing->setCurrentIterationToNull();
  }

  // For the EffectTiming members, getComputedTiming is equivalent to getTiming
  // except that the fill and duration must be resolved.
  //
  // https://drafts.csswg.org/web-animations-1/#dom-animationeffect-getcomputedtiming
  computed_timing->setDelay(start_delay * 1000);
  computed_timing->setEndDelay(end_delay * 1000);
  computed_timing->setFill(
      Timing::FillModeString(ResolvedFillMode(is_keyframe_effect)));
  computed_timing->setIterationStart(iteration_start);
  computed_timing->setIterations(iteration_count);

  UnrestrictedDoubleOrString duration;
  duration.SetUnrestrictedDouble(IterationDuration().InMillisecondsF());
  computed_timing->setDuration(duration);

  computed_timing->setDirection(Timing::PlaybackDirectionString(direction));
  computed_timing->setEasing(timing_function->ToString());

  return computed_timing;
}

Timing::CalculatedTiming Timing::CalculateTimings(
    base::Optional<double> local_time,
    base::Optional<Phase> timeline_phase,
    AnimationDirection animation_direction,
    bool is_keyframe_effect,
    base::Optional<double> playback_rate) const {
  const double active_duration = ActiveDuration();

  Timing::Phase current_phase = CalculatePhase(
      active_duration, local_time, timeline_phase, animation_direction, *this);

  const base::Optional<AnimationTimeDelta> active_time =
      CalculateActiveTime(active_duration, ResolvedFillMode(is_keyframe_effect),
                          local_time, current_phase, *this);

  base::Optional<double> progress;
  const double local_iteration_duration = IterationDuration().InSecondsF();

  const base::Optional<double> overall_progress = CalculateOverallProgress(
      current_phase, active_time, local_iteration_duration, iteration_count,
      iteration_start);
  const base::Optional<double> simple_iteration_progress =
      CalculateSimpleIterationProgress(current_phase, overall_progress,
                                       iteration_start, active_time,
                                       active_duration, iteration_count);
  const base::Optional<double> current_iteration =
      CalculateCurrentIteration(current_phase, active_time, iteration_count,
                                overall_progress, simple_iteration_progress);
  const bool current_direction_is_forwards =
      IsCurrentDirectionForwards(current_iteration, direction);
  const base::Optional<double> directed_progress = CalculateDirectedProgress(
      simple_iteration_progress, current_iteration, direction);

  progress = CalculateTransformedProgress(current_phase, directed_progress,
                                          current_direction_is_forwards,
                                          timing_function);

  AnimationTimeDelta time_to_next_iteration = AnimationTimeDelta::Max();
  // Conditionally compute the time to next iteration, which is only
  // applicable if the iteration duration is non-zero.
  if (local_iteration_duration) {
    const double start_offset =
        MultiplyZeroAlwaysGivesZero(iteration_start, local_iteration_duration);
    DCHECK_GE(start_offset, 0);
    const base::Optional<AnimationTimeDelta> offset_active_time =
        CalculateOffsetActiveTime(active_duration, active_time, start_offset);
    const base::Optional<AnimationTimeDelta> iteration_time =
        CalculateIterationTime(local_iteration_duration, active_duration,
                               offset_active_time, start_offset, current_phase,
                               *this);
    if (iteration_time) {
      // active_time cannot be null if iteration_time is not null.
      DCHECK(active_time);
      time_to_next_iteration =
          AnimationTimeDelta::FromSecondsD(local_iteration_duration) -
          iteration_time.value();
      if (AnimationTimeDelta::FromSecondsD(active_duration) -
              active_time.value() <
          time_to_next_iteration)
        time_to_next_iteration = AnimationTimeDelta::Max();
    }
  }

  CalculatedTiming calculated = CalculatedTiming();
  calculated.phase = current_phase;
  calculated.current_iteration = current_iteration;
  calculated.progress = progress;
  calculated.is_in_effect = active_time.has_value();
  // If active_time is not null then current_iteration and (transformed)
  // progress are also non-null).
  DCHECK(!calculated.is_in_effect ||
         (current_iteration.has_value() && progress.has_value()));
  calculated.is_in_play = calculated.phase == Timing::kPhaseActive;
  // https://drafts.csswg.org/web-animations-1/#current
  calculated.is_current = calculated.is_in_play ||
                          (playback_rate.has_value() && playback_rate > 0 &&
                           calculated.phase == Timing::kPhaseBefore) ||
                          (playback_rate.has_value() && playback_rate < 0 &&
                           calculated.phase == Timing::kPhaseAfter);
  calculated.local_time = local_time;
  calculated.time_to_next_iteration = time_to_next_iteration;

  return calculated;
}

}  // namespace blink
