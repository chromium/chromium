/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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
#include "third_party/blink/renderer/core/animation/timing_calculations.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(AnimationTimingCalculationsTest, ActiveTime) {
  Timing::NormalizedTiming normalized_timing;

  // calculateActiveTime(
  //     activeDuration, fillMode, localTime, parentPhase, phase, timing)

  // Before Phase
  normalized_timing.start_delay = ANIMATION_TIME_DELTA_FROM_SECONDS(10);
  normalized_timing.active_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(20);
  EXPECT_FALSE(CalculateActiveTime(normalized_timing,
                                   Timing::FillMode::FORWARDS,
                                   AnimationTimeDelta(), Timing::kPhaseBefore));
  EXPECT_FALSE(CalculateActiveTime(normalized_timing, Timing::FillMode::NONE,
                                   AnimationTimeDelta(), Timing::kPhaseBefore));
  EXPECT_EQ(AnimationTimeDelta(),
            CalculateActiveTime(normalized_timing, Timing::FillMode::BACKWARDS,
                                AnimationTimeDelta(), Timing::kPhaseBefore));
  EXPECT_EQ(AnimationTimeDelta(),
            CalculateActiveTime(normalized_timing, Timing::FillMode::BOTH,
                                AnimationTimeDelta(), Timing::kPhaseBefore));
  normalized_timing.start_delay = ANIMATION_TIME_DELTA_FROM_SECONDS(-10);
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(5),
            CalculateActiveTime(normalized_timing, Timing::FillMode::BACKWARDS,
                                ANIMATION_TIME_DELTA_FROM_SECONDS(-5),
                                Timing::kPhaseBefore));

  // Active Phase
  normalized_timing.start_delay = ANIMATION_TIME_DELTA_FROM_SECONDS(10);
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(5),
            CalculateActiveTime(normalized_timing, Timing::FillMode::FORWARDS,
                                ANIMATION_TIME_DELTA_FROM_SECONDS(15),
                                Timing::kPhaseActive));

  // After Phase
  normalized_timing.start_delay = ANIMATION_TIME_DELTA_FROM_SECONDS(10);
  normalized_timing.active_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(21);
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(21),
            CalculateActiveTime(normalized_timing, Timing::FillMode::FORWARDS,
                                ANIMATION_TIME_DELTA_FROM_SECONDS(45),
                                Timing::kPhaseAfter));
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(21),
            CalculateActiveTime(normalized_timing, Timing::FillMode::BOTH,
                                ANIMATION_TIME_DELTA_FROM_SECONDS(45),
                                Timing::kPhaseAfter));
  EXPECT_FALSE(CalculateActiveTime(
      normalized_timing, Timing::FillMode::BACKWARDS,
      ANIMATION_TIME_DELTA_FROM_SECONDS(45), Timing::kPhaseAfter));
  EXPECT_FALSE(CalculateActiveTime(normalized_timing, Timing::FillMode::NONE,
                                   ANIMATION_TIME_DELTA_FROM_SECONDS(45),
                                   Timing::kPhaseAfter));

  // None
  normalized_timing.active_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(32);
  EXPECT_FALSE(CalculateActiveTime(normalized_timing, Timing::FillMode::NONE,
                                   absl::nullopt, Timing::kPhaseNone));
}

TEST(AnimationTimingCalculationsTest, OffsetActiveTime) {
  // if the active time is null
  EXPECT_FALSE(CalculateOffsetActiveTime(ANIMATION_TIME_DELTA_FROM_SECONDS(4),
                                         absl::nullopt,
                                         ANIMATION_TIME_DELTA_FROM_SECONDS(5)));

  // normal case
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(15),
            CalculateOffsetActiveTime(ANIMATION_TIME_DELTA_FROM_SECONDS(40),
                                      ANIMATION_TIME_DELTA_FROM_SECONDS(10),
                                      ANIMATION_TIME_DELTA_FROM_SECONDS(5)));

  // infinite activeTime
  EXPECT_TRUE(CalculateOffsetActiveTime(AnimationTimeDelta::Max(),
                                        AnimationTimeDelta::Max(),
                                        AnimationTimeDelta())
                  ->is_max());

  // Edge case for active_time being within epsilon of active_duration.
  // https://crbug.com/962138
  auto active_time = ANIMATION_TIME_DELTA_FROM_SECONDS(1.3435713716800004);
  const auto active_duration =
      ANIMATION_TIME_DELTA_FROM_SECONDS(1.3435713716800002);
  EXPECT_EQ(active_time, CalculateOffsetActiveTime(active_duration, active_time,
                                                   AnimationTimeDelta()));
}

TEST(AnimationTimingCalculationsTest, IterationTime) {
  Timing timing;

  // calculateIterationTime(
  //     iterationDuration, activeDuration, scaledActiveTime, startOffset,
  //     phase, timing)

  // if the scaled active time is null
  EXPECT_FALSE(CalculateIterationTime(
      ANIMATION_TIME_DELTA_FROM_SECONDS(1),
      ANIMATION_TIME_DELTA_FROM_SECONDS(1), absl::nullopt,
      ANIMATION_TIME_DELTA_FROM_SECONDS(1), Timing::kPhaseActive, timing));

  // if (complex-conditions)...
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(12),
            CalculateIterationTime(ANIMATION_TIME_DELTA_FROM_SECONDS(12),
                                   ANIMATION_TIME_DELTA_FROM_SECONDS(12),
                                   ANIMATION_TIME_DELTA_FROM_SECONDS(12),
                                   AnimationTimeDelta(), Timing::kPhaseActive,
                                   timing));

  // otherwise
  timing.iteration_count = 10;
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(5),
            CalculateIterationTime(ANIMATION_TIME_DELTA_FROM_SECONDS(10),
                                   ANIMATION_TIME_DELTA_FROM_SECONDS(100),
                                   ANIMATION_TIME_DELTA_FROM_SECONDS(25),
                                   ANIMATION_TIME_DELTA_FROM_SECONDS(4),
                                   Timing::kPhaseActive, timing));
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(7),
            CalculateIterationTime(ANIMATION_TIME_DELTA_FROM_SECONDS(11),
                                   ANIMATION_TIME_DELTA_FROM_SECONDS(110),
                                   ANIMATION_TIME_DELTA_FROM_SECONDS(29),
                                   ANIMATION_TIME_DELTA_FROM_SECONDS(1),
                                   Timing::kPhaseActive, timing));
  timing.iteration_start = 1.1;
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(8),
            CalculateIterationTime(ANIMATION_TIME_DELTA_FROM_SECONDS(12),
                                   ANIMATION_TIME_DELTA_FROM_SECONDS(120),
                                   ANIMATION_TIME_DELTA_FROM_SECONDS(20),
                                   ANIMATION_TIME_DELTA_FROM_SECONDS(7),
                                   Timing::kPhaseActive, timing));

  // Edge case for offset_active_time being within epsilon of (active_duration
  // + start_offset). https://crbug.com/962138
  timing.iteration_count = 1;
  const AnimationTimeDelta offset_active_time =
      ANIMATION_TIME_DELTA_FROM_SECONDS(1.3435713716800004);
  const AnimationTimeDelta iteration_duration =
      ANIMATION_TIME_DELTA_FROM_SECONDS(1.3435713716800002);
  const AnimationTimeDelta active_duration =
      ANIMATION_TIME_DELTA_FROM_SECONDS(1.3435713716800002);
  EXPECT_NEAR(2.22045e-16,
              CalculateIterationTime(iteration_duration, active_duration,
                                     offset_active_time, AnimationTimeDelta(),
                                     Timing::kPhaseActive, timing)
                  ->InSecondsF(),
              std::numeric_limits<float>::epsilon());
}

TEST(AnimationTimingCalculationsTest, OverallProgress) {
  // If the active time is null.
  EXPECT_FALSE(CalculateOverallProgress(
      Timing::kPhaseAfter,
      /*active_time=*/absl::nullopt,
      /*iteration_duration=*/ANIMATION_TIME_DELTA_FROM_SECONDS(1.0),
      /*iteration_count=*/1.0,
      /*iteration_start=*/1.0));

  // If iteration duration is zero, calculate progress based on iteration count.
  EXPECT_EQ(3, CalculateOverallProgress(
                   Timing::kPhaseActive,
                   /*active_time=*/ANIMATION_TIME_DELTA_FROM_SECONDS(3.0),
                   /*iteration_duration=*/AnimationTimeDelta(),
                   /*iteration_count=*/3.0,
                   /*iteration_start=*/0.0));
  // ...unless in before phase, in which case progress is zero.
  EXPECT_EQ(0, CalculateOverallProgress(
                   Timing::kPhaseBefore,
                   /*active_time=*/ANIMATION_TIME_DELTA_FROM_SECONDS(3.0),
                   /*iteration_duration=*/AnimationTimeDelta(),
                   /*iteration_count=*/3.0,
                   /*iteration_start=*/0.0));
  // Edge case for duration being within Epsilon of zero.
  // crbug.com/954558
  EXPECT_EQ(1,
            CalculateOverallProgress(
                Timing::kPhaseActive,
                /*active_time=*/ANIMATION_TIME_DELTA_FROM_SECONDS(3.0),
                /*iteration_duration=*/ANIMATION_TIME_DELTA_FROM_SECONDS(1e-18),
                /*iteration_count=*/1.0,
                /*iteration_start=*/0.0));

  // Otherwise.
  EXPECT_EQ(3.0,
            CalculateOverallProgress(
                Timing::kPhaseAfter,
                /*active_time=*/ANIMATION_TIME_DELTA_FROM_SECONDS(2.5),
                /*iteration_duration=*/ANIMATION_TIME_DELTA_FROM_SECONDS(1.0),
                /*iteration_count=*/0.0,
                /*iteration_start=*/0.5));
}

TEST(AnimationTimingCalculationsTest, CalculateSimpleIterationProgress) {
  // If the overall progress is null.
  EXPECT_FALSE(CalculateSimpleIterationProgress(
      Timing::kPhaseAfter,
      /*overall_progress=*/absl::nullopt,
      /*iteration_start=*/1.0,
      /*active_time=*/absl::nullopt,
      /*active_duration=*/ANIMATION_TIME_DELTA_FROM_SECONDS(1.0),
      /*iteration_count=*/1.0));

  // If the overall progress is infinite.
  const double inf = std::numeric_limits<double>::infinity();
  EXPECT_EQ(0.5, CalculateSimpleIterationProgress(
                     Timing::kPhaseAfter,
                     /*overall_progress=*/inf,
                     /*iteration_start=*/1.5,
                     /*active_time=*/AnimationTimeDelta(),
                     /*active_duration=*/AnimationTimeDelta(),
                     /*iteration_count=*/inf));

  // Precisely on an iteration boundary.
  EXPECT_EQ(1.0, CalculateSimpleIterationProgress(
                     Timing::kPhaseAfter,
                     /*overall_progress=*/3.0,
                     /*iteration_start=*/0.0,
                     /*active_time=*/ANIMATION_TIME_DELTA_FROM_SECONDS(3.0),
                     /*active_duration=*/ANIMATION_TIME_DELTA_FROM_SECONDS(3.0),
                     /*iteration_count=*/3.0));

  // Otherwise.
  EXPECT_EQ(0.5, CalculateSimpleIterationProgress(
                     Timing::kPhaseAfter,
                     /*overall_progress=*/2.5,
                     /*iteration_start=*/0.0,
                     /*active_time=*/ANIMATION_TIME_DELTA_FROM_SECONDS(2.5),
                     /*active_duration=*/AnimationTimeDelta(),
                     /*iteration_count=*/0.0));
}

TEST(AnimationTimingCalculationsTest, CurrentIteration) {
  // If the active time is null.
  EXPECT_FALSE(CalculateCurrentIteration(Timing::kPhaseAfter,
                                         /*active_time=*/absl::nullopt,
                                         /*iteration_count=*/1.0,
                                         /*overall_progress=*/absl::nullopt,
                                         /*simple_iteration_progress=*/0));

  // If the iteration count is infinite.
  const double inf = std::numeric_limits<double>::infinity();
  EXPECT_EQ(inf, CalculateCurrentIteration(
                     Timing::kPhaseAfter,
                     /*active_time=*/ANIMATION_TIME_DELTA_FROM_SECONDS(1.0),
                     /*iteration_count=*/inf,
                     /*overall_progress=*/inf,
                     /*simple_iteration_progress=*/0.0));

  // Hold the endpoint of the final iteration of ending precisely on an
  // iteration boundary.
  EXPECT_EQ(2, CalculateCurrentIteration(
                   Timing::kPhaseAfter,
                   /*active_time=*/ANIMATION_TIME_DELTA_FROM_SECONDS(3.0),
                   /*iteration_count=*/3.0,
                   /*overall_progress=*/3.0,
                   /*simple_iteration_progress=*/1.0));

  // Edge case for zero-duration animation.
  // crbug.com/954558
  EXPECT_EQ(0, CalculateCurrentIteration(Timing::kPhaseAfter,
                                         /*active_time=*/AnimationTimeDelta(),
                                         /*iteration_count=*/1.0,
                                         /*overall_progress=*/0.0,
                                         /*simple_iteration_progress=*/1.0));

  // Otherwise.
  EXPECT_EQ(2, CalculateCurrentIteration(
                   Timing::kPhaseAfter,
                   /*active_time=*/ANIMATION_TIME_DELTA_FROM_SECONDS(2.5),
                   /*iteration_count=*/0.0,
                   /*overall_progress=*/2.5,
                   /*simple_iteration_progress=*/0.5));
}

TEST(AnimationTimingCalculationsTest, IsCurrentDirectionForwards) {
  // IsCurrentDirectionForwards(current_iteration,
  //                            direction);

  EXPECT_TRUE(IsCurrentDirectionForwards(0, Timing::PlaybackDirection::NORMAL));
  EXPECT_TRUE(IsCurrentDirectionForwards(1, Timing::PlaybackDirection::NORMAL));
  EXPECT_TRUE(IsCurrentDirectionForwards(
      0, Timing::PlaybackDirection::ALTERNATE_NORMAL));
  EXPECT_TRUE(IsCurrentDirectionForwards(
      1, Timing::PlaybackDirection::ALTERNATE_REVERSE));

  EXPECT_FALSE(
      IsCurrentDirectionForwards(0, Timing::PlaybackDirection::REVERSE));
  EXPECT_FALSE(
      IsCurrentDirectionForwards(1, Timing::PlaybackDirection::REVERSE));
  EXPECT_FALSE(IsCurrentDirectionForwards(
      0, Timing::PlaybackDirection::ALTERNATE_REVERSE));
  EXPECT_FALSE(IsCurrentDirectionForwards(
      1, Timing::PlaybackDirection::ALTERNATE_NORMAL));
}

TEST(AnimationTimingCalculationsTest, CalculateDirectedProgress) {
  // CalculateDirectedProgress(simple_iteration_progress,
  //                           current_iteration,
  //                           direction);

  // if the simple iteration progress is null
  EXPECT_FALSE(CalculateDirectedProgress(absl::nullopt, absl::nullopt,
                                         Timing::PlaybackDirection::NORMAL));

  // forwards
  EXPECT_EQ(0,
            CalculateDirectedProgress(0, 8, Timing::PlaybackDirection::NORMAL));
  EXPECT_EQ(1,
            CalculateDirectedProgress(1, 8, Timing::PlaybackDirection::NORMAL));
  EXPECT_EQ(0,
            CalculateDirectedProgress(0, 9, Timing::PlaybackDirection::NORMAL));
  EXPECT_EQ(1,
            CalculateDirectedProgress(1, 9, Timing::PlaybackDirection::NORMAL));
  EXPECT_EQ(0, CalculateDirectedProgress(
                   0, 8, Timing::PlaybackDirection::ALTERNATE_NORMAL));
  EXPECT_EQ(1, CalculateDirectedProgress(
                   1, 8, Timing::PlaybackDirection::ALTERNATE_NORMAL));
  EXPECT_EQ(0, CalculateDirectedProgress(
                   0, 9, Timing::PlaybackDirection::ALTERNATE_REVERSE));
  EXPECT_EQ(1, CalculateDirectedProgress(
                   1, 9, Timing::PlaybackDirection::ALTERNATE_REVERSE));

  // reverse
  EXPECT_EQ(
      1, CalculateDirectedProgress(0, 8, Timing::PlaybackDirection::REVERSE));
  EXPECT_EQ(
      0, CalculateDirectedProgress(1, 8, Timing::PlaybackDirection::REVERSE));
  EXPECT_EQ(
      1, CalculateDirectedProgress(0, 9, Timing::PlaybackDirection::REVERSE));
  EXPECT_EQ(
      0, CalculateDirectedProgress(1, 9, Timing::PlaybackDirection::REVERSE));
  EXPECT_EQ(1, CalculateDirectedProgress(
                   0, 9, Timing::PlaybackDirection::ALTERNATE_NORMAL));
  EXPECT_EQ(0, CalculateDirectedProgress(
                   1, 9, Timing::PlaybackDirection::ALTERNATE_NORMAL));
  EXPECT_EQ(1, CalculateDirectedProgress(
                   0, 8, Timing::PlaybackDirection::ALTERNATE_REVERSE));
  EXPECT_EQ(0, CalculateDirectedProgress(
                   1, 8, Timing::PlaybackDirection::ALTERNATE_REVERSE));
}

TEST(AnimationTimingCalculationsTest, TransformedProgress) {
  // CalculateTransformedProgress(
  //     phase, directed_progress,
  //     is_current_direction_forward, timing_function)

  scoped_refptr<TimingFunction> timing_function =
      StepsTimingFunction::Create(4, StepsTimingFunction::StepPosition::END);

  // directed_progress is null.
  EXPECT_FALSE(CalculateTransformedProgress(Timing::kPhaseActive, absl::nullopt,
                                            true, timing_function));

  // At step boundaries.
  // Forward direction.
  EXPECT_EQ(0, CalculateTransformedProgress(Timing::kPhaseBefore, 0, true,
                                            timing_function));
  EXPECT_EQ(0, CalculateTransformedProgress(Timing::kPhaseBefore, 0.25, true,
                                            timing_function));
  EXPECT_EQ(0.25, CalculateTransformedProgress(Timing::kPhaseAfter, 0.25, true,
                                               timing_function));
  EXPECT_EQ(0.25, CalculateTransformedProgress(Timing::kPhaseBefore, 0.5, true,
                                               timing_function));
  EXPECT_EQ(0.5, CalculateTransformedProgress(Timing::kPhaseAfter, 0.5, true,
                                              timing_function));
  EXPECT_EQ(0.5, CalculateTransformedProgress(Timing::kPhaseBefore, 0.75, true,
                                              timing_function));
  EXPECT_EQ(0.75, CalculateTransformedProgress(Timing::kPhaseAfter, 0.75, true,
                                               timing_function));
  EXPECT_EQ(0.75, CalculateTransformedProgress(Timing::kPhaseBefore, 1, true,
                                               timing_function));
  EXPECT_EQ(1, CalculateTransformedProgress(Timing::kPhaseAfter, 1, true,
                                            timing_function));
  // Reverse direction.
  EXPECT_EQ(1, CalculateTransformedProgress(Timing::kPhaseBefore, 1, false,
                                            timing_function));
  EXPECT_EQ(0.75, CalculateTransformedProgress(Timing::kPhaseAfter, 1, false,
                                               timing_function));
  EXPECT_EQ(0.75, CalculateTransformedProgress(Timing::kPhaseBefore, 0.75,
                                               false, timing_function));
  EXPECT_EQ(0.5, CalculateTransformedProgress(Timing::kPhaseAfter, 0.75, false,
                                              timing_function));
  EXPECT_EQ(0.5, CalculateTransformedProgress(Timing::kPhaseBefore, 0.5, false,
                                              timing_function));
  EXPECT_EQ(0.25, CalculateTransformedProgress(Timing::kPhaseAfter, 0.5, false,
                                               timing_function));
  EXPECT_EQ(0.25, CalculateTransformedProgress(Timing::kPhaseBefore, 0.25,
                                               false, timing_function));
  EXPECT_EQ(0, CalculateTransformedProgress(Timing::kPhaseAfter, 0.25, false,
                                            timing_function));

  // Edges cases
  EXPECT_EQ(1, CalculateTransformedProgress(Timing::kPhaseAfter, 1 - 1e-16,
                                            true, timing_function));
  scoped_refptr<TimingFunction> step_start_timing_function =
      StepsTimingFunction::Create(4, StepsTimingFunction::StepPosition::START);
  EXPECT_EQ(0, CalculateTransformedProgress(Timing::kPhaseAfter, 1e-16, false,
                                            step_start_timing_function));
}

}  // namespace blink
