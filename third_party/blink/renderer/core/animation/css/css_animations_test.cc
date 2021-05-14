// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_animations.h"

#include "cc/animation/animation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_value.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_delegate.h"

namespace {

const double kTolerance = 1e-5;

const double kTimeToleranceMilliseconds = 0.1;
}

namespace blink {

class CSSAnimationsTest : public RenderingTest {
 public:
  CSSAnimationsTest() {
    EnablePlatform();
    platform()->SetThreadedAnimationEnabled(true);
  }

  void SetUp() override {
    platform()->SetAutoAdvanceNowToPendingTasks(false);
    EnableCompositing();
    RenderingTest::SetUp();
    SetUpAnimationClockForTesting();
    // Advance timer to document time.
    platform()->AdvanceClockSeconds(
        GetDocument().Timeline().ZeroTime().InSecondsF());
  }

  void TearDown() override {
    platform()->SetAutoAdvanceNowToPendingTasks(true);
    platform()->RunUntilIdle();
  }

  base::TimeTicks TimelineTime() {
    return platform()->test_task_runner()->NowTicks();
  }

  void StartAnimationOnCompositor(Animation* animation) {
    static_cast<CompositorAnimationDelegate*>(animation)
        ->NotifyAnimationStarted(TimelineTime().since_origin().InSecondsF(),
                                 animation->CompositorGroup());
  }

  void AdvanceClockSeconds(double seconds) {
    platform()->AdvanceClockSeconds(seconds);
    platform()->RunUntilIdle();
    GetPage().Animator().ServiceScriptedAnimations(
        platform()->test_task_runner()->NowTicks());
  }

  double GetContrastFilterAmount(Element* element) {
    EXPECT_EQ(1u, element->GetComputedStyle()->Filter().size());
    const FilterOperation* filter =
        element->GetComputedStyle()->Filter().Operations()[0];
    EXPECT_EQ(FilterOperation::OperationType::CONTRAST, filter->GetType());
    return static_cast<const BasicComponentTransferFilterOperation*>(filter)
        ->Amount();
  }

 private:
  void SetUpAnimationClockForTesting() {
    GetPage().Animator().Clock().ResetTimeForTesting();
  }
};

// Verify that a composited animation is retargeted according to its composited
// time.
TEST_F(CSSAnimationsTest, RetargetedTransition) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #test { transition: filter linear 1s; }
      .contrast1 { filter: contrast(50%); }
      .contrast2 { filter: contrast(0%); }
    </style>
    <div id='test'></div>
  )HTML");
  Element* element = GetDocument().getElementById("test");
  element->setAttribute(html_names::kClassAttr, "contrast1");
  UpdateAllLifecyclePhasesForTest();
  ElementAnimations* animations = element->GetElementAnimations();
  EXPECT_EQ(1u, animations->Animations().size());
  Animation* animation = (*animations->Animations().begin()).key;
  // Start animation on compositor and advance .8 seconds.
  StartAnimationOnCompositor(animation);
  EXPECT_TRUE(animation->HasActiveAnimationsOnCompositor());
  AdvanceClockSeconds(0.8);

  // Starting the second transition should retarget the active transition.
  element->setAttribute(html_names::kClassAttr, "contrast2");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_NEAR(0.6, GetContrastFilterAmount(element), kTolerance);

  // As it has been retargeted, advancing halfway should go to 0.3.
  AdvanceClockSeconds(0.5);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_NEAR(0.3, GetContrastFilterAmount(element), kTolerance);
}

// Test that when an incompatible in progress compositor transition
// would be retargeted it does not incorrectly combine with a new
// transition target.
TEST_F(CSSAnimationsTest, IncompatibleRetargetedTransition) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #test { transition: filter 1s; }
      .saturate { filter: saturate(20%); }
      .contrast { filter: contrast(20%); }
    </style>
    <div id='test'></div>
  )HTML");
  Element* element = GetDocument().getElementById("test");
  element->setAttribute(html_names::kClassAttr, "saturate");
  UpdateAllLifecyclePhasesForTest();
  ElementAnimations* animations = element->GetElementAnimations();
  EXPECT_EQ(1u, animations->Animations().size());
  Animation* animation = (*animations->Animations().begin()).key;

  // Start animation on compositor and advance partially.
  StartAnimationOnCompositor(animation);
  EXPECT_TRUE(animation->HasActiveAnimationsOnCompositor());
  AdvanceClockSeconds(0.003);

  // The computed style still contains no filter until the next frame.
  EXPECT_TRUE(element->GetComputedStyle()->Filter().IsEmpty());

  // Now we start a contrast filter. Since it will try to combine with
  // the in progress saturate filter, and be incompatible, there should
  // be no transition and it should immediately apply on the next frame.
  element->setAttribute(html_names::kClassAttr, "contrast");
  EXPECT_TRUE(element->GetComputedStyle()->Filter().IsEmpty());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0.2, GetContrastFilterAmount(element));
}

// The following group of tests verify that composited CSS animations are
// well behaved when updated via the web-animations API. Verifies that changes
// are synced with the compositor.

class CSSAnimationsCompositorSyncTest : public CSSAnimationsTest {
 public:
  CSSAnimationsCompositorSyncTest() = default;

  void SetUp() override {
    CSSAnimationsTest::SetUp();
    CreateOpacityAnimation();
  }

  // Creates a composited animation for opacity, and advances to the midpoint
  // of the animation. Verifies that the state of the animation is in sync
  // between the main thread and compositor.
  void CreateOpacityAnimation() {
    SetBodyInnerHTML(R"HTML(
      <style>
        #test { transition: opacity linear 1s; }
        .fade { opacity: 0; }
      </style>
      <div id='test'></div>
    )HTML");

    element_ = GetDocument().getElementById("test");
    UpdateAllLifecyclePhasesForTest();
    ElementAnimations* animations = element_->GetElementAnimations();
    EXPECT_FALSE(animations);

    element_->setAttribute(html_names::kClassAttr, "fade");
    UpdateAllLifecyclePhasesForTest();
    SyncAnimationOnCompositor(/*needs_start_time*/ true);

    Animation* animation = GetAnimation();
    EXPECT_TRUE(animation->HasActiveAnimationsOnCompositor());
    VerifyCompositorPlaybackRate(1.0);
    VerifyCompositorTimeOffset(0.0);
    VerifyCompositorIterationTime(0);
    int compositor_group = animation->CompositorGroup();

    AdvanceClockSeconds(0.5);
    UpdateAllLifecyclePhasesForTest();
    EXPECT_NEAR(0.5, element_->GetComputedStyle()->Opacity(), kTolerance);
    EXPECT_EQ(compositor_group, animation->CompositorGroup());
    VerifyCompositorPlaybackRate(1.0);
    VerifyCompositorTimeOffset(0.0);
    VerifyCompositorIterationTime(500);
    VerifyCompositorOpacity(0.5);
  }

  Animation* GetAnimation() {
    // Note that the animations are stored as weak references and we cannot
    // persist the reference.
    ElementAnimations* element_animations = element_->GetElementAnimations();
    EXPECT_EQ(1u, element_animations->Animations().size());
    return (*element_animations->Animations().begin()).key;
  }

  void NotifyStartTime() {
    Animation* animation = GetAnimation();
    cc::KeyframeModel* keyframe_model = GetCompositorKeyframeForOpacity();
    base::TimeTicks start_time = keyframe_model->start_time();
    static_cast<CompositorAnimationDelegate*>(animation)
        ->NotifyAnimationStarted(start_time.since_origin().InSecondsF(),
                                 animation->CompositorGroup());
  }

  void SyncAnimationOnCompositor(bool needs_start_time) {
    // Verifies that the compositor animation requires a synchronization on the
    // start time.
    cc::KeyframeModel* keyframe_model = GetCompositorKeyframeForOpacity();
    EXPECT_EQ(needs_start_time, !keyframe_model->has_set_start_time());
    EXPECT_TRUE(keyframe_model->needs_synchronized_start_time());

    // Set the opacity keyframe model into a running state and sync with
    // blink::Animation.
    base::TimeTicks timeline_time = TimelineTime();
    keyframe_model->SetRunState(cc::KeyframeModel::RUNNING, TimelineTime());
    if (needs_start_time)
      keyframe_model->set_start_time(timeline_time);
    keyframe_model->set_needs_synchronized_start_time(false);
    NotifyStartTime();
  }

  cc::KeyframeModel* GetCompositorKeyframeForOpacity() {
    cc::Animation* cc_animation =
        GetAnimation()->GetCompositorAnimation()->CcAnimation();
    return cc_animation->GetKeyframeModel(cc::TargetProperty::OPACITY);
  }

  void VerifyCompositorPlaybackRate(double expected_value) {
    cc::KeyframeModel* keyframe_model = GetCompositorKeyframeForOpacity();
    EXPECT_NEAR(expected_value, keyframe_model->playback_rate(), kTolerance);
  }

  void VerifyCompositorTimeOffset(double expected_value) {
    cc::KeyframeModel* keyframe_model = GetCompositorKeyframeForOpacity();
    EXPECT_NEAR(expected_value, keyframe_model->time_offset().InMillisecondsF(),
                kTimeToleranceMilliseconds);
  }

  base::TimeDelta CompositorIterationTime() {
    cc::KeyframeModel* keyframe_model = GetCompositorKeyframeForOpacity();
    return keyframe_model->TrimTimeToCurrentIteration(TimelineTime());
  }

  void VerifyCompositorIterationTime(double expected_value) {
    base::TimeDelta iteration_time = CompositorIterationTime();
    EXPECT_NEAR(expected_value, iteration_time.InMillisecondsF(),
                kTimeToleranceMilliseconds);
  }

  void VerifyCompositorOpacity(double expected_value) {
    cc::KeyframeModel* keyframe_model = GetCompositorKeyframeForOpacity();
    base::TimeDelta iteration_time = CompositorIterationTime();
    const gfx::FloatAnimationCurve* opacity_curve =
        gfx::FloatAnimationCurve::ToFloatAnimationCurve(
            keyframe_model->curve());
    EXPECT_NEAR(expected_value, opacity_curve->GetValue(iteration_time),
                kTolerance);
  }

  Persistent<Element> element_;
};

// Verifies that changes to the playback rate are synced with the compositor.
TEST_F(CSSAnimationsCompositorSyncTest, UpdatePlaybackRate) {
  Animation* animation = GetAnimation();
  int compositor_group = animation->CompositorGroup();

  animation->updatePlaybackRate(0.5, ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  // Compositor animation needs to restart and will have a new compositor group.
  int post_update_compositor_group = animation->CompositorGroup();
  EXPECT_NE(compositor_group, post_update_compositor_group);
  SyncAnimationOnCompositor(/*needs_start_time*/ true);

  // No jump in opacity after changing the playback rate.
  EXPECT_NEAR(0.5, element_->GetComputedStyle()->Opacity(), kTolerance);
  VerifyCompositorPlaybackRate(0.5);
  // The time offset tells the compositor where to seek into the animation, and
  // is calculated as follows:
  // time_offset = current_time / playback_rate = 0.5 / 0.5 = 1.0.
  VerifyCompositorTimeOffset(1000);
  VerifyCompositorIterationTime(500);
  VerifyCompositorOpacity(0.5);

  // Advances the clock, and ensures that the compositor animation is not
  // restarted and that it remains in sync.
  AdvanceClockSeconds(0.5);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_NEAR(0.25, element_->GetComputedStyle()->Opacity(), kTolerance);
  EXPECT_EQ(post_update_compositor_group, animation->CompositorGroup());
  VerifyCompositorTimeOffset(1000);
  VerifyCompositorIterationTime(750);
  VerifyCompositorOpacity(0.25);
}

// Verifies that reversing an animation is synced with the compositor.
TEST_F(CSSAnimationsCompositorSyncTest, Reverse) {
  Animation* animation = GetAnimation();
  int compositor_group = animation->CompositorGroup();

  animation->reverse(ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  // Verify update in web-animation API.
  EXPECT_NEAR(-1, animation->playbackRate(), kTolerance);

  // Verify there is no jump in opacity after changing the play direction
  EXPECT_NEAR(0.5, element_->GetComputedStyle()->Opacity(), kTolerance);

  // Compositor animation needs to restart and will have a new compositor group.
  int post_update_compositor_group = animation->CompositorGroup();
  EXPECT_NE(compositor_group, post_update_compositor_group);
  SyncAnimationOnCompositor(/*needs_start_time*/ true);

  // Verify updates to cc Keyframe model.
  VerifyCompositorPlaybackRate(-1.0);
  VerifyCompositorTimeOffset(500);
  VerifyCompositorIterationTime(500);
  VerifyCompositorOpacity(0.5);

  // Advances the clock, and ensures that the compositor animation is not
  // restarted and that it remains in sync.
  AdvanceClockSeconds(0.25);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_NEAR(0.75, element_->GetComputedStyle()->Opacity(), kTolerance);
  EXPECT_EQ(post_update_compositor_group, animation->CompositorGroup());
  VerifyCompositorIterationTime(250);
  VerifyCompositorOpacity(0.75);
}

// Verifies that setting the start time on a running animation restarts the
// compositor animation in sync with blink.
TEST_F(CSSAnimationsCompositorSyncTest, SetStartTime) {
  Animation* animation = GetAnimation();
  int compositor_group = animation->CompositorGroup();

#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  V8CSSNumberish* start_time = animation->startTime();
  V8CSSNumberish* current_time = animation->currentTime();
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  CSSNumberish start_time, current_time;
  animation->startTime(start_time);
  animation->currentTime(current_time);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

  // Partially rewind the animation via setStartTime.
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  V8CSSNumberish* new_start_time = MakeGarbageCollected<V8CSSNumberish>(
      start_time->GetAsDouble() + (current_time->GetAsDouble() / 2));
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  CSSNumberish new_start_time = CSSNumberish::FromDouble(
      start_time.GetAsDouble() + (current_time.GetAsDouble() / 2));
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)

  animation->setStartTime(new_start_time, ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  // Verify blink updates.
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  current_time = animation->currentTime();
  EXPECT_TRUE(current_time->IsDouble());
  EXPECT_NEAR(250, current_time->GetAsDouble(), kTimeToleranceMilliseconds);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  animation->currentTime(current_time);
  EXPECT_TRUE(current_time.IsDouble());
  EXPECT_NEAR(250, current_time.GetAsDouble(), kTimeToleranceMilliseconds);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  EXPECT_NEAR(0.75, element_->GetComputedStyle()->Opacity(), kTolerance);

  // Compositor animation needs to restart and will have a new compositor group.
  int post_update_compositor_group = animation->CompositorGroup();
  EXPECT_NE(compositor_group, post_update_compositor_group);
  SyncAnimationOnCompositor(/*needs_start_time*/ false);

  // Verify updates to cc Keyframe model.
  VerifyCompositorPlaybackRate(1.0);
  VerifyCompositorTimeOffset(0.0);
  VerifyCompositorIterationTime(250);
  VerifyCompositorOpacity(0.75);

  // Advances the clock, and ensures that the compositor animation is not
  // restarted and that it remains in sync.
  AdvanceClockSeconds(0.25);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_NEAR(0.5, element_->GetComputedStyle()->Opacity(), kTolerance);
  EXPECT_EQ(post_update_compositor_group, animation->CompositorGroup());
  VerifyCompositorIterationTime(500);
  VerifyCompositorOpacity(0.5);
}

// Verifies that setting the current time on a running animation restarts the
// compositor animation in sync with blink.
TEST_F(CSSAnimationsCompositorSyncTest, SetCurrentTime) {
  Animation* animation = GetAnimation();
  int compositor_group = animation->CompositorGroup();

  // Advance current time.
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  animation->setCurrentTime(MakeGarbageCollected<V8CSSNumberish>(750),
                            ASSERT_NO_EXCEPTION);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  animation->setCurrentTime(CSSNumberish::FromDouble(750), ASSERT_NO_EXCEPTION);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  UpdateAllLifecyclePhasesForTest();

  // Verify blink updates.
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  V8CSSNumberish* current_time = animation->currentTime();
  EXPECT_TRUE(current_time->IsDouble());
  EXPECT_NEAR(750, current_time->GetAsDouble(), kTimeToleranceMilliseconds);
#else   // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  CSSNumberish current_time;
  animation->currentTime(current_time);
  EXPECT_TRUE(current_time.IsDouble());
  EXPECT_NEAR(750, current_time.GetAsDouble(), kTimeToleranceMilliseconds);
#endif  // defined(USE_BLINK_V8_BINDING_NEW_IDL_UNION)
  EXPECT_NEAR(0.25, element_->GetComputedStyle()->Opacity(), kTolerance);

  // Compositor animation needs to restart and will have a new compositor group.
  int post_update_compositor_group = animation->CompositorGroup();
  EXPECT_NE(compositor_group, post_update_compositor_group);
  SyncAnimationOnCompositor(/*needs_start_time*/ false);

  // Verify updates to cc Keyframe model.
  VerifyCompositorPlaybackRate(1.0);
  VerifyCompositorTimeOffset(0.0);
  VerifyCompositorIterationTime(750);
  VerifyCompositorOpacity(0.25);

  // Advances the clock, and ensures that the compositor animation is not
  // restarted and that it remains in sync.
  AdvanceClockSeconds(0.2);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_NEAR(0.05, element_->GetComputedStyle()->Opacity(), kTolerance);
  EXPECT_EQ(post_update_compositor_group, animation->CompositorGroup());
  VerifyCompositorIterationTime(950);
  VerifyCompositorOpacity(0.05);
}

}  // namespace blink
