// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/timing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_computed_effect_timing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_effect_timing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_range_offset.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_string_unrestricteddouble.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_double_string.h"
#include "third_party/blink/renderer/core/animation/timing_calculations.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_values.h"

namespace blink {

Timing::V8Delay* Timing::Delay::ToV8Delay() const {
  // TODO(crbug.com/1216527) support delay as percentage.
  return MakeGarbageCollected<V8Delay>(AsTimeValue().InMillisecondsF());
}

V8FillMode::Enum Timing::FillModeEnum(FillMode fill_mode) {
  switch (fill_mode) {
    case FillMode::NONE:
      return V8FillMode::Enum::kNone;
    case FillMode::FORWARDS:
      return V8FillMode::Enum::kForwards;
    case FillMode::BACKWARDS:
      return V8FillMode::Enum::kBackwards;
    case FillMode::BOTH:
      return V8FillMode::Enum::kBoth;
    case FillMode::AUTO:
      return V8FillMode::Enum::kAuto;
  }
}

Timing::FillMode Timing::EnumToFillMode(V8FillMode::Enum fill_mode) {
  switch (fill_mode) {
    case V8FillMode::Enum::kNone:
      return Timing::FillMode::NONE;
    case V8FillMode::Enum::kBackwards:
      return Timing::FillMode::BACKWARDS;
    case V8FillMode::Enum::kBoth:
      return Timing::FillMode::BOTH;
    case V8FillMode::Enum::kForwards:
      return Timing::FillMode::FORWARDS;
    case V8FillMode::Enum::kAuto:
      return Timing::FillMode::AUTO;
  }
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
  NOTREACHED_IN_MIGRATION();
  return "normal";
}

Timing::FillMode Timing::ResolvedFillMode(bool is_keyframe_effect) const {
  if (fill_mode != Timing::FillMode::AUTO)
    return fill_mode;

  // https://w3.org/TR/web-animations-1/#the-effecttiming-dictionaries
  if (is_keyframe_effect)
    return Timing::FillMode::NONE;
  return Timing::FillMode::BOTH;
}

EffectTiming* Timing::ConvertToEffectTiming() const {
  EffectTiming* effect_timing = EffectTiming::Create();

  // Specified values used here so that inputs match outputs for JS API calls
  effect_timing->setDelay(start_delay.ToV8Delay());
  effect_timing->setEndDelay(end_delay.ToV8Delay());
  effect_timing->setFill(FillModeEnum(fill_mode));
  effect_timing->setIterationStart(iteration_start);
  effect_timing->setIterations(iteration_count);
  V8UnionCSSNumericValueOrStringOrUnrestrictedDouble* duration;
  if (iteration_duration) {
    duration = MakeGarbageCollected<
        V8UnionCSSNumericValueOrStringOrUnrestrictedDouble>(
        iteration_duration->InMillisecondsF());
  } else {
    duration = MakeGarbageCollected<
        V8UnionCSSNumericValueOrStringOrUnrestrictedDouble>("auto");
  }
  effect_timing->setDuration(duration);
  effect_timing->setDirection(PlaybackDirectionString(direction));
  effect_timing->setEasing(timing_function->ToString());

  return effect_timing;
}

// Converts values to CSSNumberish based on corresponding timeline type
V8CSSNumberish* Timing::ToComputedValue(
    std::optional<AnimationTimeDelta> time,
    std::optional<AnimationTimeDelta> max_time) const {
  if (time) {
    // A valid timeline_duration indicates use of progress based timeline. We
    // need to convert values to percentages using timeline_duration as 100%
    if (max_time) {
      return MakeGarbageCollected<V8CSSNumberish>(
          CSSUnitValues::percent((time.value() / max_time.value()) * 100));
    } else {
      // For time based timeline, simply return the value in milliseconds.
      return MakeGarbageCollected<V8CSSNumberish>(
          time.value().InMillisecondsF());
    }
  }
  return nullptr;
}

ComputedEffectTiming* Timing::getComputedTiming(
    const CalculatedTiming& calculated_timing,
    const NormalizedTiming& normalized_timing,
    bool is_keyframe_effect) const {
  ComputedEffectTiming* computed_timing = ComputedEffectTiming::Create();

  // ComputedEffectTiming members.
  computed_timing->setEndTime(ToComputedValue(
      normalized_timing.end_time, normalized_timing.timeline_duration));
  computed_timing->setActiveDuration(ToComputedValue(
      normalized_timing.active_duration, normalized_timing.timeline_duration));
  computed_timing->setLocalTime(ToComputedValue(
      calculated_timing.local_time, normalized_timing.timeline_duration));

  if (calculated_timing.is_in_effect) {
    DCHECK(calculated_timing.current_iteration);
    DCHECK(calculated_timing.progress);
    computed_timing->setProgress(calculated_timing.progress.value());
    computed_timing->setCurrentIteration(
        calculated_timing.current_iteration.value());
  } else {
    computed_timing->setProgress(std::nullopt);
    computed_timing->setCurrentIteration(std::nullopt);
  }

  // For the EffectTiming members, getComputedTiming is equivalent to getTiming
  // except that the fill and duration must be resolved.
  //
  // https://w3.org/TR/web-animations-1/#dom-animationeffect-getcomputedtiming

  // TODO(crbug.com/1216527): Animation effect timing members start_delay and
  // end_delay should be CSSNumberish
  computed_timing->setDelay(start_delay.ToV8Delay());
  computed_timing->setEndDelay(end_delay.ToV8Delay());
  computed_timing->setFill(
      Timing::FillModeEnum(ResolvedFillMode(is_keyframe_effect)));
  computed_timing->setIterationStart(iteration_start);
  computed_timing->setIterations(iteration_count);

  V8CSSNumberish* computed_duration =
      ToComputedValue(normalized_timing.iteration_duration,
                      normalized_timing.timeline_duration);
  if (computed_duration->IsCSSNumericValue()) {
    if (normalized_timing.timeline_duration) {
      computed_timing->setDuration(
          MakeGarbageCollected<
              V8UnionCSSNumericValueOrStringOrUnrestrictedDouble>(
              computed_duration->GetAsCSSNumericValue()));
    }
  } else {
    computed_timing->setDuration(
        MakeGarbageCollected<
            V8UnionCSSNumericValueOrStringOrUnrestrictedDouble>(
            computed_duration->GetAsDouble()));
  }

  computed_timing->setDirection(Timing::PlaybackDirectionString(direction));
  computed_timing->setEasing(timing_function->ToString());

  return computed_timing;
}

Timing::CalculatedTiming Timing::CalculateTimings(
    std::optional<AnimationTimeDelta> local_time,
    bool is_idle,
    const NormalizedTiming& normalized_timing,
    AnimationDirection animation_direction,
    bool is_keyframe_effect,
    std::optional<double> playback_rate) const {
  const AnimationTimeDelta active_duration = normalized_timing.active_duration;
  const AnimationTimeDelta duration = normalized_timing.iteration_duration;

  Timing::Phase current_phase = TimingCalculations::CalculatePhase(
      normalized_timing, local_time, animation_direction);

  const std::optional<AnimationTimeDelta> active_time =
      TimingCalculations::CalculateActiveTime(
          normalized_timing, ResolvedFillMode(is_keyframe_effect), local_time,
          current_phase);

  std::optional<double> progress;

  const std::optional<double> overall_progress =
      TimingCalculations::CalculateOverallProgress(current_phase, active_time,
                                                   duration, iteration_count,
                                                   iteration_start);
  const std::optional<double> simple_iteration_progress =
      TimingCalculations::CalculateSimpleIterationProgress(
          current_phase, overall_progress, iteration_start, active_time,
          active_duration, iteration_count);
  const std::optional<double> current_iteration =
      TimingCalculations::CalculateCurrentIteration(
          current_phase, active_time, iteration_count, overall_progress,
          simple_iteration_progress);
  const bool current_direction_is_forwards =
      TimingCalculations::IsCurrentDirectionForwards(current_iteration,
                                                     direction);
  const std::optional<double> directed_progress =
      TimingCalculations::CalculateDirectedProgress(
          simple_iteration_progress, current_iteration, direction);

  progress = TimingCalculations::CalculateTransformedProgress(
      current_phase, directed_progress, current_direction_is_forwards,
      timing_function);

  AnimationTimeDelta time_to_next_iteration = AnimationTimeDelta::Max();
  // Conditionally compute the time to next iteration, which is only
  // applicable if the iteration duration is non-zero.
  if (!duration.is_zero()) {
    const AnimationTimeDelta start_offset =
        TimingCalculations::MultiplyZeroAlwaysGivesZero(duration,
                                                        iteration_start);
    DCHECK_GE(start_offset, AnimationTimeDelta());
    const std::optional<AnimationTimeDelta> offset_active_time =
        TimingCalculations::CalculateOffsetActiveTime(
            active_duration, active_time, start_offset);
    const std::optional<AnimationTimeDelta> iteration_time =
        TimingCalculations::CalculateIterationTime(
            duration, active_duration, offset_active_time, start_offset,
            current_phase, *this);
    if (iteration_time) {
      // active_time cannot be null if iteration_time is not null.
      DCHECK(active_time);
      time_to_next_iteration = duration - iteration_time.value();
      if (active_duration - active_time.value() < time_to_next_iteration)
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

  // https://w3.org/TR/web-animations-1/#current
  calculated.is_current = calculated.is_in_play ||
                          (playback_rate.has_value() && playback_rate > 0 &&
                           calculated.phase == Timing::kPhaseBefore) ||
                          (playback_rate.has_value() && playback_rate < 0 &&
                           calculated.phase == Timing::kPhaseAfter) ||
                          (!is_idle && normalized_timing.timeline_duration);

  calculated.local_time = local_time;
  calculated.time_to_next_iteration = time_to_next_iteration;

  return calculated;
}

}  // namespace blink
