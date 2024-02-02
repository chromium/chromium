// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_timeline_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_timeline_options.h"
#include "third_party/blink/renderer/core/animation/animation_test_helpers.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
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
  options->setAxis("block");
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
  EXPECT_TRUE(compositor_timeline.get());
}

TEST_F(ScrollTimelineUtilTest, ConvertOrientationPhysicalCases) {
  // For physical the writing-mode and directionality shouldn't matter, so make
  // sure it doesn't.
  Vector<WritingMode> writing_modes = {WritingMode::kHorizontalTb,
                                       WritingMode::kVerticalLr,
                                       WritingMode::kVerticalRl};
  Vector<TextDirection> directions = {TextDirection::kLtr, TextDirection::kRtl};

  for (const WritingMode& writing_mode : writing_modes) {
    for (const TextDirection& direction : directions) {
      ComputedStyleBuilder style_builder =
          GetDocument().GetStyleResolver().CreateComputedStyleBuilder();
      style_builder.SetWritingMode(writing_mode);
      style_builder.SetDirection(direction);
      const ComputedStyle* style = style_builder.TakeStyle();
      EXPECT_EQ(ConvertOrientation(ScrollTimeline::ScrollAxis::kY, style),
                CompositorScrollTimeline::ScrollDown);
      EXPECT_EQ(ConvertOrientation(ScrollTimeline::ScrollAxis::kX, style),
                CompositorScrollTimeline::ScrollRight);
    }
  }
}

TEST_F(ScrollTimelineUtilTest, ConvertOrientationLogical) {
  // horizontal-tb, ltr
  ComputedStyleBuilder builder =
      GetDocument().GetStyleResolver().CreateComputedStyleBuilder();
  builder.SetWritingMode(WritingMode::kHorizontalTb);
  builder.SetDirection(TextDirection::kLtr);
  const ComputedStyle* style = builder.TakeStyle();
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::ScrollAxis::kBlock, style),
            CompositorScrollTimeline::ScrollDown);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::ScrollAxis::kInline, style),
            CompositorScrollTimeline::ScrollRight);

  // vertical-lr, ltr
  builder = GetDocument().GetStyleResolver().CreateComputedStyleBuilder();
  builder.SetWritingMode(WritingMode::kVerticalLr);
  builder.SetDirection(TextDirection::kLtr);
  style = builder.TakeStyle();
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::ScrollAxis::kBlock, style),
            CompositorScrollTimeline::ScrollRight);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::ScrollAxis::kInline, style),
            CompositorScrollTimeline::ScrollDown);

  // vertical-rl, ltr
  builder = GetDocument().GetStyleResolver().CreateComputedStyleBuilder();
  builder.SetWritingMode(WritingMode::kVerticalRl);
  builder.SetDirection(TextDirection::kLtr);
  style = builder.TakeStyle();
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::ScrollAxis::kBlock, style),
            CompositorScrollTimeline::ScrollLeft);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::ScrollAxis::kInline, style),
            CompositorScrollTimeline::ScrollDown);

  // horizontal-tb, rtl
  builder = GetDocument().GetStyleResolver().CreateComputedStyleBuilder();
  builder.SetWritingMode(WritingMode::kHorizontalTb);
  builder.SetDirection(TextDirection::kRtl);
  style = builder.TakeStyle();
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::ScrollAxis::kBlock, style),
            CompositorScrollTimeline::ScrollDown);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::ScrollAxis::kInline, style),
            CompositorScrollTimeline::ScrollLeft);

  // vertical-lr, rtl
  builder = GetDocument().GetStyleResolver().CreateComputedStyleBuilder();
  builder.SetWritingMode(WritingMode::kVerticalLr);
  builder.SetDirection(TextDirection::kRtl);
  style = builder.TakeStyle();
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::ScrollAxis::kBlock, style),
            CompositorScrollTimeline::ScrollRight);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::ScrollAxis::kInline, style),
            CompositorScrollTimeline::ScrollUp);

  // vertical-rl, rtl
  builder = GetDocument().GetStyleResolver().CreateComputedStyleBuilder();
  builder.SetWritingMode(WritingMode::kVerticalRl);
  builder.SetDirection(TextDirection::kRtl);
  style = builder.TakeStyle();
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::ScrollAxis::kBlock, style),
            CompositorScrollTimeline::ScrollLeft);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::ScrollAxis::kInline, style),
            CompositorScrollTimeline::ScrollUp);
}

TEST_F(ScrollTimelineUtilTest, ConvertOrientationNullStyle) {
  // When the style is nullptr we assume horizontal-tb and ltr direction. This
  // means that block is ScrollDown and inline is ScrollRight
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::ScrollAxis::kY, nullptr),
            CompositorScrollTimeline::ScrollDown);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::ScrollAxis::kX, nullptr),
            CompositorScrollTimeline::ScrollRight);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::ScrollAxis::kBlock, nullptr),
            CompositorScrollTimeline::ScrollDown);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::ScrollAxis::kInline, nullptr),
            CompositorScrollTimeline::ScrollRight);
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
