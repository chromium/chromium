// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/timing_calculations.h"

#include "base/metrics/histogram_macros.h"

namespace blink {

namespace {

inline bool EndsOnIterationBoundary(double iteration_count,
                                    double iteration_start) {
  DCHECK(std::isfinite(iteration_count));
  return !fmod(iteration_count + iteration_start, 1);
}

void RecordBoundaryMisalignment(AnimationTimeDelta misalignment) {
  // Animations require 1 microsecond precision. For a scroll-based animation,
  // percentages are internally converted to time. The animation duration in
  // microseconds is 16 * (range in pixels).
  // Refer to cc/animations/scroll_timeline.h for details.
  //
  // It is not particularly meaningful to report the misalignment as a time
  // since there is no dependency on having a high resolution timer. Instead,
  // we convert back to 16ths of a pixel by scaling accordingly.
  int sample = std::round<int>(misalignment.InMicrosecondsF());
  UMA_HISTOGRAM_EXACT_LINEAR("Blink.Animation.SDA.BoundaryMisalignment", sample,
                             64);
}

}  // namespace

double TimingCalculations::TimingCalculationEpsilon() {
  // Permit 2-bits of quantization error. Threshold based on experimentation
  // with accuracy of fmod.
  return 2.0 * std::numeric_limits<double>::epsilon();
}

AnimationTimeDelta TimingCalculations::TimeTolerance() {
  return ANIMATION_TIME_DELTA_FROM_SECONDS(0.000001 /*one microsecond*/);
}

bool TimingCalculations::IsWithinAnimationTimeEpsilon(double a, double b) {
  return std::abs(a - b) <= TimingCalculationEpsilon();
}

bool TimingCalculations::IsWithinAnimationTimeTolerance(AnimationTimeDelta a,
                                                        AnimationTimeDelta b) {
  if (a.is_inf() || b.is_inf()) {
    return a == b;
  }
  AnimationTimeDelta difference = a >= b ? a - b : b - a;
  return difference <= TimeTolerance();
}

bool TimingCalculations::LessThanOrEqualToWithinEpsilon(double a, double b) {
  return a <= b + TimingCalculationEpsilon();
}

bool TimingCalculations::LessThanOrEqualToWithinTimeTolerance(
    AnimationTimeDelta a,
    AnimationTimeDelta b) {
  return a <= b + TimeTolerance();
}

bool TimingCalculations::GreaterThanOrEqualToWithinEpsilon(double a, double b) {
  return a >= b - TimingCalculationEpsilon();
}

bool TimingCalculations::GreaterThanOrEqualToWithinTimeTolerance(
    AnimationTimeDelta a,
    AnimationTimeDelta b) {
  return a >= b - TimeTolerance();
}

bool TimingCalculations::GreaterThanWithinTimeTolerance(AnimationTimeDelta a,
                                                        AnimationTimeDelta b) {
  return a > b - TimeTolerance();
}

double TimingCalculations::MultiplyZeroAlwaysGivesZero(double x, double y) {
  DCHECK(!std::isnan(x));
  DCHECK(!std::isnan(y));
  return x && y ? x * y : 0;
}

AnimationTimeDelta TimingCalculations::MultiplyZeroAlwaysGivesZero(
    AnimationTimeDelta x,
    double y) {
  DCHECK(!std::isnan(y));
  return x.is_zero() || y == 0 ? AnimationTimeDelta() : (x * y);
}

// https://w3.org/TR/web-animations-1/#animation-effect-phases-and-states
Timing::Phase TimingCalculations::CalculatePhase(
    const Timing::NormalizedTiming& normalized,
    std::optional<AnimationTimeDelta>& local_time,
    Timing::AnimationDirection direction) {
  DCHECK(GreaterThanOrEqualToWithinTimeTolerance(normalized.active_duration,
                                                 AnimationTimeDelta()));
  if (!local_time) {
    return Timing::kPhaseNone;
  }

  AnimationTimeDelta before_active_boundary_time =
      std::max(std::min(normalized.start_delay, normalized.end_time),
               AnimationTimeDelta());
  if (IsWithinAnimationTimeTolerance(local_time.value(),
                                     before_active_boundary_time)) {
    local_time = before_active_boundary_time;
  }

  if (local_time.value() < before_active_boundary_time) {
    if (normalized.is_start_boundary_aligned) {
      RecordBoundaryMisalignment(before_active_boundary_time -
                                 local_time.value());
    }
    return Timing::kPhaseBefore;
  }
  if ((direction == Timing::AnimationDirection::kBackwards &&
       local_time.value() == before_active_boundary_time &&
       !normalized.is_start_boundary_aligned)) {
    return Timing::kPhaseBefore;
  }

  AnimationTimeDelta active_after_boundary_time =
      std::max(std::min(normalized.start_delay + normalized.active_duration,
                        normalized.end_time),
               AnimationTimeDelta());
  if (IsWithinAnimationTimeTolerance(local_time.value(),
                                     active_after_boundary_time)) {
    local_time = active_after_boundary_time;
  }
  if (local_time.value() > active_after_boundary_time) {
    if (normalized.is_end_boundary_aligned) {
      RecordBoundaryMisalignment(local_time.value() -
                                 active_after_boundary_time);
    }
    return Timing::kPhaseAfter;
  }
  if ((direction == Timing::AnimationDirection::kForwards &&
       local_time.value() == active_after_boundary_time &&
       !normalized.is_end_boundary_aligned)) {
    return Timing::kPhaseAfter;
  }
  return Timing::kPhaseActive;
}

// https://w3.org/TR/web-animations-1/#calculating-the-active-time
std::optional<AnimationTimeDelta> TimingCalculations::CalculateActiveTime(
    const Timing::NormalizedTiming& normalized,
    Timing::FillMode fill_mode,
    std::optional<AnimationTimeDelta> local_time,
    Timing::Phase phase) {
  DCHECK(GreaterThanOrEqualToWithinTimeTolerance(normalized.active_duration,
                                                 AnimationTimeDelta()));
  switch (phase) {
    case Timing::kPhaseBefore:
      if (fill_mode == Timing::FillMode::BACKWARDS ||
          fill_mode == Timing::FillMode::BOTH) {
        DCHECK(local_time.has_value());
        return std::max(local_time.value() - normalized.start_delay,
                        AnimationTimeDelta());
      }
      return std::nullopt;
    case Timing::kPhaseActive:
      DCHECK(local_time.has_value());
      return local_time.value() - normalized.start_delay;
    case Timing::kPhaseAfter:
      if (fill_mode == Timing::FillMode::FORWARDS ||
          fill_mode == Timing::FillMode::BOTH) {
        DCHECK(local_time.has_value());
        return std::max(AnimationTimeDelta(),
                        std::min(normalized.active_duration,
                                 local_time.value() - normalized.start_delay));
      }
      return std::nullopt;
    case Timing::kPhaseNone:
      DCHECK(!local_time.has_value());
      return std::nullopt;
    default:
      NOTREACHED_IN_MIGRATION();
      return std::nullopt;
  }
}

// Calculates the overall progress, which describes the number of iterations
// that have completed (including partial iterations).
// https://w3.org/TR/web-animations-1/#calculating-the-overall-progress
std::optional<double> TimingCalculations::CalculateOverallProgress(
    Timing::Phase phase,
    std::optional<AnimationTimeDelta> active_time,
    AnimationTimeDelta iteration_duration,
    double iteration_count,
    double iteration_start) {
  // 1. If the active time is unresolved, return unresolved.
  if (!active_time) {
    return std::nullopt;
  }

  // 2. Calculate an initial value for overall progress.
  double overall_progress = 0;
  if (IsWithinAnimationTimeTolerance(iteration_duration,
                                     AnimationTimeDelta())) {
    if (phase != Timing::kPhaseBefore) {
      overall_progress = iteration_count;
    }
  } else {
    overall_progress = (active_time.value() / iteration_duration);
  }

  return overall_progress + iteration_start;
}

// Calculates the simple iteration progress, which is a fraction of the progress
// through the current iteration that ignores transformations to the time
// introduced by the playback direction or timing functions applied to the
// effect.
// https://w3.org/TR/web-animations-1/#calculating-the-simple-iteration-progress
std::optional<double> TimingCalculations::CalculateSimpleIterationProgress(
    Timing::Phase phase,
    std::optional<double> overall_progress,
    double iteration_start,
    std::optional<AnimationTimeDelta> active_time,
    AnimationTimeDelta active_duration,
    double iteration_count) {
  // 1. If the overall progress is unresolved, return unresolved.
  if (!overall_progress) {
    return std::nullopt;
  }

  // 2. If overall progress is infinity, let the simple iteration progress be
  // iteration start % 1.0, otherwise, let the simple iteration progress be
  // overall progress % 1.0.
  double simple_iteration_progress = std::isinf(overall_progress.value())
                                         ? fmod(iteration_start, 1.0)
                                         : fmod(overall_progress.value(), 1.0);

  // active_time is not null is because overall_progress != null and
  // CalculateOverallProgress() only returns null when active_time is null.
  DCHECK(active_time);

  // 3. If all of the following conditions are true,
  //   * the simple iteration progress calculated above is zero, and
  //   * the animation effect is in the active phase or the after phase, and
  //   * the active time is equal to the active duration, and
  //   * the iteration count is not equal to zero.
  // let the simple iteration progress be 1.0.
  if (IsWithinAnimationTimeEpsilon(simple_iteration_progress, 0.0) &&
      (phase == Timing::kPhaseActive || phase == Timing::kPhaseAfter) &&
      IsWithinAnimationTimeTolerance(active_time.value(), active_duration) &&
      !IsWithinAnimationTimeEpsilon(iteration_count, 0.0)) {
    simple_iteration_progress = 1.0;
  }

  // 4. Return simple iteration progress.
  return simple_iteration_progress;
}

// https://w3.org/TR/web-animations-1/#calculating-the-current-iteration
std::optional<double> TimingCalculations::CalculateCurrentIteration(
    Timing::Phase phase,
    std::optional<AnimationTimeDelta> active_time,
    double iteration_count,
    std::optional<double> overall_progress,
    std::optional<double> simple_iteration_progress) {
  // 1. If the active time is unresolved, return unresolved.
  if (!active_time) {
    return std::nullopt;
  }

  // 2. If the animation effect is in the after phase and the iteration count
  // is infinity, return infinity.
  if (phase == Timing::kPhaseAfter && std::isinf(iteration_count)) {
    return std::numeric_limits<double>::infinity();
  }

  if (!overall_progress) {
    return std::nullopt;
  }

  // simple iteration progress can only be null if overall progress is null.
  DCHECK(simple_iteration_progress);

  // 3. If the simple iteration progress is 1.0, return floor(overall progress)
  // - 1.
  if (simple_iteration_progress.value() == 1.0) {
    // Safeguard for zero duration animation (crbug.com/954558).
    return fmax(0, floor(overall_progress.value()) - 1);
  }

  // 4. Otherwise, return floor(overall progress).
  return floor(overall_progress.value());
}

// https://w3.org/TR/web-animations-1/#calculating-the-directed-progress
bool TimingCalculations::IsCurrentDirectionForwards(
    std::optional<double> current_iteration,
    Timing::PlaybackDirection direction) {
  const bool current_iteration_is_even =
      !current_iteration ? false
                         : (std::isinf(current_iteration.value())
                                ? true
                                : IsWithinAnimationTimeEpsilon(
                                      fmod(current_iteration.value(), 2), 0));

  switch (direction) {
    case Timing::PlaybackDirection::NORMAL:
      return true;

    case Timing::PlaybackDirection::REVERSE:
      return false;

    case Timing::PlaybackDirection::ALTERNATE_NORMAL:
      return current_iteration_is_even;

    case Timing::PlaybackDirection::ALTERNATE_REVERSE:
      return !current_iteration_is_even;
  }
}

// https://w3.org/TR/web-animations-1/#calculating-the-directed-progress
std::optional<double> TimingCalculations::CalculateDirectedProgress(
    std::optional<double> simple_iteration_progress,
    std::optional<double> current_iteration,
    Timing::PlaybackDirection direction) {
  // 1. If the simple progress is unresolved, return unresolved.
  if (!simple_iteration_progress) {
    return std::nullopt;
  }

  // 2. Calculate the current direction.
  bool current_direction_is_forwards =
      IsCurrentDirectionForwards(current_iteration, direction);

  // 3. If the current direction is forwards then return the simple iteration
  // progress. Otherwise return 1 - simple iteration progress.
  return current_direction_is_forwards ? simple_iteration_progress.value()
                                       : 1 - simple_iteration_progress.value();
}

// https://w3.org/TR/web-animations-1/#calculating-the-transformed-progress
std::optional<double> TimingCalculations::CalculateTransformedProgress(
    Timing::Phase phase,
    std::optional<double> directed_progress,
    bool is_current_direction_forward,
    scoped_refptr<TimingFunction> timing_function) {
  if (!directed_progress) {
    return std::nullopt;
  }

  // Set the before flag to indicate if at the leading edge of an iteration.
  // This is used to determine if the left or right limit should be used if at a
  // discontinuity in the timing function.
  bool before = is_current_direction_forward ? phase == Timing::kPhaseBefore
                                             : phase == Timing::kPhaseAfter;
  TimingFunction::LimitDirection limit_direction =
      before ? TimingFunction::LimitDirection::LEFT
             : TimingFunction::LimitDirection::RIGHT;

  // Snap boundaries to correctly render step timing functions at 0 and 1.
  // (crbug.com/949373)
  if (phase == Timing::kPhaseAfter) {
    if (is_current_direction_forward &&
        IsWithinAnimationTimeEpsilon(directed_progress.value(), 1)) {
      directed_progress = 1;
    } else if (!is_current_direction_forward &&
               IsWithinAnimationTimeEpsilon(directed_progress.value(), 0)) {
      directed_progress = 0;
    }
  }

  // Return the result of evaluating the animation effect’s timing function
  // passing directed progress as the input progress value.
  return timing_function->Evaluate(directed_progress.value(), limit_direction);
}

// Offsets the active time by how far into the animation we start (i.e. the
// product of the iteration start and iteration duration). This is not part of
// the Web Animations spec; it is used for calculating the time until the next
// iteration to optimize scheduling.
std::optional<AnimationTimeDelta> TimingCalculations::CalculateOffsetActiveTime(
    AnimationTimeDelta active_duration,
    std::optional<AnimationTimeDelta> active_time,
    AnimationTimeDelta start_offset) {
  DCHECK(GreaterThanOrEqualToWithinTimeTolerance(active_duration,
                                                 AnimationTimeDelta()));
  DCHECK(GreaterThanOrEqualToWithinTimeTolerance(start_offset,
                                                 AnimationTimeDelta()));

  if (!active_time) {
    return std::nullopt;
  }

  DCHECK(GreaterThanOrEqualToWithinTimeTolerance(active_time.value(),
                                                 AnimationTimeDelta()) &&
         LessThanOrEqualToWithinTimeTolerance(active_time.value(),
                                              active_duration));

  if (active_time->is_max()) {
    return AnimationTimeDelta::Max();
  }

  return active_time.value() + start_offset;
}

// Maps the offset active time into 'iteration time space'[0], aka the offset
// into the current iteration. This is not part of the Web Animations spec (note
// that the section linked below is non-normative); it is used for calculating
// the time until the next iteration to optimize scheduling.
//
// [0] https://w3.org/TR/web-animations-1/#iteration-time-space
std::optional<AnimationTimeDelta> TimingCalculations::CalculateIterationTime(
    AnimationTimeDelta iteration_duration,
    AnimationTimeDelta active_duration,
    std::optional<AnimationTimeDelta> offset_active_time,
    AnimationTimeDelta start_offset,
    Timing::Phase phase,
    const Timing& specified) {
  DCHECK(
      GreaterThanWithinTimeTolerance(iteration_duration, AnimationTimeDelta()));
  DCHECK(IsWithinAnimationTimeTolerance(
      active_duration, MultiplyZeroAlwaysGivesZero(iteration_duration,
                                                   specified.iteration_count)));

  if (!offset_active_time) {
    return std::nullopt;
  }

  DCHECK(GreaterThanWithinTimeTolerance(offset_active_time.value(),
                                        AnimationTimeDelta()));
  DCHECK(LessThanOrEqualToWithinTimeTolerance(
      offset_active_time.value(), (active_duration + start_offset)));

  if (offset_active_time->is_max() ||
      (IsWithinAnimationTimeTolerance(offset_active_time.value() - start_offset,
                                      active_duration) &&
       specified.iteration_count &&
       EndsOnIterationBoundary(specified.iteration_count,
                               specified.iteration_start))) {
    return std::make_optional(iteration_duration);
  }

  DCHECK(!offset_active_time->is_max());
  AnimationTimeDelta iteration_time = ANIMATION_TIME_DELTA_FROM_SECONDS(
      fmod(offset_active_time->InSecondsF(), iteration_duration.InSecondsF()));

  // This implements step 3 of
  // https://w3.org/TR/web-animations-1/#calculating-the-simple-iteration-progress
  if (iteration_time.is_zero() && phase == Timing::kPhaseAfter &&
      !active_duration.is_zero() && !offset_active_time.value().is_zero()) {
    return std::make_optional(iteration_duration);
  }

  return iteration_time;
}

}  // namespace blink
