// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_timeline_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_timeline_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_timeline_options.h"
#include "third_party/blink/renderer/core/animation/animation_test_helpers.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/animation/view_timeline.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace scroll_timeline_util {

using ScrollTimelineUtilTest = PageTestBase;

// This test covers only the basic conversions for element id, time range,
// and orientation. Complex orientation conversions are tested in the
// GetOrientation* tests.
TEST_F(ScrollTimelineUtilTest, ToCompositorScrollTimeline) {
  // using animation_test_helpers::OffsetFromString;

  SetBodyInnerHTML(R"HTML(
    <style>
      #scroller {
        overflow: auto;
        width: 100px;
        height: 100px;
      }
      #contents {
        height: 1000px;
      }
    </style>
    <div id='scroller'><div id='contents'></div></div>
  )HTML");

  Element* scroller = GetElementById("scroller");
  std::optional<CompositorElementId> element_id =
      GetCompositorScrollElementId(scroller);
  ASSERT_TRUE(element_id.has_value());

  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  options->setSource(scroller);
  options->setAxis(V8ScrollAxis::Enum::kBlock);
  ScrollTimeline* timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  scoped_refptr<CompositorScrollTimeline> compositor_timeline =
      ToCompositorScrollTimeline(timeline);
  EXPECT_EQ(compositor_timeline->GetActiveIdForTest(), std::nullopt);
  EXPECT_EQ(compositor_timeline->GetPendingIdForTest(), element_id);
  EXPECT_EQ(compositor_timeline->GetDirectionForTest(),
            CompositorScrollTimeline::ScrollDown);
}

TEST_F(ScrollTimelineUtilTest, ToCompositorScrollTimelineNullParameter) {
  EXPECT_EQ(ToCompositorScrollTimeline(nullptr), nullptr);
}

TEST_F(ScrollTimelineUtilTest,
       ToCompositorScrollTimelineDocumentTimelineParameter) {
  ScopedNullExecutionContext execution_context;
  DocumentTimeline* timeline = MakeGarbageCollected<DocumentTimeline>(
      Document::CreateForTest(execution_context.GetExecutionContext()));
  EXPECT_EQ(ToCompositorScrollTimeline(timeline), nullptr);
}

TEST_F(ScrollTimelineUtilTest, ToCompositorScrollTimelineNullSource) {
  // Directly call the constructor to make it easier to pass a null
  // source. The alternative approach would require us to remove the
  // documentElement from the document.
  Element* source = nullptr;
  ScrollTimeline* timeline = ScrollTimeline::Create(
      &GetDocument(), source, ScrollTimeline::ScrollAxis::kBlock);

  scoped_refptr<CompositorScrollTimeline> compositor_timeline =
      ToCompositorScrollTimeline(timeline);
  ASSERT_TRUE(compositor_timeline.get());
  EXPECT_EQ(compositor_timeline->GetPendingIdForTest(), std::nullopt);
}

TEST_F(ScrollTimelineUtilTest, ToCompositorScrollTimelineNullLayoutBox) {
  auto* div = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  ASSERT_FALSE(div->GetLayoutBox());

  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  options->setSource(div);
  ScrollTimeline* timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  scoped_refptr<CompositorScrollTimeline> compositor_timeline =
      ToCompositorScrollTimeline(timeline);
  ASSERT_TRUE(compositor_timeline.get());
  // Without a layout box the direction is unresolved, so the cc timeline is
  // inactive.
  EXPECT_EQ(compositor_timeline->GetDirectionForTest(), std::nullopt);
}

TEST_F(ScrollTimelineUtilTest,
       ToCompositorScrollTimelineMismatchedWritingDirections) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #outer {
        overflow: scroll clip;
        width: 100px;
        height: 100px;
      }
      #inner {
        writing-mode: vertical-rl;
        overflow: clip scroll;
        width: 300px;
        height: 100px;
      }
      #subject {
        width: 50px;
        height: 300px;
      }
    </style>
    <div id='outer'><div id='inner'><div id='subject'></div></div></div>
  )HTML");

  // The timeline's block axis resolves via #inner (vertical-rl) to the
  // horizontal axis, but the matched source #outer is horizontal-tb ltr, so
  // progress must not be reversed.
  ViewTimelineOptions* options = ViewTimelineOptions::Create();
  options->setSubject(GetElementById("subject"));
  options->setAxis(V8ScrollAxis::Enum::kBlock);
  ViewTimeline* timeline =
      ViewTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  scoped_refptr<CompositorScrollTimeline> compositor_timeline =
      ToCompositorScrollTimeline(timeline);
  ASSERT_TRUE(compositor_timeline.get());
  EXPECT_EQ(compositor_timeline->GetDirectionForTest(),
            CompositorScrollTimeline::ScrollRight);
}

TEST_F(ScrollTimelineUtilTest, ToCompositorScrollTimelineOriginAtPhysicalEnd) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #rtl, #vertical-rl-rtl {
        width: 100px;
        height: 100px;
      }
      #rtl {
        direction: rtl;
        overflow: scroll clip;
      }
      #rtl > div {
        width: 1000px;
        height: 50px;
      }
      #vertical-rl-rtl {
        writing-mode: vertical-rl;
        direction: rtl;
        overflow: clip scroll;
      }
      #vertical-rl-rtl > div {
        width: 50px;
        height: 1000px;
      }
    </style>
    <div id='rtl'><div></div></div>
    <div id='vertical-rl-rtl'><div></div></div>
  )HTML");

  auto direction_for = [&](const char* id, V8ScrollAxis::Enum axis) {
    ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
    options->setSource(GetElementById(id));
    options->setAxis(axis);
    ScrollTimeline* timeline =
        ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);
    return ToCompositorScrollTimeline(timeline)->GetDirectionForTest();
  };

  // A negative minimum scroll offset means the scroll origin sits at the
  // physical end of the axis, so the resolved direction points toward the
  // physical start.
  EXPECT_EQ(direction_for("rtl", V8ScrollAxis::Enum::kX),
            CompositorScrollTimeline::ScrollLeft);
  // In vertical-rl rtl, the inline axis is vertical with its scroll origin
  // at the physical bottom.
  EXPECT_EQ(direction_for("vertical-rl-rtl", V8ScrollAxis::Enum::kInline),
            CompositorScrollTimeline::ScrollUp);
}

TEST_F(ScrollTimelineUtilTest, GetCompositorScrollElementIdNullNode) {
  EXPECT_EQ(GetCompositorScrollElementId(nullptr), std::nullopt);
}

TEST_F(ScrollTimelineUtilTest, GetCompositorScrollElementIdNullLayoutObject) {
  auto* div = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  ASSERT_FALSE(div->GetLayoutObject());
  EXPECT_EQ(GetCompositorScrollElementId(nullptr), std::nullopt);
}

TEST_F(ScrollTimelineUtilTest, GetCompositorScrollElementIdNoUniqueId) {
  SetBodyInnerHTML("<div id='test'></div>");
  Element* test = GetElementById("test");
  ASSERT_TRUE(test->GetLayoutObject());
  EXPECT_EQ(GetCompositorScrollElementId(test), std::nullopt);
}

}  // namespace scroll_timeline_util

}  // namespace blink
