// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/worklet_animation.h"

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/animation_effect_or_animation_effect_sequence.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/animation/worklet_animation_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

namespace {

// Only expect precision up to 1 microsecond with an additional epsilon to
// account for float conversion error (mainly due to timeline time getting
// converted between float and base::TimeDelta).
static constexpr double time_error_ms = 0.001 + 1e-13;

#define EXPECT_TIME_NEAR(expected, value) \
  EXPECT_NEAR(expected, value, time_error_ms)

KeyframeEffectModelBase* CreateEffectModel() {
  StringKeyframeVector frames_mixed_properties;
  Persistent<StringKeyframe> keyframe = MakeGarbageCollected<StringKeyframe>();
  keyframe->SetOffset(0);
  keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "0",
                                SecureContextMode::kInsecureContext, nullptr);
  frames_mixed_properties.push_back(keyframe);
  keyframe = MakeGarbageCollected<StringKeyframe>();
  keyframe->SetOffset(1);
  keyframe->SetCSSPropertyValue(CSSPropertyID::kOpacity, "1",
                                SecureContextMode::kInsecureContext, nullptr);
  frames_mixed_properties.push_back(keyframe);
  return MakeGarbageCollected<StringKeyframeEffectModel>(
      frames_mixed_properties);
}

KeyframeEffect* CreateKeyframeEffect(Element* element) {
  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);
  return MakeGarbageCollected<KeyframeEffect>(element, CreateEffectModel(),
                                              timing);
}

WorkletAnimation* CreateWorkletAnimation(
    ScriptState* script_state,
    Element* element,
    const String& animator_name,
    ScrollTimeline* scroll_timeline = nullptr) {
  AnimationEffectOrAnimationEffectSequence effects;
  AnimationEffect* effect = CreateKeyframeEffect(element);
  effects.SetAnimationEffect(effect);
  DocumentTimelineOrScrollTimeline timeline;
  if (scroll_timeline)
    timeline.SetScrollTimeline(scroll_timeline);
  scoped_refptr<SerializedScriptValue> options;

  ScriptState::Scope scope(script_state);
  return WorkletAnimation::Create(script_state, animator_name, effects,
                                  timeline, std::move(options),
                                  ASSERT_NO_EXCEPTION);
}

base::TimeDelta ToTimeDelta(double milliseconds) {
  return base::TimeDelta::FromMillisecondsD(milliseconds);
}

}  // namespace

class WorkletAnimationTest : public RenderingTest {
 public:
  WorkletAnimationTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

  void SetUp() override {
    RenderingTest::SetUp();
    element_ = GetDocument().CreateElementForBinding("test");
    GetDocument().body()->appendChild(element_);
    // Animator has to be registered before constructing WorkletAnimation. For
    // unit test this is faked by adding the animator name to
    // WorkletAnimationController.
    animator_name_ = "WorkletAnimationTest";
    GetDocument().GetWorkletAnimationController().SynchronizeAnimatorName(
        animator_name_);
    worklet_animation_ =
        CreateWorkletAnimation(GetScriptState(), element_, animator_name_);
    GetDocument().Timeline().ResetForTesting();
    GetDocument().GetAnimationClock().ResetTimeForTesting();
  }

  void SimulateFrame(double milliseconds) {
    base::TimeTicks tick =
        GetDocument().Timeline().ZeroTime() + ToTimeDelta(milliseconds);
    GetDocument().GetAnimationClock().UpdateTime(tick);
    GetDocument().GetWorkletAnimationController().UpdateAnimationStates();
    GetDocument().GetWorkletAnimationController().UpdateAnimationTimings(
        kTimingUpdateForAnimationFrame);
  }

  ScriptState* GetScriptState() {
    return ToScriptStateForMainWorld(&GetFrame());
  }

  Persistent<Element> element_;
  Persistent<WorkletAnimation> worklet_animation_;
  String animator_name_;
};

TEST_F(WorkletAnimationTest, WorkletAnimationInElementAnimations) {
  worklet_animation_->play(ASSERT_NO_EXCEPTION);
  EXPECT_EQ(1u,
            element_->EnsureElementAnimations().GetWorkletAnimations().size());
  worklet_animation_->cancel();
  EXPECT_EQ(0u,
            element_->EnsureElementAnimations().GetWorkletAnimations().size());
}

TEST_F(WorkletAnimationTest, StyleHasCurrentAnimation) {
  scoped_refptr<ComputedStyle> style =
      GetDocument().EnsureStyleResolver().StyleForElement(element_).get();
  EXPECT_EQ(false, style->HasCurrentOpacityAnimation());
  worklet_animation_->play(ASSERT_NO_EXCEPTION);
  element_->EnsureElementAnimations().UpdateAnimationFlags(*style);
  EXPECT_EQ(true, style->HasCurrentOpacityAnimation());
}

TEST_F(WorkletAnimationTest,
       CurrentTimeFromDocumentTimelineIsOffsetByStartTime) {
  WorkletAnimationId id = worklet_animation_->GetWorkletAnimationId();

  SimulateFrame(111);
  worklet_animation_->play(ASSERT_NO_EXCEPTION);
  worklet_animation_->UpdateCompositingState();

  std::unique_ptr<AnimationWorkletDispatcherInput> state =
      std::make_unique<AnimationWorkletDispatcherInput>();
  worklet_animation_->UpdateInputState(state.get());
  // First state request sets the start time and thus current time should be 0.
  std::unique_ptr<AnimationWorkletInput> input =
      state->TakeWorkletState(id.worklet_id);
  EXPECT_TIME_NEAR(0, input->added_and_updated_animations[0].current_time);

  SimulateFrame(111 + 123.4);
  state.reset(new AnimationWorkletDispatcherInput);
  worklet_animation_->UpdateInputState(state.get());
  input = state->TakeWorkletState(id.worklet_id);
  EXPECT_TIME_NEAR(123.4, input->updated_animations[0].current_time);
}

TEST_F(WorkletAnimationTest,
       CurrentTimeFromScrollTimelineNotOffsetByStartTime) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { width: 200px; height: 200px; }
    </style>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);
  ASSERT_TRUE(scroller->HasOverflowClip());
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  scrollable_area->SetScrollOffset(ScrollOffset(0, 20), kProgrammaticScroll);
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);
  WorkletAnimation* worklet_animation = CreateWorkletAnimation(
      GetScriptState(), element_, animator_name_, scroll_timeline);

  worklet_animation->play(ASSERT_NO_EXCEPTION);
  worklet_animation->UpdateCompositingState();

  scrollable_area->SetScrollOffset(ScrollOffset(0, 40), kProgrammaticScroll);

  bool is_null;
  EXPECT_TIME_NEAR(40, worklet_animation->currentTime(is_null));

  scrollable_area->SetScrollOffset(ScrollOffset(0, 70), kProgrammaticScroll);
  EXPECT_TIME_NEAR(70, worklet_animation->currentTime(is_null));
}

// Verifies correctness of current time when playback rate is set while the
// animation is in idle state.
TEST_F(WorkletAnimationTest, DocumentTimelineSetPlaybackRate) {
  double playback_rate = 2.0;

  SimulateFrame(111.0);
  worklet_animation_->setPlaybackRate(GetScriptState(), playback_rate);
  worklet_animation_->play(ASSERT_NO_EXCEPTION);
  worklet_animation_->UpdateCompositingState();
  // Zero current time is not impacted by playback rate.
  bool is_null;
  EXPECT_TIME_NEAR(0, worklet_animation_->currentTime(is_null));
  // Play the animation until second_ticks.
  SimulateFrame(111.0 + 123.4);
  // Verify that the current time is updated playback_rate faster than the
  // timeline time.
  EXPECT_TIME_NEAR(123.4 * playback_rate,
                   worklet_animation_->currentTime(is_null));
}

// Verifies correctness of current time when playback rate is set while the
// animation is playing.
TEST_F(WorkletAnimationTest, DocumentTimelineSetPlaybackRateWhilePlaying) {
  SimulateFrame(0);
  double playback_rate = 0.5;
  // Start animation.
  SimulateFrame(111.0);
  worklet_animation_->play(ASSERT_NO_EXCEPTION);
  worklet_animation_->UpdateCompositingState();
  // Update playback rate after second tick.
  SimulateFrame(111.0 + 123.4);
  worklet_animation_->setPlaybackRate(GetScriptState(), playback_rate);
  // Verify current time after third tick.
  SimulateFrame(111.0 + 123.4 + 200.0);
  bool is_null;
  EXPECT_TIME_NEAR(123.4 + 200.0 * playback_rate,
                   worklet_animation_->currentTime(is_null));
}

TEST_F(WorkletAnimationTest, PausePlay) {
  bool is_null;
  SimulateFrame(0);
  worklet_animation_->play(ASSERT_NO_EXCEPTION);
  EXPECT_EQ(Animation::kPending, worklet_animation_->PlayState());
  SimulateFrame(0);
  EXPECT_EQ(Animation::kRunning, worklet_animation_->PlayState());
  EXPECT_TRUE(worklet_animation_->Playing());
  EXPECT_TIME_NEAR(0, worklet_animation_->currentTime(is_null));
  SimulateFrame(10);
  worklet_animation_->pause(ASSERT_NO_EXCEPTION);
  EXPECT_EQ(Animation::kPaused, worklet_animation_->PlayState());
  EXPECT_FALSE(worklet_animation_->Playing());
  EXPECT_TIME_NEAR(10, worklet_animation_->currentTime(is_null));
  SimulateFrame(20);
  EXPECT_EQ(Animation::kPaused, worklet_animation_->PlayState());
  EXPECT_TIME_NEAR(10, worklet_animation_->currentTime(is_null));
  worklet_animation_->play(ASSERT_NO_EXCEPTION);
  EXPECT_EQ(Animation::kPending, worklet_animation_->PlayState());
  SimulateFrame(20);
  EXPECT_EQ(Animation::kRunning, worklet_animation_->PlayState());
  EXPECT_TRUE(worklet_animation_->Playing());
  EXPECT_TIME_NEAR(10, worklet_animation_->currentTime(is_null));
  SimulateFrame(30);
  EXPECT_EQ(Animation::kRunning, worklet_animation_->PlayState());
  EXPECT_TIME_NEAR(20, worklet_animation_->currentTime(is_null));
}

// Verifies correctness of current time when playback rate is set while
// scroll-linked animation is in idle state.
TEST_F(WorkletAnimationTest, ScrollTimelineSetPlaybackRate) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { width: 200px; height: 200px; }
    </style>
    <div id='scroller'>
      <div id='spacer'></div>
    </div>
  )HTML");

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);
  ASSERT_TRUE(scroller->HasOverflowClip());
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  scrollable_area->SetScrollOffset(ScrollOffset(0, 20), kProgrammaticScroll);
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);
  WorkletAnimation* worklet_animation = CreateWorkletAnimation(
      GetScriptState(), element_, animator_name_, scroll_timeline);

  DummyExceptionStateForTesting exception_state;
  double playback_rate = 2.0;

  // Set playback rate while the animation is in 'idle' state.
  worklet_animation->setPlaybackRate(GetScriptState(), playback_rate);
  worklet_animation->play(exception_state);
  worklet_animation->UpdateCompositingState();

  // Initial current time increased by playback rate.
  bool is_null;
  EXPECT_TIME_NEAR(40, worklet_animation->currentTime(is_null));

  // Update scroll offset.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 40), kProgrammaticScroll);

  // Verify that the current time is updated playback_rate faster than the
  // timeline time.
  EXPECT_TIME_NEAR(40 + 20 * playback_rate,
                   worklet_animation->currentTime(is_null));
}

// Verifies correctness of current time when playback rate is set while the
// scroll-linked animation is playing.
TEST_F(WorkletAnimationTest, ScrollTimelineSetPlaybackRateWhilePlaying) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { width: 200px; height: 200px; }
    </style>
    <div id='scroller'>
      <div id='spacer'></div>
    </div>
  )HTML");

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);
  ASSERT_TRUE(scroller->HasOverflowClip());
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);
  WorkletAnimation* worklet_animation = CreateWorkletAnimation(
      GetScriptState(), element_, animator_name_, scroll_timeline);

  double playback_rate = 0.5;

  // Start the animation.
  DummyExceptionStateForTesting exception_state;
  worklet_animation->play(exception_state);
  worklet_animation->UpdateCompositingState();

  // Update scroll offset and playback rate.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 40), kProgrammaticScroll);
  worklet_animation->setPlaybackRate(GetScriptState(), playback_rate);

  // Verify the current time after another scroll offset update.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 80), kProgrammaticScroll);
  bool is_null;
  EXPECT_TIME_NEAR(40 + 40 * playback_rate,
                   worklet_animation->currentTime(is_null));
}

// Verifies correcteness of worklet animation start and current time when
// inactive timeline becomes active.
TEST_F(WorkletAnimationTest, ScrollTimelineNewlyActive) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: visible; width: 100px; height: 100px; }
      #spacer { width: 200px; height: 200px; }
    </style>
    <div id='scroller'>
      <div id='spacer'></div>
    </div>
  )HTML");

  Element* scroller_element = GetElementById("scroller");

  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(scroller_element);
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);
  ASSERT_FALSE(scroll_timeline->IsActive());

  WorkletAnimation* worklet_animation = CreateWorkletAnimation(
      GetScriptState(), element_, animator_name_, scroll_timeline);

  // Start the animation.
  DummyExceptionStateForTesting exception_state;
  worklet_animation->play(exception_state);
  worklet_animation->UpdateCompositingState();

  // Scroll timeline is inactive, thus the current and start times are
  // unresolved.
  bool is_null;
  worklet_animation->currentTime(is_null);
  EXPECT_TRUE(is_null);

  worklet_animation->startTime(is_null);
  EXPECT_TRUE(is_null);

  // Make the timeline active.
  scroller_element->setAttribute(html_names::kStyleAttr,
                                 "overflow:scroll;width:100px;height:100px;");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(scroll_timeline->IsActive());

  // As the timeline becomes newly active, start and current time must be
  // initialized to zero.
  double current_time = worklet_animation->currentTime(is_null);
  EXPECT_FALSE(is_null);
  EXPECT_TIME_NEAR(0, current_time);
  double start_time = worklet_animation->startTime(is_null);
  EXPECT_FALSE(is_null);
  EXPECT_TIME_NEAR(0, start_time);
}

// Verifies correcteness of worklet animation start and current time when
// active timeline becomes inactive and then active again.
TEST_F(WorkletAnimationTest, ScrollTimelineNewlyInactive) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { width: 200px; height: 200px; }
    </style>
    <div id='scroller'>
      <div id='spacer'></div>
    </div>
  )HTML");

  Element* scroller_element = GetElementById("scroller");

  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(scroller_element);
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(scroll_timeline->IsActive());

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);
  ASSERT_TRUE(scroller->HasOverflowClip());
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  scrollable_area->SetScrollOffset(ScrollOffset(0, 40), kProgrammaticScroll);

  WorkletAnimation* worklet_animation = CreateWorkletAnimation(
      GetScriptState(), element_, animator_name_, scroll_timeline);

  // Start the animation.
  DummyExceptionStateForTesting exception_state;
  worklet_animation->play(exception_state);
  worklet_animation->UpdateCompositingState();

  // Scroll timeline is active, thus the current and start times are resolved.
  bool is_null;
  double current_time = worklet_animation->currentTime(is_null);
  EXPECT_FALSE(is_null);
  EXPECT_TIME_NEAR(40, current_time);

  double start_time = worklet_animation->startTime(is_null);
  EXPECT_FALSE(is_null);
  EXPECT_TIME_NEAR(0, start_time);

  // Make the timeline inactive.
  scroller_element->setAttribute(html_names::kStyleAttr,
                                 "overflow:visible;width:100px;height:100px;");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_FALSE(scroll_timeline->IsActive());

  // As the timeline becomes newly inactive, start time must be unresolved and
  // current time the same as previous current time.
  start_time = worklet_animation->startTime(is_null);
  EXPECT_TRUE(is_null);
  current_time = worklet_animation->currentTime(is_null);
  EXPECT_FALSE(is_null);
  EXPECT_TIME_NEAR(40, current_time);

  // Make the timeline active again.
  scroller_element->setAttribute(html_names::kStyleAttr,
                                 "overflow:scroll;width:100px;height:100px;");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(scroll_timeline->IsActive());

  // As the timeline becomes newly active, start time must be recalculated and
  // current time same as the previous current time.
  start_time = worklet_animation->startTime(is_null);
  EXPECT_FALSE(is_null);
  EXPECT_TIME_NEAR(0, start_time);
  current_time = worklet_animation->currentTime(is_null);
  EXPECT_FALSE(is_null);
  EXPECT_TIME_NEAR(40, current_time);
}

}  //  namespace blink
