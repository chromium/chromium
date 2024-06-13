// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_timeline.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_timeline_options.h"
#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/animation/animation_test_helpers.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/animation/pending_animations.h"
#include "third_party/blink/renderer/core/animation/timing_calculations.h"
#include "third_party/blink/renderer/core/animation/view_timeline.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

namespace {

static constexpr double percent_precision = 0.01;

#define EXPECT_CURRENT_TIME_AS_PERCENT_NEAR(expected, animation)          \
  EXPECT_NEAR(expected,                                                   \
              (animation->CurrentTimeInternal()->InMillisecondsF() /      \
               animation->timeline()->GetDuration()->InMillisecondsF()) * \
                  100,                                                    \
              percent_precision);

Animation* CreateTestAnimation(AnimationTimeline* timeline) {
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(0.1);
  return Animation::Create(MakeGarbageCollected<KeyframeEffect>(
                               nullptr,
                               MakeGarbageCollected<StringKeyframeEffectModel>(
                                   StringKeyframeVector()),
                               timing),
                           timeline, ASSERT_NO_EXCEPTION);
}

Animation* CreateCompositableTestAnimation(Element* target,
                                           AnimationTimeline* timeline) {
  KeyframeEffect* effect =
      animation_test_helpers::CreateSimpleKeyframeEffectForTest(
          target, CSSPropertyID::kTranslate, "50px", "100px");
  effect->Model()->SnapshotAllCompositorKeyframesIfNecessary(
      *target, target->GetDocument().GetStyleResolver().InitialStyle(),
      /* parent_style */ nullptr);
  return MakeGarbageCollected<Animation>(
      timeline->GetDocument()->GetExecutionContext(), timeline, effect);
}

}  // namespace

class ScrollTimelineTest : public RenderingTest {
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }

 public:
  void SimulateFrame() {
    // Advance time by 100 ms.
    auto new_time = GetAnimationClock().CurrentTime() + base::Milliseconds(100);
    GetPage().Animator().ServiceScriptedAnimations(new_time);
  }

  wtf_size_t TimelinesCount() const {
    return GetDocument()
        .GetDocumentAnimations()
        .GetTimelinesForTesting()
        .size();
  }

  wtf_size_t AnimationsCount() const {
    wtf_size_t count = 0;
    for (auto timeline :
         GetDocument().GetDocumentAnimations().GetTimelinesForTesting()) {
      count += timeline->GetAnimations().size();
    }
    return count;
  }
};

class TestScrollTimeline : public ScrollTimeline {
 public:
  TestScrollTimeline(Document* document, Element* source, bool snapshot = true)
      : ScrollTimeline(document,
                       ScrollTimeline::ReferenceType::kSource,
                       source,
                       ScrollAxis::kY) {
    if (snapshot) {
      UpdateSnapshot();
    }
  }

  void Trace(Visitor* visitor) const override {
    ScrollTimeline::Trace(visitor);
  }

  // UpdateSnapshot has 'protected' visibility.
  void UpdateSnapshotForTesting() { UpdateSnapshot(); }

  AnimationTimeDelta CalculateIntrinsicIterationDurationForTest(
      const std::optional<TimelineOffset>& range_start,
      const std::optional<TimelineOffset>& range_end) {
    Timing timing;
    timing.iteration_count = 1;
    TimelineRange timeline_range = GetTimelineRange();
    return CalculateIntrinsicIterationDuration(timeline_range, range_start,
                                               range_end, timing);
  }
};

class TestViewTimeline : public ViewTimeline {
 public:
  TestViewTimeline(Document* document, Element* subject, bool snapshot = true)
      : ViewTimeline(document, subject, ScrollAxis::kY, TimelineInset()) {
    if (snapshot) {
      UpdateSnapshot();
    }
  }

  void UpdateSnapshotForTesting() { UpdateSnapshot(); }
};

class TestDeferredTimeline : public DeferredTimeline {
 public:
  explicit TestDeferredTimeline(Document* document, bool snapshot = true)
      : DeferredTimeline(document) {
    if (snapshot) {
      UpdateSnapshot();
    }
  }
};

TEST_F(ScrollTimelineTest, CurrentTimeIsNullIfSourceIsNotScrollable) {
  SetBodyInnerHTML(R"HTML(
    <style>#scroller { width: 100px; height: 100px; }</style>
    <div id='scroller'></div>
  )HTML");

  auto* scroller =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);

  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  options->setSource(GetElementById("scroller"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  EXPECT_FALSE(scroll_timeline->CurrentTimeSeconds().has_value());
  EXPECT_FALSE(scroll_timeline->IsActive());
}

TEST_F(ScrollTimelineTest,
       UsingDocumentScrollingElementShouldCorrectlyResolveToDocument) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #content { width: 10000px; height: 10000px; }
    </style>
    <div id='content'></div>
  )HTML");

  EXPECT_EQ(GetDocument().documentElement(), GetDocument().scrollingElement());
  // Create the ScrollTimeline with Document.scrollingElement() as source. The
  // resolved scroll source should be the Document.
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  options->setSource(GetDocument().scrollingElement());
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(&GetDocument(), scroll_timeline->ResolvedSource());
}

TEST_F(ScrollTimelineTest,
       ChangingDocumentScrollingElementShouldNotImpactScrollTimeline) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #body { overflow: scroll; width: 100px; height: 100px; }
      #content { width: 10000px; height: 10000px; }
    </style>
    <div id='content'></div>
  )HTML");

  // In QuirksMode, the body is the scrolling element
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  EXPECT_EQ(GetDocument().body(), GetDocument().scrollingElement());

  // Create the ScrollTimeline with Document.scrollingElement() as source. The
  // resolved scroll source should be the Document.
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  options->setSource(GetDocument().scrollingElement());
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(&GetDocument(), scroll_timeline->ResolvedSource());

  // Now change the Document.scrollingElement(). In NoQuirksMode, the
  // documentElement is the scrolling element and not the body.
  GetDocument().SetCompatibilityMode(Document::kNoQuirksMode);
  EXPECT_NE(GetDocument().documentElement(), GetDocument().body());
  EXPECT_EQ(GetDocument().documentElement(), GetDocument().scrollingElement());

  // Changing the scrollingElement should not impact the previously resolved
  // scroll source. Note that at this point the scroll timeline's scroll source
  // is still body element which is no longer the scrolling element. So if we
  // were to re-resolve the scroll source, it would not map to Document.
  EXPECT_EQ(&GetDocument(), scroll_timeline->ResolvedSource());
}

TEST_F(ScrollTimelineTest, AttachOrDetachAnimationWithNullSource) {
  // Directly call the constructor to make it easier to pass a null
  // source. The alternative approach would require us to remove the
  // documentElement from the document.
  Element* scroll_source = nullptr;
  Persistent<ScrollTimeline> scroll_timeline = ScrollTimeline::Create(
      &GetDocument(), scroll_source, ScrollTimeline::ScrollAxis::kBlock);

  // Sanity checks.
  ASSERT_EQ(scroll_timeline->source(), nullptr);
  ASSERT_EQ(scroll_timeline->ResolvedSource(), nullptr);

  NonThrowableExceptionState exception_state;
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);
  Animation* animation =
      Animation::Create(MakeGarbageCollected<KeyframeEffect>(
                            nullptr,
                            MakeGarbageCollected<StringKeyframeEffectModel>(
                                StringKeyframeVector()),
                            timing),
                        scroll_timeline, exception_state);
  EXPECT_EQ(1u, scroll_timeline->GetAnimations().size());
  EXPECT_TRUE(scroll_timeline->GetAnimations().Contains(animation));

  animation = nullptr;
  scroll_timeline = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0u, AnimationsCount());
}

TEST_F(ScrollTimelineTest, AnimationIsGarbageCollectedWhenScrollerIsRemoved) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { width: 200px; height: 200px; }
    </style>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");

  TestScrollTimeline* scroll_timeline =
      MakeGarbageCollected<TestScrollTimeline>(&GetDocument(),
                                               GetElementById("scroller"));
  NonThrowableExceptionState exception_state;
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);
  Animation* animation =
      Animation::Create(MakeGarbageCollected<KeyframeEffect>(
                            nullptr,
                            MakeGarbageCollected<StringKeyframeEffectModel>(
                                StringKeyframeVector()),
                            timing),
                        scroll_timeline, exception_state);
  animation->play();
  UpdateAllLifecyclePhasesForTest();

  animation->finish();
  animation = nullptr;
  scroll_timeline = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
  // Scroller is alive, animation is not GC'ed.
  EXPECT_EQ(1u, AnimationsCount());

  GetElementById("scroller")->remove();
  UpdateAllLifecyclePhasesForTest();
  ThreadState::Current()->CollectAllGarbageForTesting();
  // Scroller is removed and unreachable, animation is GC'ed.
  EXPECT_EQ(0u, AnimationsCount());
}

TEST_F(ScrollTimelineTest, AnimationPersistsWhenFinished) {
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
  Persistent<TestScrollTimeline> scroll_timeline =
      MakeGarbageCollected<TestScrollTimeline>(&GetDocument(),
                                               GetElementById("scroller"));
  Animation* animation = CreateTestAnimation(scroll_timeline);
  animation->play();
  animation->SetDeferredStartTimeForTesting();
  SimulateFrame();

  // Scroll to finished:
  scrollable_area->SetScrollOffset(ScrollOffset(0, 100),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  EXPECT_EQ("finished", animation->playState());

  // Animation should still persist after GC.
  animation = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
  ASSERT_EQ(1u, scroll_timeline->GetAnimations().size());
  animation = *scroll_timeline->GetAnimations().begin();

  // Scroll back to 50%. The animation should update, even though it was
  // previously in a finished state.
  ScrollOffset offset(0, 50);  // 10 + (90 - 10) * 0.5 = 50
  scrollable_area->SetScrollOffset(offset,
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  EXPECT_EQ("running", animation->playState());
  EXPECT_CURRENT_TIME_AS_PERCENT_NEAR(50.0, animation);
}

TEST_F(ScrollTimelineTest, AnimationPersistsWhenSourceBecomesNonScrollable) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { width: 100px; height: 100px; }
      #spacer { width: 200px; height: 200px; }
      .scroll { overflow: scroll; }
    </style>
    <div id='scroller' class='scroll'>
      <div id ='spacer'></div>
    </div>
  )HTML");

  auto* scroller =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("scroller"));
  Persistent<TestScrollTimeline> scroll_timeline =
      MakeGarbageCollected<TestScrollTimeline>(&GetDocument(),
                                               GetElementById("scroller"));
  Animation* animation = CreateTestAnimation(scroll_timeline);
  animation->play();
  animation->SetDeferredStartTimeForTesting();
  SimulateFrame();

  // Scroll to 50%:
  ASSERT_TRUE(scroller->GetScrollableArea());
  ScrollOffset offset_50(0, 50);
  scroller->GetScrollableArea()->SetScrollOffset(
      offset_50, mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  EXPECT_CURRENT_TIME_AS_PERCENT_NEAR(50.0, animation);

  // Make #scroller non-scrollable.
  GetElementById("scroller")->classList().Remove(AtomicString("scroll"));
  UpdateAllLifecyclePhasesForTest();
  scroller = To<LayoutBoxModelObject>(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);
  EXPECT_FALSE(scroller->GetScrollableArea());

  // ScrollTimeline should now have an unresolved current time.
  SimulateFrame();
  EXPECT_FALSE(scroll_timeline->CurrentTimeSeconds().has_value());

  // Animation should still persist after GC.
  animation = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
  ASSERT_EQ(1u, scroll_timeline->GetAnimations().size());
  animation = *scroll_timeline->GetAnimations().begin();

  // Make #scroller scrollable again.
  GetElementById("scroller")->classList().Add(AtomicString("scroll"));
  UpdateAllLifecyclePhasesForTest();
  scroller = To<LayoutBoxModelObject>(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);
  ASSERT_TRUE(scroller->GetScrollableArea());

  // Scroll to 40%:
  ScrollOffset offset_40(0, 40);
  scroller->GetScrollableArea()->SetScrollOffset(
      offset_40, mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  EXPECT_CURRENT_TIME_AS_PERCENT_NEAR(40.0, animation);
}

TEST_F(ScrollTimelineTest, ScheduleFrameOnlyWhenScrollOffsetChanges) {
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

  Element* scroller_element = GetElementById("scroller");
  TestScrollTimeline* scroll_timeline =
      MakeGarbageCollected<TestScrollTimeline>(&GetDocument(),
                                               scroller_element);

  NonThrowableExceptionState exception_state;
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);
  Animation* scroll_animation =
      Animation::Create(MakeGarbageCollected<KeyframeEffect>(
                            nullptr,
                            MakeGarbageCollected<StringKeyframeEffectModel>(
                                StringKeyframeVector()),
                            timing),
                        scroll_timeline, exception_state);
  scroll_animation->play();
  UpdateAllLifecyclePhasesForTest();

  // Validate that no frame is scheduled when there is no scroll change.
  GetChromeClient().UnsetAnimationScheduled();
  GetFrame().ScheduleNextServiceForScrollSnapshotClients();
  EXPECT_FALSE(GetChromeClient().AnimationScheduled());

  // Validate that frame is scheduled when scroll changes.
  GetChromeClient().UnsetAnimationScheduled();
  scrollable_area->SetScrollOffset(ScrollOffset(0, 30),
                                   mojom::blink::ScrollType::kProgrammatic);
  GetFrame().ScheduleNextServiceForScrollSnapshotClients();
  EXPECT_TRUE(GetChromeClient().AnimationScheduled());
}

// This test verifies scenario when scroll timeline is updated as a result of
// layout run. In this case the expectation is that at the end of paint
// lifecycle phase scroll timeline schedules a new frame that runs animations
// update.
TEST_F(ScrollTimelineTest, ScheduleFrameWhenScrollerLayoutChanges) {
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
  Element* scroller_element = GetElementById("scroller");

  // Use empty offsets as 'auto'.
  TestScrollTimeline* scroll_timeline =
      MakeGarbageCollected<TestScrollTimeline>(&GetDocument(),
                                               scroller_element);
  NonThrowableExceptionState exception_state;
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);
  Animation* scroll_animation =
      Animation::Create(MakeGarbageCollected<KeyframeEffect>(
                            nullptr,
                            MakeGarbageCollected<StringKeyframeEffectModel>(
                                StringKeyframeVector()),
                            timing),
                        scroll_timeline, exception_state);
  scroll_animation->play();
  UpdateAllLifecyclePhasesForTest();
  // Validate that frame is scheduled when scroller layout changes that causes
  // current time to change. Here we change the scroller max offset which
  // affects current time because endScrollOffset is 'auto'.
  Element* spacer_element = GetElementById("spacer");
  spacer_element->setAttribute(html_names::kStyleAttr,
                               AtomicString("height:1000px;"));
  GetChromeClient().UnsetAnimationScheduled();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetChromeClient().AnimationScheduled());

  // Also test changing the scroller height, which also affect the max offset.
  GetElementById("scroller")
      ->setAttribute(html_names::kStyleAttr, AtomicString("height: 200px"));
  GetChromeClient().UnsetAnimationScheduled();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetChromeClient().AnimationScheduled());
}

TEST_F(ScrollTimelineTest,
       TimelineInvalidationWhenScrollerDisplayPropertyChanges) {
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
  Element* scroller_element = GetElementById("scroller");

  // Use empty offsets as 'auto'.
  TestScrollTimeline* scroll_timeline =
      MakeGarbageCollected<TestScrollTimeline>(&GetDocument(),
                                               scroller_element);
  NonThrowableExceptionState exception_state;
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(30);
  Animation* scroll_animation =
      Animation::Create(MakeGarbageCollected<KeyframeEffect>(
                            nullptr,
                            MakeGarbageCollected<StringKeyframeEffectModel>(
                                StringKeyframeVector()),
                            timing),
                        scroll_timeline, exception_state);
  scroll_animation->play();
  UpdateAllLifecyclePhasesForTest();

  scroller_element->setAttribute(html_names::kStyleAttr,
                                 AtomicString("display:table-cell;"));
  GetChromeClient().UnsetAnimationScheduled();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetChromeClient().AnimationScheduled());
}

// Verify that scroll timeline current time is updated once upon construction
// and at the top of every animation frame.
TEST_F(ScrollTimelineTest, CurrentTimeUpdateAfterNewAnimationFrame) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { height: 1000px; }
    </style>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");

  auto* scroller =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);
  ASSERT_TRUE(scroller->IsScrollContainer());
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  options->setSource(GetElementById("scroller"));

  scrollable_area->SetScrollOffset(ScrollOffset(0, 5),
                                   mojom::blink::ScrollType::kProgrammatic);

  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  double time_before = scroll_timeline->CurrentTimeSeconds().value();

  scrollable_area->SetScrollOffset(ScrollOffset(0, 10),
                                   mojom::blink::ScrollType::kProgrammatic);
  // Verify that the current time didn't change before there is a new animation
  // frame.
  EXPECT_EQ(time_before, scroll_timeline->CurrentTimeSeconds().value());

  // Simulate a new animation frame  which allows the timeline to compute a new
  // current time.
  SimulateFrame();

  // Verify that current time did change in the new animation frame.
  EXPECT_NE(time_before, scroll_timeline->CurrentTimeSeconds().value());
}

TEST_F(ScrollTimelineTest, FinishedAnimationPlaysOnReversedScrolling) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { width: 200px; height: 200px; }
    </style>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");
  Element* scroller_element = GetElementById("scroller");
  auto* scroller =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  TestScrollTimeline* scroll_timeline =
      MakeGarbageCollected<TestScrollTimeline>(&GetDocument(),
                                               scroller_element);
  NonThrowableExceptionState exception_state;
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(0.1);
  Animation* scroll_animation =
      Animation::Create(MakeGarbageCollected<KeyframeEffect>(
                            nullptr,
                            MakeGarbageCollected<StringKeyframeEffectModel>(
                                StringKeyframeVector()),
                            timing),
                        scroll_timeline, exception_state);
  scroll_animation->play();
  UpdateAllLifecyclePhasesForTest();

  // Scroll to finished state.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 100),
                                   mojom::blink::ScrollType::kProgrammatic);
  // Simulate a new animation frame  which allows the timeline to compute a new
  // current time.
  SimulateFrame();
  ASSERT_EQ("finished", scroll_animation->playState());
  // Verify that the animation was not removed from animations needing update
  // list.
  EXPECT_EQ(1u, scroll_timeline->AnimationsNeedingUpdateCount());

  // Scroll back.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 50),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  // Verify that the animation as back to running.
  EXPECT_EQ("running", scroll_animation->playState());
}

TEST_F(ScrollTimelineTest, CancelledAnimationDetachedFromTimeline) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { width: 200px; height: 200px; }
    </style>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");
  TestScrollTimeline* scroll_timeline =
      MakeGarbageCollected<TestScrollTimeline>(&GetDocument(),
                                               GetElementById("scroller"));
  NonThrowableExceptionState exception_state;
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(0.1);
  Animation* scroll_animation =
      Animation::Create(MakeGarbageCollected<KeyframeEffect>(
                            nullptr,
                            MakeGarbageCollected<StringKeyframeEffectModel>(
                                StringKeyframeVector()),
                            timing),
                        scroll_timeline, exception_state);
  scroll_animation->play();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(1u, scroll_timeline->AnimationsNeedingUpdateCount());

  scroll_animation->cancel();
  // Simulate a new animation frame  which allows the timeline to compute a new
  // current time.
  SimulateFrame();
  ASSERT_EQ("idle", scroll_animation->playState());
  // Verify that the animation is removed from animations needing update
  // list.
  EXPECT_EQ(0u, scroll_timeline->AnimationsNeedingUpdateCount());
}

class AnimationEventListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event* event) override {
    event_received_ = true;
  }
  bool EventReceived() const { return event_received_; }
  void ResetEventReceived() { event_received_ = false; }

 private:
  bool event_received_ = false;
};

TEST_F(ScrollTimelineTest,
       FiringAnimationEventsByFinishedAnimationOnReversedScrolling) {
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
  TestScrollTimeline* scroll_timeline =
      MakeGarbageCollected<TestScrollTimeline>(&GetDocument(),
                                               GetElementById("scroller"));
  NonThrowableExceptionState exception_state;
  Timing timing;
  timing.iteration_duration = ANIMATION_TIME_DELTA_FROM_SECONDS(0.1);
  Animation* scroll_animation =
      Animation::Create(MakeGarbageCollected<KeyframeEffect>(
                            nullptr,
                            MakeGarbageCollected<StringKeyframeEffectModel>(
                                StringKeyframeVector()),
                            timing),
                        scroll_timeline, exception_state);
  auto* event_listener = MakeGarbageCollected<AnimationEventListener>();
  scroll_animation->addEventListener(event_type_names::kFinish, event_listener);

  scroll_animation->play();
  UpdateAllLifecyclePhasesForTest();
  // Scroll to finished state.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 100),
                                   mojom::blink::ScrollType::kProgrammatic);
  // Simulate a new animation frame  which allows the timeline to compute a new
  // current time.
  SimulateFrame();
  ASSERT_TRUE(event_listener->EventReceived());
  event_listener->ResetEventReceived();

  // Verify finished event does not re-fire.
  SimulateFrame();
  EXPECT_FALSE(event_listener->EventReceived());

  // Scroll back.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 80),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  // Verify finished event is not fired on reverse scroll from finished state.
  EXPECT_FALSE(event_listener->EventReceived());

  // Scroll forward to finished state.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 100),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  // Verify animation finished event is fired.
  EXPECT_TRUE(event_listener->EventReceived());
  event_listener->ResetEventReceived();

  scrollable_area->SetScrollOffset(ScrollOffset(0, 95),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  // Verify animation finished event is fired only once in finished state.
  EXPECT_FALSE(event_listener->EventReceived());
}

TEST_F(ScrollTimelineTest, WeakReferences) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { width: 200px; height: 200px; }
    </style>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");

  Persistent<TestScrollTimeline> scroll_timeline =
      MakeGarbageCollected<TestScrollTimeline>(&GetDocument(),
                                               GetElementById("scroller"));

  EXPECT_EQ(0u, scroll_timeline->GetAnimations().size());

  // Attaching an animation to a ScrollTimeline, and never playing it:
  Animation* animation = CreateTestAnimation(scroll_timeline);
  DCHECK(animation);
  animation = nullptr;
  EXPECT_EQ(1u, scroll_timeline->GetAnimations().size());

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0u, scroll_timeline->GetAnimations().size());

  // Playing, then canceling an animation:
  animation = CreateTestAnimation(scroll_timeline);
  EXPECT_EQ(1u, scroll_timeline->GetAnimations().size());

  animation->play();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1u, scroll_timeline->GetAnimations().size());

  animation->cancel();
  // UpdateAllLifecyclePhasesForTest does not call Animation::Update with
  // reason=kTimingUpdateForAnimationFrame, which is required in order to lose
  // all strong references to the animation. Hence the explicit call to
  // SimulateFrame().
  SimulateFrame();
  UpdateAllLifecyclePhasesForTest();
  animation = nullptr;

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(0u, scroll_timeline->GetAnimations().size());
}

TEST_F(ScrollTimelineTest, WeakViewTimelines) {
  SetBodyInnerHTML(R"HTML(
    <div id='scroller'>
      <div></div>
      <div></div>
      <div></div>
      <div></div>
      <div></div>
      <div></div>
      <div></div>
      <div></div>
      <div></div>
      <div></div>
    </div>
  )HTML");

  wtf_size_t base_count = TimelinesCount();

  StaticElementList* list =
      GetDocument().QuerySelectorAll(AtomicString("#scroller > div"));
  ASSERT_TRUE(list);
  EXPECT_EQ(10u, list->length());

  HeapVector<Member<Animation>> animations;

  for (wtf_size_t i = 0; i < list->length(); ++i) {
    Element* element = list->item(i);
    Animation* animation = CreateTestAnimation(
        MakeGarbageCollected<TestViewTimeline>(&GetDocument(), element));
    animation->play();
    animations.push_back(animation);
  }

  SimulateFrame();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(base_count + 10u, TimelinesCount());

  // With all animations canceled, there should be no reason for the timelines
  // to persist anymore.
  for (const Member<Animation>& animation : animations) {
    animation->cancel();
  }
  animations.clear();

  // SimulateFrame needed to lose all strong references the animations,
  // see ScrollTimelineTest.WeakReferences.
  SimulateFrame();
  UpdateAllLifecyclePhasesForTest();

  ThreadState::Current()->CollectAllGarbageForTesting();

  EXPECT_EQ(base_count, TimelinesCount());
}

TEST_F(ScrollTimelineTest, ScrollTimelineOffsetZoom) {
  using ScrollOffsets = cc::ScrollTimeline::ScrollOffsets;

  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller {
        overflow-y: auto;
        width: 100px;
        height: 100px;
        border: 20px solid black;
      }
      .spacer {
        height: 200px;
      }
    }
    </style>
    <div id='scroller'>
      <div class='spacer'></div>
    </div>
  )HTML");

  // zoom = 1
  {
    auto* timeline = MakeGarbageCollected<TestScrollTimeline>(
        &GetDocument(), GetElementById("scroller"));
    std::optional<ScrollOffsets> scroll_offsets =
        timeline->GetResolvedScrollOffsets();
    ASSERT_TRUE(scroll_offsets.has_value());
    EXPECT_EQ(0.0, scroll_offsets->start);
    EXPECT_EQ(100.0, scroll_offsets->end);
  }

  // zoom = 2
  GetFrame().SetLayoutZoomFactor(2.0f);
  UpdateAllLifecyclePhasesForTest();

  {
    auto* timeline = MakeGarbageCollected<TestScrollTimeline>(
        &GetDocument(), GetElementById("scroller"));
    std::optional<ScrollOffsets> scroll_offsets =
        timeline->GetResolvedScrollOffsets();
    ASSERT_TRUE(scroll_offsets.has_value());
    EXPECT_EQ(0.0, scroll_offsets->start);
    EXPECT_EQ(200.0, scroll_offsets->end);
  }
}

TEST_F(ScrollTimelineTest, ViewTimelineOffsetZoom) {
  using ScrollOffsets = cc::ScrollTimeline::ScrollOffsets;

  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller {
        overflow-y: auto;
        width: 100px;
        height: 100px;
        border: 20px solid black;
      }
      .spacer {
        height: 200px;
      }
      #subject {
        height: 100px;
      }
    }
    </style>
    <div id='scroller'>
      <div class='spacer'></div>
      <div id='subject'></div>
      <div class='spacer'></div>
    </div>
  )HTML");

  // zoom = 1
  {
    auto* timeline = MakeGarbageCollected<TestViewTimeline>(
        &GetDocument(), GetElementById("subject"));
    std::optional<ScrollOffsets> scroll_offsets =
        timeline->GetResolvedScrollOffsets();
    ASSERT_TRUE(scroll_offsets.has_value());
    EXPECT_EQ(100.0, scroll_offsets->start);
    EXPECT_EQ(300.0, scroll_offsets->end);

    ASSERT_TRUE(timeline->startOffset());
    EXPECT_EQ("100px", timeline->startOffset()->toString());
    ASSERT_TRUE(timeline->endOffset());
    EXPECT_EQ("300px", timeline->endOffset()->toString());
  }

  // zoom = 2
  GetFrame().SetLayoutZoomFactor(2.0f);
  UpdateAllLifecyclePhasesForTest();

  {
    auto* timeline = MakeGarbageCollected<TestViewTimeline>(
        &GetDocument(), GetElementById("subject"));
    std::optional<ScrollOffsets> scroll_offsets =
        timeline->GetResolvedScrollOffsets();
    ASSERT_TRUE(scroll_offsets.has_value());
    EXPECT_EQ(200.0, scroll_offsets->start);
    EXPECT_EQ(600.0, scroll_offsets->end);

    // Web-facing APIs should still report unzoomed values.
    ASSERT_TRUE(timeline->startOffset());
    EXPECT_EQ("100px", timeline->startOffset()->toString());
    ASSERT_TRUE(timeline->endOffset());
    EXPECT_EQ("300px", timeline->endOffset()->toString());
  }
}

TEST_F(ScrollTimelineTest, ScrollTimelineGetTimelineRange) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller {
        overflow-y: auto;
        width: 100px;
        height: 100px;
      }
      .spacer {
        height: 400px;
      }
    }
    </style>
    <div id='scroller'>
      <div class='spacer'></div>
    </div>
  )HTML");

  auto* timeline = MakeGarbageCollected<TestScrollTimeline>(
      &GetDocument(), GetElementById("scroller"), /* snapshot */ false);

  // GetTimelineRange before taking a snapshot.
  EXPECT_TRUE(timeline->GetTimelineRange().IsEmpty());

  timeline->UpdateSnapshotForTesting();
  EXPECT_EQ(TimelineRange(TimelineRange::ScrollOffsets(0, 300),
                          TimelineRange::ViewOffsets(0, 0)),
            timeline->GetTimelineRange());
}

TEST_F(ScrollTimelineTest, ViewTimelineGetTimelineRange) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller {
        overflow-y: auto;
        width: 100px;
        height: 100px;
        border: 20px solid black;
      }
      .spacer {
        height: 200px;
      }
      #subject {
        height: 100px;
      }
    }
    </style>
    <div id='scroller'>
      <div class='spacer'></div>
      <div id='subject'></div>
      <div class='spacer'></div>
    </div>
  )HTML");

  auto* timeline = MakeGarbageCollected<TestViewTimeline>(
      &GetDocument(), GetElementById("subject"), /* snapshot */ false);

  // GetTimelineRange before taking a snapshot.
  EXPECT_TRUE(timeline->GetTimelineRange().IsEmpty());

  timeline->UpdateSnapshotForTesting();
  EXPECT_EQ(TimelineRange(TimelineRange::ScrollOffsets(100, 300),
                          TimelineRange::ViewOffsets(100, 100)),
            timeline->GetTimelineRange());
}

TEST_F(ScrollTimelineTest, ScrollTimelineCalculateIntrinsicIterationDuration) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller {
        overflow-y: auto;
        width: 100px;
        height: 100px;
      }
      .spacer {
        height: 400px;
      }
    }
    </style>
    <div id='scroller'>
      <div class='spacer'></div>
    </div>
  )HTML");

  auto* timeline = MakeGarbageCollected<TestScrollTimeline>(
      &GetDocument(), GetElementById("scroller"));

  AnimationTimeDelta duration = timeline->GetDuration().value();

  using NamedRange = TimelineOffset::NamedRange;

  // [0, 300]
  EXPECT_TRUE(TimingCalculations::IsWithinAnimationTimeTolerance(
      duration, timeline->CalculateIntrinsicIterationDurationForTest(
                    /* range_start */ std::optional<TimelineOffset>(),
                    /* range_end */ std::optional<TimelineOffset>())));

  // [0, 300] (explicit)
  EXPECT_TRUE(TimingCalculations::IsWithinAnimationTimeTolerance(
      duration,
      timeline->CalculateIntrinsicIterationDurationForTest(
          /* range_start */ TimelineOffset(NamedRange::kNone, Length::Fixed(0)),
          /* range_end */ TimelineOffset(NamedRange::kNone,
                                         Length::Fixed(300)))));

  // [50, 200]
  EXPECT_TRUE(TimingCalculations::IsWithinAnimationTimeTolerance(
      duration / 2.0, timeline->CalculateIntrinsicIterationDurationForTest(
                          /* range_start */
                          TimelineOffset(NamedRange::kNone, Length::Fixed(50)),
                          /* range_end */ TimelineOffset(NamedRange::kNone,
                                                         Length::Fixed(200)))));

  // [50, 200] (kEntry)
  // The name part of the TimelineOffset is ignored.
  EXPECT_TRUE(TimingCalculations::IsWithinAnimationTimeTolerance(
      duration / 2.0,
      timeline->CalculateIntrinsicIterationDurationForTest(
          /* range_start */
          TimelineOffset(NamedRange::kEntry, Length::Fixed(50)),
          /* range_end */
          TimelineOffset(NamedRange::kEntry, Length::Fixed(200)))));

  // [50, 50]
  EXPECT_TRUE(TimingCalculations::IsWithinAnimationTimeTolerance(
      AnimationTimeDelta(),
      timeline->CalculateIntrinsicIterationDurationForTest(
          /* range_start */
          TimelineOffset(NamedRange::kNone, Length::Fixed(50)),
          /* range_end */ TimelineOffset(NamedRange::kNone,
                                         Length::Fixed(50)))));
}

TEST_F(ScrollTimelineTest, CompositedDeferredTimelineReattachment) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller {
        overflow: scroll;
        width: 100px;
        height: 100px;
        will-change: transform;
        background-color: white;
      }
      #target {
        width: 50px;
        height: 50px;
        will-change: transform;
        background-color: green;
      }
      #spacer { width: 200px; height: 200px; }
    </style>
    <div id='target'></div>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");

  TestScrollTimeline* scroll_timeline =
      MakeGarbageCollected<TestScrollTimeline>(&GetDocument(),
                                               GetElementById("scroller"));
  TestDeferredTimeline* deferred_timeline =
      MakeGarbageCollected<TestDeferredTimeline>(&GetDocument());

  deferred_timeline->AttachTimeline(scroll_timeline);

  Animation* animation = CreateCompositableTestAnimation(
      GetElementById("target"), deferred_timeline);

  animation->SetDeferredStartTimeForTesting();
  animation->play();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(animation->CheckCanStartAnimationOnCompositor(nullptr),
            CompositorAnimations::kNoFailure);

  EXPECT_FALSE(animation->CompositorPending());
  EXPECT_TRUE(deferred_timeline->CompositorTimeline());

  // Change timeline attachment for deferred timeline.
  deferred_timeline->DetachTimeline(scroll_timeline);
  deferred_timeline->AttachTimeline(MakeGarbageCollected<TestScrollTimeline>(
      &GetDocument(), GetElementById("scroller")));

  // Changing attachment should mark animations compositor pending,
  // and clear the compositor timeline.
  EXPECT_TRUE(animation->CompositorPending());
  EXPECT_FALSE(deferred_timeline->CompositorTimeline());
}

}  //  namespace blink
