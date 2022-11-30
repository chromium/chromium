// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_timeline_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_timeline_options.h"
#include "third_party/blink/renderer/core/animation/animation_test_helpers.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
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
  absl::optional<CompositorElementId> element_id =
      GetCompositorScrollElementId(scroller);
  ASSERT_TRUE(element_id.has_value());

  ScrollTimelineOptions* options = ScrollTimelineOptions::Create();
  options->setSource(scroller);
  options->setOrientation("block");
  ScrollTimeline* timeline =
      ScrollTimeline::Create(GetDocument(), options, ASSERT_NO_EXCEPTION);

  scoped_refptr<CompositorScrollTimeline> compositor_timeline =
      ToCompositorScrollTimeline(timeline);
  EXPECT_EQ(compositor_timeline->GetActiveIdForTest(), absl::nullopt);
  EXPECT_EQ(compositor_timeline->GetPendingIdForTest(), element_id);
  EXPECT_EQ(compositor_timeline->GetDirectionForTest(),
            CompositorScrollTimeline::ScrollDown);
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

TEST_F(ScrollTimelineUtilTest, ToCompositorScrollTimelineNullSource) {
  // Directly call the constructor to make it easier to pass a null
  // source. The alternative approach would require us to remove the
  // documentElement from the document.
  Element* source = nullptr;
  ScrollTimeline* timeline = ScrollTimeline::Create(
      &GetDocument(), source, ScrollTimeline::ScrollDirection::kBlock);

  scoped_refptr<CompositorScrollTimeline> compositor_timeline =
      ToCompositorScrollTimeline(timeline);
  ASSERT_TRUE(compositor_timeline.get());
  EXPECT_EQ(compositor_timeline->GetPendingIdForTest(), absl::nullopt);
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

  scoped_refptr<ComputedStyle> style =
      GetDocument().GetStyleResolver().CreateComputedStyle();
  for (const WritingMode& writing_mode : writing_modes) {
    for (const TextDirection& direction : directions) {
      style->SetWritingMode(writing_mode);
      style->SetDirection(direction);
      EXPECT_EQ(ConvertOrientation(ScrollTimeline::ScrollDirection::kVertical,
                                   style.get()),
                CompositorScrollTimeline::ScrollDown);
      EXPECT_EQ(ConvertOrientation(ScrollTimeline::ScrollDirection::kHorizontal,
                                   style.get()),
                CompositorScrollTimeline::ScrollRight);
    }
  }
}

TEST_F(ScrollTimelineUtilTest, ConvertOrientationLogical) {
  scoped_refptr<ComputedStyle> style =
      GetDocument().GetStyleResolver().CreateComputedStyle();

  // horizontal-tb, ltr
  style->SetWritingMode(WritingMode::kHorizontalTb);
  style->SetDirection(TextDirection::kLtr);
  EXPECT_EQ(
      ConvertOrientation(ScrollTimeline::ScrollDirection::kBlock, style.get()),
      CompositorScrollTimeline::ScrollDown);
  EXPECT_EQ(
      ConvertOrientation(ScrollTimeline::ScrollDirection::kInline, style.get()),
      CompositorScrollTimeline::ScrollRight);

  // vertical-lr, ltr
  style->SetWritingMode(WritingMode::kVerticalLr);
  style->SetDirection(TextDirection::kLtr);
  EXPECT_EQ(
      ConvertOrientation(ScrollTimeline::ScrollDirection::kBlock, style.get()),
      CompositorScrollTimeline::ScrollRight);
  EXPECT_EQ(
      ConvertOrientation(ScrollTimeline::ScrollDirection::kInline, style.get()),
      CompositorScrollTimeline::ScrollDown);

  // vertical-rl, ltr
  style->SetWritingMode(WritingMode::kVerticalRl);
  style->SetDirection(TextDirection::kLtr);
  EXPECT_EQ(
      ConvertOrientation(ScrollTimeline::ScrollDirection::kBlock, style.get()),
      CompositorScrollTimeline::ScrollLeft);
  EXPECT_EQ(
      ConvertOrientation(ScrollTimeline::ScrollDirection::kInline, style.get()),
      CompositorScrollTimeline::ScrollDown);

  // horizontal-tb, rtl
  style->SetWritingMode(WritingMode::kHorizontalTb);
  style->SetDirection(TextDirection::kRtl);
  EXPECT_EQ(
      ConvertOrientation(ScrollTimeline::ScrollDirection::kBlock, style.get()),
      CompositorScrollTimeline::ScrollDown);
  EXPECT_EQ(
      ConvertOrientation(ScrollTimeline::ScrollDirection::kInline, style.get()),
      CompositorScrollTimeline::ScrollLeft);

  // vertical-lr, rtl
  style->SetWritingMode(WritingMode::kVerticalLr);
  style->SetDirection(TextDirection::kRtl);
  EXPECT_EQ(
      ConvertOrientation(ScrollTimeline::ScrollDirection::kBlock, style.get()),
      CompositorScrollTimeline::ScrollRight);
  EXPECT_EQ(
      ConvertOrientation(ScrollTimeline::ScrollDirection::kInline, style.get()),
      CompositorScrollTimeline::ScrollUp);

  // vertical-rl, rtl
  style->SetWritingMode(WritingMode::kVerticalRl);
  style->SetDirection(TextDirection::kRtl);
  EXPECT_EQ(
      ConvertOrientation(ScrollTimeline::ScrollDirection::kBlock, style.get()),
      CompositorScrollTimeline::ScrollLeft);
  EXPECT_EQ(
      ConvertOrientation(ScrollTimeline::ScrollDirection::kInline, style.get()),
      CompositorScrollTimeline::ScrollUp);
}

TEST_F(ScrollTimelineUtilTest, ConvertOrientationNullStyle) {
  // When the style is nullptr we assume horizontal-tb and ltr direction. This
  // means that block is ScrollDown and inline is ScrollRight
  EXPECT_EQ(
      ConvertOrientation(ScrollTimeline::ScrollDirection::kVertical, nullptr),
      CompositorScrollTimeline::ScrollDown);
  EXPECT_EQ(
      ConvertOrientation(ScrollTimeline::ScrollDirection::kHorizontal, nullptr),
      CompositorScrollTimeline::ScrollRight);
  EXPECT_EQ(
      ConvertOrientation(ScrollTimeline::ScrollDirection::kBlock, nullptr),
      CompositorScrollTimeline::ScrollDown);
  EXPECT_EQ(
      ConvertOrientation(ScrollTimeline::ScrollDirection::kInline, nullptr),
      CompositorScrollTimeline::ScrollRight);
}

TEST_F(ScrollTimelineUtilTest, GetCompositorScrollElementIdNullNode) {
  EXPECT_EQ(GetCompositorScrollElementId(nullptr), absl::nullopt);
}

TEST_F(ScrollTimelineUtilTest, GetCompositorScrollElementIdNullLayoutObject) {
  auto* div = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  ASSERT_FALSE(div->GetLayoutObject());
  EXPECT_EQ(GetCompositorScrollElementId(nullptr), absl::nullopt);
}

TEST_F(ScrollTimelineUtilTest, GetCompositorScrollElementIdNoUniqueId) {
  SetBodyInnerHTML("<div id='test'></div>");
  Element* test = GetElementById("test");
  ASSERT_TRUE(test->GetLayoutObject());
  EXPECT_EQ(GetCompositorScrollElementId(test), absl::nullopt);
}

}  // namespace scroll_timeline_util

}  // namespace blink
