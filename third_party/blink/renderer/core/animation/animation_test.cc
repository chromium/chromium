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

#include "third_party/blink/renderer/core/animation/animation.h"

#include <memory>

#include "base/bits.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/double_or_scroll_timeline_auto_keyword.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_optional_effect_timing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_timeline_options.h"
#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_double.h"
#include "third_party/blink/renderer/core/animation/css_number_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/pending_animations.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"
#include "third_party/blink/renderer/platform/animation/compositor_keyframe_model.h"
#include "third_party/blink/renderer/platform/animation/compositor_target_property.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

double MillisecondsToSeconds(double milliseconds) {
  return milliseconds / 1000;
}

}  // namespace

void ExpectRelativeErrorWithinEpsilon(double expected, double observed) {
  EXPECT_NEAR(1.0, observed / expected, std::numeric_limits<double>::epsilon());
}

class AnimationAnimationTestNoCompositing : public RenderingTest {
 public:
  AnimationAnimationTestNoCompositing()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

  void SetUp() override {
    last_frame_time = 0;
    RenderingTest::SetUp();
    SetUpWithoutStartingTimeline();
    StartTimeline();
  }

  void SetUpWithoutStartingTimeline() {
    GetDocument().GetAnimationClock().ResetTimeForTesting();
    timeline = GetDocument().Timeline();
    timeline->ResetForTesting();
    animation = timeline->Play(nullptr);
    animation->setStartTime(0);
    animation->setEffect(MakeAnimation());
  }

  void StartTimeline() { SimulateFrame(0); }

  KeyframeEffectModelBase* MakeSimpleEffectModel() {
    PropertyHandle PropertyHandleOpacity(GetCSSPropertyOpacity());
    TransitionKeyframe* start_keyframe =
        MakeGarbageCollected<TransitionKeyframe>(PropertyHandleOpacity);
    start_keyframe->SetValue(std::make_unique<TypedInterpolationValue>(
        CSSNumberInterpolationType(PropertyHandleOpacity),
        std::make_unique<InterpolableNumber>(1.0)));
    start_keyframe->SetOffset(0.0);
    // Egregious hack: Sideload the compositor value.
    // This is usually set in a part of the rendering process SimulateFrame
    // doesn't call.
    start_keyframe->SetCompositorValue(
        MakeGarbageCollected<CompositorKeyframeDouble>(1.0));
    TransitionKeyframe* end_keyframe =
        MakeGarbageCollected<TransitionKeyframe>(PropertyHandleOpacity);
    end_keyframe->SetValue(std::make_unique<TypedInterpolationValue>(
        CSSNumberInterpolationType(PropertyHandleOpacity),
        std::make_unique<InterpolableNumber>(0.0)));
    end_keyframe->SetOffset(1.0);
    // Egregious hack: Sideload the compositor value.
    end_keyframe->SetCompositorValue(
        MakeGarbageCollected<CompositorKeyframeDouble>(0.0));

    TransitionKeyframeVector keyframes;
    keyframes.push_back(start_keyframe);
    keyframes.push_back(end_keyframe);

    return MakeGarbageCollected<TransitionKeyframeEffectModel>(keyframes);
  }

  void ResetWithCompositedAnimation() {
    // Get rid of the default animation.
    animation->cancel();

    RunDocumentLifecycle();

    SetBodyInnerHTML("<div id='target'></div>");

    MakeCompositedAnimation();
  }

  void MakeCompositedAnimation() {
    // Create a compositable animation; in this case opacity from 1 to 0.
    Timing timing;
    timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);

    Persistent<StringKeyframe> start_keyframe =
        MakeGarbageCollected<StringKeyframe>();
    start_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "1.0",
                                        SecureContextMode::kInsecureContext,
                                        nullptr);
    Persistent<StringKeyframe> end_keyframe =
        MakeGarbageCollected<StringKeyframe>();
    end_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "0.0",
                                      SecureContextMode::kInsecureContext,
                                      nullptr);

    StringKeyframeVector keyframes;
    keyframes.push_back(start_keyframe);
    keyframes.push_back(end_keyframe);

    Element* element = GetElementById("target");
    auto* model = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);
    animation = timeline->Play(
        MakeGarbageCollected<KeyframeEffect>(element, model, timing));

    // After creating the animation we need to clean the lifecycle so that the
    // animation can be pushed to the compositor.
    UpdateAllLifecyclePhasesForTest();

    GetDocument().GetAnimationClock().UpdateTime(base::TimeTicks());
    GetDocument().GetPendingAnimations().Update(nullptr, true);
  }

  KeyframeEffectModelBase* MakeEmptyEffectModel() {
    return MakeGarbageCollected<StringKeyframeEffectModel>(
        StringKeyframeVector());
  }

  KeyframeEffect* MakeAnimation(
      double duration = 30,
      Timing::FillMode fill_mode = Timing::FillMode::AUTO) {
    Timing timing;
    timing.iteration_duration = AnimationTimeDelta::FromSecondsD(duration);
    timing.fill_mode = fill_mode;
    return MakeGarbageCollected<KeyframeEffect>(nullptr, MakeEmptyEffectModel(),
                                                timing);
  }

  bool SimulateFrame(double time_ms) {
    if (animation->pending())
      animation->NotifyReady(MillisecondsToSeconds(last_frame_time));
    SimulateMicrotask();

    last_frame_time = time_ms;
    const auto* paint_artifact_compositor =
        GetDocument().GetFrame()->View()->GetPaintArtifactCompositor();
    GetDocument().GetAnimationClock().UpdateTime(
        base::TimeTicks() + base::TimeDelta::FromMillisecondsD(time_ms));
    GetDocument().GetPendingAnimations().Update(paint_artifact_compositor,
                                                false);
    // The timeline does not know about our animation, so we have to explicitly
    // call update().
    return animation->Update(kTimingUpdateForAnimationFrame);
  }

  void SimulateAwaitReady() { SimulateFrame(last_frame_time); }

  void SimulateMicrotask() {
    Microtask::PerformCheckpoint(V8PerIsolateData::MainThreadIsolate());
  }

  void SimulateFrameForScrollAnimations() {
    // Advance time by 100 ms.
    auto new_time = GetAnimationClock().CurrentTime() +
                    base::TimeDelta::FromMilliseconds(100);
    GetPage().Animator().ServiceScriptedAnimations(new_time);
  }

  Persistent<DocumentTimeline> timeline;
  Persistent<Animation> animation;

 private:
  double last_frame_time;
};

class AnimationAnimationTestCompositing
    : public AnimationAnimationTestNoCompositing {
  void SetUp() override {
    EnableCompositing();
    AnimationAnimationTestNoCompositing::SetUp();
  }
};

class AnimationAnimationTestCompositeAfterPaint
    : public AnimationAnimationTestNoCompositing {
  void SetUp() override {
    EnableCompositing();
    AnimationAnimationTestNoCompositing::SetUp();
  }

  ScopedCompositeAfterPaintForTest enable_cap{true};
};

TEST_F(AnimationAnimationTestNoCompositing, InitialState) {
  SetUpWithoutStartingTimeline();
  animation = timeline->Play(nullptr);
  EXPECT_EQ(0, animation->currentTime());
  EXPECT_TRUE(animation->pending());
  EXPECT_FALSE(animation->Paused());
  EXPECT_EQ(1, animation->playbackRate());
  EXPECT_FALSE(animation->startTime().has_value());

  StartTimeline();
  EXPECT_EQ("finished", animation->playState());
  EXPECT_EQ(0, timeline->currentTime());
  EXPECT_EQ(0, animation->currentTime());
  EXPECT_FALSE(animation->Paused());
  EXPECT_FALSE(animation->pending());
  EXPECT_EQ(1, animation->playbackRate());
  EXPECT_EQ(0, animation->startTime());
}

TEST_F(AnimationAnimationTestNoCompositing, CurrentTimeDoesNotSetOutdated) {
  EXPECT_FALSE(animation->Outdated());
  EXPECT_EQ(0, animation->currentTime());
  EXPECT_FALSE(animation->Outdated());
  // FIXME: We should split simulateFrame into a version that doesn't update
  // the animation and one that does, as most of the tests don't require
  // update() to be called.
  GetDocument().GetAnimationClock().UpdateTime(
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(10000));
  EXPECT_EQ(10000, animation->currentTime());
  EXPECT_FALSE(animation->Outdated());
}

TEST_F(AnimationAnimationTestNoCompositing, SetCurrentTime) {
  EXPECT_EQ("running", animation->playState());
  animation->setCurrentTime(10000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_EQ(10000, animation->currentTime());

  SimulateFrame(10000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_EQ(20000, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing, SetCurrentTimeNegative) {
  animation->setCurrentTime(-10000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_EQ(-10000, animation->currentTime());

  SimulateFrame(20000);
  EXPECT_EQ(10000, animation->currentTime());
  animation->setPlaybackRate(-2);
  animation->setCurrentTime(-10000);
  EXPECT_EQ("finished", animation->playState());
  // A seek can set current time outside the range [0, EffectEnd()].
  EXPECT_EQ(-10000, animation->currentTime());

  SimulateFrame(40000);
  // Hold current time even though outside normal range for the animation.
  EXPECT_FALSE(animation->pending());
  EXPECT_EQ("finished", animation->playState());
  EXPECT_EQ(-10000, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing,
       SetCurrentTimeNegativeWithoutSimultaneousPlaybackRateChange) {
  SimulateFrame(20000);
  EXPECT_EQ(20000, animation->currentTime());
  EXPECT_EQ("running", animation->playState());

  // Reversing the direction preserves current time.
  animation->setPlaybackRate(-1);
  EXPECT_EQ("running", animation->playState());
  EXPECT_EQ(20000, animation->currentTime());
  SimulateAwaitReady();

  SimulateFrame(30000);
  EXPECT_EQ(10000, animation->currentTime());
  EXPECT_EQ("running", animation->playState());

  animation->setCurrentTime(-10000);
  EXPECT_EQ("finished", animation->playState());
}

TEST_F(AnimationAnimationTestNoCompositing, SetCurrentTimePastContentEnd) {
  animation->setCurrentTime(50000);
  EXPECT_EQ("finished", animation->playState());
  EXPECT_EQ(50000, animation->currentTime());

  SimulateFrame(20000);
  EXPECT_EQ("finished", animation->playState());
  EXPECT_EQ(50000, animation->currentTime());
  // Reversing the play direction changes the play state from finished to
  // running.
  animation->setPlaybackRate(-2);
  animation->setCurrentTime(50000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_EQ(50000, animation->currentTime());
  SimulateAwaitReady();

  SimulateFrame(40000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_EQ(10000, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing, SetCurrentTimeMax) {
  double limit = std::numeric_limits<double>::max();
  animation->setCurrentTime(limit);
  ExpectRelativeErrorWithinEpsilon(limit, animation->currentTime().value());

  SimulateFrame(100000);
  ExpectRelativeErrorWithinEpsilon(limit, animation->currentTime().value());
}

TEST_F(AnimationAnimationTestNoCompositing, SetCurrentTimeSetsStartTime) {
  EXPECT_EQ(0, animation->startTime());
  animation->setCurrentTime(1000);
  EXPECT_EQ(-1000, animation->startTime());

  SimulateFrame(1000);
  EXPECT_EQ(-1000, animation->startTime());
  EXPECT_EQ(2000, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing, SetStartTime) {
  SimulateFrame(20000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_EQ(0, animation->startTime());
  EXPECT_EQ(20000, animation->currentTime());
  animation->setStartTime(10000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_EQ(10000, animation->startTime());
  EXPECT_EQ(10000, animation->currentTime());

  SimulateFrame(30000);
  EXPECT_EQ(10000, animation->startTime());
  EXPECT_EQ(20000, animation->currentTime());
  animation->setStartTime(-20000);
  EXPECT_EQ("finished", animation->playState());
}

TEST_F(AnimationAnimationTestNoCompositing, SetStartTimeLimitsAnimation) {
  // Setting the start time is a seek operation, which is not constrained by the
  // normal limits on the animation.
  animation->setStartTime(-50000);
  EXPECT_EQ("finished", animation->playState());
  EXPECT_TRUE(animation->Limited());
  EXPECT_EQ(50000, animation->currentTime());
  animation->setPlaybackRate(-1);
  EXPECT_EQ("running", animation->playState());
  animation->setStartTime(-100000);
  EXPECT_EQ("finished", animation->playState());
  EXPECT_EQ(-100000, animation->currentTime());
  EXPECT_TRUE(animation->Limited());
}

TEST_F(AnimationAnimationTestNoCompositing, SetStartTimeOnLimitedAnimation) {
  // The setStartTime method is a seek and thus not constrained by the normal
  // limits on the animation.
  SimulateFrame(30000);
  animation->setStartTime(-10000);
  EXPECT_EQ("finished", animation->playState());
  EXPECT_EQ(40000, animation->currentTime());
  EXPECT_TRUE(animation->Limited());

  animation->setCurrentTime(50000);
  EXPECT_EQ(50000, animation->currentTime());
  animation->setStartTime(-40000);
  EXPECT_EQ(70000, animation->currentTime());
  EXPECT_EQ("finished", animation->playState());
  EXPECT_TRUE(animation->Limited());
}

TEST_F(AnimationAnimationTestNoCompositing, StartTimePauseFinish) {
  NonThrowableExceptionState exception_state;
  animation->pause();
  EXPECT_EQ("paused", animation->playState());
  EXPECT_TRUE(animation->pending());
  SimulateAwaitReady();
  EXPECT_FALSE(animation->pending());
  EXPECT_FALSE(animation->startTime().has_value());
  animation->finish(exception_state);
  EXPECT_EQ("finished", animation->playState());
  EXPECT_FALSE(animation->pending());
  EXPECT_EQ(-30000, animation->startTime());
}

TEST_F(AnimationAnimationTestNoCompositing, FinishWhenPaused) {
  NonThrowableExceptionState exception_state;
  animation->pause();
  EXPECT_EQ("paused", animation->playState());
  EXPECT_TRUE(animation->pending());

  SimulateFrame(10000);
  EXPECT_EQ("paused", animation->playState());
  EXPECT_FALSE(animation->pending());
  animation->finish(exception_state);
  EXPECT_EQ("finished", animation->playState());
}

TEST_F(AnimationAnimationTestNoCompositing, StartTimeFinishPause) {
  NonThrowableExceptionState exception_state;
  animation->finish(exception_state);
  EXPECT_EQ(-30000, animation->startTime());
  animation->pause();
  EXPECT_EQ("paused", animation->playState());
  EXPECT_TRUE(animation->pending());
  SimulateAwaitReady();
  EXPECT_FALSE(animation->pending());
  EXPECT_FALSE(animation->startTime().has_value());
}

TEST_F(AnimationAnimationTestNoCompositing, StartTimeWithZeroPlaybackRate) {
  animation->setPlaybackRate(0);
  EXPECT_EQ("running", animation->playState());
  SimulateAwaitReady();
  EXPECT_TRUE(animation->startTime().has_value());

  SimulateFrame(10000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_EQ(0, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing, PausePlay) {
  // Pause the animation at the 10s mark.
  SimulateFrame(10000);
  animation->pause();
  EXPECT_EQ("paused", animation->playState());
  EXPECT_TRUE(animation->pending());
  EXPECT_EQ(10000, animation->currentTime());

  // Resume playing the animation at the 20s mark.
  SimulateFrame(20000);
  EXPECT_EQ("paused", animation->playState());
  EXPECT_FALSE(animation->pending());
  EXPECT_EQ(10000, animation->currentTime());
  animation->play();
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());
  SimulateAwaitReady();
  EXPECT_FALSE(animation->pending());

  // Advance another 10s.
  SimulateFrame(30000);
  EXPECT_EQ(20000, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing, PlayRewindsToStart) {
  // Auto-replay when starting from limit.
  animation->setCurrentTime(30000);
  animation->play();
  EXPECT_EQ(0, animation->currentTime());

  // Auto-replay when starting past the upper bound.
  animation->setCurrentTime(40000);
  animation->play();
  EXPECT_EQ(0, animation->currentTime());
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());

  // Snap to start of the animation if playing in forward direction starting
  // from a negative value of current time.
  SimulateFrame(10000);
  EXPECT_FALSE(animation->pending());
  animation->setCurrentTime(-10000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_FALSE(animation->pending());
  animation->play();
  EXPECT_EQ(0, animation->currentTime());
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());
  SimulateAwaitReady();
  EXPECT_EQ("running", animation->playState());
  EXPECT_FALSE(animation->pending());
}

TEST_F(AnimationAnimationTestNoCompositing, PlayRewindsToEnd) {
  // Snap to end when playing a reversed animation from the start.
  animation->setPlaybackRate(-1);
  animation->play();
  EXPECT_EQ(30000, animation->currentTime());

  // Snap to end if playing a reversed animation starting past the upper limit.
  animation->setCurrentTime(40000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());
  animation->play();
  EXPECT_EQ(30000, animation->currentTime());
  EXPECT_TRUE(animation->pending());

  SimulateFrame(10000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_FALSE(animation->pending());

  // Snap to the end if playing a reversed animation starting with a negative
  // value for current time.
  animation->setCurrentTime(-10000);
  animation->play();
  EXPECT_EQ(30000, animation->currentTime());
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());

  SimulateFrame(20000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_FALSE(animation->pending());
}

TEST_F(AnimationAnimationTestNoCompositing,
       PlayWithPlaybackRateZeroDoesNotSeek) {
  // When playback rate is zero, any value set for the current time effectively
  // becomes the hold time.
  animation->setPlaybackRate(0);
  animation->play();
  EXPECT_EQ(0, animation->currentTime());

  animation->setCurrentTime(40000);
  animation->play();
  EXPECT_EQ(40000, animation->currentTime());

  animation->setCurrentTime(-10000);
  animation->play();
  EXPECT_EQ(-10000, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing,
       PlayAfterPauseWithPlaybackRateZeroUpdatesPlayState) {
  animation->pause();
  animation->setPlaybackRate(0);

  SimulateFrame(1000);
  EXPECT_EQ("paused", animation->playState());
  animation->play();
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());
}

TEST_F(AnimationAnimationTestNoCompositing, Reverse) {
  animation->setCurrentTime(10000);
  animation->pause();
  animation->reverse();
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());
  // Effective playback rate does not kick in until the animation is ready.
  EXPECT_EQ(1, animation->playbackRate());
  EXPECT_EQ(10000, animation->currentTime());
  SimulateAwaitReady();
  EXPECT_FALSE(animation->pending());
  EXPECT_EQ(-1, animation->playbackRate());
  // Updating the playback rate does not change current time.
  EXPECT_EQ(10000, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing,
       ReverseHoldsCurrentTimeWithPlaybackRateZero) {
  animation->setCurrentTime(10000);
  animation->setPlaybackRate(0);
  animation->pause();
  animation->reverse();
  SimulateAwaitReady();
  EXPECT_EQ("running", animation->playState());
  EXPECT_EQ(0, animation->playbackRate());
  EXPECT_EQ(10000, animation->currentTime());

  SimulateFrame(20000);
  EXPECT_EQ(10000, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing, ReverseSeeksToStart) {
  animation->setCurrentTime(-10000);
  animation->setPlaybackRate(-1);
  animation->reverse();
  EXPECT_EQ(0, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing, ReverseSeeksToEnd) {
  animation->setCurrentTime(40000);
  animation->reverse();
  EXPECT_EQ(30000, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing, ReverseBeyondLimit) {
  animation->setCurrentTime(40000);
  animation->setPlaybackRate(-1);
  animation->reverse();
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());
  EXPECT_EQ(0, animation->currentTime());

  animation->setCurrentTime(-10000);
  animation->reverse();
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());
  EXPECT_EQ(30000, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing, Finish) {
  NonThrowableExceptionState exception_state;
  animation->finish(exception_state);
  // Finished snaps to the end of the animation.
  EXPECT_EQ(30000, animation->currentTime());
  EXPECT_EQ("finished", animation->playState());
  // Finished is a synchronous operation.
  EXPECT_FALSE(animation->pending());

  animation->setPlaybackRate(-1);
  animation->finish(exception_state);
  EXPECT_EQ(0, animation->currentTime());
  EXPECT_EQ("finished", animation->playState());
  EXPECT_FALSE(animation->pending());
}

TEST_F(AnimationAnimationTestNoCompositing, FinishAfterEffectEnd) {
  NonThrowableExceptionState exception_state;
  // OK to set current time out of bounds.
  animation->setCurrentTime(40000);
  animation->finish(exception_state);
  // The finish method triggers a snap to the upper boundary.
  EXPECT_EQ(30000, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing, FinishBeforeStart) {
  NonThrowableExceptionState exception_state;
  animation->setCurrentTime(-10000);
  animation->setPlaybackRate(-1);
  animation->finish(exception_state);
  EXPECT_EQ(0, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing,
       FinishDoesNothingWithPlaybackRateZero) {
  // Cannot finish an animation that has a playback rate of zero.
  DummyExceptionStateForTesting exception_state;
  animation->setCurrentTime(10000);
  animation->setPlaybackRate(0);
  animation->finish(exception_state);
  EXPECT_EQ(10000, animation->currentTime());
  EXPECT_TRUE(exception_state.HadException());
}

TEST_F(AnimationAnimationTestNoCompositing, FinishRaisesException) {
  // Cannot finish an animation that has an infinite iteration-count and a
  // non-zero iteration-duration.
  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(1);
  timing.iteration_count = std::numeric_limits<double>::infinity();
  animation->setEffect(MakeGarbageCollected<KeyframeEffect>(
      nullptr, MakeEmptyEffectModel(), timing));
  animation->setCurrentTime(10000);

  DummyExceptionStateForTesting exception_state;
  animation->finish(exception_state);
  EXPECT_EQ(10000, animation->currentTime());
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            exception_state.CodeAs<DOMExceptionCode>());
}

TEST_F(AnimationAnimationTestNoCompositing, LimitingAtEffectEnd) {
  SimulateFrame(30000);
  EXPECT_EQ(30000, animation->currentTime());
  EXPECT_TRUE(animation->Limited());

  // Cannot run past the end of the animation without a seek.
  SimulateFrame(40000);
  EXPECT_EQ(30000, animation->currentTime());
  EXPECT_FALSE(animation->Paused());
}

TEST_F(AnimationAnimationTestNoCompositing, LimitingAtStart) {
  SimulateFrame(30000);
  animation->setPlaybackRate(-2);
  SimulateAwaitReady();

  SimulateFrame(45000);
  EXPECT_EQ(0, animation->currentTime());
  EXPECT_TRUE(animation->Limited());

  SimulateFrame(60000);
  EXPECT_EQ(0, animation->currentTime());
  EXPECT_FALSE(animation->Paused());
}

TEST_F(AnimationAnimationTestNoCompositing, LimitingWithNoEffect) {
  animation->setEffect(nullptr);
  EXPECT_TRUE(animation->Limited());
  SimulateFrame(30000);
  EXPECT_EQ(0, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing, SetPlaybackRate) {
  animation->setPlaybackRate(2);
  SimulateAwaitReady();
  EXPECT_EQ(2, animation->playbackRate());
  EXPECT_EQ(0, animation->currentTime());

  SimulateFrame(10000);
  EXPECT_EQ(20000, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing, SetPlaybackRateWhilePaused) {
  SimulateFrame(10000);
  animation->pause();
  EXPECT_EQ(10000, animation->currentTime());
  animation->setPlaybackRate(2);
  EXPECT_EQ(10000, animation->currentTime());
  SimulateAwaitReady();

  SimulateFrame(20000);
  animation->play();
  // Change to playback rate does not alter current time.
  EXPECT_EQ(10000, animation->currentTime());
  SimulateAwaitReady();

  SimulateFrame(25000);
  EXPECT_EQ(20000, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing, SetPlaybackRateWhileLimited) {
  // Animation plays until it hits the upper bound.
  SimulateFrame(40000);
  EXPECT_EQ(30000, animation->currentTime());
  EXPECT_TRUE(animation->Limited());
  animation->setPlaybackRate(2);
  SimulateAwaitReady();

  // Already at the end of the animation.
  SimulateFrame(50000);
  EXPECT_EQ(30000, animation->currentTime());
  animation->setPlaybackRate(-2);
  SimulateAwaitReady();

  SimulateFrame(60000);
  EXPECT_FALSE(animation->Limited());
  EXPECT_EQ(10000, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing, SetPlaybackRateZero) {
  SimulateFrame(10000);
  animation->setPlaybackRate(0);
  EXPECT_EQ(10000, animation->currentTime());

  SimulateFrame(20000);
  EXPECT_EQ(10000, animation->currentTime());
  animation->setCurrentTime(20000);
  EXPECT_EQ(20000, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing, SetPlaybackRateMax) {
  animation->setPlaybackRate(std::numeric_limits<double>::max());
  EXPECT_EQ(std::numeric_limits<double>::max(), animation->playbackRate());
  EXPECT_EQ(0, animation->currentTime());
  SimulateAwaitReady();

  SimulateFrame(1);
  EXPECT_EQ(30000, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing, UpdatePlaybackRate) {
  animation->updatePlaybackRate(2);
  EXPECT_EQ(1, animation->playbackRate());
  SimulateAwaitReady();
  EXPECT_EQ(2, animation->playbackRate());
  EXPECT_EQ(0, animation->currentTime());

  SimulateFrame(10000);
  EXPECT_EQ(20000, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing, UpdatePlaybackRateWhilePaused) {
  animation->pause();

  // Pending playback rate on pending-paused animation is picked up after async
  // tick.
  EXPECT_EQ("paused", animation->playState());
  EXPECT_TRUE(animation->pending());
  animation->updatePlaybackRate(2);
  EXPECT_EQ(1, animation->playbackRate());
  SimulateAwaitReady();
  EXPECT_EQ(2, animation->playbackRate());
  EXPECT_FALSE(animation->pending());

  // Pending playback rate on a paused animation is resolved immediately.
  animation->updatePlaybackRate(3);
  EXPECT_FALSE(animation->pending());
  EXPECT_EQ(3, animation->playbackRate());
}

TEST_F(AnimationAnimationTestNoCompositing, UpdatePlaybackRateWhileLimited) {
  NonThrowableExceptionState exception_state;
  animation->finish(exception_state);
  EXPECT_EQ(30000, animation->currentTime());

  // Updating playback rate does not affect current time.
  animation->updatePlaybackRate(2);
  EXPECT_EQ(30000, animation->currentTime());

  // Updating payback rate is resolved immediately for an animation in the
  // finished state.
  EXPECT_EQ(2, animation->playbackRate());
}

TEST_F(AnimationAnimationTestNoCompositing, UpdatePlaybackRateWhileRunning) {
  animation->play();
  SimulateFrame(1000);
  animation->updatePlaybackRate(2);

  // Updating playback rate triggers pending state for the play state.
  // Pending playback rate is not resolved until next async tick.
  EXPECT_TRUE(animation->pending());
  EXPECT_EQ(1, animation->playbackRate());
  SimulateAwaitReady();
  EXPECT_FALSE(animation->pending());
  EXPECT_EQ(2, animation->playbackRate());
}

TEST_F(AnimationAnimationTestNoCompositing, SetEffect) {
  animation = timeline->Play(nullptr);
  animation->setStartTime(0);
  AnimationEffect* effect1 = MakeAnimation();
  AnimationEffect* effect2 = MakeAnimation();
  animation->setEffect(effect1);
  EXPECT_EQ(effect1, animation->effect());
  EXPECT_EQ(0, animation->currentTime());
  animation->setCurrentTime(15000);
  animation->setEffect(effect2);
  EXPECT_EQ(15000, animation->currentTime());
  EXPECT_EQ(nullptr, effect1->GetAnimationForTesting());
  EXPECT_EQ(animation, effect2->GetAnimationForTesting());
  EXPECT_EQ(effect2, animation->effect());
}

TEST_F(AnimationAnimationTestNoCompositing, SetEffectLimitsAnimation) {
  animation->setCurrentTime(20000);
  animation->setEffect(MakeAnimation(10));
  EXPECT_EQ(20000, animation->currentTime());
  EXPECT_TRUE(animation->Limited());
  SimulateFrame(10000);
  EXPECT_EQ(20000, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing, SetEffectUnlimitsAnimation) {
  animation->setCurrentTime(40000);
  animation->setEffect(MakeAnimation(60));
  EXPECT_FALSE(animation->Limited());
  EXPECT_EQ(40000, animation->currentTime());
  SimulateFrame(10000);
  EXPECT_EQ(50000, animation->currentTime());
}

TEST_F(AnimationAnimationTestNoCompositing, EmptyAnimationsDontUpdateEffects) {
  animation = timeline->Play(nullptr);
  animation->Update(kTimingUpdateOnDemand);
  EXPECT_EQ(base::nullopt, animation->TimeToEffectChange());

  SimulateFrame(1234);
  EXPECT_EQ(base::nullopt, animation->TimeToEffectChange());
}

TEST_F(AnimationAnimationTestNoCompositing, AnimationsDisassociateFromEffect) {
  AnimationEffect* animation_node = animation->effect();
  Animation* animation2 = timeline->Play(animation_node);
  EXPECT_EQ(nullptr, animation->effect());
  animation->setEffect(animation_node);
  EXPECT_EQ(nullptr, animation2->effect());
}

TEST_F(AnimationAnimationTestNoCompositing, AnimationsReturnTimeToNextEffect) {
  Timing timing;
  timing.start_delay = 1;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(1);
  timing.end_delay = 1;
  auto* keyframe_effect = MakeGarbageCollected<KeyframeEffect>(
      nullptr, MakeEmptyEffectModel(), timing);
  animation = timeline->Play(keyframe_effect);
  animation->setStartTime(0);

  // Next effect change at end of start delay.
  SimulateFrame(0);
  EXPECT_EQ(AnimationTimeDelta::FromSecondsD(1),
            animation->TimeToEffectChange());

  // Next effect change at end of start delay.
  SimulateFrame(500);
  EXPECT_EQ(AnimationTimeDelta::FromSecondsD(0.5),
            animation->TimeToEffectChange());

  // Start of active phase.
  SimulateFrame(1000);
  EXPECT_EQ(AnimationTimeDelta(), animation->TimeToEffectChange());

  // Still in active phase.
  SimulateFrame(1500);
  EXPECT_EQ(AnimationTimeDelta(), animation->TimeToEffectChange());

  // Start of the after phase. Next effect change at end of after phase.
  SimulateFrame(2000);
  EXPECT_EQ(AnimationTimeDelta::FromSecondsD(1),
            animation->TimeToEffectChange());

  // Still in effect if fillmode = forward|both.
  SimulateFrame(3000);
  EXPECT_EQ(base::nullopt, animation->TimeToEffectChange());

  // Reset to start of animation. Next effect at the end of the start delay.
  animation->setCurrentTime(0);
  SimulateFrame(3000);
  EXPECT_EQ(AnimationTimeDelta::FromSecondsD(1),
            animation->TimeToEffectChange());

  // Start delay is scaled by playback rate.
  animation->setPlaybackRate(2);
  SimulateFrame(3000);
  EXPECT_EQ(AnimationTimeDelta::FromSecondsD(0.5),
            animation->TimeToEffectChange());

  // Effectively a paused animation.
  animation->setPlaybackRate(0);
  animation->Update(kTimingUpdateOnDemand);
  EXPECT_EQ(base::nullopt, animation->TimeToEffectChange());

  // Reversed animation from end time. Next effect after end delay.
  animation->setCurrentTime(3000);
  animation->setPlaybackRate(-1);
  animation->Update(kTimingUpdateOnDemand);
  SimulateFrame(3000);
  EXPECT_EQ(AnimationTimeDelta::FromSecondsD(1),
            animation->TimeToEffectChange());

  // End delay is scaled by playback rate.
  animation->setPlaybackRate(-2);
  animation->Update(kTimingUpdateOnDemand);
  SimulateFrame(3000);
  EXPECT_EQ(AnimationTimeDelta::FromSecondsD(0.5),
            animation->TimeToEffectChange());
}

TEST_F(AnimationAnimationTestNoCompositing, TimeToNextEffectWhenPaused) {
  EXPECT_EQ(AnimationTimeDelta(), animation->TimeToEffectChange());
  animation->pause();
  EXPECT_TRUE(animation->pending());
  EXPECT_EQ("paused", animation->playState());
  SimulateAwaitReady();
  EXPECT_FALSE(animation->pending());
  animation->Update(kTimingUpdateOnDemand);
  EXPECT_EQ(base::nullopt, animation->TimeToEffectChange());
}

TEST_F(AnimationAnimationTestNoCompositing,
       TimeToNextEffectWhenCancelledBeforeStart) {
  EXPECT_EQ(AnimationTimeDelta(), animation->TimeToEffectChange());
  animation->setCurrentTime(-8000);
  animation->setPlaybackRate(2);
  EXPECT_EQ("running", animation->playState());
  animation->cancel();
  EXPECT_EQ("idle", animation->playState());
  EXPECT_FALSE(animation->pending());
  animation->Update(kTimingUpdateOnDemand);
  // This frame will fire the finish event event though no start time has been
  // received from the compositor yet, as cancel() nukes start times.
  EXPECT_EQ(base::nullopt, animation->TimeToEffectChange());
}

TEST_F(AnimationAnimationTestNoCompositing,
       TimeToNextEffectWhenCancelledBeforeStartReverse) {
  EXPECT_EQ(AnimationTimeDelta(), animation->TimeToEffectChange());
  animation->setCurrentTime(9000);
  animation->setPlaybackRate(-3);
  EXPECT_EQ("running", animation->playState());
  animation->cancel();
  EXPECT_EQ("idle", animation->playState());
  EXPECT_FALSE(animation->pending());
  animation->Update(kTimingUpdateOnDemand);
  EXPECT_EQ(base::nullopt, animation->TimeToEffectChange());
}

TEST_F(AnimationAnimationTestNoCompositing,
       TimeToNextEffectSimpleCancelledBeforeStart) {
  EXPECT_EQ(AnimationTimeDelta(), animation->TimeToEffectChange());
  EXPECT_EQ("running", animation->playState());
  animation->cancel();
  EXPECT_EQ("idle", animation->playState());
  EXPECT_FALSE(animation->pending());
  animation->Update(kTimingUpdateOnDemand);
  EXPECT_EQ(base::nullopt, animation->TimeToEffectChange());
}

TEST_F(AnimationAnimationTestNoCompositing, AttachedAnimations) {
  Persistent<Element> element = GetDocument().CreateElementForBinding("foo");

  Timing timing;
  auto* keyframe_effect = MakeGarbageCollected<KeyframeEffect>(
      element.Get(), MakeEmptyEffectModel(), timing);
  Animation* animation = timeline->Play(keyframe_effect);
  SimulateFrame(0);
  timeline->ServiceAnimations(kTimingUpdateForAnimationFrame);
  EXPECT_EQ(
      1U, element->GetElementAnimations()->Animations().find(animation)->value);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(element->GetElementAnimations()->Animations().IsEmpty());
}

TEST_F(AnimationAnimationTestNoCompositing, HasLowerCompositeOrdering) {
  Animation* animation1 = timeline->Play(nullptr);
  Animation* animation2 = timeline->Play(nullptr);
  EXPECT_TRUE(Animation::HasLowerCompositeOrdering(
      animation1, animation2,
      Animation::CompareAnimationsOrdering::kPointerOrder));
}

TEST_F(AnimationAnimationTestNoCompositing, PlayAfterCancel) {
  animation->cancel();
  EXPECT_EQ("idle", animation->playState());
  EXPECT_FALSE(animation->currentTime().has_value());
  EXPECT_FALSE(animation->startTime().has_value());
  animation->play();
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());
  EXPECT_EQ(0, animation->currentTime());
  EXPECT_FALSE(animation->startTime().has_value());
  SimulateAwaitReady();
  EXPECT_FALSE(animation->pending());
  EXPECT_EQ(0, animation->currentTime());
  EXPECT_EQ(0, animation->startTime());

  SimulateFrame(10000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_EQ(10000, animation->currentTime());
  EXPECT_EQ(0, animation->startTime());
}

TEST_F(AnimationAnimationTestNoCompositing, PlayBackwardsAfterCancel) {
  animation->setPlaybackRate(-1);
  animation->setCurrentTime(15000);
  animation->cancel();
  EXPECT_EQ("idle", animation->playState());
  EXPECT_FALSE(animation->pending());
  EXPECT_FALSE(animation->currentTime().has_value());
  EXPECT_FALSE(animation->startTime().has_value());

  // Snap to the end of the animation.
  animation->play();
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());
  EXPECT_EQ(30000, animation->currentTime());
  EXPECT_FALSE(animation->startTime().has_value());
  SimulateAwaitReady();
  EXPECT_FALSE(animation->pending());
  EXPECT_EQ(30000, animation->startTime());

  SimulateFrame(10000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_EQ(20000, animation->currentTime());
  EXPECT_EQ(30000, animation->startTime());
}

TEST_F(AnimationAnimationTestNoCompositing, ReverseAfterCancel) {
  animation->cancel();
  EXPECT_EQ("idle", animation->playState());
  EXPECT_FALSE(animation->pending());
  EXPECT_FALSE(animation->currentTime().has_value());
  EXPECT_FALSE(animation->startTime().has_value());

  // Reverse snaps to the end of the animation.
  animation->reverse();
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());
  EXPECT_EQ(30000, animation->currentTime());
  EXPECT_FALSE(animation->startTime().has_value());
  SimulateAwaitReady();
  EXPECT_FALSE(animation->pending());
  EXPECT_EQ(30000, animation->startTime());

  SimulateFrame(10000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_EQ(20000, animation->currentTime());
  EXPECT_EQ(30000, animation->startTime());
}

TEST_F(AnimationAnimationTestNoCompositing, FinishAfterCancel) {
  NonThrowableExceptionState exception_state;
  animation->cancel();
  EXPECT_EQ("idle", animation->playState());
  EXPECT_FALSE(animation->currentTime().has_value());
  EXPECT_FALSE(animation->startTime().has_value());

  animation->finish(exception_state);
  EXPECT_EQ("finished", animation->playState());
  EXPECT_EQ(30000, animation->currentTime());
  EXPECT_EQ(-30000, animation->startTime());
}

TEST_F(AnimationAnimationTestNoCompositing, PauseAfterCancel) {
  animation->cancel();
  EXPECT_EQ("idle", animation->playState());
  EXPECT_FALSE(animation->currentTime().has_value());
  EXPECT_FALSE(animation->startTime().has_value());
  animation->pause();
  EXPECT_EQ("paused", animation->playState());
  EXPECT_TRUE(animation->pending());
  EXPECT_EQ(0, animation->currentTime());
  EXPECT_FALSE(animation->startTime().has_value());
  SimulateAwaitReady();
  EXPECT_FALSE(animation->pending());
  EXPECT_EQ(0, animation->currentTime());
  EXPECT_FALSE(animation->startTime().has_value());
}

// crbug.com/1052217
TEST_F(AnimationAnimationTestNoCompositing, SetPlaybackRateAfterFinish) {
  animation->setEffect(MakeAnimation(30, Timing::FillMode::FORWARDS));
  animation->finish();
  animation->Update(kTimingUpdateOnDemand);
  EXPECT_EQ("finished", animation->playState());
  EXPECT_EQ(base::nullopt, animation->TimeToEffectChange());

  // Reversing a finished animation marks the animation as outdated. Required
  // to recompute the time to next interval.
  animation->setPlaybackRate(-1);
  EXPECT_EQ("running", animation->playState());
  EXPECT_EQ(animation->playbackRate(), -1);
  EXPECT_TRUE(animation->Outdated());
  animation->Update(kTimingUpdateOnDemand);
  EXPECT_EQ(0, animation->TimeToEffectChange()->InSecondsF());
  EXPECT_FALSE(animation->Outdated());
}

TEST_F(AnimationAnimationTestNoCompositing, UpdatePlaybackRateAfterFinish) {
  animation->setEffect(MakeAnimation(30, Timing::FillMode::FORWARDS));
  animation->finish();
  animation->Update(kTimingUpdateOnDemand);
  EXPECT_EQ("finished", animation->playState());
  EXPECT_EQ(base::nullopt, animation->TimeToEffectChange());

  // Reversing a finished animation marks the animation as outdated. Required
  // to recompute the time to next interval. The pending playback rate is
  // immediately applied when updatePlaybackRate is called on a non-running
  // animation.
  animation->updatePlaybackRate(-1);
  EXPECT_EQ("running", animation->playState());
  EXPECT_EQ(animation->playbackRate(), -1);
  EXPECT_TRUE(animation->Outdated());
  animation->Update(kTimingUpdateOnDemand);
  EXPECT_EQ(0, animation->TimeToEffectChange()->InSecondsF());
  EXPECT_FALSE(animation->Outdated());
}

TEST_F(AnimationAnimationTestCompositeAfterPaint,
       NoCompositeWithoutCompositedElementId) {
  SetBodyInnerHTML(
      "<div id='foo' style='position: relative; will-change: "
      "opacity;'>composited</div>"
      "<div id='bar' style='position: relative'>not composited</div>");

  LayoutObject* object_composited = GetLayoutObjectByElementId("foo");
  LayoutObject* object_not_composited = GetLayoutObjectByElementId("bar");

  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);
  auto* keyframe_effect_composited = MakeGarbageCollected<KeyframeEffect>(
      To<Element>(object_composited->GetNode()), MakeSimpleEffectModel(),
      timing);
  Animation* animation_composited = timeline->Play(keyframe_effect_composited);
  auto* keyframe_effect_not_composited = MakeGarbageCollected<KeyframeEffect>(
      To<Element>(object_not_composited->GetNode()), MakeSimpleEffectModel(),
      timing);
  Animation* animation_not_composited =
      timeline->Play(keyframe_effect_not_composited);

  SimulateFrame(0);
  EXPECT_EQ(animation_composited->CheckCanStartAnimationOnCompositorInternal(),
            CompositorAnimations::kNoFailure);
  const PaintArtifactCompositor* paint_artifact_compositor =
      GetDocument().View()->GetPaintArtifactCompositor();
  ASSERT_TRUE(paint_artifact_compositor);
  EXPECT_EQ(animation_composited->CheckCanStartAnimationOnCompositor(
                paint_artifact_compositor),
            CompositorAnimations::kNoFailure);
  EXPECT_NE(animation_not_composited->CheckCanStartAnimationOnCompositor(
                paint_artifact_compositor),
            CompositorAnimations::kNoFailure);
}

// Regression test for http://crbug.com/819591 . If a compositable animation is
// played and then paused before any start time is set (either blink or
// compositor side), the pausing must still set compositor pending or the pause
// won't be synced.
TEST_F(AnimationAnimationTestCompositing,
       SetCompositorPendingWithUnresolvedStartTimes) {
  ResetWithCompositedAnimation();

  // At this point, the animation exists on both the compositor and blink side,
  // but no start time has arrived on either side. The compositor is currently
  // synced, no update is pending.
  EXPECT_FALSE(animation->CompositorPendingForTesting());

  // However, if we pause the animation then the compositor should still be
  // marked pending. This is required because otherwise the compositor will go
  // ahead and start playing the animation once it receives a start time (e.g.
  // on the next compositor frame).
  animation->pause();

  EXPECT_TRUE(animation->CompositorPendingForTesting());
}

TEST_F(AnimationAnimationTestCompositing, PreCommitWithUnresolvedStartTimes) {
  ResetWithCompositedAnimation();

  // At this point, the animation exists on both the compositor and blink side,
  // but no start time has arrived on either side. The compositor is currently
  // synced, no update is pending.
  EXPECT_FALSE(animation->CompositorPendingForTesting());

  // At this point, a call to PreCommit should bail out and tell us to wait for
  // next commit because there are no resolved start times.
  EXPECT_FALSE(animation->PreCommit(0, nullptr, true));
}

namespace {
int GenerateHistogramValue(CompositorAnimations::FailureReason reason) {
  // The enum values in CompositorAnimations::FailureReasons are stored as 2^i
  // as they are a bitmask, but are recorded into the histogram as (i+1) to give
  // sequential histogram values. The exception is kNoFailure, which is stored
  // as 0 and recorded as 0.
  if (reason == CompositorAnimations::kNoFailure)
    return CompositorAnimations::kNoFailure;
  return base::bits::CountTrailingZeroBits(static_cast<uint32_t>(reason)) + 1;
}
}  // namespace

TEST_F(AnimationAnimationTestCompositing, PreCommitRecordsHistograms) {
  const std::string histogram_name =
      "Blink.Animation.CompositedAnimationFailureReason";

  // Initially the animation in this test has no target, so it is invalid.
  {
    HistogramTester histogram;
    ASSERT_TRUE(animation->PreCommit(0, nullptr, true));
    histogram.ExpectBucketCount(
        histogram_name,
        GenerateHistogramValue(CompositorAnimations::kInvalidAnimationOrEffect),
        1);
  }

  // Restart the animation with a target and compositing state.
  {
    HistogramTester histogram;
    ResetWithCompositedAnimation();
    histogram.ExpectBucketCount(
        histogram_name,
        GenerateHistogramValue(CompositorAnimations::kNoFailure), 1);
  }

  // Now make the playback rate 0. This trips both the invalid animation and
  // unsupported timing parameter reasons.
  animation->setPlaybackRate(0);
  animation->NotifyReady(100);
  {
    HistogramTester histogram;
    ASSERT_TRUE(animation->PreCommit(0, nullptr, true));
    histogram.ExpectBucketCount(
        histogram_name,
        GenerateHistogramValue(CompositorAnimations::kInvalidAnimationOrEffect),
        1);
    histogram.ExpectBucketCount(
        histogram_name,
        GenerateHistogramValue(
            CompositorAnimations::kEffectHasUnsupportedTimingParameters),
        1);
  }
  animation->setPlaybackRate(1);

  // Finally, change the keyframes to something unsupported by the compositor.
  Persistent<StringKeyframe> start_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(
      CSSPropertyID::kLeft, "0", SecureContextMode::kInsecureContext, nullptr);
  Persistent<StringKeyframe> end_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  end_keyframe->SetCSSPropertyValue(CSSPropertyID::kLeft, "100px",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  To<KeyframeEffect>(animation->effect())
      ->SetKeyframes({start_keyframe, end_keyframe});
  UpdateAllLifecyclePhasesForTest();
  {
    HistogramTester histogram;
    ASSERT_TRUE(animation->PreCommit(0, nullptr, true));
    histogram.ExpectBucketCount(
        histogram_name,
        GenerateHistogramValue(CompositorAnimations::kUnsupportedCSSProperty),
        1);
  }
}

// crbug.com/990000.
TEST_F(AnimationAnimationTestCompositing, ReplaceCompositedAnimation) {
  const std::string histogram_name =
      "Blink.Animation.CompositedAnimationFailureReason";

  // Start with a composited animation.
  ResetWithCompositedAnimation();
  ASSERT_TRUE(animation->HasActiveAnimationsOnCompositor());

  // Replace the animation. The new animation should not be incompatible and
  // therefore able to run on the compositor.
  animation->cancel();
  MakeCompositedAnimation();
  ASSERT_TRUE(animation->HasActiveAnimationsOnCompositor());
}

TEST_F(AnimationAnimationTestCompositing, SetKeyframesCausesCompositorPending) {
  ResetWithCompositedAnimation();

  // At this point, the animation exists on both the compositor and blink side,
  // but no start time has arrived on either side. The compositor is currently
  // synced, no update is pending.
  EXPECT_FALSE(animation->CompositorPendingForTesting());

  // Now change the keyframes; this should mark the animation as compositor
  // pending as we need to sync the compositor side.
  Persistent<StringKeyframe> start_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "0.0",
                                      SecureContextMode::kInsecureContext,
                                      nullptr);
  Persistent<StringKeyframe> end_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  end_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "1.0",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  StringKeyframeVector keyframes;
  keyframes.push_back(start_keyframe);
  keyframes.push_back(end_keyframe);

  To<KeyframeEffect>(animation->effect())->SetKeyframes(keyframes);

  EXPECT_TRUE(animation->CompositorPendingForTesting());
}

// crbug.com/1057076
// Infinite duration animations should not run on the compositor.
TEST_F(AnimationAnimationTestCompositing, InfiniteDurationAnimation) {
  ResetWithCompositedAnimation();
  EXPECT_EQ(CompositorAnimations::kNoFailure,
            animation->CheckCanStartAnimationOnCompositor(nullptr));

  OptionalEffectTiming* effect_timing = OptionalEffectTiming::Create();
  effect_timing->setDuration(UnrestrictedDoubleOrString::FromUnrestrictedDouble(
      std::numeric_limits<double>::infinity()));
  animation->effect()->updateTiming(effect_timing);
  EXPECT_EQ(CompositorAnimations::kEffectHasUnsupportedTimingParameters,
            animation->CheckCanStartAnimationOnCompositor(nullptr));
}

TEST_F(AnimationAnimationTestCompositing,
       ScrollLinkedAnimationCanBeComposited) {
  ResetWithCompositedAnimation();
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { will-change: transform; overflow: scroll; width: 100px; height: 100px; }
      #target { width: 100px; height: 200px; will-change: opacity;}
      #spacer { width: 200px; height: 2000px; }
    </style>
    <div id ='scroller'>
      <div id ='target'></div>
      <div id ='spacer'></div>
    </div>
  )HTML");

  // Create ScrollTimeline
  auto* scroller =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->SetScrollOffset(ScrollOffset(0, 20),
                                   mojom::blink::ScrollType::kProgrammatic);
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  // Create KeyframeEffect
  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);

  Persistent<StringKeyframe> start_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "1.0",
                                      SecureContextMode::kInsecureContext,
                                      nullptr);
  Persistent<StringKeyframe> end_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  end_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "0.0",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  StringKeyframeVector keyframes;
  keyframes.push_back(start_keyframe);
  keyframes.push_back(end_keyframe);

  Element* element = GetElementById("target");
  auto* model = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);

  // Create scroll-linked animation
  NonThrowableExceptionState exception_state;
  Animation* scroll_animation = Animation::Create(
      MakeGarbageCollected<KeyframeEffect>(element, model, timing),
      scroll_timeline, exception_state);

  model->SnapshotAllCompositorKeyframesIfNecessary(
      *element, *ComputedStyle::Create(), nullptr);

  UpdateAllLifecyclePhasesForTest();
  scroll_animation->play();
  EXPECT_EQ(scroll_animation->CheckCanStartAnimationOnCompositor(nullptr),
            CompositorAnimations::kNoFailure);
}

TEST_F(AnimationAnimationTestCompositing,
       StartScrollLinkedAnimationWithStartTimeIfApplicable) {
  ResetWithCompositedAnimation();
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { will-change: transform; overflow: scroll; width: 100px; height: 100px; }
      #target { width: 100px; height: 200px; will-change: opacity;}
      #spacer { width: 200px; height: 700px; }
    </style>
    <div id ='scroller'>
      <div id ='target'></div>
      <div id ='spacer'></div>
    </div>
  )HTML");

  // Create ScrollTimeline
  auto* scroller =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->SetScrollOffset(ScrollOffset(0, 100),
                                   mojom::blink::ScrollType::kProgrammatic);
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  // Create KeyframeEffect
  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);

  Persistent<StringKeyframe> start_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "1.0",
                                      SecureContextMode::kInsecureContext,
                                      nullptr);
  Persistent<StringKeyframe> end_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  end_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "0.0",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  StringKeyframeVector keyframes;
  keyframes.push_back(start_keyframe);
  keyframes.push_back(end_keyframe);

  Element* element = GetElementById("target");
  auto* model = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);

  KeyframeEffect* keyframe_effect =
      MakeGarbageCollected<KeyframeEffect>(element, model, timing);

  // Create scroll-linked animation
  NonThrowableExceptionState exception_state;
  Animation* scroll_animation =
      Animation::Create(keyframe_effect, scroll_timeline, exception_state);

  model->SnapshotAllCompositorKeyframesIfNecessary(
      *element, *ComputedStyle::Create(), nullptr);

  UpdateAllLifecyclePhasesForTest();
  const double TEST_START_TIME = 10;
  scroll_animation->setStartTime(TEST_START_TIME);
  scroll_animation->play();
  EXPECT_EQ(scroll_animation->CheckCanStartAnimationOnCompositor(nullptr),
            CompositorAnimations::kNoFailure);
  // Start the animation on compositor. The time offset of the compositor
  // keyframe should be unset if we start the animation with its start time.
  scroll_animation->PreCommit(1, nullptr, true);
  cc::KeyframeModel* keyframe_model =
      keyframe_effect->GetAnimationForTesting()
          ->GetCompositorAnimation()
          ->CcAnimation()
          ->GetKeyframeModel(compositor_target_property::OPACITY);
  EXPECT_EQ(keyframe_model->start_time() - base::TimeTicks(),
            base::TimeDelta::FromMilliseconds(TEST_START_TIME));
  EXPECT_EQ(keyframe_model->time_offset(), base::TimeDelta());
}

// Verifies correctness of scroll linked animation current and start times in
// various animation states.
TEST_F(AnimationAnimationTestNoCompositing, ScrollLinkedAnimationCreation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { width: 200px; height: 200px; }
    </style>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");

  auto* scroller =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->SetScrollOffset(ScrollOffset(0, 20),
                                   mojom::blink::ScrollType::kProgrammatic);
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  NonThrowableExceptionState exception_state;
  Animation* scroll_animation =
      Animation::Create(MakeAnimation(), scroll_timeline, exception_state);

  // Verify start and current times in Idle state.
  EXPECT_FALSE(scroll_animation->startTime().has_value());
  EXPECT_FALSE(scroll_animation->currentTime().has_value());

  scroll_animation->play();

  // Verify start and current times in Pending state.
  EXPECT_EQ(0, scroll_animation->startTime());
  EXPECT_EQ(20, scroll_animation->currentTime());

  UpdateAllLifecyclePhasesForTest();
  // Verify start and current times in Playing state.
  EXPECT_EQ(0, scroll_animation->startTime());
  EXPECT_EQ(20, scroll_animation->currentTime());

  // Verify current time after scroll.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 40),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrameForScrollAnimations();
  EXPECT_EQ(40, scroll_animation->currentTime());
}

// Verifies that finished composited scroll-linked animations restart on
// compositor upon reverse scrolling.
TEST_F(AnimationAnimationTestCompositing,
       FinishedScrollLinkedAnimationRestartsOnReverseScrolling) {
  ResetWithCompositedAnimation();
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { will-change: transform; overflow: scroll; width: 100px; height: 100px; }
      #target { width: 100px; height: 200px; will-change: opacity;}
      #spacer { width: 200px; height: 700px; }
    </style>
    <div id ='scroller'>
      <div id ='target'></div>
      <div id ='spacer'></div>
    </div>
  )HTML");

  auto* scroller =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller->UsesCompositedScrolling());

  // Create ScrollTimeline
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  // Create KeyframeEffect
  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);
  Persistent<StringKeyframe> start_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "1.0",
                                      SecureContextMode::kInsecureContext,
                                      nullptr);
  Persistent<StringKeyframe> end_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  end_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "0.0",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  StringKeyframeVector keyframes;
  keyframes.push_back(start_keyframe);
  keyframes.push_back(end_keyframe);

  Element* element = GetElementById("target");
  auto* model = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);

  KeyframeEffect* keyframe_effect =
      MakeGarbageCollected<KeyframeEffect>(element, model, timing);

  // Create scroll-linked animation
  NonThrowableExceptionState exception_state;
  Animation* scroll_animation =
      Animation::Create(keyframe_effect, scroll_timeline, exception_state);
  model->SnapshotAllCompositorKeyframesIfNecessary(
      *element, *ComputedStyle::Create(), nullptr);
  UpdateAllLifecyclePhasesForTest();

  scroll_animation->play();
  EXPECT_EQ(scroll_animation->playState(), "running");
  GetDocument().GetPendingAnimations().Update(nullptr, true);
  EXPECT_TRUE(scroll_animation->HasActiveAnimationsOnCompositor());

  // Advances the animation to "finished" state. The composited animation will
  // be destroyed accordingly.
  scroll_animation->setCurrentTime(50000);
  EXPECT_EQ(scroll_animation->playState(), "finished");
  scroll_animation->Update(kTimingUpdateForAnimationFrame);
  GetDocument().GetPendingAnimations().Update(nullptr, true);
  EXPECT_FALSE(scroll_animation->HasActiveAnimationsOnCompositor());

  // Restarting the animation should create a new compositor animation.
  scroll_animation->setCurrentTime(100);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(scroll_animation->playState(), "running");
  scroll_animation->Update(kTimingUpdateForAnimationFrame);
  GetDocument().GetPendingAnimations().Update(nullptr, true);
  EXPECT_TRUE(scroll_animation->HasActiveAnimationsOnCompositor());
}

TEST_F(AnimationAnimationTestNoCompositing,
       RemoveCanceledAnimationFromActiveSet) {
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->Update(kTimingUpdateForAnimationFrame));
  SimulateFrame(1000);
  EXPECT_TRUE(animation->Update(kTimingUpdateForAnimationFrame));
  animation->cancel();
  EXPECT_EQ("idle", animation->playState());
  EXPECT_FALSE(animation->Update(kTimingUpdateForAnimationFrame));
}

TEST_F(AnimationAnimationTestNoCompositing,
       RemoveFinishedAnimationFromActiveSet) {
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->Update(kTimingUpdateForAnimationFrame));
  SimulateFrame(1000);
  EXPECT_TRUE(animation->Update(kTimingUpdateForAnimationFrame));

  // Synchronous completion.
  animation->finish();
  EXPECT_EQ("finished", animation->playState());
  EXPECT_FALSE(animation->Update(kTimingUpdateForAnimationFrame));

  // Play creates a new pending finished promise.
  animation->play();
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->Update(kTimingUpdateForAnimationFrame));

  // Asynchronous completion.
  animation->setCurrentTime(50000);
  EXPECT_EQ("finished", animation->playState());
  EXPECT_FALSE(animation->Update(kTimingUpdateForAnimationFrame));
}

TEST_F(AnimationAnimationTestNoCompositing,
       PendingActivityWithFinishedPromise) {
  // No pending activity even when running if there is no finished promise
  // or event listener.
  EXPECT_EQ("running", animation->playState());
  SimulateFrame(1000);
  EXPECT_FALSE(animation->HasPendingActivity());

  // An unresolved finished promise indicates pending activity.
  ScriptState* script_state =
      ToScriptStateForMainWorld(GetDocument().GetFrame());
  animation->finished(script_state);
  EXPECT_TRUE(animation->HasPendingActivity());

  // Resolving the finished promise clears the pending activity.
  animation->setCurrentTime(50000);
  EXPECT_EQ("finished", animation->playState());
  SimulateMicrotask();
  EXPECT_FALSE(animation->Update(kTimingUpdateForAnimationFrame));
  EXPECT_FALSE(animation->HasPendingActivity());

  // Playing an already finished animation creates a new pending finished
  // promise.
  animation->play();
  EXPECT_EQ("running", animation->playState());
  SimulateFrame(2000);
  EXPECT_TRUE(animation->HasPendingActivity());
  // Cancel rejects the finished promise and creates a new pending finished
  // promise.
  // TODO(crbug.com/960944): Investigate if this should return false to prevent
  // holding onto the animation indefinitely.
  animation->cancel();
  EXPECT_TRUE(animation->HasPendingActivity());
}

class MockEventListener final : public NativeEventListener {
 public:
  MOCK_METHOD2(Invoke, void(ExecutionContext*, Event*));
};

TEST_F(AnimationAnimationTestNoCompositing,
       PendingActivityWithFinishedEventListener) {
  EXPECT_EQ("running", animation->playState());
  EXPECT_FALSE(animation->HasPendingActivity());

  // Attaching a listener for the finished event indicates pending activity.
  Persistent<MockEventListener> event_listener =
      MakeGarbageCollected<MockEventListener>();
  animation->addEventListener(event_type_names::kFinish, event_listener);
  EXPECT_TRUE(animation->HasPendingActivity());

  // Synchronous finish clears pending activity.
  animation->finish();
  EXPECT_EQ("finished", animation->playState());
  EXPECT_FALSE(animation->Update(kTimingUpdateForAnimationFrame));
  EXPECT_TRUE(animation->HasPendingActivity());
  animation->pending_finished_event_ = nullptr;
  EXPECT_FALSE(animation->HasPendingActivity());

  // Playing an already finished animation resets the finished state.
  animation->play();
  EXPECT_EQ("running", animation->playState());
  SimulateFrame(2000);
  EXPECT_TRUE(animation->HasPendingActivity());

  // Finishing the animation asynchronously clears the pending activity.
  animation->setCurrentTime(50000);
  EXPECT_EQ("finished", animation->playState());
  SimulateMicrotask();
  EXPECT_FALSE(animation->Update(kTimingUpdateForAnimationFrame));
  EXPECT_TRUE(animation->HasPendingActivity());
  animation->pending_finished_event_ = nullptr;
  EXPECT_FALSE(animation->HasPendingActivity());

  // Canceling an animation clears the pending activity.
  animation->play();
  EXPECT_EQ("running", animation->playState());
  SimulateFrame(2000);
  animation->cancel();
  EXPECT_EQ("idle", animation->playState());
  EXPECT_FALSE(animation->Update(kTimingUpdateForAnimationFrame));
  EXPECT_FALSE(animation->HasPendingActivity());
}

class AnimationPendingAnimationsTest : public RenderingTest {
 public:
  AnimationPendingAnimationsTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

  enum CompositingMode { kComposited, kNonComposited };

  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
    GetDocument().GetAnimationClock().ResetTimeForTesting();
    timeline = GetDocument().Timeline();
    timeline->ResetForTesting();
  }

  Animation* MakeAnimation(const char* target, CompositingMode mode) {
    Timing timing;
    timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);
    Persistent<StringKeyframe> start_keyframe =
        MakeGarbageCollected<StringKeyframe>();
    start_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "1.0",
                                        SecureContextMode::kInsecureContext,
                                        nullptr);
    Persistent<StringKeyframe> end_keyframe =
        MakeGarbageCollected<StringKeyframe>();
    end_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "0.0",
                                      SecureContextMode::kInsecureContext,
                                      nullptr);

    StringKeyframeVector keyframes;
    keyframes.push_back(start_keyframe);
    keyframes.push_back(end_keyframe);

    Element* element = GetElementById(target);
    auto* model = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);

    Animation* animation = timeline->Play(
        MakeGarbageCollected<KeyframeEffect>(element, model, timing));

    if (mode == kNonComposited) {
      // Having a playback rate of zero is one of several ways to force an
      // animation to be non-composited.
      animation->updatePlaybackRate(0);
    }

    return animation;
  }

  bool Update() {
    UpdateAllLifecyclePhasesForTest();
    GetDocument().GetAnimationClock().UpdateTime(base::TimeTicks());
    return GetDocument().GetPendingAnimations().Update(nullptr, true);
  }

  void NotifyAnimationStarted(Animation* animation) {
    animation->GetDocument()
        ->GetPendingAnimations()
        .NotifyCompositorAnimationStarted(0, animation->CompositorGroup());
  }

  void restartAnimation(Animation* animation) {
    animation->cancel();
    animation->play();
  }

  Persistent<DocumentTimeline> timeline;
};

TEST_F(AnimationPendingAnimationsTest, PendingAnimationStartSynchronization) {
  RunDocumentLifecycle();
  SetBodyInnerHTML("<div id='foo'></div><div id='bar'></div>");

  Persistent<Animation> animA = MakeAnimation("foo", kComposited);
  Persistent<Animation> animB = MakeAnimation("bar", kNonComposited);

  // B's start time synchronized with A's start time.
  EXPECT_TRUE(Update());
  EXPECT_TRUE(animA->pending());
  EXPECT_TRUE(animB->pending());
  EXPECT_TRUE(animA->HasActiveAnimationsOnCompositor());
  EXPECT_FALSE(animB->HasActiveAnimationsOnCompositor());
  NotifyAnimationStarted(animA);
  EXPECT_FALSE(animA->pending());
  EXPECT_FALSE(animB->pending());
}

TEST_F(AnimationPendingAnimationsTest,
       PendingAnimationCancelUnblocksSynchronizedStart) {
  RunDocumentLifecycle();
  SetBodyInnerHTML("<div id='foo'></div><div id='bar'></div>");

  Persistent<Animation> animA = MakeAnimation("foo", kComposited);
  Persistent<Animation> animB = MakeAnimation("bar", kNonComposited);

  EXPECT_TRUE(Update());
  EXPECT_TRUE(animA->pending());
  EXPECT_TRUE(animB->pending());
  animA->cancel();

  // Animation A no longer blocks B from starting.
  EXPECT_FALSE(Update());
  EXPECT_FALSE(animB->pending());
}

TEST_F(AnimationPendingAnimationsTest,
       PendingAnimationOnlySynchronizeStartsOfNewlyPendingAnimations) {
  RunDocumentLifecycle();
  SetBodyInnerHTML(
      "<div id='foo'></div><div id='bar'></div><div id='baz'></div>");

  Persistent<Animation> animA = MakeAnimation("foo", kComposited);
  Persistent<Animation> animB = MakeAnimation("bar", kNonComposited);

  // This test simulates the conditions in crbug.com/666710. The start of a
  // non-composited animation is deferred in order to synchronize with a
  // composited animation, which is canceled before it starts. Subsequent frames
  // produce new composited animations which prevented the non-composited
  // animation from ever starting. Non-composited animations should not be
  // synchronize with new composited animations if queued up in a prior call to
  // PendingAnimations::Update.
  EXPECT_TRUE(Update());
  EXPECT_TRUE(animA->pending());
  EXPECT_TRUE(animB->pending());
  animA->cancel();

  Persistent<Animation> animC = MakeAnimation("baz", kComposited);
  Persistent<Animation> animD = MakeAnimation("bar", kNonComposited);

  EXPECT_TRUE(Update());
  // B's is unblocked despite newly created composited animation.
  EXPECT_FALSE(animB->pending());
  EXPECT_TRUE(animC->pending());
  // D's start time is synchronized with C's start.
  EXPECT_TRUE(animD->pending());
  NotifyAnimationStarted(animC);
  EXPECT_FALSE(animC->pending());
  EXPECT_FALSE(animD->pending());
}

TEST_F(AnimationAnimationTestCompositing,
       ScrollLinkedAnimationNotCompositedIfScrollSourceIsNotComposited) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      /* to prevent the mock overlay scrollbar from affecting compositing. */
      #scroller::-webkit-scrollbar { display: none; }
      #target { width: 100px; height: 200px; will-change: transform; }
      #spacer { width: 200px; height: 2000px; }
    </style>
    <div id ='scroller'>
      <div id ='target'></div>
      <div id ='spacer'></div>
    </div>
  )HTML");

  // Create ScrollTimeline
  auto* scroller =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_FALSE(scroller->UsesCompositedScrolling());
  scrollable_area->SetScrollOffset(ScrollOffset(0, 20),
                                   mojom::blink::ScrollType::kProgrammatic);
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  // Create KeyframeEffect
  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);

  Persistent<StringKeyframe> start_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "1.0",
                                      SecureContextMode::kInsecureContext,
                                      nullptr);
  Persistent<StringKeyframe> end_keyframe =
      MakeGarbageCollected<StringKeyframe>();
  end_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "0.0",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  StringKeyframeVector keyframes;
  keyframes.push_back(start_keyframe);
  keyframes.push_back(end_keyframe);

  Element* element = GetElementById("target");
  auto* model = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);

  // Create scroll-linked animation
  NonThrowableExceptionState exception_state;
  Animation* scroll_animation = Animation::Create(
      MakeGarbageCollected<KeyframeEffect>(element, model, timing),
      scroll_timeline, exception_state);

  model->SnapshotAllCompositorKeyframesIfNecessary(
      *element, *ComputedStyle::Create(), nullptr);

  UpdateAllLifecyclePhasesForTest();
  scroll_animation->play();
  EXPECT_EQ(scroll_animation->CheckCanStartAnimationOnCompositor(nullptr),
            CompositorAnimations::kTimelineSourceHasInvalidCompositingState);
}

}  // namespace blink
