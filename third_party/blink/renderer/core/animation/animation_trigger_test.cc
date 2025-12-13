// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_range_offset.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/css/css_animation.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/animation/timeline_trigger.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"

namespace blink {
namespace {

void ExpectRelativeErrorWithinEpsilon(double expected, double observed) {
  EXPECT_NEAR(1.0, observed / expected, std::numeric_limits<double>::epsilon());
}

}  // namespace

class AnimationTriggerTest : public PaintTestConfigurations,
                             public RenderingTest {
 public:
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
};

INSTANTIATE_PAINT_TEST_SUITE_P(AnimationTriggerTest);

TEST_P(AnimationTriggerTest, ComputeBoundariesTest) {
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

  ScrollTimeline& timeline = *To<ScrollTimeline>(trigger->timeline());
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
  TriggerBoundaries boundaries = trigger->ComputeTriggerBoundariesForTest(
      dummy_offset, timeline_source, timeline);
  ExpectRelativeErrorWithinEpsilon(boundaries.start, cover_10_px);
  ExpectRelativeErrorWithinEpsilon(boundaries.end, cover_90_px);
  ExpectRelativeErrorWithinEpsilon(boundaries.exit_start, cover_10_px);
  ExpectRelativeErrorWithinEpsilon(boundaries.exit_end, cover_90_px);

  // contain 20% contain 80% auto normal.
  trigger->SetRangeBoundariesForTest(contain_20, contain_80, auto_offset,
                                     normal);
  boundaries = trigger->ComputeTriggerBoundariesForTest(
      dummy_offset, timeline_source, timeline);
  ExpectRelativeErrorWithinEpsilon(boundaries.start, contain_20_px);
  ExpectRelativeErrorWithinEpsilon(boundaries.end, contain_80_px);
  ExpectRelativeErrorWithinEpsilon(boundaries.exit_start, contain_20_px);
  ExpectRelativeErrorWithinEpsilon(boundaries.exit_end, cover_100_px);

  // cover 10% cover 90% normal auto.
  trigger->SetRangeBoundariesForTest(cover_10, cover_90, normal, auto_offset);
  boundaries = trigger->ComputeTriggerBoundariesForTest(
      dummy_offset, timeline_source, timeline);
  ExpectRelativeErrorWithinEpsilon(boundaries.start, cover_10_px);
  ExpectRelativeErrorWithinEpsilon(boundaries.end, cover_90_px);
  ExpectRelativeErrorWithinEpsilon(boundaries.exit_start, cover_0_px);
  ExpectRelativeErrorWithinEpsilon(boundaries.exit_end, cover_90_px);

  // contain 20% contain 80% normal normal.
  trigger->SetRangeBoundariesForTest(contain_20, contain_80, normal, normal);
  boundaries = trigger->ComputeTriggerBoundariesForTest(
      dummy_offset, timeline_source, timeline);
  ExpectRelativeErrorWithinEpsilon(boundaries.start, contain_20_px);
  ExpectRelativeErrorWithinEpsilon(boundaries.end, contain_80_px);
  ExpectRelativeErrorWithinEpsilon(boundaries.exit_start, cover_0_px);
  ExpectRelativeErrorWithinEpsilon(boundaries.exit_end, cover_100_px);

  // contain 20% contain 80% cover 10% normal.
  trigger->SetRangeBoundariesForTest(contain_20, contain_80, cover_10, normal);
  boundaries = trigger->ComputeTriggerBoundariesForTest(
      dummy_offset, timeline_source, timeline);
  ExpectRelativeErrorWithinEpsilon(boundaries.start, contain_20_px);
  ExpectRelativeErrorWithinEpsilon(boundaries.end, contain_80_px);
  ExpectRelativeErrorWithinEpsilon(boundaries.exit_start, cover_10_px);
  ExpectRelativeErrorWithinEpsilon(boundaries.exit_end, cover_100_px);

  // contain 20% contain 80% cover 10% auto.
  trigger->SetRangeBoundariesForTest(contain_20, contain_80, cover_10,
                                     auto_offset);
  boundaries = trigger->ComputeTriggerBoundariesForTest(
      dummy_offset, timeline_source, timeline);
  ExpectRelativeErrorWithinEpsilon(boundaries.start, contain_20_px);
  ExpectRelativeErrorWithinEpsilon(boundaries.end, contain_80_px);
  ExpectRelativeErrorWithinEpsilon(boundaries.exit_start, cover_10_px);
  ExpectRelativeErrorWithinEpsilon(boundaries.exit_end, contain_80_px);

  // contain 20% contain 80% cover 10% cover 90%.
  trigger->SetRangeBoundariesForTest(contain_20, contain_80, cover_10,
                                     cover_90);
  boundaries = trigger->ComputeTriggerBoundariesForTest(
      dummy_offset, timeline_source, timeline);
  ExpectRelativeErrorWithinEpsilon(boundaries.start, contain_20_px);
  ExpectRelativeErrorWithinEpsilon(boundaries.end, contain_80_px);
  ExpectRelativeErrorWithinEpsilon(boundaries.exit_start, cover_10_px);
  ExpectRelativeErrorWithinEpsilon(boundaries.exit_end, cover_90_px);
}

}  // namespace blink
