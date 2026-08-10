// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/timeline_trigger.h"

#include "base/strings/stringprintf.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_range_offset.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/css/css_animation.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {
namespace {

void ExpectRelativeErrorWithinEpsilon(double expected, double observed) {
  EXPECT_NEAR(1.0, observed / expected, std::numeric_limits<double>::epsilon());
}

}  // namespace

class TimelineTriggerTest : public RenderingTest {
 public:
  TimelineTriggerTest()
      : RenderingTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    EnablePlatform();
    platform()->SetThreadedAnimationEnabled(true);
  }

  TimelineTrigger::RangeBoundary* MakeRangeOffsetBoundary(
      std::optional<V8TimelineRange::Enum> range,
      std::optional<int> pct) {
    TimelineRangeOffset* offset = MakeGarbageCollected<TimelineRangeOffset>();
    if (range) {
      offset->setRangeName(V8TimelineRange(*range));
    }
    if (pct) {
      offset->setOffset(
          CSSNumericValue::FromCSSValue(*CSSNumericLiteralValue::Create(
              *pct, CSSNumericLiteralValue::UnitType::kPercentage)));
    }
    return MakeGarbageCollected<TimelineTrigger::RangeBoundary>(offset);
  }

  void AdvanceClockSeconds(double seconds) {
    PageTestBase::FastForwardBy(base::Seconds(seconds));
    GetPage().Animator().ServiceScriptedAnimations(platform()->NowTicks());
  }
};

TEST_F(TimelineTriggerTest, ComputeBoundariesTest) {
  using RangeBoundary = TimelineTrigger::RangeBoundary;
  using TriggerBoundaries = TimelineTrigger::TriggerBoundaries;
  SetBodyInnerHTML(R"HTML(
    <style>
     @keyframes anim {
        from { opacity: 0; }
        to { opacity: 1; }
      }
      #scroller {
        overflow-y: scroll; width: 100px; height: 100px;
      }
      #target {
        animation: anim 1s both;
        width: 100px; height: 50px; background: blue;
        timeline-trigger: --trigger view();
        animation-trigger: --trigger;
      }
      #spacer { width: 200px; height: 200px; }
    </style>
    <div id ='scroller'>
      <div id ='spacer'></div>
      <div id ='target'></div>
      <div id ='spacer'></div>
    </div>
  )HTML");

  Element* target = GetDocument().getElementById(AtomicString("target"));
  TimelineTrigger* trigger =
      DynamicTo<TimelineTrigger>(target->NamedTriggers()->begin()->value.Get());
  EXPECT_NE(trigger, nullptr);

  UpdateAllLifecyclePhasesForTest();

  ScrollTimeline& timeline = *To<ScrollTimeline>(trigger->Timeline());
  Element& timeline_source = *To<Element>(timeline.ComputeResolvedSource());

  RangeBoundary* cover_10 =
      MakeRangeOffsetBoundary(V8TimelineRange::Enum::kCover, 10);
  RangeBoundary* cover_90 =
      MakeRangeOffsetBoundary(V8TimelineRange::Enum::kCover, 90);
  RangeBoundary* contain_20 =
      MakeRangeOffsetBoundary(V8TimelineRange::Enum::kContain, 20);
  RangeBoundary* contain_80 =
      MakeRangeOffsetBoundary(V8TimelineRange::Enum::kContain, 80);
  TimelineTrigger::RangeBoundary* normal =
      MakeGarbageCollected<RangeBoundary>("normal");
  TimelineTrigger::RangeBoundary* auto_offset =
      MakeGarbageCollected<RangeBoundary>("auto");

  // cover     0%  -> 100px;
  // cover   100%  -> 250px;
  // contain   0%  -> 150px;
  // contain 100%  -> 200px;
  double cover_0_px = 100;
  double cover_100_px = 250;
  double cover_10_px = cover_0_px + 0.1 * (cover_100_px - cover_0_px);
  double cover_90_px = cover_0_px + 0.9 * (cover_100_px - cover_0_px);
  double contain_0_px = 150;
  double contain_100_px = 200;
  double contain_20_px = contain_0_px + 0.2 * (contain_100_px - contain_0_px);
  double contain_80_px = contain_0_px + 0.8 * (contain_100_px - contain_0_px);

  // cover 10% cover 90% auto auto.
  trigger->SetRangeBoundariesForTest(cover_10, cover_90, auto_offset,
                                     auto_offset);
  double dummy_offset = 0;
  std::optional<TriggerBoundaries> boundaries =
      trigger->ComputeTriggerBoundariesForTest(dummy_offset, timeline_source,
                                               timeline);
  ASSERT_TRUE(boundaries.has_value());
  ExpectRelativeErrorWithinEpsilon(boundaries->activation_start, cover_10_px);
  ExpectRelativeErrorWithinEpsilon(boundaries->activation_end, cover_90_px);
  ExpectRelativeErrorWithinEpsilon(boundaries->active_start, cover_10_px);
  ExpectRelativeErrorWithinEpsilon(boundaries->active_end, cover_90_px);

  // contain 20% contain 80% auto normal.
  trigger->SetRangeBoundariesForTest(contain_20, contain_80, auto_offset,
                                     normal);
  boundaries = trigger->ComputeTriggerBoundariesForTest(
      dummy_offset, timeline_source, timeline);
  ASSERT_TRUE(boundaries.has_value());
  ExpectRelativeErrorWithinEpsilon(boundaries->activation_start, contain_20_px);
  ExpectRelativeErrorWithinEpsilon(boundaries->activation_end, contain_80_px);
  ExpectRelativeErrorWithinEpsilon(boundaries->active_start, contain_20_px);
  ExpectRelativeErrorWithinEpsilon(boundaries->active_end, cover_100_px);

  // cover 10% cover 90% normal auto.
  trigger->SetRangeBoundariesForTest(cover_10, cover_90, normal, auto_offset);
  boundaries = trigger->ComputeTriggerBoundariesForTest(
      dummy_offset, timeline_source, timeline);
  ASSERT_TRUE(boundaries.has_value());
  ExpectRelativeErrorWithinEpsilon(boundaries->activation_start, cover_10_px);
  ExpectRelativeErrorWithinEpsilon(boundaries->activation_end, cover_90_px);
  ExpectRelativeErrorWithinEpsilon(boundaries->active_start, cover_0_px);
  ExpectRelativeErrorWithinEpsilon(boundaries->active_end, cover_90_px);

  // contain 20% contain 80% normal normal.
  trigger->SetRangeBoundariesForTest(contain_20, contain_80, normal, normal);
  boundaries = trigger->ComputeTriggerBoundariesForTest(
      dummy_offset, timeline_source, timeline);
  ASSERT_TRUE(boundaries.has_value());
  ExpectRelativeErrorWithinEpsilon(boundaries->activation_start, contain_20_px);
  ExpectRelativeErrorWithinEpsilon(boundaries->activation_end, contain_80_px);
  ExpectRelativeErrorWithinEpsilon(boundaries->active_start, cover_0_px);
  ExpectRelativeErrorWithinEpsilon(boundaries->active_end, cover_100_px);

  // contain 20% contain 80% cover 10% normal.
  trigger->SetRangeBoundariesForTest(contain_20, contain_80, cover_10, normal);
  boundaries = trigger->ComputeTriggerBoundariesForTest(
      dummy_offset, timeline_source, timeline);
  ASSERT_TRUE(boundaries.has_value());
  ExpectRelativeErrorWithinEpsilon(boundaries->activation_start, contain_20_px);
  ExpectRelativeErrorWithinEpsilon(boundaries->activation_end, contain_80_px);
  ExpectRelativeErrorWithinEpsilon(boundaries->active_start, cover_10_px);
  ExpectRelativeErrorWithinEpsilon(boundaries->active_end, cover_100_px);

  // contain 20% contain 80% cover 10% auto.
  trigger->SetRangeBoundariesForTest(contain_20, contain_80, cover_10,
                                     auto_offset);
  boundaries = trigger->ComputeTriggerBoundariesForTest(
      dummy_offset, timeline_source, timeline);
  ASSERT_TRUE(boundaries.has_value());
  ExpectRelativeErrorWithinEpsilon(boundaries->activation_start, contain_20_px);
  ExpectRelativeErrorWithinEpsilon(boundaries->activation_end, contain_80_px);
  ExpectRelativeErrorWithinEpsilon(boundaries->active_start, cover_10_px);
  ExpectRelativeErrorWithinEpsilon(boundaries->active_end, contain_80_px);

  // contain 20% contain 80% cover 10% cover 90%.
  trigger->SetRangeBoundariesForTest(contain_20, contain_80, cover_10,
                                     cover_90);
  boundaries = trigger->ComputeTriggerBoundariesForTest(
      dummy_offset, timeline_source, timeline);
  ASSERT_TRUE(boundaries.has_value());
  ExpectRelativeErrorWithinEpsilon(boundaries->activation_start, contain_20_px);
  ExpectRelativeErrorWithinEpsilon(boundaries->activation_end, contain_80_px);
  ExpectRelativeErrorWithinEpsilon(boundaries->active_start, cover_10_px);
  ExpectRelativeErrorWithinEpsilon(boundaries->active_end, cover_90_px);
}

class TimelineTriggerPlayBackwardsForwardsTest : public TimelineTriggerTest {
 public:
  void Initialize(const char* behavior) {
    std::string html = base::StringPrintf(R"HTML(
      <style>
       @keyframes expand {
          from { transform: scaleX(1); }
          to { transform: scaleX(5); }
        }
        .subject, .target {
          height: 50px; width: 50px; background-color: red;
        }
        #target {
          animation: expand linear 5s both;
          animation-trigger: --trigger %s;
        }
        #subject {
          timeline-trigger: --trigger view() contain;
        }
        .scroller {
          overflow-y: scroll; height: 500px; width: 500px;
        }
        #space { width: 50px; height: 600px; }
      </style>
      <div id="scroller" class="scroller">
        <div id="space"></div>
        <div id="subject" class="subject"></div>
        <div id="space"></div>
      </div>
      <div id="target" class="target"></div>
    )HTML",
                                          behavior);
    SetBodyInnerHTML(String(html.c_str()));
  }
};

TEST_F(TimelineTriggerPlayBackwardsForwardsTest, BackwardsInterruptsForwards) {
  Initialize("play-forwards play-backwards");

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ElementAnimations* animations = target->GetElementAnimations();
  CSSAnimation* animation =
      DynamicTo<CSSAnimation>((*animations->Animations().begin()).key.Get());
  Element* subject = GetDocument().getElementById(AtomicString("subject"));
  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(animation->CurrentTimeInternal().value(), AnimationTimeDelta());
  EXPECT_EQ(animation->CalculateAnimationPlayState(),
            V8AnimationPlayState::Enum::kPaused);
  EXPECT_EQ(animation->EffectivePlaybackRate(), 1);

  // Scroll the subject into view.
  subject->scrollIntoView(nullptr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(animation->EffectivePlaybackRate(), 1);
  EXPECT_EQ(animation->CalculateAnimationPlayState(),
            V8AnimationPlayState::Enum::kRunning);

  AdvanceClockSeconds(0.5);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(animation->EffectivePlaybackRate(), 1);
  EXPECT_EQ(animation->CalculateAnimationPlayState(),
            V8AnimationPlayState::Enum::kRunning);
  AnimationTimeDelta current_time = animation->CurrentTimeInternal().value();
  EXPECT_GT(current_time, AnimationTimeDelta());

  // Scroll the subject out of view
  scroller->scrollTo(nullptr, 0, 0);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(animation->EffectivePlaybackRate(), -1);
  EXPECT_EQ(animation->CalculateAnimationPlayState(),
            V8AnimationPlayState::Enum::kRunning);

  AdvanceClockSeconds(0.25);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(animation->EffectivePlaybackRate(), -1);
  EXPECT_EQ(animation->CalculateAnimationPlayState(),
            V8AnimationPlayState::Enum::kRunning);
  EXPECT_LT(animation->CurrentTimeInternal().value(), current_time);
  EXPECT_GT(animation->CurrentTimeInternal().value(), AnimationTimeDelta());
  current_time = animation->CurrentTimeInternal().value();

  AdvanceClockSeconds(0.3);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(animation->EffectivePlaybackRate(), -1);
  EXPECT_EQ(animation->CalculateAnimationPlayState(),
            V8AnimationPlayState::Enum::kFinished);
  EXPECT_EQ(animation->CurrentTimeInternal().value(), AnimationTimeDelta());
}

TEST_F(TimelineTriggerPlayBackwardsForwardsTest,
       PlayForwardsFinishedAtEndNoOp) {
  Initialize("play-forwards");

  Element* target = GetDocument().getElementById(AtomicString("target"));
  CSSAnimation* animation = DynamicTo<CSSAnimation>(
      (*target->GetElementAnimations()->Animations().begin()).key.Get());
  Element* subject = GetDocument().getElementById(AtomicString("subject"));

  UpdateAllLifecyclePhasesForTest();

  animation->finish(ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(animation->CalculateAnimationPlayState(),
            V8AnimationPlayState::Enum::kFinished);
  EXPECT_EQ(animation->CurrentTimeInternal().value(),
            ANIMATION_TIME_DELTA_FROM_SECONDS(5));
  EXPECT_EQ(animation->EffectivePlaybackRate(), 1);

  subject->scrollIntoView(nullptr);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(animation->CalculateAnimationPlayState(),
            V8AnimationPlayState::Enum::kFinished);
  EXPECT_EQ(animation->CurrentTimeInternal().value(),
            ANIMATION_TIME_DELTA_FROM_SECONDS(5));
}

TEST_F(TimelineTriggerPlayBackwardsForwardsTest,
       PlayForwardsReversesFinishedAtStart) {
  Initialize("play-forwards");

  Element* target = GetDocument().getElementById(AtomicString("target"));
  CSSAnimation* animation = DynamicTo<CSSAnimation>(
      (*target->GetElementAnimations()->Animations().begin()).key.Get());
  Element* subject = GetDocument().getElementById(AtomicString("subject"));

  UpdateAllLifecyclePhasesForTest();

  animation->updatePlaybackRate(-1, ASSERT_NO_EXCEPTION);
  animation->finish(ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(animation->CalculateAnimationPlayState(),
            V8AnimationPlayState::Enum::kFinished);
  EXPECT_EQ(animation->CurrentTimeInternal().value(),
            ANIMATION_TIME_DELTA_FROM_SECONDS(0));
  EXPECT_EQ(animation->EffectivePlaybackRate(), -1);

  subject->scrollIntoView(nullptr);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(animation->CalculateAnimationPlayState(),
            V8AnimationPlayState::Enum::kRunning);
  EXPECT_EQ(animation->EffectivePlaybackRate(), 1);

  AdvanceClockSeconds(1.0);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(animation->CurrentTimeInternal().value(),
            ANIMATION_TIME_DELTA_FROM_SECONDS(1));
}

TEST_F(TimelineTriggerPlayBackwardsForwardsTest,
       PlayBackwardsFinishedAtStartNoOp) {
  Initialize("play-backwards");

  Element* target = GetDocument().getElementById(AtomicString("target"));
  CSSAnimation* animation = DynamicTo<CSSAnimation>(
      (*target->GetElementAnimations()->Animations().begin()).key.Get());
  Element* subject = GetDocument().getElementById(AtomicString("subject"));

  UpdateAllLifecyclePhasesForTest();

  animation->updatePlaybackRate(-1, ASSERT_NO_EXCEPTION);
  animation->finish(ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(animation->CalculateAnimationPlayState(),
            V8AnimationPlayState::Enum::kFinished);
  EXPECT_EQ(animation->CurrentTimeInternal().value(),
            ANIMATION_TIME_DELTA_FROM_SECONDS(0));
  EXPECT_EQ(animation->EffectivePlaybackRate(), -1);

  subject->scrollIntoView(nullptr);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(animation->CalculateAnimationPlayState(),
            V8AnimationPlayState::Enum::kFinished);
  EXPECT_EQ(animation->CurrentTimeInternal().value(),
            ANIMATION_TIME_DELTA_FROM_SECONDS(0));
}

TEST_F(TimelineTriggerPlayBackwardsForwardsTest,
       PlayBackwardsReversesFinishedAtEnd) {
  Initialize("play-backwards");

  Element* target = GetDocument().getElementById(AtomicString("target"));
  CSSAnimation* animation = DynamicTo<CSSAnimation>(
      (*target->GetElementAnimations()->Animations().begin()).key.Get());
  Element* subject = GetDocument().getElementById(AtomicString("subject"));

  UpdateAllLifecyclePhasesForTest();

  animation->finish(ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(animation->CalculateAnimationPlayState(),
            V8AnimationPlayState::Enum::kFinished);
  EXPECT_EQ(animation->CurrentTimeInternal().value(),
            ANIMATION_TIME_DELTA_FROM_SECONDS(5));
  EXPECT_EQ(animation->EffectivePlaybackRate(), 1);

  subject->scrollIntoView(nullptr);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(animation->CalculateAnimationPlayState(),
            V8AnimationPlayState::Enum::kRunning);
  EXPECT_EQ(animation->EffectivePlaybackRate(), -1);

  AdvanceClockSeconds(1.0);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(animation->CurrentTimeInternal().value(),
            ANIMATION_TIME_DELTA_FROM_SECONDS(4));
}

TEST_F(TimelineTriggerTest, CSSUseCounter) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kTimelineTrigger));

  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller {
        overflow-y: scroll; width: 100px; height: 100px;
      }
      #target {
        timeline-trigger: --trigger scroll();
      }
    </style>
    <div id='scroller'></div>
    <div id='target'></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kTimelineTrigger));
}

TEST_F(TimelineTriggerTest, JSUseCounter) {
  EXPECT_FALSE(GetDocument().IsUseCounted(WebFeature::kTimelineTrigger));
  GetDocument().GetSettings()->SetScriptEnabled(true);

  ClassicScript::CreateUnspecifiedScript(
      "new TimelineTrigger([{timeline: document.timeline}]);")
      ->RunScript(GetDocument().domWindow());

  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kTimelineTrigger));
}

TEST_F(TimelineTriggerTest, ScrollAndTimeDrivenAnimationReplay) {
  SetBodyInnerHTML(R"HTML(
    <style>
      @keyframes anim {
        from { opacity: 0; }
        to { opacity: 1; }
      }
      #scroller {
        overflow-y: scroll; width: 100px; height: 100px;
        scroll-timeline: --scroll-timeline y;
      }
      #wrapper {
        timeline-trigger: --trigger scroll();
      }
      #sda-target {
        animation: anim 5s both;
        animation-timeline: --scroll-timeline;
        animation-trigger: --trigger replay;
        height: 100px;
      }
      #tda-target {
        animation: anim 5s both;
        animation-trigger: --trigger replay;
        height: 50px;
      }
      #spacer { width: 100px; height: 200px; }
    </style>
    <div id='scroller'>
      <div id='spacer'></div>
      <div id='wrapper'>
        <div id='sda-target'></div>
        <div id='tda-target'></div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* sda_target =
      GetDocument().getElementById(AtomicString("sda-target"));
  CSSAnimation* sda = DynamicTo<CSSAnimation>(
      (*sda_target->GetElementAnimations()->Animations().begin()).key.Get());
  Element* tda_target =
      GetDocument().getElementById(AtomicString("tda-target"));
  CSSAnimation* tda = DynamicTo<CSSAnimation>(
      (*tda_target->GetElementAnimations()->Animations().begin()).key.Get());

  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  Element* wrapper = GetDocument().getElementById(AtomicString("wrapper"));

  // Retrieve the trigger from wrapper.
  TimelineTrigger* trigger = DynamicTo<TimelineTrigger>(
      wrapper->NamedTriggers()->begin()->value.Get());
  // Verify both animations are registered with the trigger.
  EXPECT_EQ(trigger->getAnimations().size(), 2u);

  // Make progress on both animations, advance time for tda, scroll position for
  // sda.

  // Scroll to 50% of the scroll range.
  // Content height = 200 (spacer) + 150 (wrapper) = 350px.
  // Scroller height = 100px.
  // Scroll range = 350 - 100 = 250px.
  // 50% of scroll range = 125px.
  scroller->scrollTo(nullptr, 0, 125);
  // Advance clock to let time-driven animation progress.
  AdvanceClockSeconds(1.0);
  UpdateAllLifecyclePhasesForTest();

  // Verify initial progress of scroll-driven animation is 50%.
  double sda_duration_ms = sda->timeline()->GetDuration()->InMillisecondsF();
  double sda_current_time_ms = sda->CurrentTimeInternal()->InMillisecondsF();
  EXPECT_NEAR(50.0, (sda_current_time_ms / sda_duration_ms) * 100.0, 0.1);

  // Verify time-driven animation progressed.
  EXPECT_EQ(tda->CurrentTimeInternal().value(),
            ANIMATION_TIME_DELTA_FROM_SECONDS(1));

  // Manually trigger the "activate" behavior (replay).
  cc::AnimationTriggerDelegate* delegate =
      static_cast<cc::AnimationTriggerDelegate*>(trigger);
  delegate->NotifyActivated(platform()->NowTicks());

  // Verify:
  // - Scroll-driven animation did NOT rewind (still 50%).
  // - Time-driven animation DID rewind to 0.
  sda_current_time_ms = sda->CurrentTimeInternal()->InMillisecondsF();
  EXPECT_NEAR(50.0, (sda_current_time_ms / sda_duration_ms) * 100.0, 0.1);
  EXPECT_EQ(tda->CurrentTimeInternal().value(), AnimationTimeDelta());

  UpdateAllLifecyclePhasesForTest();

  // Verify they remain in the same state after lifecycle update.
  sda_current_time_ms = sda->CurrentTimeInternal()->InMillisecondsF();
  EXPECT_NEAR(50.0, (sda_current_time_ms / sda_duration_ms) * 100.0, 0.1);
  EXPECT_EQ(tda->CurrentTimeInternal().value(), AnimationTimeDelta());
}

}  // namespace blink
