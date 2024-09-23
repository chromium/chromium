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

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_computed_effect_timing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_optional_effect_timing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_range_offset.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_string_unrestricteddouble.h"
#include "third_party/blink/renderer/core/animation/animation_effect_owner.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_values.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

Timing::V8Delay* CreateTimeDelay(double delay_in_ms) {
  return MakeGarbageCollected<Timing::V8Delay>(delay_in_ms);
}

class MockAnimationEffectOwner
    : public GarbageCollected<MockAnimationEffectOwner>,
      public AnimationEffectOwner {
 public:
  MOCK_CONST_METHOD0(SequenceNumber, unsigned());
  MOCK_CONST_METHOD0(Playing, bool());
  MOCK_CONST_METHOD0(IsEventDispatchAllowed, bool());
  MOCK_CONST_METHOD0(EffectSuppressed, bool());
  MOCK_CONST_METHOD0(ReplaceStateRemoved, bool());
  MOCK_METHOD0(EffectInvalidated, void());
  MOCK_METHOD0(UpdateIfNecessary, void());
  MOCK_METHOD0(GetAnimation, Animation*());
};

class TestAnimationEffectEventDelegate : public AnimationEffect::EventDelegate {
 public:
  void OnEventCondition(const AnimationEffect& animation_node,
                        Timing::Phase current_phase) override {
    event_triggered_ = true;
  }
  bool RequiresIterationEvents(const AnimationEffect& animation_node) override {
    return true;
  }
  void Reset() { event_triggered_ = false; }
  bool EventTriggered() { return event_triggered_; }

 private:
  bool event_triggered_;
};

class TestAnimationEffect : public AnimationEffect {
 public:
  TestAnimationEffect(
      const Timing& specified,
      TestAnimationEffectEventDelegate* event_delegate =
          MakeGarbageCollected<TestAnimationEffectEventDelegate>())
      : AnimationEffect(specified, event_delegate),
        event_delegate_(event_delegate) {}

  void UpdateInheritedTime(double time) {
    UpdateInheritedTime(time, kTimingUpdateForAnimationFrame);
  }

  void UpdateInheritedTime(double time, TimingUpdateReason reason) {
    event_delegate_->Reset();
    AnimationEffect::UpdateInheritedTime(
        ANIMATION_TIME_DELTA_FROM_SECONDS(time),
        /* is_idle */ false,
        /* inherited_playback_rate */ 1.0, reason);
  }

  bool Affects(const PropertyHandle&) const override { return false; }
  void UpdateChildrenAndEffects() const override {}
  void WillDetach() {}
  TestAnimationEffectEventDelegate* EventDelegate() {
    return event_delegate_.Get();
  }
  AnimationTimeDelta CalculateTimeToEffectChange(
      bool forwards,
      std::optional<AnimationTimeDelta> local_time,
      AnimationTimeDelta time_to_next_iteration) const override {
    local_time_ = local_time;
    time_to_next_iteration_ = time_to_next_iteration;
    return AnimationTimeDelta::Max();
  }
  std::optional<AnimationTimeDelta> TimelineDuration() const override {
    return std::nullopt;
  }
  double TakeLocalTime() {
    DCHECK(local_time_);
    const double result = local_time_->InSecondsF();
    local_time_.reset();
    return result;
  }

  std::optional<AnimationTimeDelta> TakeTimeToNextIteration() {
    const std::optional<AnimationTimeDelta> result = time_to_next_iteration_;
    time_to_next_iteration_.reset();
    return result;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(event_delegate_);
    AnimationEffect::Trace(visitor);
  }

 private:
  Member<TestAnimationEffectEventDelegate> event_delegate_;
  mutable std::optional<AnimationTimeDelta> local_time_;
  mutable std::optional<AnimationTimeDelta> time_to_next_iteration_;
};

TEST(AnimationAnimationEffectTest, Sanity) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(2);
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(0);

  EXPECT_EQ(Timing::kPhaseActive, animation_node->GetPhase());
  EXPECT_TRUE(animation_node->IsInPlay());
  EXPECT_TRUE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(2),
            animation_node->NormalizedTiming().active_duration);
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);

  EXPECT_EQ(Timing::kPhaseActive, animation_node->GetPhase());
  EXPECT_TRUE(animation_node->IsInPlay());
  EXPECT_TRUE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(2),
            animation_node->NormalizedTiming().active_duration);
  EXPECT_EQ(0.5, animation_node->Progress());

  animation_node->UpdateInheritedTime(2);

  EXPECT_EQ(Timing::kPhaseAfter, animation_node->GetPhase());
  EXPECT_FALSE(animation_node->IsInPlay());
  EXPECT_FALSE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(2),
            animation_node->NormalizedTiming().active_duration);
  EXPECT_EQ(1, animation_node->Progress());

  animation_node->UpdateInheritedTime(3);

  EXPECT_EQ(Timing::kPhaseAfter, animation_node->GetPhase());
  EXPECT_FALSE(animation_node->IsInPlay());
  EXPECT_FALSE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(2),
            animation_node->NormalizedTiming().active_duration);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, FillAuto) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1);
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(2);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, FillForwards) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1);
  timing.fill_mode = Timing::FillMode::FORWARDS;
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_FALSE(animation_node->Progress());

  animation_node->UpdateInheritedTime(2);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, FillBackwards) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1);
  timing.fill_mode = Timing::FillMode::BACKWARDS;
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(2);
  EXPECT_FALSE(animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, FillBoth) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1);
  timing.fill_mode = Timing::FillMode::BOTH;
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(2);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, StartDelay) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1);
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.start_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(0.5));
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(0);
  EXPECT_FALSE(animation_node->Progress());

  animation_node->UpdateInheritedTime(0.5);
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(1.5);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, ZeroIteration) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1);
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_count = 0;
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(AnimationTimeDelta(),
            animation_node->NormalizedTiming().active_duration);
  EXPECT_FALSE(animation_node->CurrentIteration());
  EXPECT_FALSE(animation_node->Progress());

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(AnimationTimeDelta(),
            animation_node->NormalizedTiming().active_duration);
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, InfiniteIteration) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1);
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_count = std::numeric_limits<double>::infinity();
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_FALSE(animation_node->CurrentIteration());
  EXPECT_FALSE(animation_node->Progress());

  EXPECT_EQ(AnimationTimeDelta::Max(),
            animation_node->NormalizedTiming().active_duration);

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, Iteration) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.iteration_count = 2;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(2);
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0.5, animation_node->Progress());

  animation_node->UpdateInheritedTime(2);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(2);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(5);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, IterationStart) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.iteration_start = 1.2;
  timing.iteration_count = 2.2;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1);
  timing.fill_mode = Timing::FillMode::BOTH;
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_NEAR(0.2, animation_node->Progress().value(), 0.000000000000001);

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_NEAR(0.2, animation_node->Progress().value(), 0.000000000000001);

  animation_node->UpdateInheritedTime(10);
  EXPECT_EQ(3, animation_node->CurrentIteration());
  EXPECT_NEAR(0.4, animation_node->Progress().value(), 0.000000000000001);
}

TEST(AnimationAnimationEffectTest, IterationAlternate) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.iteration_count = 10;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1);
  timing.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(0.75);
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0.75, animation_node->Progress());

  animation_node->UpdateInheritedTime(1.75);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(0.25, animation_node->Progress());

  animation_node->UpdateInheritedTime(2.75);
  EXPECT_EQ(2, animation_node->CurrentIteration());
  EXPECT_EQ(0.75, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, IterationAlternateReverse) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.iteration_count = 10;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1);
  timing.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(0.75);
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0.25, animation_node->Progress());

  animation_node->UpdateInheritedTime(1.75);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(0.75, animation_node->Progress());

  animation_node->UpdateInheritedTime(2.75);
  EXPECT_EQ(2, animation_node->CurrentIteration());
  EXPECT_EQ(0.25, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, ZeroDurationSanity) {
  test::TaskEnvironment task_environment;
  Timing timing;
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(0);

  EXPECT_EQ(Timing::kPhaseAfter, animation_node->GetPhase());
  EXPECT_FALSE(animation_node->IsInPlay());
  EXPECT_FALSE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(AnimationTimeDelta(),
            animation_node->NormalizedTiming().active_duration);
  EXPECT_EQ(1, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);

  EXPECT_EQ(Timing::kPhaseAfter, animation_node->GetPhase());
  EXPECT_FALSE(animation_node->IsInPlay());
  EXPECT_FALSE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(AnimationTimeDelta(),
            animation_node->NormalizedTiming().active_duration);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, ZeroDurationFillForwards) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_FALSE(animation_node->Progress());

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(1, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, ZeroDurationFillBackwards) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.fill_mode = Timing::FillMode::BACKWARDS;
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(0);
  EXPECT_FALSE(animation_node->Progress());

  animation_node->UpdateInheritedTime(1);
  EXPECT_FALSE(animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, ZeroDurationFillBoth) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.fill_mode = Timing::FillMode::BOTH;
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(1, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, ZeroDurationStartDelay) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.start_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(0.5));
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(0);
  EXPECT_FALSE(animation_node->Progress());

  animation_node->UpdateInheritedTime(0.5);
  EXPECT_EQ(1, animation_node->Progress());

  animation_node->UpdateInheritedTime(1.5);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, ZeroDurationIterationStartAndCount) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.iteration_start = 0.1;
  timing.iteration_count = 0.2;
  timing.fill_mode = Timing::FillMode::BOTH;
  timing.start_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(0.3));
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(0.1, animation_node->Progress());

  animation_node->UpdateInheritedTime(0.3);
  EXPECT_DOUBLE_EQ(0.3, animation_node->Progress().value());

  animation_node->UpdateInheritedTime(1);
  EXPECT_DOUBLE_EQ(0.3, animation_node->Progress().value());
}

// FIXME: Needs specification work.
TEST(AnimationAnimationEffectTest, ZeroDurationInfiniteIteration) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_count = std::numeric_limits<double>::infinity();
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(AnimationTimeDelta(),
            animation_node->NormalizedTiming().active_duration);
  EXPECT_FALSE(animation_node->CurrentIteration());
  EXPECT_FALSE(animation_node->Progress());

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(AnimationTimeDelta(),
            animation_node->NormalizedTiming().active_duration);
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation_node->CurrentIteration());
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, ZeroDurationIteration) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_count = 2;
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_FALSE(animation_node->CurrentIteration());
  EXPECT_FALSE(animation_node->Progress());

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(1, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, ZeroDurationIterationStart) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.iteration_start = 1.2;
  timing.iteration_count = 2.2;
  timing.fill_mode = Timing::FillMode::BOTH;
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_NEAR(0.2, animation_node->Progress().value(), 0.000000000000001);

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(3, animation_node->CurrentIteration());
  EXPECT_NEAR(0.4, animation_node->Progress().value(), 0.000000000000001);

  animation_node->UpdateInheritedTime(10);
  EXPECT_EQ(3, animation_node->CurrentIteration());
  EXPECT_NEAR(0.4, animation_node->Progress().value(), 0.000000000000001);
}

TEST(AnimationAnimationEffectTest, ZeroDurationIterationAlternate) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_count = 2;
  timing.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_FALSE(animation_node->CurrentIteration());
  EXPECT_FALSE(animation_node->Progress());

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, ZeroDurationIterationAlternateReverse) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_count = 2;
  timing.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_FALSE(animation_node->CurrentIteration());
  EXPECT_FALSE(animation_node->Progress());

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(1, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, InfiniteDurationSanity) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::Max();
  timing.iteration_count = 1;
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(0);

  EXPECT_EQ(AnimationTimeDelta::Max(),
            animation_node->NormalizedTiming().active_duration);
  EXPECT_EQ(Timing::kPhaseActive, animation_node->GetPhase());
  EXPECT_TRUE(animation_node->IsInPlay());
  EXPECT_TRUE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);

  EXPECT_EQ(AnimationTimeDelta::Max(),
            animation_node->NormalizedTiming().active_duration);
  EXPECT_EQ(Timing::kPhaseActive, animation_node->GetPhase());
  EXPECT_TRUE(animation_node->IsInPlay());
  EXPECT_TRUE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());
}

// FIXME: Needs specification work.
TEST(AnimationAnimationEffectTest, InfiniteDurationZeroIterations) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::Max();
  timing.iteration_count = 0;
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(0);

  EXPECT_EQ(AnimationTimeDelta(),
            animation_node->NormalizedTiming().active_duration);
  EXPECT_EQ(Timing::kPhaseAfter, animation_node->GetPhase());
  EXPECT_FALSE(animation_node->IsInPlay());
  EXPECT_FALSE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);

  EXPECT_EQ(Timing::kPhaseAfter, animation_node->GetPhase());
  EXPECT_EQ(Timing::kPhaseAfter, animation_node->GetPhase());
  EXPECT_FALSE(animation_node->IsInPlay());
  EXPECT_FALSE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, InfiniteDurationInfiniteIterations) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::Max();
  timing.iteration_count = std::numeric_limits<double>::infinity();
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(0);

  EXPECT_EQ(AnimationTimeDelta::Max(),
            animation_node->NormalizedTiming().active_duration);
  EXPECT_EQ(Timing::kPhaseActive, animation_node->GetPhase());
  EXPECT_TRUE(animation_node->IsInPlay());
  EXPECT_TRUE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);

  EXPECT_EQ(AnimationTimeDelta::Max(),
            animation_node->NormalizedTiming().active_duration);
  EXPECT_EQ(Timing::kPhaseActive, animation_node->GetPhase());
  EXPECT_TRUE(animation_node->IsInPlay());
  EXPECT_TRUE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, EndTime) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.start_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(1));
  timing.end_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(2));
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(4);
  timing.iteration_count = 2;
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);
  EXPECT_EQ(ANIMATION_TIME_DELTA_FROM_SECONDS(11),
            animation_node->NormalizedTiming().end_time);
}

TEST(AnimationAnimationEffectTest, Events) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1);
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_count = 2;
  timing.start_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(1));
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(0.0, kTimingUpdateOnDemand);
  EXPECT_FALSE(animation_node->EventDelegate()->EventTriggered());

  animation_node->UpdateInheritedTime(0.0, kTimingUpdateForAnimationFrame);
  EXPECT_TRUE(animation_node->EventDelegate()->EventTriggered());

  animation_node->UpdateInheritedTime(1.5, kTimingUpdateOnDemand);
  EXPECT_FALSE(animation_node->EventDelegate()->EventTriggered());

  animation_node->UpdateInheritedTime(1.5, kTimingUpdateForAnimationFrame);
  EXPECT_TRUE(animation_node->EventDelegate()->EventTriggered());
}

TEST(AnimationAnimationEffectTest, TimeToEffectChange) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1);
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_start = 0.2;
  timing.iteration_count = 2.5;
  timing.start_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(1));
  timing.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;
  auto* animation_node = MakeGarbageCollected<TestAnimationEffect>(timing);

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(0, animation_node->TakeLocalTime());
  std::optional<AnimationTimeDelta> time_to_next_iteration =
      animation_node->TakeTimeToNextIteration();
  EXPECT_TRUE(time_to_next_iteration);
  EXPECT_TRUE(time_to_next_iteration->is_max());

  // Normal iteration.
  animation_node->UpdateInheritedTime(1.75);
  EXPECT_EQ(1.75, animation_node->TakeLocalTime());
  time_to_next_iteration = animation_node->TakeTimeToNextIteration();
  EXPECT_TRUE(time_to_next_iteration);
  EXPECT_NEAR(0.05, time_to_next_iteration->InSecondsF(), 0.000000000000001);

  // Reverse iteration.
  animation_node->UpdateInheritedTime(2.75);
  EXPECT_EQ(2.75, animation_node->TakeLocalTime());
  time_to_next_iteration = animation_node->TakeTimeToNextIteration();
  EXPECT_TRUE(time_to_next_iteration);
  EXPECT_NEAR(0.05, time_to_next_iteration->InSecondsF(), 0.000000000000001);

  // Item ends before iteration finishes.
  animation_node->UpdateInheritedTime(3.4);
  EXPECT_EQ(Timing::kPhaseActive, animation_node->GetPhase());
  EXPECT_EQ(3.4, animation_node->TakeLocalTime());
  time_to_next_iteration = animation_node->TakeTimeToNextIteration();
  EXPECT_TRUE(time_to_next_iteration);
  EXPECT_TRUE(time_to_next_iteration->is_max());

  // Item has finished.
  animation_node->UpdateInheritedTime(3.5);
  EXPECT_EQ(Timing::kPhaseAfter, animation_node->GetPhase());
  EXPECT_EQ(3.5, animation_node->TakeLocalTime());
  time_to_next_iteration = animation_node->TakeTimeToNextIteration();
  EXPECT_TRUE(time_to_next_iteration);
  EXPECT_TRUE(time_to_next_iteration->is_max());
}

TEST(AnimationAnimationEffectTest, UpdateTiming) {
  test::TaskEnvironment task_environment;
  Timing timing;
  auto* effect = MakeGarbageCollected<TestAnimationEffect>(timing);

  EXPECT_EQ(0, effect->getTiming()->delay()->GetAsDouble());
  OptionalEffectTiming* effect_timing = OptionalEffectTiming::Create();
  effect_timing->setDelay(CreateTimeDelay(2));
  effect->updateTiming(effect_timing);
  EXPECT_EQ(2, effect->getTiming()->delay()->GetAsDouble());
  effect_timing = OptionalEffectTiming::Create();
  effect_timing->setEndDelay(CreateTimeDelay(0.5));
  effect->updateTiming(effect_timing);
  EXPECT_EQ(0.5, effect->getTiming()->endDelay()->GetAsDouble());
  effect_timing = OptionalEffectTiming::Create();
  effect_timing->setFill("backwards");
  effect->updateTiming(effect_timing);
  EXPECT_EQ("backwards", effect->getTiming()->fill());

  EXPECT_EQ(0, effect->getTiming()->iterationStart());
  effect_timing = OptionalEffectTiming::Create();
  effect_timing->setIterationStart(2);
  effect->updateTiming(effect_timing);
  EXPECT_EQ(2, effect->getTiming()->iterationStart());

  EXPECT_EQ(1, effect->getTiming()->iterations());
  effect_timing = OptionalEffectTiming::Create();
  effect_timing->setIterations(10);
  effect->updateTiming(effect_timing);
  EXPECT_EQ(10, effect->getTiming()->iterations());

  EXPECT_EQ("normal", effect->getTiming()->direction());
  effect_timing = OptionalEffectTiming::Create();
  effect_timing->setDirection("reverse");
  effect->updateTiming(effect_timing);
  EXPECT_EQ("reverse", effect->getTiming()->direction());

  EXPECT_EQ("linear", effect->getTiming()->easing());
  effect_timing = OptionalEffectTiming::Create();
  effect_timing->setEasing("ease-in-out");
  effect->updateTiming(effect_timing);
  EXPECT_EQ("ease-in-out", effect->getTiming()->easing());

  EXPECT_EQ("auto", effect->getTiming()->duration()->GetAsString());
  effect_timing = OptionalEffectTiming::Create();
  effect_timing->setDuration(
      MakeGarbageCollected<V8UnionCSSNumericValueOrStringOrUnrestrictedDouble>(
          2.5));
  effect->updateTiming(effect_timing);
  EXPECT_EQ(2.5, effect->getTiming()->duration()->GetAsUnrestrictedDouble());
}

TEST(AnimationAnimationEffectTest, UpdateTimingThrowsWhenExpected) {
  test::TaskEnvironment task_environment;
  Timing timing;
  auto* effect = MakeGarbageCollected<TestAnimationEffect>(timing);

  // iterationStart must be non-negative
  {
    DummyExceptionStateForTesting exception_state;
    OptionalEffectTiming* effect_timing = OptionalEffectTiming::Create();
    effect_timing->setIterationStart(-10);
    effect->updateTiming(effect_timing, exception_state);
    EXPECT_TRUE(exception_state.HadException());
  }

  // iterations must be non-negative and non-null.
  {
    DummyExceptionStateForTesting exception_state;
    OptionalEffectTiming* effect_timing = OptionalEffectTiming::Create();
    effect_timing->setIterations(-2);
    effect->updateTiming(effect_timing, exception_state);
    EXPECT_TRUE(exception_state.HadException());
  }

  {
    DummyExceptionStateForTesting exception_state;
    OptionalEffectTiming* effect_timing = OptionalEffectTiming::Create();
    effect_timing->setIterations(std::numeric_limits<double>::quiet_NaN());
    effect->updateTiming(effect_timing, exception_state);
    EXPECT_TRUE(exception_state.HadException());
  }

  // If it is a number, duration must be non-negative and non-null.
  {
    DummyExceptionStateForTesting exception_state;
    OptionalEffectTiming* effect_timing = OptionalEffectTiming::Create();
    effect_timing->setDuration(
        MakeGarbageCollected<
            V8UnionCSSNumericValueOrStringOrUnrestrictedDouble>(-100));
    effect->updateTiming(effect_timing, exception_state);
    EXPECT_TRUE(exception_state.HadException());
  }

  {
    DummyExceptionStateForTesting exception_state;
    OptionalEffectTiming* effect_timing = OptionalEffectTiming::Create();
    effect_timing->setDuration(
        MakeGarbageCollected<
            V8UnionCSSNumericValueOrStringOrUnrestrictedDouble>(
            std::numeric_limits<double>::quiet_NaN()));
    effect->updateTiming(effect_timing, exception_state);
    EXPECT_TRUE(exception_state.HadException());
  }

  // easing must be a valid timing function
  {
    DummyExceptionStateForTesting exception_state;
    OptionalEffectTiming* effect_timing = OptionalEffectTiming::Create();
    effect_timing->setEasing("my-custom-timing-function");
    effect->updateTiming(effect_timing, exception_state);
    EXPECT_TRUE(exception_state.HadException());
  }
}

TEST(AnimationAnimationEffectTest, UpdateTimingInformsOwnerOnChange) {
  test::TaskEnvironment task_environment;
  Timing timing;
  auto* effect = MakeGarbageCollected<TestAnimationEffect>(timing);

  MockAnimationEffectOwner* owner =
      MakeGarbageCollected<MockAnimationEffectOwner>();
  effect->Attach(owner);

  EXPECT_CALL(*owner, EffectInvalidated()).Times(1);

  OptionalEffectTiming* effect_timing = OptionalEffectTiming::Create();
  effect_timing->setDelay(CreateTimeDelay(5));
  effect->updateTiming(effect_timing);
}

TEST(AnimationAnimationEffectTest, UpdateTimingNoChange) {
  test::TaskEnvironment task_environment;
  Timing timing;
  timing.start_delay = Timing::Delay(AnimationTimeDelta());
  timing.end_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(5));
  timing.fill_mode = Timing::FillMode::BOTH;
  timing.iteration_start = 0.1;
  timing.iteration_count = 3;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(2);
  timing.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;
  timing.timing_function = CubicBezierTimingFunction::Create(1, 1, 0.3, 0.3);
  auto* effect = MakeGarbageCollected<TestAnimationEffect>(timing);

  MockAnimationEffectOwner* owner =
      MakeGarbageCollected<MockAnimationEffectOwner>();
  effect->Attach(owner);

  // None of the below calls to updateTime should cause the AnimationEffect to
  // update, as they all match the existing timing information.
  EXPECT_CALL(*owner, EffectInvalidated()).Times(0);

  OptionalEffectTiming* effect_timing = OptionalEffectTiming::Create();
  effect->updateTiming(effect_timing);

  effect_timing = OptionalEffectTiming::Create();
  effect_timing->setDelay(CreateTimeDelay(0));
  effect->updateTiming(effect_timing);

  effect_timing = OptionalEffectTiming::Create();
  effect_timing->setEndDelay(CreateTimeDelay(5000));
  effect_timing->setFill("both");
  effect_timing->setIterationStart(0.1);
  effect->updateTiming(effect_timing);

  effect_timing = OptionalEffectTiming::Create();
  effect_timing->setIterations(3);
  effect_timing->setDuration(
      MakeGarbageCollected<V8UnionCSSNumericValueOrStringOrUnrestrictedDouble>(
          2000));
  effect_timing->setDirection("alternate-reverse");
  effect->updateTiming(effect_timing);

  effect_timing = OptionalEffectTiming::Create();
  effect_timing->setEasing("cubic-bezier(1, 1, 0.3, 0.3)");
  effect->updateTiming(effect_timing);
}

}  // namespace blink
