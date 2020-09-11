// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_timeline.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_timeline_options.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect_model.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

// Only expect precision up to 1 microsecond with an additional epsilon to
// account for float conversion error (mainly due to timeline time getting
// converted between float and base::TimeDelta).
static constexpr double time_error_ms = 0.001 + 1e-13;

#define EXPECT_TIME_NEAR(expected, value) \
  EXPECT_NEAR(expected, value, time_error_ms)

StringOrScrollTimelineElementBasedOffset OffsetFromString(const String& value) {
  StringOrScrollTimelineElementBasedOffset result;
  result.SetString(value);
  return result;
}

HeapVector<Member<ScrollTimelineOffset>>* CreateScrollOffsets(
    ScrollTimelineOffset* start_scroll_offset =
        MakeGarbageCollected<ScrollTimelineOffset>(
            CSSNumericLiteralValue::Create(
                10.0,
                CSSPrimitiveValue::UnitType::kPixels)),
    ScrollTimelineOffset* end_scroll_offset =
        MakeGarbageCollected<ScrollTimelineOffset>(
            CSSNumericLiteralValue::Create(
                90.0,
                CSSPrimitiveValue::UnitType::kPixels))) {
  HeapVector<Member<ScrollTimelineOffset>>* scroll_offsets =
      MakeGarbageCollected<HeapVector<Member<ScrollTimelineOffset>>>();
  scroll_offsets->push_back(start_scroll_offset);
  scroll_offsets->push_back(end_scroll_offset);
  return scroll_offsets;
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
    auto new_time = GetAnimationClock().CurrentTime() +
                    base::TimeDelta::FromMilliseconds(100);
    GetPage().Animator().ServiceScriptedAnimations(new_time);
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
  TestScrollTimeline(Document* document,
                     Element* scroll_source,
                     HeapVector<Member<ScrollTimelineOffset>>* scroll_offsets =
                         CreateScrollOffsets())
      : ScrollTimeline(document,
                       scroll_source,
                       ScrollTimeline::Vertical,
                       scroll_offsets,
                       100.0),
        next_service_scheduled_(false) {}

  void ScheduleServiceOnNextFrame() override {
    ScrollTimeline::ScheduleServiceOnNextFrame();
    next_service_scheduled_ = true;
  }
  void Trace(Visitor* visitor) const override {
    ScrollTimeline::Trace(visitor);
  }
  bool NextServiceScheduled() const { return next_service_scheduled_; }
  void ResetNextServiceScheduled() { next_service_scheduled_ = false; }

 private:
  bool next_service_scheduled_;
};

TEST_F(ScrollTimelineTest, CurrentTimeIsNullIfScrollSourceIsNotScrollable) {
  SetBodyInnerHTML(R"HTML(
    <style>#scroller { width: 100px; height: 100px; }</style>
    <div id='scroller'></div>
  )HTML");

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);

  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  EXPECT_FALSE(scroll_timeline->currentTime().has_value());
  EXPECT_FALSE(scroll_timeline->IsActive());
}

TEST_F(ScrollTimelineTest,
       CurrentTimeIsNullIfScrollOffsetIsBeyondStartAndEndScrollOffset) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { height: 1000px; }
    </style>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);
  ASSERT_TRUE(scroller->HasNonVisibleOverflow());
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  options->setStartScrollOffset(OffsetFromString("10px"));
  options->setEndScrollOffset(OffsetFromString("90px"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  bool current_time_is_null = true;
  scrollable_area->SetScrollOffset(ScrollOffset(0, 5),
                                   mojom::blink::ScrollType::kProgrammatic);
  // Simulate a new animation frame  which allows the timeline to compute new
  // current time.
  SimulateFrame();
  EXPECT_EQ(scroll_timeline->currentTime(), 0);
  EXPECT_EQ("before", scroll_timeline->phase());

  current_time_is_null = true;
  scrollable_area->SetScrollOffset(ScrollOffset(0, 10),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  EXPECT_EQ(scroll_timeline->currentTime(), 0);
  EXPECT_EQ("active", scroll_timeline->phase());

  current_time_is_null = true;
  scrollable_area->SetScrollOffset(ScrollOffset(0, 50),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  EXPECT_EQ(scroll_timeline->currentTime(), 50);
  EXPECT_EQ("active", scroll_timeline->phase());

  current_time_is_null = true;
  scrollable_area->SetScrollOffset(ScrollOffset(0, 90),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  EXPECT_EQ(scroll_timeline->currentTime(), time_range.GetAsDouble());
  EXPECT_EQ("after", scroll_timeline->phase());

  current_time_is_null = true;
  scrollable_area->SetScrollOffset(ScrollOffset(0, 100),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  EXPECT_EQ(scroll_timeline->currentTime(), time_range.GetAsDouble());
  EXPECT_EQ("after", scroll_timeline->phase());
  EXPECT_TRUE(scroll_timeline->IsActive());
}

TEST_F(ScrollTimelineTest,
       CurrentTimeIsNullIfEndScrollOffsetIsLessThanStartScrollOffset) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { height: 1000px; }
    </style>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);
  ASSERT_TRUE(scroller->HasNonVisibleOverflow());
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  options->setStartScrollOffset(OffsetFromString("80px"));
  options->setEndScrollOffset(OffsetFromString("40px"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  bool current_time_is_null = true;
  scrollable_area->SetScrollOffset(ScrollOffset(0, 20),
                                   mojom::blink::ScrollType::kProgrammatic);
  // Simulate a new animation frame  which allows the timeline to compute new
  // current time.
  SimulateFrame();
  EXPECT_EQ(0, scroll_timeline->currentTime());
  EXPECT_EQ("before", scroll_timeline->phase());

  current_time_is_null = true;
  scrollable_area->SetScrollOffset(ScrollOffset(0, 60),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  EXPECT_EQ(0, scroll_timeline->currentTime());
  EXPECT_EQ("before", scroll_timeline->phase());

  current_time_is_null = true;
  scrollable_area->SetScrollOffset(ScrollOffset(0, 100),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  EXPECT_EQ(time_range.GetAsDouble(), scroll_timeline->currentTime());
  EXPECT_EQ("after", scroll_timeline->phase());
  EXPECT_TRUE(scroll_timeline->IsActive());
}

TEST_F(ScrollTimelineTest, PhasesAreCorrectWhenUsingOffsets) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { height: 1000px; }
    </style>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);
  ASSERT_TRUE(scroller->HasNonVisibleOverflow());
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));
  options->setStartScrollOffset(OffsetFromString("10px"));
  options->setEndScrollOffset(OffsetFromString("90px"));
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  EXPECT_EQ(scroll_timeline->phase(), "before");

  scrollable_area->SetScrollOffset(ScrollOffset(0, 10),
                                   mojom::blink::ScrollType::kProgrammatic);
  // Simulate a new animation frame  which allows the timeline to compute new
  // current phase and time.
  SimulateFrame();
  EXPECT_EQ(scroll_timeline->phase(), "active");

  scrollable_area->SetScrollOffset(ScrollOffset(0, 50),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  EXPECT_EQ(scroll_timeline->phase(), "active");

  scrollable_area->SetScrollOffset(ScrollOffset(0, 90),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  EXPECT_EQ(scroll_timeline->phase(), "after");

  scrollable_area->SetScrollOffset(ScrollOffset(0, 100),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  EXPECT_EQ(scroll_timeline->phase(), "after");
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
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetDocument().scrollingElement());
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(&GetDocument(), scroll_timeline->ResolvedScrollSource());
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
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetDocument().scrollingElement());
  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(&GetDocument(), scroll_timeline->ResolvedScrollSource());

  // Now change the Document.scrollingElement(). In NoQuirksMode, the
  // documentElement is the scrolling element and not the body.
  GetDocument().SetCompatibilityMode(Document::kNoQuirksMode);
  EXPECT_NE(GetDocument().documentElement(), GetDocument().body());
  EXPECT_EQ(GetDocument().documentElement(), GetDocument().scrollingElement());

  // Changing the scrollingElement should not impact the previously resolved
  // scroll source. Note that at this point the scroll timeline's scroll source
  // is still body element which is no longer the scrolling element. So if we
  // were to re-resolve the scroll source, it would not map to Document.
  EXPECT_EQ(&GetDocument(), scroll_timeline->ResolvedScrollSource());
}

TEST_F(ScrollTimelineTest, AttachOrDetachAnimationWithNullScrollSource) {
  // Directly call the constructor to make it easier to pass a null
  // scrollSource. The alternative approach would require us to remove the
  // documentElement from the document.
  Element* scroll_source = nullptr;
  Persistent<ScrollTimeline> scroll_timeline =
      MakeGarbageCollected<ScrollTimeline>(&GetDocument(), scroll_source,
                                           ScrollTimeline::Block,
                                           CreateScrollOffsets(), 100);

  // Sanity checks.
  ASSERT_EQ(scroll_timeline->scrollSource(), nullptr);
  ASSERT_EQ(scroll_timeline->ResolvedScrollSource(), nullptr);

  NonThrowableExceptionState exception_state;
  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);
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
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);
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

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->SetScrollOffset(ScrollOffset(0, 20),
                                   mojom::blink::ScrollType::kProgrammatic);

  Element* scroller_element = GetElementById("scroller");
  TestScrollTimeline* scroll_timeline =
      MakeGarbageCollected<TestScrollTimeline>(&GetDocument(),
                                               scroller_element);

  NonThrowableExceptionState exception_state;
  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);
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
  scroll_timeline->ResetNextServiceScheduled();
  scroll_timeline->ScheduleNextService();
  EXPECT_FALSE(scroll_timeline->NextServiceScheduled());

  // Validate that frame is scheduled when scroll changes.
  scroll_timeline->ResetNextServiceScheduled();
  scrollable_area->SetScrollOffset(ScrollOffset(0, 30),
                                   mojom::blink::ScrollType::kProgrammatic);
  scroll_timeline->ScheduleNextService();
  EXPECT_TRUE(scroll_timeline->NextServiceScheduled());
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
  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->SetScrollOffset(ScrollOffset(0, 20),
                                   mojom::blink::ScrollType::kProgrammatic);
  Element* scroller_element = GetElementById("scroller");

  // Use empty offsets as 'auto'.
  TestScrollTimeline* scroll_timeline =
      MakeGarbageCollected<TestScrollTimeline>(
          &GetDocument(), scroller_element,
          CreateScrollOffsets(MakeGarbageCollected<ScrollTimelineOffset>(),
                              MakeGarbageCollected<ScrollTimelineOffset>()));
  NonThrowableExceptionState exception_state;
  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);
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
  spacer_element->setAttribute(html_names::kStyleAttr, "height:1000px;");
  scroll_timeline->ResetNextServiceScheduled();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(scroll_timeline->NextServiceScheduled());
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
  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  scrollable_area->SetScrollOffset(ScrollOffset(0, 20),
                                   mojom::blink::ScrollType::kProgrammatic);
  Element* scroller_element = GetElementById("scroller");

  // Use empty offsets as 'auto'.
  TestScrollTimeline* scroll_timeline =
      MakeGarbageCollected<TestScrollTimeline>(
          &GetDocument(), scroller_element,
          CreateScrollOffsets(MakeGarbageCollected<ScrollTimelineOffset>(),
                              MakeGarbageCollected<ScrollTimelineOffset>()));
  NonThrowableExceptionState exception_state;
  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(30);
  Animation* scroll_animation =
      Animation::Create(MakeGarbageCollected<KeyframeEffect>(
                            nullptr,
                            MakeGarbageCollected<StringKeyframeEffectModel>(
                                StringKeyframeVector()),
                            timing),
                        scroll_timeline, exception_state);
  scroll_animation->play();
  UpdateAllLifecyclePhasesForTest();

  scroller_element->setAttribute(html_names::kStyleAttr, "display:table-cell;");
  scroll_timeline->ResetNextServiceScheduled();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(scroll_timeline->NextServiceScheduled());
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

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);
  ASSERT_TRUE(scroller->HasNonVisibleOverflow());
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(GetElementById("scroller"));

  scrollable_area->SetScrollOffset(ScrollOffset(0, 5),
                                   mojom::blink::ScrollType::kProgrammatic);

  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  double time_before = scroll_timeline->currentTime().value();

  scrollable_area->SetScrollOffset(ScrollOffset(0, 10),
                                   mojom::blink::ScrollType::kProgrammatic);
  // Verify that the current time didn't change before there is a new animation
  // frame.
  EXPECT_EQ(time_before, scroll_timeline->currentTime().value());

  // Simulate a new animation frame  which allows the timeline to compute a new
  // current time.
  SimulateFrame();

  // Verify that current time did change in the new animation frame.
  EXPECT_NE(time_before, scroll_timeline->currentTime().value());
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
  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  TestScrollTimeline* scroll_timeline =
      MakeGarbageCollected<TestScrollTimeline>(&GetDocument(),
                                               scroller_element);
  NonThrowableExceptionState exception_state;
  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(0.1);
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
  scrollable_area->SetScrollOffset(ScrollOffset(0, 91),
                                   mojom::blink::ScrollType::kProgrammatic);
  // Simulate a new animation frame  which allows the timeline to compute a new
  // current time.
  SimulateFrame();
  ASSERT_EQ("finished", scroll_animation->playState());
  // Verify that the animation was not removed from animations needing update
  // list.
  EXPECT_EQ(1u, scroll_timeline->AnimationsNeedingUpdateCount());

  // Scroll back.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 80),
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
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(0.1);
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
  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  TestScrollTimeline* scroll_timeline =
      MakeGarbageCollected<TestScrollTimeline>(&GetDocument(),
                                               GetElementById("scroller"));
  NonThrowableExceptionState exception_state;
  Timing timing;
  timing.iteration_duration = AnimationTimeDelta::FromSecondsD(0.1);
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
  scrollable_area->SetScrollOffset(ScrollOffset(0, 91),
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
  scrollable_area->SetScrollOffset(ScrollOffset(0, 91),
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

TEST_F(ScrollTimelineTest, MultipleScrollOffsetsCurrentTimeCalculations) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller { overflow: scroll; width: 100px; height: 100px; }
      #spacer { height: 1000px; }
    </style>
    <div id='scroller'>
      <div id ='spacer'></div>
    </div>
  )HTML");

  LayoutBoxModelObject* scroller =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"));
  ASSERT_TRUE(scroller);
  ASSERT_TRUE(scroller->HasNonVisibleOverflow());
  PaintLayerScrollableArea* scrollable_area = scroller->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  double time_range = 100.0;
  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  options->setTimeRange(
      DoubleOrScrollTimelineAutoKeyword::FromDouble(time_range));
  options->setScrollSource(GetElementById("scroller"));
  HeapVector<StringOrScrollTimelineElementBasedOffset> scroll_offsets;
  scroll_offsets.push_back(OffsetFromString("10px"));
  scroll_offsets.push_back(OffsetFromString("20px"));
  scroll_offsets.push_back(OffsetFromString("40px"));
  scroll_offsets.push_back(OffsetFromString("90px"));
  options->setScrollOffsets(scroll_offsets);

  ScrollTimeline* scroll_timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  EXPECT_EQ(scroll_timeline->currentTime(), 0);

  scrollable_area->SetScrollOffset(ScrollOffset(0, 10),
                                   mojom::blink::ScrollType::kProgrammatic);
  // Simulate a new animation frame  which allows the timeline to compute new
  // current phase and time.
  SimulateFrame();
  EXPECT_EQ(0, scroll_timeline->currentTime().value());

  scrollable_area->SetScrollOffset(ScrollOffset(0, 12),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();

  unsigned int offset = 0;
  double w = 1.0 / 3.0;                      // offset weight
  double p = (12.0 - 10.0) / (20.0 - 10.0);  // progress within the offset
  EXPECT_TIME_NEAR((offset + p) * w * time_range,
                   scroll_timeline->currentTime().value());

  scrollable_area->SetScrollOffset(ScrollOffset(0, 20),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  offset = 1;
  p = 0;
  EXPECT_TIME_NEAR((offset + p) * w * time_range,
                   scroll_timeline->currentTime().value());

  scrollable_area->SetScrollOffset(ScrollOffset(0, 30),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  p = (30.0 - 20.0) / (40.0 - 20.0);
  EXPECT_TIME_NEAR((offset + p) * w * time_range,
                   scroll_timeline->currentTime().value());

  scrollable_area->SetScrollOffset(ScrollOffset(0, 40),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  offset = 2;
  p = 0;
  EXPECT_TIME_NEAR((offset + p) * w * time_range,
                   scroll_timeline->currentTime().value());

  scrollable_area->SetScrollOffset(ScrollOffset(0, 80),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  p = (80.0 - 40.0) / (90.0 - 40.0);
  EXPECT_TIME_NEAR((offset + p) * w * time_range,
                   scroll_timeline->currentTime().value());

  scrollable_area->SetScrollOffset(ScrollOffset(0, 90),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  EXPECT_EQ(100, scroll_timeline->currentTime().value());

  scrollable_area->SetScrollOffset(ScrollOffset(0, 100),
                                   mojom::blink::ScrollType::kProgrammatic);
  SimulateFrame();
  EXPECT_EQ(100, scroll_timeline->currentTime().value());
}

}  //  namespace blink
