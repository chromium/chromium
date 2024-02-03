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

#include "third_party/blink/renderer/core/animation/timing_calculations.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(AnimationTimingCalculationsTest, ActiveTime) {
  test::TaskEnvironment task_environment;
  Timing::NormalizedTiming normalized_timing;

  // calculateActiveTime(
  //     activeDuration, fillMode, localTime, parentPhase, phase, timing)

  // Before Phase
  normalized_timing.start_delay = ANIMATION_TIME_DELTA_FROM_SECONDS(10);
  normalized_timing.active_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(20);
  EXPECT_FALSE(TimingCalculations::CalculateActiveTime(
      normalized_timing, Timing::FillMode::FORWARDS, AnimationTimeDelta(),
      Timing::kPhaseBefore));
  EXPECT_FALSE(TimingCalculations::CalculateActiveTime(
      normalized_timing, Timing::FillMode::NONE, AnimationTimeDelta(),
      Timing::kPhaseBefore));
  EXPECT_EQ(AnimationTimeDelta(),
            TimingCalculations::CalculateActiveTime(
                normalized_timing, Timing::FillMode::BACKWARDS,
                AnimationTimeDelta(), Timing::kPhaseBefore));
  EXPECT_EQ(AnimationTimeDelta(),
            TimingCalculations::CalculateActiveTime(
                normalized_timing, Timing::FillMode::BOTH, AnimationTimeDelta(),
                Timing::kPhaseBefore));
  normalized_timing.start_delay = ANIMATION_TIME_DELTA_FROM_SECONDS(-10);
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(5),
            TimingCalculations::CalculateActiveTime(
                normalized_timing, Timing::FillMode::BACKWARDS,
                ANIMATION_TIME_DELTA_FROM_SECONDS(-5), Timing::kPhaseBefore));

  // Active Phase
  normalized_timing.start_delay = ANIMATION_TIME_DELTA_FROM_SECONDS(10);
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(5),
            TimingCalculations::CalculateActiveTime(
                normalized_timing, Timing::FillMode::FORWARDS,
                ANIMATION_TIME_DELTA_FROM_SECONDS(15), Timing::kPhaseActive));

  // After Phase
  normalized_timing.start_delay = ANIMATION_TIME_DELTA_FROM_SECONDS(10);
  normalized_timing.active_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(21);
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(21),
            TimingCalculations::CalculateActiveTime(
                normalized_timing, Timing::FillMode::FORWARDS,
                ANIMATION_TIME_DELTA_FROM_SECONDS(45), Timing::kPhaseAfter));
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(21),
            TimingCalculations::CalculateActiveTime(
                normalized_timing, Timing::FillMode::BOTH,
                ANIMATION_TIME_DELTA_FROM_SECONDS(45), Timing::kPhaseAfter));
  EXPECT_FALSE(TimingCalculations::CalculateActiveTime(
      normalized_timing, Timing::FillMode::BACKWARDS,
      ANIMATION_TIME_DELTA_FROM_SECONDS(45), Timing::kPhaseAfter));
  EXPECT_FALSE(TimingCalculations::CalculateActiveTime(
      normalized_timing, Timing::FillMode::NONE,
      ANIMATION_TIME_DELTA_FROM_SECONDS(45), Timing::kPhaseAfter));

  // None
  normalized_timing.active_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(32);
  EXPECT_FALSE(TimingCalculations::CalculateActiveTime(
      normalized_timing, Timing::FillMode::NONE, std::nullopt,
      Timing::kPhaseNone));
}

TEST(AnimationTimingCalculationsTest, OffsetActiveTime) {
  test::TaskEnvironment task_environment;
  // if the active time is null
  EXPECT_FALSE(TimingCalculations::CalculateOffsetActiveTime(
      ANIMATION_TIME_DELTA_FROM_SECONDS(4), std::nullopt,
      ANIMATION_TIME_DELTA_FROM_SECONDS(5)));

  // normal case
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(15),
            TimingCalculations::CalculateOffsetActiveTime(
                ANIMATION_TIME_DELTA_FROM_SECONDS(40),
                ANIMATION_TIME_DELTA_FROM_SECONDS(10),
                ANIMATION_TIME_DELTA_FROM_SECONDS(5)));

  // infinite activeTime
  EXPECT_TRUE(TimingCalculations::CalculateOffsetActiveTime(
                  AnimationTimeDelta::Max(), AnimationTimeDelta::Max(),
                  AnimationTimeDelta())
                  ->is_max());

  // Edge case for active_time being within epsilon of active_duration.
  // https://crbug.com/962138
  auto active_time = ANIMATION_TIME_DELTA_FROM_SECONDS(1.3435713716800004);
  const auto active_duration =
      ANIMATION_TIME_DELTA_FROM_SECONDS(1.3435713716800002);
  EXPECT_EQ(active_time,
            TimingCalculations::CalculateOffsetActiveTime(
                active_duration, active_time, AnimationTimeDelta()));
}

TEST(AnimationTimingCalculationsTest, IterationTime) {
  test::TaskEnvironment task_environment;
  Timing timing;

  // calculateIterationTime(
  //     iterationDuration, activeDuration, scaledActiveTime, startOffset,
  //     phase, timing)

  // if the scaled active time is null
  EXPECT_FALSE(TimingCalculations::CalculateIterationTime(
      ANIMATION_TIME_DELTA_FROM_SECONDS(1),
      ANIMATION_TIME_DELTA_FROM_SECONDS(1), std::nullopt,
      ANIMATION_TIME_DELTA_FROM_SECONDS(1), Timing::kPhaseActive, timing));

  // if (complex-conditions)...
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(12),
            TimingCalculations::CalculateIterationTime(
                ANIMATION_TIME_DELTA_FROM_SECONDS(12),
                ANIMATION_TIME_DELTA_FROM_SECONDS(12),
                ANIMATION_TIME_DELTA_FROM_SECONDS(12), AnimationTimeDelta(),
                Timing::kPhaseActive, timing));

  // otherwise
  timing.iteration_count = 10;
  EXPECT_EQ(
      ANIMATION_TIME_DELTA_FROM_SECONDS(5),
      TimingCalculations::CalculateIterationTime(
          ANIMATION_TIME_DELTA_FROM_SECONDS(10),
          ANIMATION_TIME_DELTA_FROM_SECONDS(100),
          ANIMATION_TIME_DELTA_FROM_SECONDS(25),
          ANIMATION_TIME_DELTA_FROM_SECONDS(4), Timing::kPhaseActive, timing));
  EXPECT_EQ(
      ANIMATION_TIME_DELTA_FROM_SECONDS(7),
      TimingCalculations::CalculateIterationTime(
          ANIMATION_TIME_DELTA_FROM_SECONDS(11),
          ANIMATION_TIME_DELTA_FROM_SECONDS(110),
          ANIMATION_TIME_DELTA_FROM_SECONDS(29),
          ANIMATION_TIME_DELTA_FROM_SECONDS(1), Timing::kPhaseActive, timing));
  timing.iteration_start = 1.1;
  EXPECT_EQ(
      ANIMATION_TIME_DELTA_FROM_SECONDS(8),
      TimingCalculations::CalculateIterationTime(
          ANIMATION_TIME_DELTA_FROM_SECONDS(12),
          ANIMATION_TIME_DELTA_FROM_SECONDS(120),
          ANIMATION_TIME_DELTA_FROM_SECONDS(20),
          ANIMATION_TIME_DELTA_FROM_SECONDS(7), Timing::kPhaseActive, timing));

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
              TimingCalculations::CalculateIterationTime(
                  iteration_duration, active_duration, offset_active_time,
                  AnimationTimeDelta(), Timing::kPhaseActive, timing)
                  ->InSecondsF(),
              std::numeric_limits<float>::epsilon());
}

TEST(AnimationTimingCalculationsTest, OverallProgress) {
  test::TaskEnvironment task_environment;
  // If the active time is null.
  EXPECT_FALSE(TimingCalculations::CalculateOverallProgress(
      Timing::kPhaseAfter,
      /*active_time=*/std::nullopt,
      /*iteration_duration=*/ANIMATION_TIME_DELTA_FROM_SECONDS(1.0),
      /*iteration_count=*/1.0,
      /*iteration_start=*/1.0));

  // If iteration duration is zero, calculate progress based on iteration count.
  EXPECT_EQ(3, TimingCalculations::CalculateOverallProgress(
                   Timing::kPhaseActive,
                   /*active_time=*/ANIMATION_TIME_DELTA_FROM_SECONDS(3.0),
                   /*iteration_duration=*/AnimationTimeDelta(),
                   /*iteration_count=*/3.0,
                   /*iteration_start=*/0.0));
  // ...unless in before phase, in which case progress is zero.
  EXPECT_EQ(0, TimingCalculations::CalculateOverallProgress(
                   Timing::kPhaseBefore,
                   /*active_time=*/ANIMATION_TIME_DELTA_FROM_SECONDS(3.0),
                   /*iteration_duration=*/AnimationTimeDelta(),
                   /*iteration_count=*/3.0,
                   /*iteration_start=*/0.0));
  // Edge case for duration being within Epsilon of zero.
  // crbug.com/954558
  EXPECT_EQ(1,
            TimingCalculations::CalculateOverallProgress(
                Timing::kPhaseActive,
                /*active_time=*/ANIMATION_TIME_DELTA_FROM_SECONDS(3.0),
                /*iteration_duration=*/ANIMATION_TIME_DELTA_FROM_SECONDS(1e-18),
                /*iteration_count=*/1.0,
                /*iteration_start=*/0.0));

  // Otherwise.
  EXPECT_EQ(3.0,
            TimingCalculations::CalculateOverallProgress(
                Timing::kPhaseAfter,
                /*active_time=*/ANIMATION_TIME_DELTA_FROM_SECONDS(2.5),
                /*iteration_duration=*/ANIMATION_TIME_DELTA_FROM_SECONDS(1.0),
                /*iteration_count=*/0.0,
                /*iteration_start=*/0.5));
}

TEST(AnimationTimingCalculationsTest, CalculateSimpleIterationProgress) {
  test::TaskEnvironment task_environment;
  // If the overall progress is null.
  EXPECT_FALSE(TimingCalculations::CalculateSimpleIterationProgress(
      Timing::kPhaseAfter,
      /*overall_progress=*/std::nullopt,
      /*iteration_start=*/1.0,
      /*active_time=*/std::nullopt,
      /*active_duration=*/ANIMATION_TIME_DELTA_FROM_SECONDS(1.0),
      /*iteration_count=*/1.0));

  // If the overall progress is infinite.
  const double inf = std::numeric_limits<double>::infinity();
  EXPECT_EQ(0.5, TimingCalculations::CalculateSimpleIterationProgress(
                     Timing::kPhaseAfter,
                     /*overall_progress=*/inf,
                     /*iteration_start=*/1.5,
                     /*active_time=*/AnimationTimeDelta(),
                     /*active_duration=*/AnimationTimeDelta(),
                     /*iteration_count=*/inf));

  // Precisely on an iteration boundary.
  EXPECT_EQ(1.0, TimingCalculations::CalculateSimpleIterationProgress(
                     Timing::kPhaseAfter,
                     /*overall_progress=*/3.0,
                     /*iteration_start=*/0.0,
                     /*active_time=*/ANIMATION_TIME_DELTA_FROM_SECONDS(3.0),
                     /*active_duration=*/ANIMATION_TIME_DELTA_FROM_SECONDS(3.0),
                     /*iteration_count=*/3.0));

  // Otherwise.
  EXPECT_EQ(0.5, TimingCalculations::CalculateSimpleIterationProgress(
                     Timing::kPhaseAfter,
                     /*overall_progress=*/2.5,
                     /*iteration_start=*/0.0,
                     /*active_time=*/ANIMATION_TIME_DELTA_FROM_SECONDS(2.5),
                     /*active_duration=*/AnimationTimeDelta(),
                     /*iteration_count=*/0.0));
}

TEST(AnimationTimingCalculationsTest, CurrentIteration) {
  test::TaskEnvironment task_environment;
  // If the active time is null.
  EXPECT_FALSE(TimingCalculations::CalculateCurrentIteration(
      Timing::kPhaseAfter,
      /*active_time=*/std::nullopt,
      /*iteration_count=*/1.0,
      /*overall_progress=*/std::nullopt,
      /*simple_iteration_progress=*/0));

  // If the iteration count is infinite.
  const double inf = std::numeric_limits<double>::infinity();
  EXPECT_EQ(inf, TimingCalculations::CalculateCurrentIteration(
                     Timing::kPhaseAfter,
                     /*active_time=*/ANIMATION_TIME_DELTA_FROM_SECONDS(1.0),
                     /*iteration_count=*/inf,
                     /*overall_progress=*/inf,
                     /*simple_iteration_progress=*/0.0));

  // Hold the endpoint of the final iteration of ending precisely on an
  // iteration boundary.
  EXPECT_EQ(2, TimingCalculations::CalculateCurrentIteration(
                   Timing::kPhaseAfter,
                   /*active_time=*/ANIMATION_TIME_DELTA_FROM_SECONDS(3.0),
                   /*iteration_count=*/3.0,
                   /*overall_progress=*/3.0,
                   /*simple_iteration_progress=*/1.0));

  // Edge case for zero-duration animation.
  // crbug.com/954558
  EXPECT_EQ(0, TimingCalculations::CalculateCurrentIteration(
                   Timing::kPhaseAfter,
                   /*active_time=*/AnimationTimeDelta(),
                   /*iteration_count=*/1.0,
                   /*overall_progress=*/0.0,
                   /*simple_iteration_progress=*/1.0));

  // Otherwise.
  EXPECT_EQ(2, TimingCalculations::CalculateCurrentIteration(
                   Timing::kPhaseAfter,
                   /*active_time=*/ANIMATION_TIME_DELTA_FROM_SECONDS(2.5),
                   /*iteration_count=*/0.0,
                   /*overall_progress=*/2.5,
                   /*simple_iteration_progress=*/0.5));
}

TEST(AnimationTimingCalculationsTest, IsCurrentDirectionForwards) {
  test::TaskEnvironment task_environment;
  // IsCurrentDirectionForwards(current_iteration,
  //                            direction);

  EXPECT_TRUE(TimingCalculations::IsCurrentDirectionForwards(
      0, Timing::PlaybackDirection::NORMAL));
  EXPECT_TRUE(TimingCalculations::IsCurrentDirectionForwards(
      1, Timing::PlaybackDirection::NORMAL));
  EXPECT_TRUE(TimingCalculations::IsCurrentDirectionForwards(
      0, Timing::PlaybackDirection::ALTERNATE_NORMAL));
  EXPECT_TRUE(TimingCalculations::IsCurrentDirectionForwards(
      1, Timing::PlaybackDirection::ALTERNATE_REVERSE));

  EXPECT_FALSE(TimingCalculations::IsCurrentDirectionForwards(
      0, Timing::PlaybackDirection::REVERSE));
  EXPECT_FALSE(TimingCalculations::IsCurrentDirectionForwards(
      1, Timing::PlaybackDirection::REVERSE));
  EXPECT_FALSE(TimingCalculations::IsCurrentDirectionForwards(
      0, Timing::PlaybackDirection::ALTERNATE_REVERSE));
  EXPECT_FALSE(TimingCalculations::IsCurrentDirectionForwards(
      1, Timing::PlaybackDirection::ALTERNATE_NORMAL));
}

TEST(AnimationTimingCalculationsTest, CalculateDirectedProgress) {
  test::TaskEnvironment task_environment;
  // CalculateDirectedProgress(simple_iteration_progress,
  //                           current_iteration,
  //                           direction);

  // if the simple iteration progress is null
  EXPECT_FALSE(TimingCalculations::CalculateDirectedProgress(
      std::nullopt, std::nullopt, Timing::PlaybackDirection::NORMAL));

  // forwards
  EXPECT_EQ(0, TimingCalculations::CalculateDirectedProgress(
                   0, 8, Timing::PlaybackDirection::NORMAL));
  EXPECT_EQ(1, TimingCalculations::CalculateDirectedProgress(
                   1, 8, Timing::PlaybackDirection::NORMAL));
  EXPECT_EQ(0, TimingCalculations::CalculateDirectedProgress(
                   0, 9, Timing::PlaybackDirection::NORMAL));
  EXPECT_EQ(1, TimingCalculations::CalculateDirectedProgress(
                   1, 9, Timing::PlaybackDirection::NORMAL));
  EXPECT_EQ(0, TimingCalculations::CalculateDirectedProgress(
                   0, 8, Timing::PlaybackDirection::ALTERNATE_NORMAL));
  EXPECT_EQ(1, TimingCalculations::CalculateDirectedProgress(
                   1, 8, Timing::PlaybackDirection::ALTERNATE_NORMAL));
  EXPECT_EQ(0, TimingCalculations::CalculateDirectedProgress(
                   0, 9, Timing::PlaybackDirection::ALTERNATE_REVERSE));
  EXPECT_EQ(1, TimingCalculations::CalculateDirectedProgress(
                   1, 9, Timing::PlaybackDirection::ALTERNATE_REVERSE));

  // reverse
  EXPECT_EQ(1, TimingCalculations::CalculateDirectedProgress(
                   0, 8, Timing::PlaybackDirection::REVERSE));
  EXPECT_EQ(0, TimingCalculations::CalculateDirectedProgress(
                   1, 8, Timing::PlaybackDirection::REVERSE));
  EXPECT_EQ(1, TimingCalculations::CalculateDirectedProgress(
                   0, 9, Timing::PlaybackDirection::REVERSE));
  EXPECT_EQ(0, TimingCalculations::CalculateDirectedProgress(
                   1, 9, Timing::PlaybackDirection::REVERSE));
  EXPECT_EQ(1, TimingCalculations::CalculateDirectedProgress(
                   0, 9, Timing::PlaybackDirection::ALTERNATE_NORMAL));
  EXPECT_EQ(0, TimingCalculations::CalculateDirectedProgress(
                   1, 9, Timing::PlaybackDirection::ALTERNATE_NORMAL));
  EXPECT_EQ(1, TimingCalculations::CalculateDirectedProgress(
                   0, 8, Timing::PlaybackDirection::ALTERNATE_REVERSE));
  EXPECT_EQ(0, TimingCalculations::CalculateDirectedProgress(
                   1, 8, Timing::PlaybackDirection::ALTERNATE_REVERSE));
}

TEST(AnimationTimingCalculationsTest, TransformedProgress) {
  test::TaskEnvironment task_environment;
  // CalculateTransformedProgress(
  //     phase, directed_progress,
  //     is_current_direction_forward, timing_function)

  scoped_refptr<TimingFunction> timing_function =
      StepsTimingFunction::Create(4, StepsTimingFunction::StepPosition::END);

  // directed_progress is null.
  EXPECT_FALSE(TimingCalculations::CalculateTransformedProgress(
      Timing::kPhaseActive, std::nullopt, true, timing_function));

  // At step boundaries.
  // Forward direction.
  EXPECT_EQ(0, TimingCalculations::CalculateTransformedProgress(
                   Timing::kPhaseBefore, 0, true, timing_function));
  EXPECT_EQ(0, TimingCalculations::CalculateTransformedProgress(
                   Timing::kPhaseBefore, 0.25, true, timing_function));
  EXPECT_EQ(0.25, TimingCalculations::CalculateTransformedProgress(
                      Timing::kPhaseAfter, 0.25, true, timing_function));
  EXPECT_EQ(0.25, TimingCalculations::CalculateTransformedProgress(
                      Timing::kPhaseBefore, 0.5, true, timing_function));
  EXPECT_EQ(0.5, TimingCalculations::CalculateTransformedProgress(
                     Timing::kPhaseAfter, 0.5, true, timing_function));
  EXPECT_EQ(0.5, TimingCalculations::CalculateTransformedProgress(
                     Timing::kPhaseBefore, 0.75, true, timing_function));
  EXPECT_EQ(0.75, TimingCalculations::CalculateTransformedProgress(
                      Timing::kPhaseAfter, 0.75, true, timing_function));
  EXPECT_EQ(0.75, TimingCalculations::CalculateTransformedProgress(
                      Timing::kPhaseBefore, 1, true, timing_function));
  EXPECT_EQ(1, TimingCalculations::CalculateTransformedProgress(
                   Timing::kPhaseAfter, 1, true, timing_function));
  // Reverse direction.
  EXPECT_EQ(1, TimingCalculations::CalculateTransformedProgress(
                   Timing::kPhaseBefore, 1, false, timing_function));
  EXPECT_EQ(0.75, TimingCalculations::CalculateTransformedProgress(
                      Timing::kPhaseAfter, 1, false, timing_function));
  EXPECT_EQ(0.75, TimingCalculations::CalculateTransformedProgress(
                      Timing::kPhaseBefore, 0.75, false, timing_function));
  EXPECT_EQ(0.5, TimingCalculations::CalculateTransformedProgress(
                     Timing::kPhaseAfter, 0.75, false, timing_function));
  EXPECT_EQ(0.5, TimingCalculations::CalculateTransformedProgress(
                     Timing::kPhaseBefore, 0.5, false, timing_function));
  EXPECT_EQ(0.25, TimingCalculations::CalculateTransformedProgress(
                      Timing::kPhaseAfter, 0.5, false, timing_function));
  EXPECT_EQ(0.25, TimingCalculations::CalculateTransformedProgress(
                      Timing::kPhaseBefore, 0.25, false, timing_function));
  EXPECT_EQ(0, TimingCalculations::CalculateTransformedProgress(
                   Timing::kPhaseAfter, 0.25, false, timing_function));

  // Edges cases
  EXPECT_EQ(1, TimingCalculations::CalculateTransformedProgress(
                   Timing::kPhaseAfter, 1 - 1e-16, true, timing_function));
  scoped_refptr<TimingFunction> step_start_timing_function =
      StepsTimingFunction::Create(4, StepsTimingFunction::StepPosition::START);
  EXPECT_EQ(0,
            TimingCalculations::CalculateTransformedProgress(
                Timing::kPhaseAfter, 1e-16, false, step_start_timing_function));
}

TEST(AnimationTimingCalculationsTest, AlignmentHistogram) {
  test::TaskEnvironment task_environment;
  Timing::NormalizedTiming normalized_timing;
  normalized_timing.active_duration = ANIMATION_TIME_DELTA_FROM_MILLISECONDS(1);
  normalized_timing.end_time = ANIMATION_TIME_DELTA_FROM_SECONDS(1);
  std::optional<AnimationTimeDelta> local_time =
      ANIMATION_TIME_DELTA_FROM_MILLISECONDS(1);

  const std::string histogram_name = "Blink.Animation.SDA.BoundaryMisalignment";
  base::HistogramTester histogram_tester;

  EXPECT_EQ(Timing::kPhaseAfter, TimingCalculations::CalculatePhase(
                                     normalized_timing, local_time,
                                     Timing::AnimationDirection::kForwards));
  histogram_tester.ExpectBucketCount(histogram_name, 0, 0);

  normalized_timing.is_start_boundary_aligned = true;
  EXPECT_EQ(Timing::kPhaseAfter, TimingCalculations::CalculatePhase(
                                     normalized_timing, local_time,
                                     Timing::AnimationDirection::kForwards));
  histogram_tester.ExpectBucketCount(histogram_name, 0, 0);

  normalized_timing.is_end_boundary_aligned = true;
  EXPECT_EQ(Timing::kPhaseActive, TimingCalculations::CalculatePhase(
                                      normalized_timing, local_time,
                                      Timing::AnimationDirection::kForwards));
  histogram_tester.ExpectBucketCount(histogram_name, 0, 0);

  local_time = ANIMATION_TIME_DELTA_FROM_MILLISECONDS(1.003);
  EXPECT_EQ(Timing::kPhaseAfter, TimingCalculations::CalculatePhase(
                                     normalized_timing, local_time,
                                     Timing::AnimationDirection::kForwards));
  histogram_tester.ExpectBucketCount(histogram_name, 3, 1);

  // Repeat and ensure the counter increments.
  EXPECT_EQ(Timing::kPhaseAfter, TimingCalculations::CalculatePhase(
                                     normalized_timing, local_time,
                                     Timing::AnimationDirection::kForwards));
  histogram_tester.ExpectBucketCount(histogram_name, 3, 2);

  normalized_timing.is_end_boundary_aligned = false;
  EXPECT_EQ(Timing::kPhaseAfter, TimingCalculations::CalculatePhase(
                                     normalized_timing, local_time,
                                     Timing::AnimationDirection::kForwards));
  // Value remains unchanged.
  histogram_tester.ExpectBucketCount(histogram_name, 3, 2);

  local_time = ANIMATION_TIME_DELTA_FROM_MILLISECONDS(0);
  EXPECT_EQ(Timing::kPhaseActive, TimingCalculations::CalculatePhase(
                                      normalized_timing, local_time,
                                      Timing::AnimationDirection::kBackwards));
  histogram_tester.ExpectBucketCount(histogram_name, 0, 0);

  normalized_timing.is_start_boundary_aligned = false;
  EXPECT_EQ(Timing::kPhaseBefore, TimingCalculations::CalculatePhase(
                                      normalized_timing, local_time,
                                      Timing::AnimationDirection::kBackwards));
  histogram_tester.ExpectBucketCount(histogram_name, 0, 0);

  normalized_timing.is_start_boundary_aligned = true;
  local_time = ANIMATION_TIME_DELTA_FROM_MILLISECONDS(-0.005);
  EXPECT_EQ(Timing::kPhaseBefore, TimingCalculations::CalculatePhase(
                                      normalized_timing, local_time,
                                      Timing::AnimationDirection::kBackwards));
  histogram_tester.ExpectBucketCount(histogram_name, 5, 1);

  normalized_timing.is_start_boundary_aligned = false;
  EXPECT_EQ(Timing::kPhaseBefore, TimingCalculations::CalculatePhase(
                                      normalized_timing, local_time,
                                      Timing::AnimationDirection::kBackwards));
  // Value remains unchanged.
  histogram_tester.ExpectBucketCount(histogram_name, 5, 1);
}

}  // namespace blink
