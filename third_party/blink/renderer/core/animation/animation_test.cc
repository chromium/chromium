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

#include <bit>
#include <memory>
#include <tuple>

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "cc/trees/target_property.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_optional_effect_timing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_timeline_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_string_unrestricteddouble.h"
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
#include "third_party/blink/renderer/core/css/cssom/css_unit_values.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

void ExpectRelativeErrorWithinEpsilon(double expected, double observed) {
  EXPECT_NEAR(1.0, observed / expected, std::numeric_limits<double>::epsilon());
}

class AnimationAnimationTestNoCompositing : public PaintTestConfigurations,
                                            public RenderingTest {
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
    animation->setStartTime(MakeGarbageCollected<V8CSSNumberish>(0),
                            ASSERT_NO_EXCEPTION);
    animation->setEffect(MakeAnimation());
  }

  void StartTimeline() { SimulateFrame(0); }

  KeyframeEffectModelBase* MakeSimpleEffectModel() {
    PropertyHandle PropertyHandleOpacity(GetCSSPropertyOpacity());
    static CSSNumberInterpolationType opacity_type(PropertyHandleOpacity);
    TransitionKeyframe* start_keyframe =
        MakeGarbageCollected<TransitionKeyframe>(PropertyHandleOpacity);
    start_keyframe->SetValue(MakeGarbageCollected<TypedInterpolationValue>(
        opacity_type, MakeGarbageCollected<InterpolableNumber>(1.0)));
    start_keyframe->SetOffset(0.0);
    // Egregious hack: Sideload the compositor value.
    // This is usually set in a part of the rendering process SimulateFrame
    // doesn't call.
    start_keyframe->SetCompositorValue(
        MakeGarbageCollected<CompositorKeyframeDouble>(1.0));
    TransitionKeyframe* end_keyframe =
        MakeGarbageCollected<TransitionKeyframe>(PropertyHandleOpacity);
    end_keyframe->SetValue(MakeGarbageCollected<TypedInterpolationValue>(
        opacity_type, MakeGarbageCollected<InterpolableNumber>(0.0)));
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

    SetBodyInnerHTML(R"HTML(
      <div id='target' style='width: 1px; height: 1px; background: green'></div>
    )HTML");

    MakeCompositedAnimation();
  }

  void MakeCompositedAnimation() {
    // Create a compositable animation; in this case opacity from 1 to 0.
    Timing timing;
    timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);

    StringKeyframe* start_keyframe = MakeGarbageCollected<StringKeyframe>();
    start_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "1.0",
                                        SecureContextMode::kInsecureContext,
                                        nullptr);
    StringKeyframe* end_keyframe = MakeGarbageCollected<StringKeyframe>();
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
    timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(duration);
    timing.fill_mode = fill_mode;
    return MakeGarbageCollected<KeyframeEffect>(nullptr, MakeEmptyEffectModel(),
                                                timing);
  }

  void SimulateFrame(double time_ms) {
    if (animation->pending()) {
      animation->NotifyReady(
          ANIMATION_TIME_DELTA_FROM_MILLISECONDS(last_frame_time));
    }
    SimulateMicrotask();

    last_frame_time = time_ms;
    const auto* paint_artifact_compositor =
        GetDocument().GetFrame()->View()->GetPaintArtifactCompositor();
    GetDocument().GetAnimationClock().UpdateTime(base::TimeTicks() +
                                                 base::Milliseconds(time_ms));

    // The timeline does not know about our animation, so we have to explicitly
    // call update().
    animation->Update(kTimingUpdateForAnimationFrame);
    GetDocument().GetPendingAnimations().Update(paint_artifact_compositor,
                                                false);
  }

  void SimulateAwaitReady() { SimulateFrame(last_frame_time); }

  void SimulateMicrotask() {
    GetDocument().GetAgent().event_loop()->PerformMicrotaskCheckpoint();
  }

  void SimulateFrameForScrollAnimations() {
    // Advance time by 100 ms.
    auto new_time = GetAnimationClock().CurrentTime() + base::Milliseconds(100);
    GetPage().Animator().ServiceScriptedAnimations(new_time);
  }

  bool StartTimeIsSet(Animation* for_animation) {
    return for_animation->startTime();
  }

  bool CurrentTimeIsSet(Animation* for_animation) {
    return for_animation->currentTime();
  }

  double GetStartTimeMs(Animation* for_animation) {
    return for_animation->startTime()->GetAsDouble();
  }

  double GetCurrentTimeMs(Animation* for_animation) {
    return for_animation->currentTime()->GetAsDouble();
  }

  double GetStartTimePercent(Animation* for_animation) {
    return for_animation->startTime()
        ->GetAsCSSNumericValue()
        ->to(CSSPrimitiveValue::UnitType::kPercentage)
        ->value();
  }

  double GetCurrentTimePercent(Animation* for_animation) {
    return for_animation->currentTime()
        ->GetAsCSSNumericValue()
        ->to(CSSPrimitiveValue::UnitType::kPercentage)
        ->value();
  }

  bool UsesCompositedScrolling(const LayoutBox& box) const {
    auto* pac = GetDocument().GetFrame()->View()->GetPaintArtifactCompositor();
    auto* property_trees =
        pac->RootLayer()->layer_tree_host()->property_trees();
    const auto* cc_scroll = property_trees->scroll_tree().Node(
        box.FirstFragment().PaintProperties()->Scroll()->CcNodeId(
            property_trees->sequence_number()));
    return cc_scroll && cc_scroll->is_composited;
  }

#define EXPECT_TIME(expected, observed) \
  EXPECT_NEAR(expected, observed, Animation::kTimeToleranceMs)

  Persistent<DocumentTimeline> timeline;
  Persistent<Animation> animation;

 private:
  double last_frame_time;
};

class AnimationAnimationTestCompositing
    : public AnimationAnimationTestNoCompositing {
 public:
  Animation* CreateAnimation(CSSPropertyID property_id,
                             String from,
                             String to) {
    Timing timing;
    timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);

    StringKeyframe* start_keyframe = MakeGarbageCollected<StringKeyframe>();
    start_keyframe->SetCSSPropertyValue(
        property_id, from, SecureContextMode::kInsecureContext, nullptr);
    StringKeyframe* end_keyframe = MakeGarbageCollected<StringKeyframe>();
    end_keyframe->SetCSSPropertyValue(
        property_id, to, SecureContextMode::kInsecureContext, nullptr);

    StringKeyframeVector keyframes;
    keyframes.push_back(start_keyframe);
    keyframes.push_back(end_keyframe);

    Element* element = GetElementById("target");
    auto* model = MakeGarbageCollected<StringKeyframeEffectModel>(keyframes);

    NonThrowableExceptionState exception_state;
    DocumentTimeline* timeline =
        MakeGarbageCollected<DocumentTimeline>(&GetDocument());
    return Animation::Create(
        MakeGarbageCollected<KeyframeEffect>(element, model, timing), timeline,
        exception_state);
  }

 private:
  void SetUp() override {
    EnableCompositing();
    AnimationAnimationTestNoCompositing::SetUp();
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(AnimationAnimationTestNoCompositing);
INSTANTIATE_PAINT_TEST_SUITE_P(AnimationAnimationTestCompositing);

TEST_P(AnimationAnimationTestNoCompositing, InitialState) {
  SetUpWithoutStartingTimeline();
  animation = timeline->Play(nullptr);
  EXPECT_TIME(0, GetCurrentTimeMs(animation));
  EXPECT_TRUE(animation->pending());
  EXPECT_FALSE(animation->Paused());
  EXPECT_EQ(1, animation->playbackRate());
  EXPECT_FALSE(StartTimeIsSet(animation));

  StartTimeline();
  EXPECT_EQ("finished", animation->playState());
  EXPECT_TIME(0, timeline->CurrentTimeMilliseconds().value());
  EXPECT_TIME(0, GetCurrentTimeMs(animation));
  EXPECT_FALSE(animation->Paused());
  EXPECT_FALSE(animation->pending());
  EXPECT_EQ(1, animation->playbackRate());
  EXPECT_TIME(0, GetStartTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, CurrentTimeDoesNotSetOutdated) {
  EXPECT_FALSE(animation->Outdated());
  EXPECT_TIME(0, GetCurrentTimeMs(animation));
  EXPECT_FALSE(animation->Outdated());
  // FIXME: We should split simulateFrame into a version that doesn't update
  // the animation and one that does, as most of the tests don't require
  // update() to be called.
  GetDocument().GetAnimationClock().UpdateTime(base::TimeTicks() +
                                               base::Milliseconds(10000));
  EXPECT_TIME(10000, GetCurrentTimeMs(animation));
  EXPECT_FALSE(animation->Outdated());
}

TEST_P(AnimationAnimationTestNoCompositing, SetCurrentTime) {
  EXPECT_EQ("running", animation->playState());
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(10000),
                            ASSERT_NO_EXCEPTION);
  EXPECT_EQ("running", animation->playState());
  EXPECT_TIME(10000, GetCurrentTimeMs(animation));

  SimulateFrame(10000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_TIME(20000, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, SetCurrentTimeNegative) {
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(-10000),
                            ASSERT_NO_EXCEPTION);
  EXPECT_EQ("running", animation->playState());
  EXPECT_TIME(-10000, GetCurrentTimeMs(animation));

  SimulateFrame(20000);
  EXPECT_TIME(10000, GetCurrentTimeMs(animation));
  animation->setPlaybackRate(-2);
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(-10000),
                            ASSERT_NO_EXCEPTION);
  EXPECT_EQ("finished", animation->playState());
  // A seek can set current time outside the range [0, EffectEnd()].
  EXPECT_TIME(-10000, GetCurrentTimeMs(animation));

  SimulateFrame(40000);
  // Hold current time even though outside normal range for the animation.
  EXPECT_FALSE(animation->pending());
  EXPECT_EQ("finished", animation->playState());
  EXPECT_TIME(-10000, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing,
       SetCurrentTimeNegativeWithoutSimultaneousPlaybackRateChange) {
  SimulateFrame(20000);
  EXPECT_TIME(20000, GetCurrentTimeMs(animation));
  EXPECT_EQ("running", animation->playState());

  // Reversing the direction preserves current time.
  animation->setPlaybackRate(-1);
  EXPECT_EQ("running", animation->playState());
  EXPECT_TIME(20000, GetCurrentTimeMs(animation));
  SimulateAwaitReady();

  SimulateFrame(30000);
  EXPECT_TIME(10000, GetCurrentTimeMs(animation));
  EXPECT_EQ("running", animation->playState());

  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(-10000),
                            ASSERT_NO_EXCEPTION);
  EXPECT_EQ("finished", animation->playState());
}

TEST_P(AnimationAnimationTestNoCompositing, SetCurrentTimePastContentEnd) {
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(50000),
                            ASSERT_NO_EXCEPTION);
  EXPECT_EQ("finished", animation->playState());
  EXPECT_TIME(50000, GetCurrentTimeMs(animation));

  SimulateFrame(20000);
  EXPECT_EQ("finished", animation->playState());
  EXPECT_TIME(50000, GetCurrentTimeMs(animation));
  // Reversing the play direction changes the play state from finished to
  // running.
  animation->setPlaybackRate(-2);
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(50000),
                            ASSERT_NO_EXCEPTION);
  EXPECT_EQ("running", animation->playState());
  EXPECT_TIME(50000, GetCurrentTimeMs(animation));
  SimulateAwaitReady();

  SimulateFrame(40000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_TIME(10000, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestCompositing, SetCurrentTimeMax) {
  ResetWithCompositedAnimation();
  EXPECT_EQ(CompositorAnimations::kNoFailure,
            animation->CheckCanStartAnimationOnCompositor(nullptr));
  double limit = std::numeric_limits<double>::max();
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(limit),
                            ASSERT_NO_EXCEPTION);
  V8CSSNumberish* current_time = animation->currentTime();
  ExpectRelativeErrorWithinEpsilon(limit, current_time->GetAsDouble());
  EXPECT_TRUE(animation->CheckCanStartAnimationOnCompositor(nullptr) &
              CompositorAnimations::kEffectHasUnsupportedTimingParameters);
  SimulateFrame(100000);
  current_time = animation->currentTime();
  ExpectRelativeErrorWithinEpsilon(limit, current_time->GetAsDouble());
}

TEST_P(AnimationAnimationTestCompositing, SetCurrentTimeAboveMaxTimeDelta) {
  // Similar to the SetCurrentTimeMax test. The limit is much less, but still
  // too large to be expressed as a 64-bit int and thus not able to run on the
  // compositor.
  ResetWithCompositedAnimation();
  EXPECT_EQ(CompositorAnimations::kNoFailure,
            animation->CheckCanStartAnimationOnCompositor(nullptr));
  double limit = 1e30;
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(limit),
                            ASSERT_NO_EXCEPTION);
  std::ignore = animation->currentTime();
  EXPECT_TRUE(animation->CheckCanStartAnimationOnCompositor(nullptr) &
              CompositorAnimations::kEffectHasUnsupportedTimingParameters);
}

TEST_P(AnimationAnimationTestNoCompositing, SetCurrentTimeSetsStartTime) {
  EXPECT_TIME(0, GetStartTimeMs(animation));
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(1000),
                            ASSERT_NO_EXCEPTION);
  EXPECT_TIME(-1000, GetStartTimeMs(animation));

  SimulateFrame(1000);
  EXPECT_TIME(-1000, GetStartTimeMs(animation));
  EXPECT_TIME(2000, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, SetStartTime) {
  SimulateFrame(20000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_TIME(0, GetStartTimeMs(animation));
  EXPECT_TIME(20000, GetCurrentTimeMs(animation));
  animation->setStartTime(MakeGarbageCollected<V8CSSNumberish>(10000),
                          ASSERT_NO_EXCEPTION);
  EXPECT_EQ("running", animation->playState());
  EXPECT_TIME(10000, GetStartTimeMs(animation));
  EXPECT_TIME(10000, GetCurrentTimeMs(animation));

  SimulateFrame(30000);
  EXPECT_TIME(10000, GetStartTimeMs(animation));
  EXPECT_TIME(20000, GetCurrentTimeMs(animation));
  animation->setStartTime(MakeGarbageCollected<V8CSSNumberish>(-20000),
                          ASSERT_NO_EXCEPTION);
  EXPECT_EQ("finished", animation->playState());
}

TEST_P(AnimationAnimationTestNoCompositing, SetStartTimeLimitsAnimation) {
  // Setting the start time is a seek operation, which is not constrained by the
  // normal limits on the animation.
  animation->setStartTime(MakeGarbageCollected<V8CSSNumberish>(-50000),
                          ASSERT_NO_EXCEPTION);
  EXPECT_EQ("finished", animation->playState());
  EXPECT_TRUE(animation->Limited());
  EXPECT_TIME(50000, GetCurrentTimeMs(animation));
  animation->setPlaybackRate(-1);
  EXPECT_EQ("running", animation->playState());
  animation->setStartTime(MakeGarbageCollected<V8CSSNumberish>(-100000),
                          ASSERT_NO_EXCEPTION);
  EXPECT_EQ("finished", animation->playState());
  EXPECT_TIME(-100000, GetCurrentTimeMs(animation));
  EXPECT_TRUE(animation->Limited());
}

TEST_P(AnimationAnimationTestNoCompositing, SetStartTimeOnLimitedAnimation) {
  // The setStartTime method is a seek and thus not constrained by the normal
  // limits on the animation.
  SimulateFrame(30000);
  animation->setStartTime(MakeGarbageCollected<V8CSSNumberish>(-10000),
                          ASSERT_NO_EXCEPTION);
  EXPECT_EQ("finished", animation->playState());
  EXPECT_TIME(40000, GetCurrentTimeMs(animation));
  EXPECT_TRUE(animation->Limited());

  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(50000),
                            ASSERT_NO_EXCEPTION);
  EXPECT_TIME(50000, GetCurrentTimeMs(animation));
  animation->setStartTime(MakeGarbageCollected<V8CSSNumberish>(-40000),
                          ASSERT_NO_EXCEPTION);
  EXPECT_TIME(70000, GetCurrentTimeMs(animation));
  EXPECT_EQ("finished", animation->playState());
  EXPECT_TRUE(animation->Limited());
}

TEST_P(AnimationAnimationTestNoCompositing, StartTimePauseFinish) {
  NonThrowableExceptionState exception_state;
  animation->pause();
  EXPECT_EQ("paused", animation->playState());
  EXPECT_TRUE(animation->pending());
  SimulateAwaitReady();
  EXPECT_FALSE(animation->pending());
  EXPECT_FALSE(StartTimeIsSet(animation));
  animation->finish(exception_state);
  EXPECT_EQ("finished", animation->playState());
  EXPECT_FALSE(animation->pending());
  EXPECT_TIME(-30000, GetStartTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, FinishWhenPaused) {
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

TEST_P(AnimationAnimationTestNoCompositing, StartTimeFinishPause) {
  NonThrowableExceptionState exception_state;
  animation->finish(exception_state);
  EXPECT_TIME(-30000, GetStartTimeMs(animation));
  animation->pause();
  EXPECT_EQ("paused", animation->playState());
  EXPECT_TRUE(animation->pending());
  SimulateAwaitReady();
  EXPECT_FALSE(animation->pending());
  EXPECT_FALSE(StartTimeIsSet(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, StartTimeWithZeroPlaybackRate) {
  animation->setPlaybackRate(0);
  EXPECT_EQ("running", animation->playState());
  SimulateAwaitReady();
  EXPECT_TRUE(StartTimeIsSet(animation));

  SimulateFrame(10000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_TIME(0, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, PausePlay) {
  // Pause the animation at the 10s mark.
  SimulateFrame(10000);
  animation->pause();
  EXPECT_EQ("paused", animation->playState());
  EXPECT_TRUE(animation->pending());
  EXPECT_TIME(10000, GetCurrentTimeMs(animation));

  // Resume playing the animation at the 20s mark.
  SimulateFrame(20000);
  EXPECT_EQ("paused", animation->playState());
  EXPECT_FALSE(animation->pending());
  EXPECT_TIME(10000, GetCurrentTimeMs(animation));
  animation->play();
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());
  SimulateAwaitReady();
  EXPECT_FALSE(animation->pending());

  // Advance another 10s.
  SimulateFrame(30000);
  EXPECT_TIME(20000, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, PlayRewindsToStart) {
  // Auto-replay when starting from limit.
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(30000),
                            ASSERT_NO_EXCEPTION);
  animation->play();
  EXPECT_TIME(0, GetCurrentTimeMs(animation));

  // Auto-replay when starting past the upper bound.
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(40000),
                            ASSERT_NO_EXCEPTION);
  animation->play();
  EXPECT_TIME(0, GetCurrentTimeMs(animation));
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());

  // Snap to start of the animation if playing in forward direction starting
  // from a negative value of current time.
  SimulateFrame(10000);
  EXPECT_FALSE(animation->pending());
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(-10000),
                            ASSERT_NO_EXCEPTION);
  EXPECT_EQ("running", animation->playState());
  EXPECT_FALSE(animation->pending());
  animation->play();
  EXPECT_TIME(0, GetCurrentTimeMs(animation));
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());
  SimulateAwaitReady();
  EXPECT_EQ("running", animation->playState());
  EXPECT_FALSE(animation->pending());
}

TEST_P(AnimationAnimationTestNoCompositing, PlayRewindsToEnd) {
  // Snap to end when playing a reversed animation from the start.
  animation->setPlaybackRate(-1);
  animation->play();
  EXPECT_TIME(30000, GetCurrentTimeMs(animation));

  // Snap to end if playing a reversed animation starting past the upper limit.
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(40000),
                            ASSERT_NO_EXCEPTION);
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());
  animation->play();
  EXPECT_TIME(30000, GetCurrentTimeMs(animation));
  EXPECT_TRUE(animation->pending());

  SimulateFrame(10000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_FALSE(animation->pending());

  // Snap to the end if playing a reversed animation starting with a negative
  // value for current time.
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(-10000),
                            ASSERT_NO_EXCEPTION);
  animation->play();
  EXPECT_TIME(30000, GetCurrentTimeMs(animation));
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());

  SimulateFrame(20000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_FALSE(animation->pending());
}

TEST_P(AnimationAnimationTestNoCompositing,
       PlayWithPlaybackRateZeroDoesNotSeek) {
  // When playback rate is zero, any value set for the current time effectively
  // becomes the hold time.
  animation->setPlaybackRate(0);
  animation->play();
  EXPECT_TIME(0, GetCurrentTimeMs(animation));

  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(40000),
                            ASSERT_NO_EXCEPTION);
  animation->play();
  EXPECT_TIME(40000, GetCurrentTimeMs(animation));

  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(-10000),
                            ASSERT_NO_EXCEPTION);
  animation->play();
  EXPECT_TIME(-10000, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing,
       PlayAfterPauseWithPlaybackRateZeroUpdatesPlayState) {
  animation->pause();
  animation->setPlaybackRate(0);

  SimulateFrame(1000);
  EXPECT_EQ("paused", animation->playState());
  animation->play();
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());
}

TEST_P(AnimationAnimationTestNoCompositing, Reverse) {
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(10000),
                            ASSERT_NO_EXCEPTION);
  animation->pause();
  animation->reverse();
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());
  // Effective playback rate does not kick in until the animation is ready.
  EXPECT_EQ(1, animation->playbackRate());
  EXPECT_TIME(10000, GetCurrentTimeMs(animation));
  SimulateAwaitReady();
  EXPECT_FALSE(animation->pending());
  EXPECT_EQ(-1, animation->playbackRate());
  // Updating the playback rate does not change current time.
  EXPECT_TIME(10000, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing,
       ReverseHoldsCurrentTimeWithPlaybackRateZero) {
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(10000),
                            ASSERT_NO_EXCEPTION);
  animation->setPlaybackRate(0);
  animation->pause();
  animation->reverse();
  SimulateAwaitReady();
  EXPECT_EQ("running", animation->playState());
  EXPECT_EQ(0, animation->playbackRate());
  EXPECT_TIME(10000, GetCurrentTimeMs(animation));

  SimulateFrame(20000);
  EXPECT_TIME(10000, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, ReverseSeeksToStart) {
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(-10000),
                            ASSERT_NO_EXCEPTION);
  animation->setPlaybackRate(-1);
  animation->reverse();
  EXPECT_TIME(0, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, ReverseSeeksToEnd) {
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(40000),
                            ASSERT_NO_EXCEPTION);
  animation->reverse();
  EXPECT_TIME(30000, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, ReverseBeyondLimit) {
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(40000),
                            ASSERT_NO_EXCEPTION);
  animation->setPlaybackRate(-1);
  animation->reverse();
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());
  EXPECT_TIME(0, GetCurrentTimeMs(animation));

  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(-10000),
                            ASSERT_NO_EXCEPTION);
  animation->reverse();
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());
  EXPECT_TIME(30000, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, Finish) {
  NonThrowableExceptionState exception_state;
  animation->finish(exception_state);
  // Finished snaps to the end of the animation.
  EXPECT_TIME(30000, GetCurrentTimeMs(animation));
  EXPECT_EQ("finished", animation->playState());
  // Finished is a synchronous operation.
  EXPECT_FALSE(animation->pending());

  animation->setPlaybackRate(-1);
  animation->finish(exception_state);
  EXPECT_TIME(0, GetCurrentTimeMs(animation));
  EXPECT_EQ("finished", animation->playState());
  EXPECT_FALSE(animation->pending());
}

TEST_P(AnimationAnimationTestNoCompositing, FinishAfterEffectEnd) {
  NonThrowableExceptionState exception_state;
  // OK to set current time out of bounds.
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(40000),
                            ASSERT_NO_EXCEPTION);
  animation->finish(exception_state);
  // The finish method triggers a snap to the upper boundary.
  EXPECT_TIME(30000, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, FinishBeforeStart) {
  NonThrowableExceptionState exception_state;
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(-10000),
                            ASSERT_NO_EXCEPTION);
  animation->setPlaybackRate(-1);
  animation->finish(exception_state);
  EXPECT_TIME(0, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing,
       FinishDoesNothingWithPlaybackRateZero) {
  // Cannot finish an animation that has a playback rate of zero.
  DummyExceptionStateForTesting exception_state;
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(10000),
                            ASSERT_NO_EXCEPTION);
  animation->setPlaybackRate(0);
  animation->finish(exception_state);
  EXPECT_TIME(10000, GetCurrentTimeMs(animation));
  EXPECT_TRUE(exception_state.HadException());
}

TEST_P(AnimationAnimationTestNoCompositing, FinishRaisesException) {
  // Cannot finish an animation that has an infinite iteration-count and a
  // non-zero iteration-duration.
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1);
  timing.iteration_count = std::numeric_limits<double>::infinity();
  animation->setEffect(MakeGarbageCollected<KeyframeEffect>(
      nullptr, MakeEmptyEffectModel(), timing));
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(10000),
                            ASSERT_NO_EXCEPTION);

  DummyExceptionStateForTesting exception_state;
  animation->finish(exception_state);
  EXPECT_TIME(10000, GetCurrentTimeMs(animation));
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(DOMExceptionCode::kInvalidStateError,
            exception_state.CodeAs<DOMExceptionCode>());
}

TEST_P(AnimationAnimationTestNoCompositing, LimitingAtEffectEnd) {
  SimulateFrame(30000);
  EXPECT_TIME(30000, GetCurrentTimeMs(animation));
  EXPECT_TRUE(animation->Limited());

  // Cannot run past the end of the animation without a seek.
  SimulateFrame(40000);
  EXPECT_TIME(30000, GetCurrentTimeMs(animation));
  EXPECT_FALSE(animation->Paused());
}

TEST_P(AnimationAnimationTestNoCompositing, LimitingAtStart) {
  SimulateFrame(30000);
  animation->setPlaybackRate(-2);
  SimulateAwaitReady();

  SimulateFrame(45000);
  EXPECT_TIME(0, GetCurrentTimeMs(animation));
  EXPECT_TRUE(animation->Limited());

  SimulateFrame(60000);
  EXPECT_TIME(0, GetCurrentTimeMs(animation));
  EXPECT_FALSE(animation->Paused());
}

TEST_P(AnimationAnimationTestNoCompositing, LimitingWithNoEffect) {
  animation->setEffect(nullptr);
  EXPECT_TRUE(animation->Limited());
  SimulateFrame(30000);
  EXPECT_TIME(0, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, SetPlaybackRate) {
  animation->setPlaybackRate(2);
  SimulateAwaitReady();
  EXPECT_EQ(2, animation->playbackRate());
  EXPECT_TIME(0, GetCurrentTimeMs(animation));

  SimulateFrame(10000);
  EXPECT_TIME(20000, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, SetPlaybackRateWhilePaused) {
  SimulateFrame(10000);
  animation->pause();
  EXPECT_TIME(10000, GetCurrentTimeMs(animation));
  animation->setPlaybackRate(2);
  EXPECT_TIME(10000, GetCurrentTimeMs(animation));
  SimulateAwaitReady();

  SimulateFrame(20000);
  animation->play();
  // Change to playback rate does not alter current time.
  EXPECT_TIME(10000, GetCurrentTimeMs(animation));
  SimulateAwaitReady();

  SimulateFrame(25000);
  EXPECT_TIME(20000, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, SetPlaybackRateWhileLimited) {
  // Animation plays until it hits the upper bound.
  SimulateFrame(40000);
  EXPECT_TIME(30000, GetCurrentTimeMs(animation));
  EXPECT_TRUE(animation->Limited());
  animation->setPlaybackRate(2);
  SimulateAwaitReady();

  // Already at the end of the animation.
  SimulateFrame(50000);
  EXPECT_TIME(30000, GetCurrentTimeMs(animation));
  animation->setPlaybackRate(-2);
  SimulateAwaitReady();

  SimulateFrame(60000);
  EXPECT_FALSE(animation->Limited());
  EXPECT_TIME(10000, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, SetPlaybackRateZero) {
  SimulateFrame(10000);
  animation->setPlaybackRate(0);
  EXPECT_TIME(10000, GetCurrentTimeMs(animation));

  SimulateFrame(20000);
  EXPECT_TIME(10000, GetCurrentTimeMs(animation));
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(20000),
                            ASSERT_NO_EXCEPTION);
  EXPECT_TIME(20000, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, SetPlaybackRateMax) {
  animation->setPlaybackRate(std::numeric_limits<double>::max());
  EXPECT_EQ(std::numeric_limits<double>::max(), animation->playbackRate());
  EXPECT_TIME(0, GetCurrentTimeMs(animation));
  SimulateAwaitReady();

  SimulateFrame(1);
  EXPECT_TIME(30000, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, UpdatePlaybackRate) {
  animation->updatePlaybackRate(2);
  EXPECT_EQ(1, animation->playbackRate());
  SimulateAwaitReady();
  EXPECT_EQ(2, animation->playbackRate());
  EXPECT_TIME(0, GetCurrentTimeMs(animation));

  SimulateFrame(10000);
  EXPECT_TIME(20000, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, UpdatePlaybackRateWhilePaused) {
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

TEST_P(AnimationAnimationTestNoCompositing, UpdatePlaybackRateWhileLimited) {
  NonThrowableExceptionState exception_state;
  animation->finish(exception_state);
  EXPECT_TIME(30000, GetCurrentTimeMs(animation));

  // Updating playback rate does not affect current time.
  animation->updatePlaybackRate(2);
  EXPECT_TIME(30000, GetCurrentTimeMs(animation));

  // Updating payback rate is resolved immediately for an animation in the
  // finished state.
  EXPECT_EQ(2, animation->playbackRate());
}

TEST_P(AnimationAnimationTestNoCompositing, UpdatePlaybackRateWhileRunning) {
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

TEST_P(AnimationAnimationTestNoCompositing, SetEffect) {
  animation = timeline->Play(nullptr);
  animation->setStartTime(MakeGarbageCollected<V8CSSNumberish>(0),
                          ASSERT_NO_EXCEPTION);
  AnimationEffect* effect1 = MakeAnimation();
  AnimationEffect* effect2 = MakeAnimation();
  animation->setEffect(effect1);
  EXPECT_EQ(effect1, animation->effect());
  EXPECT_TIME(0, GetCurrentTimeMs(animation));
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(15000),
                            ASSERT_NO_EXCEPTION);
  animation->setEffect(effect2);
  EXPECT_TIME(15000, GetCurrentTimeMs(animation));
  EXPECT_EQ(nullptr, effect1->GetAnimationForTesting());
  EXPECT_EQ(animation, effect2->GetAnimationForTesting());
  EXPECT_EQ(effect2, animation->effect());
}

TEST_P(AnimationAnimationTestNoCompositing, SetEffectLimitsAnimation) {
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(20000),
                            ASSERT_NO_EXCEPTION);
  animation->setEffect(MakeAnimation(10));
  EXPECT_TIME(20000, GetCurrentTimeMs(animation));
  EXPECT_TRUE(animation->Limited());
  SimulateFrame(10000);
  EXPECT_TIME(20000, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, SetEffectUnlimitsAnimation) {
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(40000),
                            ASSERT_NO_EXCEPTION);
  animation->setEffect(MakeAnimation(60));
  EXPECT_FALSE(animation->Limited());
  EXPECT_TIME(40000, GetCurrentTimeMs(animation));
  SimulateFrame(10000);
  EXPECT_TIME(50000, GetCurrentTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, EmptyAnimationsDontUpdateEffects) {
  animation = timeline->Play(nullptr);
  animation->Update(kTimingUpdateOnDemand);
  EXPECT_EQ(std::nullopt, animation->TimeToEffectChange());

  SimulateFrame(1234);
  EXPECT_EQ(std::nullopt, animation->TimeToEffectChange());
}

TEST_P(AnimationAnimationTestNoCompositing, AnimationsDisassociateFromEffect) {
  AnimationEffect* animation_node = animation->effect();
  Animation* animation2 = timeline->Play(animation_node);
  EXPECT_EQ(nullptr, animation->effect());
  animation->setEffect(animation_node);
  EXPECT_EQ(nullptr, animation2->effect());
}

#define EXPECT_TIMEDELTA(expected, observed)                          \
  EXPECT_NEAR(expected.InMillisecondsF(), observed.InMillisecondsF(), \
              Animation::kTimeToleranceMs)

TEST_P(AnimationAnimationTestNoCompositing, AnimationsReturnTimeToNextEffect) {
  Timing timing;
  timing.start_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(1));
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(1);
  timing.end_delay = Timing::Delay(ANIMATION_TIME_DELTA_FROM_SECONDS(1));
  auto* keyframe_effect = MakeGarbageCollected<KeyframeEffect>(
      nullptr, MakeEmptyEffectModel(), timing);
  animation = timeline->Play(keyframe_effect);
  animation->setStartTime(MakeGarbageCollected<V8CSSNumberish>(0),
                          ASSERT_NO_EXCEPTION);

  // Next effect change at end of start delay.
  SimulateFrame(0);
  EXPECT_TIMEDELTA(ANIMATION_TIME_DELTA_FROM_SECONDS(1),
                   animation->TimeToEffectChange().value());

  // Next effect change at end of start delay.
  SimulateFrame(500);
  EXPECT_TIMEDELTA(ANIMATION_TIME_DELTA_FROM_SECONDS(0.5),
                   animation->TimeToEffectChange().value());

  // Start of active phase.
  SimulateFrame(1000);
  EXPECT_TIMEDELTA(AnimationTimeDelta(),
                   animation->TimeToEffectChange().value());

  // Still in active phase.
  SimulateFrame(1500);
  EXPECT_TIMEDELTA(AnimationTimeDelta(),
                   animation->TimeToEffectChange().value());

  // Start of the after phase. Next effect change at end of after phase.
  SimulateFrame(2000);
  EXPECT_TIMEDELTA(ANIMATION_TIME_DELTA_FROM_SECONDS(1),
                   animation->TimeToEffectChange().value());

  // Still in effect if fillmode = forward|both.
  SimulateFrame(3000);
  EXPECT_EQ(std::nullopt, animation->TimeToEffectChange());

  // Reset to start of animation. Next effect at the end of the start delay.
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(0),
                            ASSERT_NO_EXCEPTION);
  SimulateFrame(3000);
  EXPECT_TIMEDELTA(ANIMATION_TIME_DELTA_FROM_SECONDS(1),
                   animation->TimeToEffectChange().value());

  // Start delay is scaled by playback rate.
  animation->setPlaybackRate(2);
  SimulateFrame(3000);
  EXPECT_TIMEDELTA(ANIMATION_TIME_DELTA_FROM_SECONDS(0.5),
                   animation->TimeToEffectChange().value());

  // Effectively a paused animation.
  animation->setPlaybackRate(0);
  animation->Update(kTimingUpdateOnDemand);
  EXPECT_EQ(std::nullopt, animation->TimeToEffectChange());

  // Reversed animation from end time. Next effect after end delay.
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(3000),
                            ASSERT_NO_EXCEPTION);
  animation->setPlaybackRate(-1);
  animation->Update(kTimingUpdateOnDemand);
  SimulateFrame(3000);
  EXPECT_TIMEDELTA(ANIMATION_TIME_DELTA_FROM_SECONDS(1),
                   animation->TimeToEffectChange().value());

  // End delay is scaled by playback rate.
  animation->setPlaybackRate(-2);
  animation->Update(kTimingUpdateOnDemand);
  SimulateFrame(3000);
  EXPECT_TIMEDELTA(ANIMATION_TIME_DELTA_FROM_SECONDS(0.5),
                   animation->TimeToEffectChange().value());
}

TEST_P(AnimationAnimationTestNoCompositing, TimeToNextEffectWhenPaused) {
  EXPECT_TIMEDELTA(AnimationTimeDelta(),
                   animation->TimeToEffectChange().value());
  animation->pause();
  EXPECT_TRUE(animation->pending());
  EXPECT_EQ("paused", animation->playState());
  SimulateAwaitReady();
  EXPECT_FALSE(animation->pending());
  animation->Update(kTimingUpdateOnDemand);
  EXPECT_EQ(std::nullopt, animation->TimeToEffectChange());
}

TEST_P(AnimationAnimationTestNoCompositing,
       TimeToNextEffectWhenCancelledBeforeStart) {
  EXPECT_TIMEDELTA(AnimationTimeDelta(),
                   animation->TimeToEffectChange().value());
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(-8000),
                            ASSERT_NO_EXCEPTION);
  animation->setPlaybackRate(2);
  EXPECT_EQ("running", animation->playState());
  animation->cancel();
  EXPECT_EQ("idle", animation->playState());
  EXPECT_FALSE(animation->pending());
  animation->Update(kTimingUpdateOnDemand);
  // This frame will fire the finish event event though no start time has been
  // received from the compositor yet, as cancel() nukes start times.
  EXPECT_EQ(std::nullopt, animation->TimeToEffectChange());
}

TEST_P(AnimationAnimationTestNoCompositing,
       TimeToNextEffectWhenCancelledBeforeStartReverse) {
  EXPECT_TIMEDELTA(AnimationTimeDelta(),
                   animation->TimeToEffectChange().value());
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(9000),
                            ASSERT_NO_EXCEPTION);
  animation->setPlaybackRate(-3);
  EXPECT_EQ("running", animation->playState());
  animation->cancel();
  EXPECT_EQ("idle", animation->playState());
  EXPECT_FALSE(animation->pending());
  animation->Update(kTimingUpdateOnDemand);
  EXPECT_EQ(std::nullopt, animation->TimeToEffectChange());
}

TEST_P(AnimationAnimationTestNoCompositing,
       TimeToNextEffectSimpleCancelledBeforeStart) {
  EXPECT_TIMEDELTA(AnimationTimeDelta(),
                   animation->TimeToEffectChange().value());
  EXPECT_EQ("running", animation->playState());
  animation->cancel();
  EXPECT_EQ("idle", animation->playState());
  EXPECT_FALSE(animation->pending());
  animation->Update(kTimingUpdateOnDemand);
  EXPECT_EQ(std::nullopt, animation->TimeToEffectChange());
}

TEST_P(AnimationAnimationTestNoCompositing, AttachedAnimations) {
  // Prevent |element| from being collected by |CollectAllGarbageForTesting|.
  Persistent<Element> element =
      GetDocument().CreateElementForBinding(AtomicString("foo"));

  Timing timing;
  auto* keyframe_effect = MakeGarbageCollected<KeyframeEffect>(
      element.Get(), MakeEmptyEffectModel(), timing);
  Animation* animation = timeline->Play(keyframe_effect);
  SimulateFrame(0);
  timeline->ServiceAnimations(kTimingUpdateForAnimationFrame);
  EXPECT_EQ(
      1U, element->GetElementAnimations()->Animations().find(animation)->value);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(element->GetElementAnimations()->Animations().empty());
}

TEST_P(AnimationAnimationTestNoCompositing, HasLowerCompositeOrdering) {
  Animation* animation1 = timeline->Play(nullptr);
  Animation* animation2 = timeline->Play(nullptr);
  EXPECT_TRUE(Animation::HasLowerCompositeOrdering(
      animation1, animation2,
      Animation::CompareAnimationsOrdering::kPointerOrder));
}

TEST_P(AnimationAnimationTestNoCompositing, PlayAfterCancel) {
  animation->cancel();
  EXPECT_EQ("idle", animation->playState());
  EXPECT_FALSE(CurrentTimeIsSet(animation));
  EXPECT_FALSE(StartTimeIsSet(animation));
  animation->play();
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());
  EXPECT_TIME(0, GetCurrentTimeMs(animation));
  EXPECT_FALSE(StartTimeIsSet(animation));
  SimulateAwaitReady();
  EXPECT_FALSE(animation->pending());
  EXPECT_TIME(0, GetCurrentTimeMs(animation));
  EXPECT_TIME(0, GetStartTimeMs(animation));

  SimulateFrame(10000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_TIME(10000, GetCurrentTimeMs(animation));
  EXPECT_TIME(0, GetStartTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, PlayBackwardsAfterCancel) {
  animation->setPlaybackRate(-1);
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(15000),
                            ASSERT_NO_EXCEPTION);
  animation->cancel();
  EXPECT_EQ("idle", animation->playState());
  EXPECT_FALSE(animation->pending());
  EXPECT_FALSE(CurrentTimeIsSet(animation));
  EXPECT_FALSE(StartTimeIsSet(animation));

  // Snap to the end of the animation.
  animation->play();
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());
  EXPECT_TIME(30000, GetCurrentTimeMs(animation));
  EXPECT_FALSE(StartTimeIsSet(animation));
  SimulateAwaitReady();
  EXPECT_FALSE(animation->pending());
  EXPECT_TIME(30000, GetStartTimeMs(animation));

  SimulateFrame(10000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_TIME(20000, GetCurrentTimeMs(animation));
  EXPECT_TIME(30000, GetStartTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, ReverseAfterCancel) {
  animation->cancel();
  EXPECT_EQ("idle", animation->playState());
  EXPECT_FALSE(animation->pending());
  EXPECT_FALSE(CurrentTimeIsSet(animation));
  EXPECT_FALSE(StartTimeIsSet(animation));

  // Reverse snaps to the end of the animation.
  animation->reverse();
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->pending());
  EXPECT_TIME(30000, GetCurrentTimeMs(animation));
  EXPECT_FALSE(StartTimeIsSet(animation));
  SimulateAwaitReady();
  EXPECT_FALSE(animation->pending());
  EXPECT_TIME(30000, GetStartTimeMs(animation));

  SimulateFrame(10000);
  EXPECT_EQ("running", animation->playState());
  EXPECT_TIME(20000, GetCurrentTimeMs(animation));
  EXPECT_TIME(30000, GetStartTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, FinishAfterCancel) {
  NonThrowableExceptionState exception_state;
  animation->cancel();
  EXPECT_EQ("idle", animation->playState());
  EXPECT_FALSE(CurrentTimeIsSet(animation));
  EXPECT_FALSE(StartTimeIsSet(animation));

  animation->finish(exception_state);
  EXPECT_EQ("finished", animation->playState());
  EXPECT_TIME(30000, GetCurrentTimeMs(animation));
  EXPECT_TIME(-30000, GetStartTimeMs(animation));
}

TEST_P(AnimationAnimationTestNoCompositing, PauseAfterCancel) {
  animation->cancel();
  EXPECT_EQ("idle", animation->playState());
  EXPECT_FALSE(CurrentTimeIsSet(animation));
  EXPECT_FALSE(StartTimeIsSet(animation));
  animation->pause();
  EXPECT_EQ("paused", animation->playState());
  EXPECT_TRUE(animation->pending());
  EXPECT_TIME(0, GetCurrentTimeMs(animation));
  EXPECT_FALSE(StartTimeIsSet(animation));
  SimulateAwaitReady();
  EXPECT_FALSE(animation->pending());
  EXPECT_TIME(0, GetCurrentTimeMs(animation));
  EXPECT_FALSE(StartTimeIsSet(animation));
}

// crbug.com/1052217
TEST_P(AnimationAnimationTestNoCompositing, SetPlaybackRateAfterFinish) {
  animation->setEffect(MakeAnimation(30, Timing::FillMode::FORWARDS));
  animation->finish();
  animation->Update(kTimingUpdateOnDemand);
  EXPECT_EQ("finished", animation->playState());
  EXPECT_EQ(std::nullopt, animation->TimeToEffectChange());

  // Reversing a finished animation marks the animation as outdated. Required
  // to recompute the time to next interval.
  animation->setPlaybackRate(-1);
  EXPECT_EQ("running", animation->playState());
  EXPECT_EQ(animation->playbackRate(), -1);
  EXPECT_TRUE(animation->Outdated());
  animation->Update(kTimingUpdateOnDemand);
  EXPECT_TIMEDELTA(AnimationTimeDelta(),
                   animation->TimeToEffectChange().value());
  EXPECT_FALSE(animation->Outdated());
}

TEST_P(AnimationAnimationTestNoCompositing, UpdatePlaybackRateAfterFinish) {
  animation->setEffect(MakeAnimation(30, Timing::FillMode::FORWARDS));
  animation->finish();
  animation->Update(kTimingUpdateOnDemand);
  EXPECT_EQ("finished", animation->playState());
  EXPECT_EQ(std::nullopt, animation->TimeToEffectChange());

  // Reversing a finished animation marks the animation as outdated. Required
  // to recompute the time to next interval. The pending playback rate is
  // immediately applied when updatePlaybackRate is called on a non-running
  // animation.
  animation->updatePlaybackRate(-1);
  EXPECT_EQ("running", animation->playState());
  EXPECT_EQ(animation->playbackRate(), -1);
  EXPECT_TRUE(animation->Outdated());
  animation->Update(kTimingUpdateOnDemand);
  EXPECT_TIMEDELTA(AnimationTimeDelta(),
                   animation->TimeToEffectChange().value());
  EXPECT_FALSE(animation->Outdated());
}

TEST_P(AnimationAnimationTestCompositing,
       NoCompositeWithoutCompositedElementId) {
  SetBodyInnerHTML(
      "<div id='foo' style='position: relative; will-change: "
      "opacity;'>composited</div>"
      "<div id='bar' style='position: relative'>not composited</div>");

  LayoutObject* object_composited = GetLayoutObjectByElementId("foo");
  LayoutObject* object_not_composited = GetLayoutObjectByElementId("bar");

  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);
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
TEST_P(AnimationAnimationTestCompositing,
       SetCompositorPendingWithUnresolvedStartTimes) {
  ResetWithCompositedAnimation();

  // At this point, the animation exists on both the compositor and blink side,
  // but no start time has arrived on either side. The compositor is currently
  // synced, no update is pending.
  EXPECT_FALSE(animation->CompositorPending());

  // However, if we pause the animation then the compositor should still be
  // marked pending. This is required because otherwise the compositor will go
  // ahead and start playing the animation once it receives a start time (e.g.
  // on the next compositor frame).
  animation->pause();

  EXPECT_TRUE(animation->CompositorPending());
}

TEST_P(AnimationAnimationTestCompositing, PreCommitWithUnresolvedStartTimes) {
  ResetWithCompositedAnimation();

  // At this point, the animation exists on both the compositor and blink side,
  // but no start time has arrived on either side. The compositor is currently
  // synced, no update is pending.
  EXPECT_FALSE(animation->CompositorPending());

  // At this point, a call to PreCommit should bail out and tell us to wait for
  // next commit because there are no resolved start times.
  EXPECT_FALSE(animation->PreCommit(0, nullptr, true));
}

// Cancel is synchronous on the main thread, but asynchronously deferred on the
// compositor to reduce thread contention.
TEST_P(AnimationAnimationTestCompositing, AsynchronousCancel) {
  // Start with a composited animation.
  ResetWithCompositedAnimation();
  ASSERT_TRUE(animation->HasActiveAnimationsOnCompositor());

  animation->cancel();
  EXPECT_TRUE(animation->HasActiveAnimationsOnCompositor());
  EXPECT_TRUE(animation->CompositorPending());
  EXPECT_TRUE(animation->CompositorPendingCancel());

  GetDocument().GetPendingAnimations().Update(nullptr, false);
  EXPECT_FALSE(animation->CompositorPending());
  EXPECT_FALSE(animation->CompositorPendingCancel());
  EXPECT_FALSE(animation->HasActiveAnimationsOnCompositor());
}

namespace {
int GenerateHistogramValue(CompositorAnimations::FailureReason reason) {
  // The enum values in CompositorAnimations::FailureReasons are stored as 2^i
  // as they are a bitmask, but are recorded into the histogram as (i+1) to give
  // sequential histogram values. The exception is kNoFailure, which is stored
  // as 0 and recorded as 0.
  if (reason == CompositorAnimations::kNoFailure)
    return CompositorAnimations::kNoFailure;
  return std::countr_zero(static_cast<uint32_t>(reason)) + 1;
}
}  // namespace

TEST_P(AnimationAnimationTestCompositing, PreCommitRecordsHistograms) {
  const std::string histogram_name =
      "Blink.Animation.CompositedAnimationFailureReason";

  // Initially the animation in this test has no target, so it is invalid.
  {
    base::HistogramTester histogram;
    ASSERT_TRUE(animation->PreCommit(0, nullptr, true));
    histogram.ExpectBucketCount(
        histogram_name,
        GenerateHistogramValue(CompositorAnimations::kInvalidAnimationOrEffect),
        1);
  }

  // Restart the animation with a target and compositing state.
  {
    base::HistogramTester histogram;
    ResetWithCompositedAnimation();
    histogram.ExpectBucketCount(
        histogram_name,
        GenerateHistogramValue(CompositorAnimations::kNoFailure), 1);
  }

  // Now make the playback rate 0. This trips both the invalid animation and
  // unsupported timing parameter reasons.
  animation->setPlaybackRate(0);
  animation->NotifyReady(ANIMATION_TIME_DELTA_FROM_SECONDS(100));
  {
    base::HistogramTester histogram;
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
  StringKeyframe* start_keyframe = MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(
      CSSPropertyID::kLeft, "0", SecureContextMode::kInsecureContext, nullptr);
  StringKeyframe* end_keyframe = MakeGarbageCollected<StringKeyframe>();
  end_keyframe->SetCSSPropertyValue(CSSPropertyID::kLeft, "100px",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  To<KeyframeEffect>(animation->effect())
      ->SetKeyframes({start_keyframe, end_keyframe});
  UpdateAllLifecyclePhasesForTest();
  {
    base::HistogramTester histogram;
    ASSERT_TRUE(animation->PreCommit(0, nullptr, true));
    histogram.ExpectBucketCount(
        histogram_name,
        GenerateHistogramValue(CompositorAnimations::kUnsupportedCSSProperty),
        1);
  }
}

// crbug.com/990000.
TEST_P(AnimationAnimationTestCompositing, ReplaceCompositedAnimation) {
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

TEST_P(AnimationAnimationTestCompositing, SetKeyframesCausesCompositorPending) {
  ResetWithCompositedAnimation();

  // At this point, the animation exists on both the compositor and blink side,
  // but no start time has arrived on either side. The compositor is currently
  // synced, no update is pending.
  EXPECT_FALSE(animation->CompositorPending());

  // Now change the keyframes; this should mark the animation as compositor
  // pending as we need to sync the compositor side.
  StringKeyframe* start_keyframe = MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "0.0",
                                      SecureContextMode::kInsecureContext,
                                      nullptr);
  StringKeyframe* end_keyframe = MakeGarbageCollected<StringKeyframe>();
  end_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "1.0",
                                    SecureContextMode::kInsecureContext,
                                    nullptr);

  StringKeyframeVector keyframes;
  keyframes.push_back(start_keyframe);
  keyframes.push_back(end_keyframe);

  To<KeyframeEffect>(animation->effect())->SetKeyframes(keyframes);

  EXPECT_TRUE(animation->CompositorPending());
}

// crbug.com/1057076
// Infinite duration animations should not run on the compositor.
TEST_P(AnimationAnimationTestCompositing, InfiniteDurationAnimation) {
  ResetWithCompositedAnimation();
  EXPECT_EQ(CompositorAnimations::kNoFailure,
            animation->CheckCanStartAnimationOnCompositor(nullptr));

  OptionalEffectTiming* effect_timing = OptionalEffectTiming::Create();
  effect_timing->setDuration(
      MakeGarbageCollected<V8UnionCSSNumericValueOrStringOrUnrestrictedDouble>(
          std::numeric_limits<double>::infinity()));
  animation->effect()->updateTiming(effect_timing);
  EXPECT_EQ(CompositorAnimations::kEffectHasUnsupportedTimingParameters,
            animation->CheckCanStartAnimationOnCompositor(nullptr));
}

TEST_P(AnimationAnimationTestCompositing, ZeroPlaybackSpeed) {
  ResetWithCompositedAnimation();
  EXPECT_EQ(CompositorAnimations::kNoFailure,
            animation->CheckCanStartAnimationOnCompositor(nullptr));

  animation->updatePlaybackRate(0.0);
  EXPECT_TRUE(CompositorAnimations::kInvalidAnimationOrEffect |
              animation->CheckCanStartAnimationOnCompositor(nullptr));

  animation->updatePlaybackRate(1.0E-120);
  EXPECT_TRUE(CompositorAnimations::kInvalidAnimationOrEffect |
              animation->CheckCanStartAnimationOnCompositor(nullptr));

  animation->updatePlaybackRate(0.0001);
  EXPECT_EQ(CompositorAnimations::kNoFailure,
            animation->CheckCanStartAnimationOnCompositor(nullptr));
}

// crbug.com/1149012
// Regression test to ensure proper restart logic for composited animations on
// relative transforms after a size change. In this test, the transform depends
// on the width and height of the box and a change to either triggers a restart
// of the animation if running.
TEST_P(AnimationAnimationTestCompositing,
       RestartCompositedAnimationOnSizeChange) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="width: 100px; height: 200px; background: blue;
                            will-change: transform">
    </div>
  )HTML");

  Animation* animation = CreateAnimation(
      CSSPropertyID::kTransform, "translate(100%, 100%)", "translate(0%, 0%)");

  UpdateAllLifecyclePhasesForTest();
  animation->play();
  KeyframeEffect* keyframe_effect =
      DynamicTo<KeyframeEffect>(animation->effect());
  ASSERT_TRUE(keyframe_effect);

  EXPECT_EQ(animation->CheckCanStartAnimationOnCompositor(nullptr),
            CompositorAnimations::kNoFailure);

  GetDocument().GetPendingAnimations().Update(nullptr, true);
  EXPECT_TRUE(animation->HasActiveAnimationsOnCompositor());

  // Kick the animation out of the play-pending state.
  animation->setStartTime(MakeGarbageCollected<V8CSSNumberish>(0),
                          ASSERT_NO_EXCEPTION);

  // No size change and animation does not require a restart.
  keyframe_effect->UpdateBoxSizeAndCheckTransformAxisAlignment(
      gfx::SizeF(100, 200));
  EXPECT_TRUE(animation->HasActiveAnimationsOnCompositor());
  EXPECT_FALSE(animation->CompositorPendingCancel());

  // Restart animation on a width change.
  keyframe_effect->UpdateBoxSizeAndCheckTransformAxisAlignment(
      gfx::SizeF(200, 200));
  // Cancel is deferred to PreCommit.
  EXPECT_TRUE(animation->HasActiveAnimationsOnCompositor());
  EXPECT_TRUE(animation->CompositorPendingCancel());

  GetDocument().GetPendingAnimations().Update(nullptr, true);
  EXPECT_TRUE(animation->HasActiveAnimationsOnCompositor());
  EXPECT_FALSE(animation->CompositorPendingCancel());

  // Restart animation on a height change.
  keyframe_effect->UpdateBoxSizeAndCheckTransformAxisAlignment(
      gfx::SizeF(200, 300));
  EXPECT_TRUE(animation->CompositorPendingCancel());
  GetDocument().GetPendingAnimations().Update(nullptr, true);
  EXPECT_FALSE(animation->CompositorPendingCancel());
}

// crbug.com/1149012
// Regression test to ensure proper restart logic for composited animations on
// relative transforms after a size change. In this test, the transform only
// depends on width and a change to the height does not trigger a restart.
TEST_P(AnimationAnimationTestCompositing,
       RestartCompositedAnimationOnWidthChange) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="width: 100px; height: 200px; background: blue;
                            will-change: transform">
    </div>
  )HTML");

  animation = CreateAnimation(CSSPropertyID::kTransform, "translateX(100%)",
                              "translateX(0%)");

  UpdateAllLifecyclePhasesForTest();
  animation->play();
  KeyframeEffect* keyframe_effect =
      DynamicTo<KeyframeEffect>(animation->effect());
  ASSERT_TRUE(keyframe_effect);

  GetDocument().GetPendingAnimations().Update(nullptr, true);
  EXPECT_TRUE(animation->HasActiveAnimationsOnCompositor());
  keyframe_effect->UpdateBoxSizeAndCheckTransformAxisAlignment(
      gfx::SizeF(100, 200));
  animation->setStartTime(MakeGarbageCollected<V8CSSNumberish>(0),
                          ASSERT_NO_EXCEPTION);

  // Transform is not height dependent and a change to the height does not force
  // an animation restart.
  keyframe_effect->UpdateBoxSizeAndCheckTransformAxisAlignment(
      gfx::SizeF(100, 300));
  EXPECT_TRUE(animation->HasActiveAnimationsOnCompositor());
  EXPECT_FALSE(animation->CompositorPendingCancel());

  // Width change forces a restart.
  keyframe_effect->UpdateBoxSizeAndCheckTransformAxisAlignment(
      gfx::SizeF(200, 300));
  EXPECT_TRUE(animation->HasActiveAnimationsOnCompositor());
  EXPECT_TRUE(animation->CompositorPendingCancel());

  GetDocument().GetPendingAnimations().Update(nullptr, true);
  EXPECT_TRUE(animation->HasActiveAnimationsOnCompositor());
  EXPECT_FALSE(animation->CompositorPendingCancel());
}

// crbug.com/1149012
// Regression test to ensure proper restart logic for composited animations on
// relative transforms after a size change.  In this test, the transition only
// affects height and a change to the width does not trigger a restart.
TEST_P(AnimationAnimationTestCompositing,
       RestartCompositedAnimationOnHeightChange) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="width: 100px; height: 200px; background: blue;
                            will-change: transform">
    </div>
  )HTML");

  animation = CreateAnimation(CSSPropertyID::kTransform, "translateY(100%)",
                              "translateY(0%)");

  UpdateAllLifecyclePhasesForTest();
  animation->play();
  KeyframeEffect* keyframe_effect =
      DynamicTo<KeyframeEffect>(animation->effect());
  ASSERT_TRUE(keyframe_effect);

  GetDocument().GetPendingAnimations().Update(nullptr, true);
  EXPECT_TRUE(animation->HasActiveAnimationsOnCompositor());
  keyframe_effect->UpdateBoxSizeAndCheckTransformAxisAlignment(
      gfx::SizeF(100, 200));
  animation->setStartTime(MakeGarbageCollected<V8CSSNumberish>(0),
                          ASSERT_NO_EXCEPTION);

  // Transform is not width dependent and a change to the width does not force
  // an animation restart.
  keyframe_effect->UpdateBoxSizeAndCheckTransformAxisAlignment(
      gfx::SizeF(300, 200));
  EXPECT_TRUE(animation->HasActiveAnimationsOnCompositor());

  // Height change forces a restart.
  keyframe_effect->UpdateBoxSizeAndCheckTransformAxisAlignment(
      gfx::SizeF(300, 400));
  EXPECT_TRUE(animation->HasActiveAnimationsOnCompositor());
  EXPECT_TRUE(animation->CompositorPending());
  EXPECT_TRUE(animation->CompositorPendingCancel());

  GetDocument().GetPendingAnimations().Update(nullptr, true);
  EXPECT_TRUE(animation->HasActiveAnimationsOnCompositor());
  EXPECT_FALSE(animation->CompositorPending());
  EXPECT_FALSE(animation->CompositorPendingCancel());
}

TEST_P(AnimationAnimationTestCompositing,
       ScrollLinkedAnimationCanBeComposited) {
  ResetWithCompositedAnimation();
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller {
        will-change: transform; overflow: scroll; width: 100px; height: 100px;
      }
      #target {
        width: 100px; height: 200px; background: blue; will-change: opacity;
      }
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
  options->setSource(GetElementById("scroller"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  // Create KeyframeEffect
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);

  StringKeyframe* start_keyframe = MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "1.0",
                                      SecureContextMode::kInsecureContext,
                                      nullptr);
  StringKeyframe* end_keyframe = MakeGarbageCollected<StringKeyframe>();
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
      *element, GetDocument().GetStyleResolver().InitialStyle(), nullptr);

  UpdateAllLifecyclePhasesForTest();
  scroll_animation->play();
  scroll_animation->SetDeferredStartTimeForTesting();
  EXPECT_EQ(scroll_animation->CheckCanStartAnimationOnCompositor(nullptr),
            CompositorAnimations::kNoFailure);
}

TEST_P(AnimationAnimationTestCompositing,
       StartScrollLinkedAnimationWithStartTimeIfApplicable) {
  ResetWithCompositedAnimation();
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller {
        will-change: transform; overflow: scroll; width: 100px; height: 100px; background: blue;
      }
      #target {
        width: 100px; height: 200px; background: blue; will-change: opacity;
      }
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
  options->setSource(GetElementById("scroller"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  // Create KeyframeEffect
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);

  StringKeyframe* start_keyframe = MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "1.0",
                                      SecureContextMode::kInsecureContext,
                                      nullptr);
  StringKeyframe* end_keyframe = MakeGarbageCollected<StringKeyframe>();
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
      *element, GetDocument().GetStyleResolver().InitialStyle(), nullptr);

  UpdateAllLifecyclePhasesForTest();
  const double TEST_START_PERCENT = 10;
  scroll_animation->play();
  scroll_animation->setStartTime(
      MakeGarbageCollected<V8CSSNumberish>(
          CSSUnitValues::percent(TEST_START_PERCENT)),
      ASSERT_NO_EXCEPTION);
  EXPECT_EQ(scroll_animation->CheckCanStartAnimationOnCompositor(nullptr),
            CompositorAnimations::kNoFailure);
  UpdateAllLifecyclePhasesForTest();
  // Start the animation on compositor. The time offset of the compositor
  // keyframe should be unset if we start the animation with its start time.
  scroll_animation->PreCommit(1, nullptr, true);
  cc::KeyframeModel* keyframe_model =
      keyframe_effect->GetAnimationForTesting()
          ->GetCompositorAnimation()
          ->CcAnimation()
          ->GetKeyframeModel(cc::TargetProperty::OPACITY);

  double timeline_duration_ms =
      scroll_timeline->GetDuration()->InMillisecondsF();
  double start_time_ms =
      (keyframe_model->start_time() - base::TimeTicks()).InMillisecondsF();
  double progress_percent = (start_time_ms / timeline_duration_ms) * 100;
  EXPECT_NEAR(progress_percent, TEST_START_PERCENT, 1e-3);
  EXPECT_EQ(keyframe_model->time_offset(), base::TimeDelta());
}

// Verifies correctness of scroll linked animation current and start times in
// various animation states.
TEST_P(AnimationAnimationTestNoCompositing, ScrollLinkedAnimationCreation) {
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
  options->setSource(GetElementById("scroller"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  NonThrowableExceptionState exception_state;
  Animation* scroll_animation =
      Animation::Create(MakeAnimation(), scroll_timeline, exception_state);

  // Verify start and current times in Idle state.
  EXPECT_FALSE(StartTimeIsSet(scroll_animation));
  EXPECT_FALSE(CurrentTimeIsSet(scroll_animation));

  scroll_animation->play();

  // Verify start and current times in Pending state.
  EXPECT_FALSE(StartTimeIsSet(scroll_animation));
  EXPECT_FALSE(CurrentTimeIsSet(scroll_animation));

  UpdateAllLifecyclePhasesForTest();
  // Verify start and current times in Playing state.
  EXPECT_TIME(0, GetStartTimePercent(scroll_animation));
  EXPECT_TIME(20, GetCurrentTimePercent(scroll_animation));

  // Verify current time after scroll.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 40),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrameForScrollAnimations();
  EXPECT_TIME(40, GetCurrentTimePercent(scroll_animation));
}

// Verifies that finished composited scroll-linked animations restart on
// compositor upon reverse scrolling.
TEST_P(AnimationAnimationTestCompositing,
       FinishedScrollLinkedAnimationRestartsOnReverseScrolling) {
  ResetWithCompositedAnimation();
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { will-change: transform; overflow: scroll; width: 100px; height: 100px; }
      #target { width: 100px; height: 200px; will-change: opacity; background: green;}
      #spacer { width: 200px; height: 700px; }
    </style>
    <div id ='scroller'>
      <div id ='target'></div>
      <div id ='spacer'></div>
    </div>
  )HTML");

  auto* scroller = GetLayoutBoxByElementId("scroller");
  ASSERT_TRUE(UsesCompositedScrolling(*scroller));

  // Create ScrollTimeline
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  options->setSource(GetElementById("scroller"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  // Create KeyframeEffect
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);
  StringKeyframe* start_keyframe = MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "1.0",
                                      SecureContextMode::kInsecureContext,
                                      nullptr);
  StringKeyframe* end_keyframe = MakeGarbageCollected<StringKeyframe>();
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
      *element, GetDocument().GetStyleResolver().InitialStyle(), nullptr);
  UpdateAllLifecyclePhasesForTest();

  scroll_animation->play();
  EXPECT_EQ(scroll_animation->playState(), "running");
  UpdateAllLifecyclePhasesForTest();
  GetDocument().GetPendingAnimations().Update(nullptr, true);
  EXPECT_TRUE(scroll_animation->HasActiveAnimationsOnCompositor());

  // Advances the animation to "finished" state. The composited animation will
  // be destroyed accordingly.
  scroll_animation->setCurrentTime(
      MakeGarbageCollected<V8CSSNumberish>(CSSUnitValues::percent(100)),
      ASSERT_NO_EXCEPTION);
  EXPECT_EQ(scroll_animation->playState(), "finished");
  scroll_animation->Update(kTimingUpdateForAnimationFrame);
  GetDocument().GetPendingAnimations().Update(nullptr, true);
  EXPECT_FALSE(scroll_animation->HasActiveAnimationsOnCompositor());

  // Restarting the animation should create a new compositor animation.
  scroll_animation->setCurrentTime(
      MakeGarbageCollected<V8CSSNumberish>(CSSUnitValues::percent(50)),
      ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(scroll_animation->playState(), "running");
  scroll_animation->Update(kTimingUpdateForAnimationFrame);
  GetDocument().GetPendingAnimations().Update(nullptr, true);
  EXPECT_TRUE(scroll_animation->HasActiveAnimationsOnCompositor());
}

TEST_P(AnimationAnimationTestNoCompositing,
       RemoveCanceledAnimationFromActiveSet) {
  EXPECT_EQ("running", animation->playState());
  EXPECT_TRUE(animation->Update(kTimingUpdateForAnimationFrame));
  SimulateFrame(1000);
  EXPECT_TRUE(animation->Update(kTimingUpdateForAnimationFrame));
  animation->cancel();
  EXPECT_EQ("idle", animation->playState());
  EXPECT_FALSE(animation->Update(kTimingUpdateForAnimationFrame));
}

TEST_P(AnimationAnimationTestNoCompositing,
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
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(50000),
                            ASSERT_NO_EXCEPTION);
  EXPECT_EQ("finished", animation->playState());
  EXPECT_FALSE(animation->Update(kTimingUpdateForAnimationFrame));
}

TEST_P(AnimationAnimationTestNoCompositing,
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
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(50000),
                            ASSERT_NO_EXCEPTION);
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

TEST_P(AnimationAnimationTestNoCompositing,
       PendingActivityWithFinishedEventListener) {
  EXPECT_EQ("running", animation->playState());
  EXPECT_FALSE(animation->HasPendingActivity());

  // Attaching a listener for the finished event indicates pending activity.
  MockEventListener* event_listener = MakeGarbageCollected<MockEventListener>();
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
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(50000),
                            ASSERT_NO_EXCEPTION);
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

TEST_P(AnimationAnimationTestCompositing, InvalidExecutionContext) {
  // Test for crbug.com/1254444. Guard against setting an invalid execution
  // context.
  EXPECT_TRUE(animation->GetExecutionContext());
  GetDocument().GetExecutionContext()->NotifyContextDestroyed();
  EXPECT_FALSE(animation->GetExecutionContext());
  Animation* original_animation = animation;
  ResetWithCompositedAnimation();
  EXPECT_TRUE(animation);
  EXPECT_NE(animation, original_animation);
  EXPECT_FALSE(animation->GetExecutionContext());
  // Cancel queues an event if there is a valid execution context.
  animation->cancel();
  EXPECT_FALSE(animation->HasPendingActivity());
}

class AnimationPendingAnimationsTest : public PaintTestConfigurations,
                                       public RenderingTest {
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
    timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);
    StringKeyframe* start_keyframe = MakeGarbageCollected<StringKeyframe>();
    start_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "1.0",
                                        SecureContextMode::kInsecureContext,
                                        nullptr);
    StringKeyframe* end_keyframe = MakeGarbageCollected<StringKeyframe>();
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

INSTANTIATE_PAINT_TEST_SUITE_P(AnimationPendingAnimationsTest);

TEST_P(AnimationPendingAnimationsTest, PendingAnimationStartSynchronization) {
  RunDocumentLifecycle();
  SetBodyInnerHTML("<div id='foo'>f</div><div id='bar'>b</div>");

  Animation* animA = MakeAnimation("foo", kComposited);
  Animation* animB = MakeAnimation("bar", kNonComposited);

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

TEST_P(AnimationPendingAnimationsTest,
       PendingAnimationCancelUnblocksSynchronizedStart) {
  RunDocumentLifecycle();
  SetBodyInnerHTML("<div id='foo'>f</div><div id='bar'>b</div>");

  Animation* animA = MakeAnimation("foo", kComposited);
  Animation* animB = MakeAnimation("bar", kNonComposited);

  EXPECT_TRUE(Update());
  EXPECT_TRUE(animA->pending());
  EXPECT_TRUE(animB->pending());
  animA->cancel();

  // Animation A no longer blocks B from starting.
  EXPECT_FALSE(Update());
  EXPECT_FALSE(animB->pending());
}

TEST_P(AnimationPendingAnimationsTest,
       PendingAnimationOnlySynchronizeStartsOfNewlyPendingAnimations) {
  RunDocumentLifecycle();
  SetBodyInnerHTML(
      "<div id='foo'>f</div><div id='bar'>b</div><div id='baz'>z</div>");

  Animation* animA = MakeAnimation("foo", kComposited);
  Animation* animB = MakeAnimation("bar", kNonComposited);

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

  Animation* animC = MakeAnimation("baz", kComposited);
  Animation* animD = MakeAnimation("bar", kNonComposited);

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

TEST_P(AnimationAnimationTestCompositing,
       ScrollLinkedAnimationCompositedEvenIfSourceIsNotComposited) {
  SetPreferCompositingToLCDText(false);
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
  auto* scroller = GetLayoutBoxByElementId("scroller");
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_FALSE(UsesCompositedScrolling(*scroller));
  scrollable_area->SetScrollOffset(ScrollOffset(0, 20),
                                   mojom::blink::ScrollType::kProgrammatic);
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  options->setSource(GetElementById("scroller"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  // Create KeyframeEffect
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);

  StringKeyframe* start_keyframe = MakeGarbageCollected<StringKeyframe>();
  start_keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "1.0",
                                      SecureContextMode::kInsecureContext,
                                      nullptr);
  StringKeyframe* end_keyframe = MakeGarbageCollected<StringKeyframe>();
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
      *element, GetDocument().GetStyleResolver().InitialStyle(), nullptr);

  UpdateAllLifecyclePhasesForTest();
  scroll_animation->play();
  scroll_animation->SetDeferredStartTimeForTesting();
  EXPECT_EQ(scroll_animation->CheckCanStartAnimationOnCompositor(nullptr),
            CompositorAnimations::kNoFailure);
}

#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
// https://crbug.com/1222646
#define MAYBE_ContentVisibleDisplayLockTest \
  DISABLED_ContentVisibleDisplayLockTest
#else
#define MAYBE_ContentVisibleDisplayLockTest ContentVisibleDisplayLockTest
#endif
TEST_P(AnimationAnimationTestCompositing, MAYBE_ContentVisibleDisplayLockTest) {
  animation->cancel();
  RunDocumentLifecycle();

  SetBodyInnerHTML(R"HTML(
    <style>
      .container {
        content-visibility: auto;
      }
      @keyframes anim {
        from { opacity: 0; }
        to { opacity: 1; }
      }
      #target {
        background-color: blue;
        width: 50px;
        height: 50px;
        animation: anim 1s linear alternate infinite;
      }
    </style>
    <div id="outer" class="container">
      <div id="inner" class="container">
        <div id ="target">
        </div>
      </div>
    </div>
  )HTML");

  RunDocumentLifecycle();

  Element* outer = GetElementById("outer");
  Element* inner = GetElementById("inner");
  Element* target = GetElementById("target");

  ElementAnimations* element_animations = target->GetElementAnimations();
  EXPECT_EQ(1u, element_animations->Animations().size());

  Animation* animation = element_animations->Animations().begin()->key;
  ASSERT_TRUE(!!animation);
  EXPECT_FALSE(animation->IsInDisplayLockedSubtree());

  inner->setAttribute(html_names::kStyleAttr,
                      AtomicString("content-visibility: hidden"));
  RunDocumentLifecycle();
  EXPECT_TRUE(animation->IsInDisplayLockedSubtree());

  inner->setAttribute(html_names::kStyleAttr,
                      AtomicString("content-visibility: visible"));
  RunDocumentLifecycle();
  EXPECT_FALSE(animation->IsInDisplayLockedSubtree());

  outer->setAttribute(html_names::kStyleAttr,
                      AtomicString("content-visibility: hidden"));
  RunDocumentLifecycle();
  EXPECT_TRUE(animation->IsInDisplayLockedSubtree());

  // Ensure that the animation has not been canceled even though display locked.
  EXPECT_EQ(1u, target->GetElementAnimations()->Animations().size());
  EXPECT_EQ(animation->playState(), "running");
}

TEST_P(AnimationAnimationTestCompositing, HiddenAnimationsDoNotTick) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes anim {
        from { opacity: 0; }
        to { opacity: 1; }
      }
      #target {
        width: 10px;
        height: 10px;
        background: rebeccapurple;
        animation: anim 30s;
      }
    </style>
    <div id="visibility" style="visibility: hidden;">
      <div id="target"></div>
    </div>
  )HTML");

  Element* target = GetElementById("target");
  ElementAnimations* element_animations = target->GetElementAnimations();
  ASSERT_EQ(1u, element_animations->Animations().size());
  Animation* animation = element_animations->Animations().begin()->key;

  RunDocumentLifecycle();

  const PaintArtifactCompositor* paint_artifact_compositor =
      GetDocument().View()->GetPaintArtifactCompositor();
  ASSERT_TRUE(paint_artifact_compositor);

  // The animation should be optimized out since no visible change.
  EXPECT_EQ(
      animation->CheckCanStartAnimationOnCompositor(paint_artifact_compositor),
      CompositorAnimations::kAnimationHasNoVisibleChange);
  EXPECT_TRUE(animation->CompositorPropertyAnimationsHaveNoEffectForTesting());
  EXPECT_TRUE(animation->AnimationHasNoEffect());

  // The next effect change should be at the end because the animation does not
  // tick while hidden.
  EXPECT_TIMEDELTA(ANIMATION_TIME_DELTA_FROM_SECONDS(30),
                   animation->TimeToEffectChange().value());
}

TEST_P(AnimationAnimationTestCompositing, HiddenAnimationsTickWhenVisible) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes anim {
        from { opacity: 0; }
        to { opacity: 1; }
      }
      #target {
        width: 10px;
        height: 10px;
        background: rebeccapurple;
        animation: anim 30s;
      }
    </style>
    <div id="visibility" style="visibility: hidden;">
      <div id="target"></div>
    </div>
  )HTML");

  Element* target = GetElementById("target");
  ElementAnimations* element_animations = target->GetElementAnimations();
  ASSERT_EQ(1u, element_animations->Animations().size());
  Animation* animation = element_animations->Animations().begin()->key;

  RunDocumentLifecycle();

  const PaintArtifactCompositor* paint_artifact_compositor =
      GetDocument().View()->GetPaintArtifactCompositor();
  ASSERT_TRUE(paint_artifact_compositor);

  // The animation should be optimized out since no visible change.
  EXPECT_EQ(
      animation->CheckCanStartAnimationOnCompositor(paint_artifact_compositor),
      CompositorAnimations::kAnimationHasNoVisibleChange);
  EXPECT_TRUE(animation->CompositorPropertyAnimationsHaveNoEffectForTesting());
  EXPECT_TRUE(animation->AnimationHasNoEffect());

  // The no-effect animation doesn't count. The one animation is
  // AnimationAnimationTestCompositing::animation_.
  EXPECT_EQ(1u, animation->TimelineInternal()->AnimationsNeedingUpdateCount());

  // The next effect change should be at the end because the animation does not
  // tick while hidden.
  EXPECT_TIMEDELTA(ANIMATION_TIME_DELTA_FROM_SECONDS(30),
                   animation->TimeToEffectChange().value());

  Element* visibility = GetElementById("visibility");
  visibility->setAttribute(html_names::kStyleAttr,
                           AtomicString("visibility: visible;"));
  RunDocumentLifecycle();

  // The animation should run on the compositor after the properties are
  // created.
  EXPECT_EQ(
      animation->CheckCanStartAnimationOnCompositor(paint_artifact_compositor),
      CompositorAnimations::kNoFailure);
  EXPECT_FALSE(animation->CompositorPropertyAnimationsHaveNoEffectForTesting());
  EXPECT_FALSE(animation->AnimationHasNoEffect());
  EXPECT_EQ(2u, animation->TimelineInternal()->AnimationsNeedingUpdateCount());

  // The next effect change should be at the end because the animation is
  // running on the compositor.
  EXPECT_TIMEDELTA(ANIMATION_TIME_DELTA_FROM_SECONDS(30),
                   animation->TimeToEffectChange().value());
}

TEST_P(AnimationAnimationTestNoCompositing,
       GetEffectTimingDelayZeroUseCounter) {
  animation->setEffect(MakeAnimation(/* duration */ 1.0));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kGetEffectTimingDelayZero));
  EXPECT_TRUE(animation->effect()->getTiming());
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kGetEffectTimingDelayZero));

  animation->setEffect(MakeAnimation(/* duration */ 0.0));
  // Should remain uncounted until getTiming is called.
  EXPECT_FALSE(
      GetDocument().IsUseCounted(WebFeature::kGetEffectTimingDelayZero));
  EXPECT_TRUE(animation->effect()->getTiming());
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kGetEffectTimingDelayZero));
}

}  // namespace blink
