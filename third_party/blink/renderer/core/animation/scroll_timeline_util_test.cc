// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_timeline_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_timeline_options.h"
#include "third_party/blink/renderer/core/animation/animation_test_helpers.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

namespace {

HeapVector<Member<ScrollTimelineOffset>>* CreateScrollOffsets(
    ScrollTimelineOffset* start_scroll_offset,
    ScrollTimelineOffset* end_scroll_offset) {
  HeapVector<Member<ScrollTimelineOffset>>* scroll_offsets =
      MakeGarbageCollected<HeapVector<Member<ScrollTimelineOffset>>>();
  scroll_offsets->push_back(start_scroll_offset);
  scroll_offsets->push_back(end_scroll_offset);
  return scroll_offsets;
}

}  // namespace

namespace scroll_timeline_util {

using ScrollTimelineUtilTest = PageTestBase;

// This test covers only the basic conversions for element id, time range,
// orientation, and start and end scroll offset. Complex orientation conversions
// are tested in the GetOrientation* tests, and complex start/end scroll offset
// resolutions are tested in blink::ScrollTimelineTest.
TEST_F(ScrollTimelineUtilTest, ToCompositorScrollTimeline) {
  using animation_test_helpers::OffsetFromString;

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
  base::Optional<CompositorElementId> element_id =
      GetCompositorScrollElementId(scroller);
  ASSERT_TRUE(element_id.has_value());

  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  options->setScrollSource(scroller);
  const double time_range = 100;
  options->setTimeRange(
      DoubleOrScrollTimelineAutoKeyword::FromDouble(time_range));
  options->setOrientation("block");
  options->setStartScrollOffset(OffsetFromString(GetDocument(), "50px"));
  options->setEndScrollOffset(OffsetFromString(GetDocument(), "auto"));
  ScrollTimeline* timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  scoped_refptr<CompositorScrollTimeline> compositor_timeline =
      ToCompositorScrollTimeline(timeline);
  EXPECT_EQ(compositor_timeline->GetActiveIdForTest(), base::nullopt);
  EXPECT_EQ(compositor_timeline->GetPendingIdForTest(), element_id);
  EXPECT_EQ(compositor_timeline->GetTimeRangeForTest(), time_range);
  EXPECT_EQ(compositor_timeline->GetDirectionForTest(),
            CompositorScrollTimeline::ScrollDown);
  EXPECT_EQ(compositor_timeline->GetStartScrollOffsetForTest(), 50);
  // 900 is contents-size - scroller-viewport == 1000 - 100
  EXPECT_EQ(compositor_timeline->GetEndScrollOffsetForTest(), 900);
}

TEST_F(ScrollTimelineUtilTest, ToCompositorScrollTimelineNullParameter) {
  EXPECT_EQ(ToCompositorScrollTimeline(nullptr), nullptr);
}

TEST_F(ScrollTimelineUtilTest,
       ToCompositorScrollTimelineDocumentTimelineParameter) {
  DocumentTimeline* timeline =
      MakeGarbageCollected<DocumentTimeline>(Document::CreateForTest());
  EXPECT_EQ(ToCompositorScrollTimeline(timeline), nullptr);
}

TEST_F(ScrollTimelineUtilTest, ToCompositorScrollTimelineNullScrollSource) {
  // Directly call the constructor to make it easier to pass a null
  // scrollSource. The alternative approach would require us to remove the
  // documentElement from the document.
  Element* scroll_source = nullptr;
  ScrollTimelineOffset* start_scroll_offset =
      MakeGarbageCollected<ScrollTimelineOffset>();
  ScrollTimelineOffset* end_scroll_offset =
      MakeGarbageCollected<ScrollTimelineOffset>();
  ScrollTimeline* timeline = MakeGarbageCollected<ScrollTimeline>(
      &GetDocument(), scroll_source, ScrollTimeline::Block,
      CreateScrollOffsets(start_scroll_offset, end_scroll_offset), 100);

  scoped_refptr<CompositorScrollTimeline> compositor_timeline =
      ToCompositorScrollTimeline(timeline);
  ASSERT_TRUE(compositor_timeline.get());
  EXPECT_EQ(compositor_timeline->GetPendingIdForTest(), base::nullopt);
}

TEST_F(ScrollTimelineUtilTest, ToCompositorScrollTimelineNullLayoutBox) {
  auto* div = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  ASSERT_FALSE(div->GetLayoutBox());

  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  DoubleOrScrollTimelineAutoKeyword time_range =
      DoubleOrScrollTimelineAutoKeyword::FromDouble(100);
  options->setTimeRange(time_range);
  options->setScrollSource(div);
  ScrollTimeline* timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  scoped_refptr<CompositorScrollTimeline> compositor_timeline =
      ToCompositorScrollTimeline(timeline);
  EXPECT_TRUE(compositor_timeline.get());
  // Here we just want to test the start/end scroll offset.
  // ToCompositorScrollTimelineNullScrollSource covers the expected pending id
  // and ConvertOrientationNullStyle covers the orientation conversion.
  EXPECT_EQ(compositor_timeline->GetStartScrollOffsetForTest(), base::nullopt);
  EXPECT_EQ(compositor_timeline->GetEndScrollOffsetForTest(), base::nullopt);
}

TEST_F(ScrollTimelineUtilTest, ConvertOrientationPhysicalCases) {
  // For physical the writing-mode and directionality shouldn't matter, so make
  // sure it doesn't.
  Vector<WritingMode> writing_modes = {WritingMode::kHorizontalTb,
                                       WritingMode::kVerticalLr,
                                       WritingMode::kVerticalRl};
  Vector<TextDirection> directions = {TextDirection::kLtr, TextDirection::kRtl};

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  for (const WritingMode& writing_mode : writing_modes) {
    for (const TextDirection& direction : directions) {
      style->SetWritingMode(writing_mode);
      style->SetDirection(direction);
      EXPECT_EQ(ConvertOrientation(ScrollTimeline::Vertical, style.get()),
                CompositorScrollTimeline::ScrollDown);
      EXPECT_EQ(ConvertOrientation(ScrollTimeline::Horizontal, style.get()),
                CompositorScrollTimeline::ScrollRight);
    }
  }
}

TEST_F(ScrollTimelineUtilTest, ConvertOrientationLogical) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();

  // horizontal-tb, ltr
  style->SetWritingMode(WritingMode::kHorizontalTb);
  style->SetDirection(TextDirection::kLtr);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::Block, style.get()),
            CompositorScrollTimeline::ScrollDown);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::Inline, style.get()),
            CompositorScrollTimeline::ScrollRight);

  // vertical-lr, ltr
  style->SetWritingMode(WritingMode::kVerticalLr);
  style->SetDirection(TextDirection::kLtr);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::Block, style.get()),
            CompositorScrollTimeline::ScrollRight);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::Inline, style.get()),
            CompositorScrollTimeline::ScrollDown);

  // vertical-rl, ltr
  style->SetWritingMode(WritingMode::kVerticalRl);
  style->SetDirection(TextDirection::kLtr);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::Block, style.get()),
            CompositorScrollTimeline::ScrollLeft);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::Inline, style.get()),
            CompositorScrollTimeline::ScrollDown);

  // horizontal-tb, rtl
  style->SetWritingMode(WritingMode::kHorizontalTb);
  style->SetDirection(TextDirection::kRtl);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::Block, style.get()),
            CompositorScrollTimeline::ScrollDown);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::Inline, style.get()),
            CompositorScrollTimeline::ScrollLeft);

  // vertical-lr, rtl
  style->SetWritingMode(WritingMode::kVerticalLr);
  style->SetDirection(TextDirection::kRtl);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::Block, style.get()),
            CompositorScrollTimeline::ScrollRight);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::Inline, style.get()),
            CompositorScrollTimeline::ScrollUp);

  // vertical-rl, rtl
  style->SetWritingMode(WritingMode::kVerticalRl);
  style->SetDirection(TextDirection::kRtl);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::Block, style.get()),
            CompositorScrollTimeline::ScrollLeft);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::Inline, style.get()),
            CompositorScrollTimeline::ScrollUp);
}

TEST_F(ScrollTimelineUtilTest, ConvertOrientationNullStyle) {
  // When the style is nullptr we assume horizontal-tb and ltr direction. This
  // means that block is ScrollDown and inline is ScrollRight
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::Vertical, nullptr),
            CompositorScrollTimeline::ScrollDown);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::Horizontal, nullptr),
            CompositorScrollTimeline::ScrollRight);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::Block, nullptr),
            CompositorScrollTimeline::ScrollDown);
  EXPECT_EQ(ConvertOrientation(ScrollTimeline::Inline, nullptr),
            CompositorScrollTimeline::ScrollRight);
}

TEST_F(ScrollTimelineUtilTest, GetCompositorScrollElementIdNullNode) {
  EXPECT_EQ(GetCompositorScrollElementId(nullptr), base::nullopt);
}

TEST_F(ScrollTimelineUtilTest, GetCompositorScrollElementIdNullLayoutObject) {
  auto* div = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  ASSERT_FALSE(div->GetLayoutObject());
  EXPECT_EQ(GetCompositorScrollElementId(nullptr), base::nullopt);
}

TEST_F(ScrollTimelineUtilTest, GetCompositorScrollElementIdNoUniqueId) {
  SetBodyInnerHTML("<div id='test'></div>");
  Element* test = GetElementById("test");
  ASSERT_TRUE(test->GetLayoutObject());
  EXPECT_EQ(GetCompositorScrollElementId(test), base::nullopt);
}

}  // namespace scroll_timeline_util

}  // namespace blink
