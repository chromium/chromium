// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class NGPaintFragmentTest : public RenderingTest,
                            private ScopedLayoutNGForTest {
 public:
  NGPaintFragmentTest(LocalFrameClient* local_frame_client = nullptr)
      : RenderingTest(local_frame_client), ScopedLayoutNGForTest(true) {}

 protected:
  const NGPaintFragment* GetPaintFragmentByElementId(const char* id) {
    const LayoutNGBlockFlow* block_flow =
        ToLayoutNGBlockFlow(GetLayoutObjectByElementId(id));
    return block_flow ? block_flow->PaintFragment() : nullptr;
  }

  const NGPaintFragment& FirstLineBoxByElementId(const char* id) {
    const NGPaintFragment* root = GetPaintFragmentByElementId(id);
    EXPECT_TRUE(root);
    EXPECT_GE(1u, root->Children().size());
    const NGPaintFragment& line_box = *root->FirstChild();
    EXPECT_EQ(NGPhysicalFragment::kFragmentLineBox,
              line_box.PhysicalFragment().Type());
    return line_box;
  }

  Vector<NGPaintFragment*, 16> ToList(
      const NGPaintFragment::ChildList& children) {
    Vector<NGPaintFragment*, 16> list;
    children.ToList(&list);
    return list;
  }
};

TEST_F(NGPaintFragmentTest, InlineFragmentsFor) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    div { font: 10px/1 Ahem; width: 10ch; }
    span { background: yellow; }
    </style>
    <body>
      <div id="container">12345 <span id="box">789 123456789 123<span> 567</div>
    </body>
  )HTML");
  auto* container =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  ASSERT_TRUE(container);
  LayoutObject* text1 = container->FirstChild();
  ASSERT_TRUE(text1 && text1->IsText());
  LayoutObject* box = text1->NextSibling();
  ASSERT_TRUE(box && box->IsLayoutInline());

  Vector<NGPaintFragment*> results;
  auto it = NGPaintFragment::InlineFragmentsFor(text1);
  results.AppendRange(it.begin(), it.end());
  EXPECT_EQ(1u, results.size());
  EXPECT_EQ(text1, results[0]->GetLayoutObject());
  EXPECT_EQ(PhysicalOffset(), results[0]->InlineOffsetToContainerBox());

  results.clear();
  it = NGPaintFragment::InlineFragmentsFor(box);
  results.AppendRange(it.begin(), it.end());
  EXPECT_EQ(3u, results.size());
  EXPECT_EQ(box, results[0]->GetLayoutObject());
  EXPECT_EQ(box, results[1]->GetLayoutObject());
  EXPECT_EQ(box, results[2]->GetLayoutObject());

  EXPECT_EQ(PhysicalOffset(60, 0), results[0]->InlineOffsetToContainerBox());
  EXPECT_EQ("789", To<NGPhysicalTextFragment>(
                       results[0]->FirstChild()->PhysicalFragment())
                       .Text());
  EXPECT_EQ(PhysicalOffset(0, 10), results[1]->InlineOffsetToContainerBox());
  EXPECT_EQ("123456789", To<NGPhysicalTextFragment>(
                             results[1]->FirstChild()->PhysicalFragment())
                             .Text());
  EXPECT_EQ(PhysicalOffset(0, 20), results[2]->InlineOffsetToContainerBox());
  EXPECT_EQ("123", To<NGPhysicalTextFragment>(
                       results[2]->FirstChild()->PhysicalFragment())
                       .Text());
}

#define EXPECT_INK_OVERFLOWS(expected, fragment)         \
  do {                                                   \
    EXPECT_EQ(expected, fragment.InkOverflow());         \
    EXPECT_EQ(expected, fragment.SelfInkOverflow());     \
    EXPECT_EQ(expected, fragment.ContentsInkOverflow()); \
  } while (false)

TEST_F(NGPaintFragmentTest, InlineBox) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    div { font: 10px Ahem; width: 10ch; }
    </style>
    <body>
      <div id="container">12345 <span id="box">XXX YYY</span></div>
    </body>
  )HTML");
  const NGPaintFragment* container = GetPaintFragmentByElementId("container");
  EXPECT_EQ(2u, container->Children().size());
  auto lines = ToList(container->Children());
  const NGPaintFragment& line1 = *lines[0];
  EXPECT_EQ(2u, line1.Children().size());

  // Inline boxes without box decorations (border, background, etc.) do not
  // generate box fragments and that their child fragments are placed directly
  // under the line box.
  auto line1_children = ToList(line1.Children());
  const NGPaintFragment& outer_text = *line1_children[0];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            outer_text.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 60, 10), outer_text);
  EXPECT_EQ(IntRect(0, 0, 60, 10), outer_text.VisualRect());

  const NGPaintFragment& inner_text1 = *line1_children[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inner_text1);
  EXPECT_EQ(IntRect(0, 0, 90, 20), inner_text1.VisualRect());

  const NGPaintFragment& line2 = *lines[1];
  EXPECT_EQ(1u, line2.Children().size());
  const NGPaintFragment& inner_text2 = *line2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inner_text2);
  EXPECT_EQ(IntRect(0, 0, 90, 20), inner_text2.VisualRect());
}

TEST_F(NGPaintFragmentTest, InlineBoxVerticalRL) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    div { font: 10px Ahem; width: 10ch; height: 10ch;
          writing-mode: vertical-rl; }
    </style>
    <body>
      <div id="container">12345 <span id="box">XXX YYY</span></div>
    </body>
  )HTML");
  const NGPaintFragment* container = GetPaintFragmentByElementId("container");
  EXPECT_EQ(2u, container->Children().size());
  auto lines = ToList(container->Children());
  const NGPaintFragment& line1 = *lines[0];
  EXPECT_EQ(2u, line1.Children().size());

  // Inline boxes without box decorations (border, background, etc.) do not
  // generate box fragments and that their child fragments are placed directly
  // under the line box.
  auto line1_children = ToList(line1.Children());
  const NGPaintFragment& outer_text = *line1_children[0];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            outer_text.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 10, 60), outer_text);
  EXPECT_EQ(IntRect(90, 0, 10, 60), outer_text.VisualRect());

  const NGPaintFragment& inner_text1 = *line1_children[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 10, 30), inner_text1);
  EXPECT_EQ(IntRect(80, 0, 20, 90), inner_text1.VisualRect());

  const NGPaintFragment& line2 = *lines[1];
  EXPECT_EQ(1u, line2.Children().size());
  const NGPaintFragment& inner_text2 = *line2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 10, 30), inner_text2);
  EXPECT_EQ(IntRect(80, 0, 20, 90), inner_text2.VisualRect());
}

TEST_F(NGPaintFragmentTest, InlineBoxWithDecorations) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    div { font: 10px Ahem; width: 10ch; }
    #box { background: blue; }
    </style>
    <body>
      <div id="container">12345 <span id="box">XXX YYY<span></div>
    </body>
  )HTML");
  const NGPaintFragment* container = GetPaintFragmentByElementId("container");
  EXPECT_EQ(2u, container->Children().size());
  auto lines = ToList(container->Children());
  const NGPaintFragment& line1 = *lines[0];
  EXPECT_EQ(2u, line1.Children().size());

  auto line1_children = ToList(line1.Children());
  const NGPaintFragment& outer_text = *line1_children[0];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            outer_text.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 60, 10), outer_text);
  EXPECT_EQ(IntRect(0, 0, 60, 10), outer_text.VisualRect());

  // Inline boxes with box decorations generate box fragments.
  const NGPaintFragment& inline_box1 = *line1_children[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inline_box1);
  EXPECT_EQ(IntRect(0, 0, 90, 20), inline_box1.VisualRect());

  EXPECT_EQ(1u, inline_box1.Children().size());
  const NGPaintFragment& inner_text1 = *inline_box1.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inner_text1);
  EXPECT_EQ(IntRect(0, 0, 90, 20), inner_text1.VisualRect());

  const NGPaintFragment& line2 = *lines[1];
  EXPECT_EQ(1u, line2.Children().size());
  const NGPaintFragment& inline_box2 = *line2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inline_box2);
  EXPECT_EQ(IntRect(0, 0, 90, 20), inline_box2.VisualRect());

  const NGPaintFragment& inner_text2 = *inline_box2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inner_text2);
  EXPECT_EQ(IntRect(0, 0, 90, 20), inner_text2.VisualRect());
}

TEST_F(NGPaintFragmentTest, InlineBoxWithDecorationsVerticalRL) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    div { font: 10px Ahem; width: 10ch; height: 10ch;
          writing-mode: vertical-rl; }
    #box { background: blue; }
    </style>
    <body>
      <div id="container">12345 <span id="box">XXX YYY<span></div>
    </body>
  )HTML");
  const NGPaintFragment* container = GetPaintFragmentByElementId("container");
  EXPECT_EQ(2u, container->Children().size());
  auto lines = ToList(container->Children());
  const NGPaintFragment& line1 = *lines[0];
  EXPECT_EQ(2u, line1.Children().size());

  auto line1_children = ToList(line1.Children());
  const NGPaintFragment& outer_text = *line1_children[0];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            outer_text.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 10, 60), outer_text);
  EXPECT_EQ(IntRect(90, 0, 10, 60), outer_text.VisualRect());

  // Inline boxes with box decorations generate box fragments.
  const NGPaintFragment& inline_box1 = *line1_children[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 10, 30), inline_box1);
  EXPECT_EQ(IntRect(80, 0, 20, 90), inline_box1.VisualRect());

  EXPECT_EQ(1u, inline_box1.Children().size());
  const NGPaintFragment& inner_text1 = *inline_box1.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 10, 30), inner_text1);
  EXPECT_EQ(IntRect(80, 0, 20, 90), inner_text1.VisualRect());

  const NGPaintFragment& line2 = *lines[1];
  EXPECT_EQ(1u, line2.Children().size());
  const NGPaintFragment& inline_box2 = *line2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 10, 30), inline_box2);
  EXPECT_EQ(IntRect(80, 0, 20, 90), inline_box2.VisualRect());

  const NGPaintFragment& inner_text2 = *inline_box2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 10, 30), inner_text2);
  EXPECT_EQ(IntRect(80, 0, 20, 90), inner_text2.VisualRect());
}

TEST_F(NGPaintFragmentTest, InlineBlock) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    div { font: 10px Ahem; width: 10ch; }
    /* box-shadow creates asymmetrical visual overflow. */
    span { display: inline-block; box-shadow: 10px 20px black; }
    #box2 { position: relative; top: 10px; width: 6px; height: 6px; }
    /* This div creates asymmetrical contents visual overflow. */
    #box2 div { margin-left: -10px; width: 50px; height: 70px; }
    </style>
    <body>
      <div id="container">12345
        <span id="box1">X</span><span id="box2"><div></div></span></div>
    </body>
  )HTML");

  const NGPaintFragment* container = GetPaintFragmentByElementId("container");
  EXPECT_TRUE(container);
  EXPECT_EQ(1u, container->Children().size());
  const NGPaintFragment& line1 = *container->FirstChild();
  EXPECT_EQ(3u, line1.Children().size());

  // Test the outer text "12345".
  const NGPaintFragment& outer_text = *line1.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            outer_text.PhysicalFragment().Type());
  EXPECT_EQ("12345 ", To<NGPhysicalTextFragment>(outer_text.PhysicalFragment())
                          .Text()
                          .ToString());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 60, 10), outer_text);
  // TODO(kojii): This is still incorrect.
  EXPECT_EQ(IntRect(0, 0, 60, 10), outer_text.VisualRect());

  // Test |InlineFragmentsFor| can find the outer text.
  LayoutObject* layout_outer_text =
      GetLayoutObjectByElementId("container")->SlowFirstChild();
  EXPECT_TRUE(layout_outer_text && layout_outer_text->IsText());
  auto fragments = NGPaintFragment::InlineFragmentsFor(layout_outer_text);
  EXPECT_TRUE(fragments.IsInLayoutNGInlineFormattingContext());
  EXPECT_NE(fragments.begin(), fragments.end());
  EXPECT_EQ(&outer_text, *fragments.begin());

  // Test the inline block "box1".
  const NGPaintFragment& box1 = *ToList(line1.Children())[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox, box1.PhysicalFragment().Type());
  EXPECT_EQ(NGPhysicalFragment::kAtomicInline,
            box1.PhysicalFragment().BoxType());
  EXPECT_EQ(PhysicalRect(0, 0, 20, 30), box1.InkOverflow());
  EXPECT_EQ(PhysicalRect(0, 0, 20, 30), box1.SelfInkOverflow());
  EXPECT_EQ(PhysicalRect(), box1.ContentsInkOverflow());
  EXPECT_EQ(IntRect(60, 0, 20, 30), box1.VisualRect());

  // Test |InlineFragmentsFor| can find "box1".
  LayoutObject* layout_box1 = GetLayoutObjectByElementId("box1");
  EXPECT_TRUE(layout_box1 && layout_box1->IsLayoutBlockFlow());
  fragments = NGPaintFragment::InlineFragmentsFor(layout_box1);
  EXPECT_TRUE(fragments.IsInLayoutNGInlineFormattingContext());
  EXPECT_NE(fragments.begin(), fragments.end());
  EXPECT_EQ(&box1, *fragments.begin());

  // Test an inline block has its own NGPaintFragment.
  const NGPaintFragment* box1_inner = GetPaintFragmentByElementId("box1");
  EXPECT_TRUE(box1_inner);
  EXPECT_EQ(box1_inner->GetLayoutObject(), box1.GetLayoutObject());

  // Test the text fragment inside of the inline block.
  const NGPaintFragment& inner_line_box = *box1_inner->FirstChild();
  const NGPaintFragment& inner_text = *inner_line_box.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 10, 10), inner_text);
  EXPECT_EQ(IntRect(60, 0, 10, 10), inner_text.VisualRect());

  // Test |InlineFragmentsFor| can find the inner text of "box1".
  LayoutObject* layout_inner_text = layout_box1->SlowFirstChild();
  EXPECT_TRUE(layout_inner_text && layout_inner_text->IsText());
  fragments = NGPaintFragment::InlineFragmentsFor(layout_inner_text);
  EXPECT_TRUE(fragments.IsInLayoutNGInlineFormattingContext());
  EXPECT_NE(fragments.begin(), fragments.end());
  EXPECT_EQ(&inner_text, *fragments.begin());

  // Test the inline block "box2".
  const NGPaintFragment& box2 = *ToList(line1.Children())[2];
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox, box2.PhysicalFragment().Type());
  EXPECT_EQ(NGPhysicalFragment::kAtomicInline,
            box2.PhysicalFragment().BoxType());
  EXPECT_EQ(PhysicalRect(-10, 0, 50, 70), box2.InkOverflow());
  EXPECT_EQ(PhysicalRect(0, 0, 16, 26), box2.SelfInkOverflow());
  EXPECT_EQ(PhysicalRect(-10, 0, 50, 70), box2.ContentsInkOverflow());
  // The extra 2 px vertical offset is because the 6px height box is placed
  // vertically center in 10px height line box.
  EXPECT_EQ(IntRect(70, 12, 16, 26), box2.VisualRect());

  GetDocument().GetFrame()->Selection().SelectAll();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(IntRect(0, 0, 60, 10), outer_text.VisualRect());
  EXPECT_EQ(IntRect(60, 0, 20, 30), box1.VisualRect());
  EXPECT_EQ(IntRect(60, 0, 10, 10), inner_text.VisualRect());
  EXPECT_EQ(IntRect(70, 12, 16, 26), box2.VisualRect());
}

TEST_F(NGPaintFragmentTest, InlineBlockVerticalRL) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    div { font: 10px Ahem; width: 10ch; height: 10ch;
          writing-mode: vertical-rl; }
    /* box-shadow creates asymmetrical visual overflow. */
    span { display: inline-block; box-shadow: 10px 20px black; }
    #box2 { position: relative; top: 10px; width: 6px; height: 6px; }
    /* This div creates asymmetrical contents visual overflow. */
    #box2 div { margin-top: -10px; width: 50px; height: 70px; }
    </style>
    <body>
      <div id="container">12345
        <span id="box1">X</span><span id="box2"><div></div></span></div>
    </body>
  )HTML");

  const NGPaintFragment* container = GetPaintFragmentByElementId("container");
  EXPECT_TRUE(container);
  EXPECT_EQ(1u, container->Children().size());
  const NGPaintFragment& line1 = *container->FirstChild();
  EXPECT_EQ(3u, line1.Children().size());

  // Test the outer text "12345".
  const NGPaintFragment& outer_text = *line1.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            outer_text.PhysicalFragment().Type());
  EXPECT_EQ("12345 ", To<NGPhysicalTextFragment>(outer_text.PhysicalFragment())
                          .Text()
                          .ToString());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 10, 60), outer_text);
  // TODO(kojii): This is still incorrect.
  EXPECT_EQ(IntRect(90, 0, 10, 60), outer_text.VisualRect());

  // Test |InlineFragmentsFor| can find the outer text.
  LayoutObject* layout_outer_text =
      GetLayoutObjectByElementId("container")->SlowFirstChild();
  EXPECT_TRUE(layout_outer_text && layout_outer_text->IsText());
  auto fragments = NGPaintFragment::InlineFragmentsFor(layout_outer_text);
  EXPECT_TRUE(fragments.IsInLayoutNGInlineFormattingContext());
  EXPECT_NE(fragments.begin(), fragments.end());
  EXPECT_EQ(&outer_text, *fragments.begin());

  // Test the inline block "box1".
  const NGPaintFragment& box1 = *ToList(line1.Children())[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox, box1.PhysicalFragment().Type());
  EXPECT_EQ(NGPhysicalFragment::kAtomicInline,
            box1.PhysicalFragment().BoxType());
  EXPECT_EQ(PhysicalRect(0, 0, 20, 30), box1.InkOverflow());
  EXPECT_EQ(PhysicalRect(0, 0, 20, 30), box1.SelfInkOverflow());
  EXPECT_TRUE(box1.ContentsInkOverflow().IsEmpty());
  EXPECT_EQ(IntRect(90, 60, 20, 30), box1.VisualRect());

  // Test |InlineFragmentsFor| can find "box1".
  LayoutObject* layout_box1 = GetLayoutObjectByElementId("box1");
  EXPECT_TRUE(layout_box1 && layout_box1->IsLayoutBlockFlow());
  fragments = NGPaintFragment::InlineFragmentsFor(layout_box1);
  EXPECT_TRUE(fragments.IsInLayoutNGInlineFormattingContext());
  EXPECT_NE(fragments.begin(), fragments.end());
  EXPECT_EQ(&box1, *fragments.begin());

  // Test an inline block has its own NGPaintFragment.
  const NGPaintFragment* box1_inner = GetPaintFragmentByElementId("box1");
  EXPECT_TRUE(box1_inner);
  EXPECT_EQ(box1_inner->GetLayoutObject(), box1.GetLayoutObject());

  // Test the text fragment inside of the inline block.
  const NGPaintFragment& inner_line_box = *box1_inner->FirstChild();
  const NGPaintFragment& inner_text = *inner_line_box.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 10, 10), inner_text);
  EXPECT_EQ(IntRect(90, 60, 10, 10), inner_text.VisualRect());

  // Test |InlineFragmentsFor| can find the inner text of "box1".
  LayoutObject* layout_inner_text = layout_box1->SlowFirstChild();
  EXPECT_TRUE(layout_inner_text && layout_inner_text->IsText());
  fragments = NGPaintFragment::InlineFragmentsFor(layout_inner_text);
  EXPECT_TRUE(fragments.IsInLayoutNGInlineFormattingContext());
  EXPECT_NE(fragments.begin(), fragments.end());
  EXPECT_EQ(&inner_text, *fragments.begin());

  // Test the inline block "box2".
  const NGPaintFragment& box2 = *ToList(line1.Children())[2];
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox, box2.PhysicalFragment().Type());
  EXPECT_EQ(NGPhysicalFragment::kAtomicInline,
            box2.PhysicalFragment().BoxType());
  // -44 is because 50px child overflows out of the left edge of the 6px box.
  // 60 width covers both the overflowing contents and the box shadow.
  EXPECT_EQ(PhysicalRect(-44, -10, 60, 70), box2.InkOverflow());
  EXPECT_EQ(PhysicalRect(0, 0, 16, 26), box2.SelfInkOverflow());
  EXPECT_EQ(PhysicalRect(-44, -10, 50, 70), box2.ContentsInkOverflow());
  // The extra 2 px horizontal offset is because the 6px width box is placed
  // horizontally center in 10px width vertical line box.
  EXPECT_EQ(IntRect(92, 80, 16, 26), box2.VisualRect());

  GetDocument().GetFrame()->Selection().SelectAll();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(IntRect(90, 0, 10, 60), outer_text.VisualRect());
  EXPECT_EQ(IntRect(90, 60, 20, 30), box1.VisualRect());
  EXPECT_EQ(IntRect(90, 60, 10, 10), inner_text.VisualRect());
  EXPECT_EQ(IntRect(92, 80, 16, 26), box2.VisualRect());
}

TEST_F(NGPaintFragmentTest, RelativeBlock) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    div { font: 10px Ahem; width: 10ch; }
    #container { position: relative; top: 10px; }
    </style>
    <body>
      <div id="container">12345 <span id="box">XXX YYY</span></div>
    </body>
  )HTML");
  const NGPaintFragment* container = GetPaintFragmentByElementId("container");
  EXPECT_EQ(2u, container->Children().size());
  const NGPaintFragment& line1 = *container->FirstChild();
  EXPECT_EQ(2u, line1.Children().size());

  const NGPaintFragment& outer_text = *line1.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            outer_text.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 60, 10), outer_text);
  EXPECT_EQ(IntRect(0, 10, 60, 10), outer_text.VisualRect());

  const NGPaintFragment& inner_text1 = *ToList(line1.Children())[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inner_text1);
  EXPECT_EQ(IntRect(0, 10, 90, 20), inner_text1.VisualRect());

  const NGPaintFragment& line2 = *ToList(container->Children())[1];
  EXPECT_EQ(1u, line2.Children().size());
  const NGPaintFragment& inner_text2 = *line2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inner_text2);
  EXPECT_EQ(IntRect(0, 10, 90, 20), inner_text2.VisualRect());
}

TEST_F(NGPaintFragmentTest, RelativeInline) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    div { font: 10px Ahem; width: 10ch; }
    #box { position: relative; top: 10px; }
    </style>
    <body>
      <div id="container">12345 <span id="box">XXX YYY<span></div>
    </body>
  )HTML");
  const NGPaintFragment* container = GetPaintFragmentByElementId("container");
  EXPECT_EQ(2u, container->Children().size());
  auto lines = ToList(container->Children());
  const NGPaintFragment& line1 = *lines[0];
  EXPECT_EQ(2u, line1.Children().size());

  auto line1_children = ToList(line1.Children());
  const NGPaintFragment& outer_text = *line1_children[0];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            outer_text.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 60, 10), outer_text);
  EXPECT_EQ(IntRect(0, 0, 60, 10), outer_text.VisualRect());

  const NGPaintFragment& inline_box1 = *line1_children[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inline_box1);
  EXPECT_EQ(IntRect(0, 10, 90, 20), inline_box1.VisualRect());

  EXPECT_EQ(1u, inline_box1.Children().size());
  const NGPaintFragment& inner_text1 = *inline_box1.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inner_text1);
  EXPECT_EQ(IntRect(0, 10, 90, 20), inner_text1.VisualRect());

  const NGPaintFragment& line2 = *lines[1];
  EXPECT_EQ(1u, line2.Children().size());
  const NGPaintFragment& inline_box2 = *line2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inline_box2);
  EXPECT_EQ(IntRect(0, 10, 90, 20), inline_box2.VisualRect());

  const NGPaintFragment& inner_text2 = *inline_box2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inner_text2);
  EXPECT_EQ(IntRect(0, 10, 90, 20), inner_text2.VisualRect());
}

TEST_F(NGPaintFragmentTest, RelativeBlockAndInline) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    div { font: 10px Ahem; width: 10ch; }
    #container, #box { position: relative; top: 10px; }
    </style>
    <body>
      <div id="container">12345 <span id="box">XXX YYY<span></div>
    </body>
  )HTML");
  const NGPaintFragment* container = GetPaintFragmentByElementId("container");
  EXPECT_EQ(2u, container->Children().size());
  auto lines = ToList(container->Children());
  const NGPaintFragment& line1 = *lines[0];
  EXPECT_EQ(2u, line1.Children().size());

  auto line1_children = ToList(line1.Children());
  const NGPaintFragment& outer_text = *line1_children[0];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            outer_text.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 60, 10), outer_text);
  EXPECT_EQ(IntRect(0, 10, 60, 10), outer_text.VisualRect());

  const NGPaintFragment& inline_box1 = *line1_children[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inline_box1);
  EXPECT_EQ(IntRect(0, 20, 90, 20), inline_box1.VisualRect());

  EXPECT_EQ(1u, inline_box1.Children().size());
  const NGPaintFragment& inner_text1 = *inline_box1.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inner_text1);
  EXPECT_EQ(IntRect(0, 20, 90, 20), inner_text1.VisualRect());

  const NGPaintFragment& line2 = *lines[1];
  EXPECT_EQ(1u, line2.Children().size());
  const NGPaintFragment& inline_box2 = *line2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inline_box2);
  EXPECT_EQ(IntRect(0, 20, 90, 20), inline_box2.VisualRect());

  const NGPaintFragment& inner_text2 = *inline_box2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inner_text2);
  EXPECT_EQ(IntRect(0, 20, 90, 20), inner_text2.VisualRect());
}

// Test that OOF should not create a NGPaintFragment.
TEST_F(NGPaintFragmentTest, OutOfFlow) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    div {
      position: relative;
    }
    span {
      position: absolute;
    }
    </style>
    <body>
      <div id="container">
        text
        <span>XXX</span>
      </div>
    </body>
  )HTML");
  const NGPaintFragment* container = GetPaintFragmentByElementId("container");
  EXPECT_EQ(1u, container->Children().size());
  auto lines = ToList(container->Children());
  EXPECT_EQ(1u, lines[0]->Children().size());
}

TEST_F(NGPaintFragmentTest, MarkLineBoxesDirtyByRemoveBr) {
  if (!RuntimeEnabledFeatures::LayoutNGLineCacheEnabled())
    return;
  SetBodyInnerHTML(
      "<div id=container>line 1<br>line 2<br id=target>line 3<br>"
      "</div>");
  Element& target = *GetDocument().getElementById("target");
  target.remove();
  const NGPaintFragment& container = *GetPaintFragmentByElementId("container");
  EXPECT_FALSE(container.FirstChild()->IsDirty());
  EXPECT_TRUE(ToList(container.Children())[1]->IsDirty());
  EXPECT_FALSE(ToList(container.Children())[2]->IsDirty());
}

static const char* inline_child_data[] = {
    "<span id='child'>XXX</span>",
    "<span id='child' style='background: yellow'>XXX</span>",
    "<span id='child' style='display: inline-block'>XXX</span>",
};

class InlineChildTest : public NGPaintFragmentTest,
                        public testing::WithParamInterface<const char*> {};

INSTANTIATE_TEST_SUITE_P(NGPaintFragmentTest,
                         InlineChildTest,
                         testing::ValuesIn(inline_child_data));

TEST_P(InlineChildTest, RemoveInlineChild) {
  SetBodyInnerHTML(String(R"HTML(
    <!DOCTYPE html>
    <style>
    </style>
    <body>
      <div id="container">
        12345
        )HTML") + GetParam() +
                   R"HTML(
        67890
      </div>
    </body>
  )HTML");
  const NGPaintFragment* container = GetPaintFragmentByElementId("container");
  const NGPaintFragment& linebox = container->Children().front();
  EXPECT_EQ(linebox.Children().size(), 3u);

  Element* child = GetElementById("child");
  child->remove();

  // Destroyed children should be eliminated immediately.
  EXPECT_EQ(linebox.Children().size(), 2u);
}

TEST_F(NGPaintFragmentTest, MarkLineBoxesDirtyByRemoveChild) {
  if (!RuntimeEnabledFeatures::LayoutNGLineCacheEnabled())
    return;
  SetBodyInnerHTML(
      "<div id=container>line 1<br><b id=target>line 2</b><br>line 3<br>"
      "</div>");
  Element& target = *GetDocument().getElementById("target");
  target.remove();
  const NGPaintFragment& container = *GetPaintFragmentByElementId("container");
  auto lines = ToList(container.Children());
  EXPECT_TRUE(lines[0]->IsDirty());
  EXPECT_FALSE(lines[1]->IsDirty());
  EXPECT_FALSE(lines[2]->IsDirty());
}

TEST_F(NGPaintFragmentTest, MarkLineBoxesDirtyByRemoveSpanWithBr) {
  if (!RuntimeEnabledFeatures::LayoutNGLineCacheEnabled())
    return;
  SetBodyInnerHTML(
      "<div id=container>line 1<br>line 2<span id=target><br></span>line 3<br>"
      "</div>");
  // |target| is a culled inline box. There is no fragment in fragment tree.
  Element& target = *GetDocument().getElementById("target");
  target.remove();
  const NGPaintFragment& container = *GetPaintFragmentByElementId("container");
  EXPECT_FALSE(container.FirstChild()->IsDirty());
  EXPECT_TRUE(ToList(container.Children())[1]->IsDirty());
  EXPECT_FALSE(ToList(container.Children())[2]->IsDirty());
}

// "ByInsert" tests are disabled, because they require |UpdateStyleAndLayout()|
// to update |IsDirty|, but NGPaintFragment maybe re-used during the layout. In
// such case, the result is not deterministic.
TEST_F(NGPaintFragmentTest, DISABLED_MarkLineBoxesDirtyByInsertAtStart) {
  if (!RuntimeEnabledFeatures::LayoutNGLineCacheEnabled())
    return;
  SetBodyInnerHTML(
      "<div id=container>line 1<br><b id=target>line 2</b><br>line 3<br>"
      "</div>");
  const NGPaintFragment& container = *GetPaintFragmentByElementId("container");
  const scoped_refptr<const NGPaintFragment> line1 = container.FirstChild();
  ASSERT_TRUE(line1->PhysicalFragment().IsLineBox()) << line1;
  const scoped_refptr<const NGPaintFragment> line2 =
      ToList(container.Children())[1];
  ASSERT_TRUE(line2->PhysicalFragment().IsLineBox()) << line2;
  const scoped_refptr<const NGPaintFragment> line3 =
      ToList(container.Children())[2];
  ASSERT_TRUE(line3->PhysicalFragment().IsLineBox()) << line3;
  Element& target = *GetDocument().getElementById("target");
  target.parentNode()->insertBefore(Text::Create(GetDocument(), "XYZ"),
                                    &target);
  GetDocument().UpdateStyleAndLayout();

  EXPECT_TRUE(line1->IsDirty());
  EXPECT_FALSE(line2->IsDirty());
  EXPECT_FALSE(line3->IsDirty());
}

// "ByInsert" tests are disabled, because they require |UpdateStyleAndLayout()|
// to update |IsDirty|, but NGPaintFragment maybe re-used during the layout. In
// such case, the result is not deterministic.
TEST_F(NGPaintFragmentTest, DISABLED_MarkLineBoxesDirtyByInsertAtLast) {
  if (!RuntimeEnabledFeatures::LayoutNGLineCacheEnabled())
    return;
  SetBodyInnerHTML(
      "<div id=container>line 1<br><b id=target>line 2</b><br>line 3<br>"
      "</div>");
  const NGPaintFragment& container = *GetPaintFragmentByElementId("container");
  const scoped_refptr<const NGPaintFragment> line1 = container.FirstChild();
  ASSERT_TRUE(line1->PhysicalFragment().IsLineBox()) << line1;
  const scoped_refptr<const NGPaintFragment> line2 =
      ToList(container.Children())[1];
  ASSERT_TRUE(line2->PhysicalFragment().IsLineBox()) << line2;
  const scoped_refptr<const NGPaintFragment> line3 =
      ToList(container.Children())[2];
  ASSERT_TRUE(line3->PhysicalFragment().IsLineBox()) << line3;
  Element& target = *GetDocument().getElementById("target");
  target.parentNode()->appendChild(Text::Create(GetDocument(), "XYZ"));
  GetDocument().UpdateStyleAndLayout();

  EXPECT_FALSE(line1->IsDirty());
  EXPECT_FALSE(line2->IsDirty());
  EXPECT_TRUE(line3->IsDirty());
}

// "ByInsert" tests are disabled, because they require |UpdateStyleAndLayout()|
// to update |IsDirty|, but NGPaintFragment maybe re-used during the layout. In
// such case, the result is not deterministic.
TEST_F(NGPaintFragmentTest, DISABLED_MarkLineBoxesDirtyByInsertAtMiddle) {
  if (!RuntimeEnabledFeatures::LayoutNGLineCacheEnabled())
    return;
  SetBodyInnerHTML(
      "<div id=container>line 1<br><b id=target>line 2</b><br>line 3<br>"
      "</div>");
  const NGPaintFragment& container = *GetPaintFragmentByElementId("container");
  const scoped_refptr<const NGPaintFragment> line1 = container.FirstChild();
  ASSERT_TRUE(line1->PhysicalFragment().IsLineBox()) << line1;
  const scoped_refptr<const NGPaintFragment> line2 =
      ToList(container.Children())[1];
  ASSERT_TRUE(line2->PhysicalFragment().IsLineBox()) << line2;
  const scoped_refptr<const NGPaintFragment> line3 =
      ToList(container.Children())[2];
  ASSERT_TRUE(line3->PhysicalFragment().IsLineBox()) << line3;
  Element& target = *GetDocument().getElementById("target");
  target.parentNode()->insertBefore(Text::Create(GetDocument(), "XYZ"),
                                    target.nextSibling());
  GetDocument().UpdateStyleAndLayout();

  EXPECT_TRUE(line1->IsDirty());
  EXPECT_FALSE(line2->IsDirty());
  EXPECT_FALSE(line3->IsDirty());
}

TEST_F(NGPaintFragmentTest, MarkLineBoxesDirtyByTextSetData) {
  if (!RuntimeEnabledFeatures::LayoutNGLineCacheEnabled())
    return;
  SetBodyInnerHTML(
      "<div id=container>line 1<br><b id=target>line 2</b><br>line "
      "3<br></div>");
  Element& target = *GetDocument().getElementById("target");
  To<Text>(*target.firstChild()).setData("abc");
  const NGPaintFragment& container = *GetPaintFragmentByElementId("container");
  auto lines = ToList(container.Children());
  // TODO(kojii): Currently we don't optimzie for <br>. We can do this, then
  // lines[0] should not be dirty.
  EXPECT_TRUE(lines[0]->IsDirty());
}

TEST_F(NGPaintFragmentTest, MarkLineBoxesDirtyWrappedLine) {
  if (!RuntimeEnabledFeatures::LayoutNGLineCacheEnabled())
    return;
  SetBodyInnerHTML(R"HTML(
    <style>
    #container {
      font-size: 10px;
      width: 10ch;
    }
    </style>
    <div id=container>
      1234567
      123456<span id="target">7</span>
    </div>)HTML");
  Element& target = *GetDocument().getElementById("target");
  target.remove();

  const NGPaintFragment& container = *GetPaintFragmentByElementId("container");
  const NGPaintFragment& line0 = *container.FirstChild();
  const NGPaintFragment& line1 = *line0.NextSibling();
  EXPECT_FALSE(line0.IsDirty());
  EXPECT_TRUE(line1.IsDirty());
}

TEST_F(NGPaintFragmentTest, MarkLineBoxesDirtyInsideInlineBlock) {
  if (!RuntimeEnabledFeatures::LayoutNGLineCacheEnabled())
    return;
  SetBodyInnerHTML(R"HTML(
    <div id=container>
      <div id="inline-block" style="display: inline-block">
        <span id="target">DELETE ME</span>
      </div>
    </div>)HTML");
  Element& target = *GetDocument().getElementById("target");
  target.remove();

  const NGPaintFragment& container = *GetPaintFragmentByElementId("container");
  const NGPaintFragment& line0 = *container.FirstChild();
  EXPECT_FALSE(line0.IsDirty());

  const NGPaintFragment& inline_block =
      *GetPaintFragmentByElementId("inline-block");
  const NGPaintFragment& inner_line0 = *inline_block.FirstChild();
  EXPECT_TRUE(inner_line0.IsDirty());
}

}  // namespace blink
