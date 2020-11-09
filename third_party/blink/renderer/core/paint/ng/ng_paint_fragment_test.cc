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
    const auto* block_flow =
        To<LayoutNGBlockFlow>(GetLayoutObjectByElementId(id));
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
  if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())
    return;
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
  EXPECT_EQ(PhysicalOffset(), results[0]->OffsetInContainerBlock());

  results.clear();
  it = NGPaintFragment::InlineFragmentsFor(box);
  results.AppendRange(it.begin(), it.end());
  EXPECT_EQ(3u, results.size());
  EXPECT_EQ(box, results[0]->GetLayoutObject());
  EXPECT_EQ(box, results[1]->GetLayoutObject());
  EXPECT_EQ(box, results[2]->GetLayoutObject());

  EXPECT_EQ(PhysicalOffset(60, 0), results[0]->OffsetInContainerBlock());
  EXPECT_EQ("789", To<NGPhysicalTextFragment>(
                       results[0]->FirstChild()->PhysicalFragment())
                       .Text());
  EXPECT_EQ(PhysicalOffset(0, 10), results[1]->OffsetInContainerBlock());
  EXPECT_EQ("123456789", To<NGPhysicalTextFragment>(
                             results[1]->FirstChild()->PhysicalFragment())
                             .Text());
  EXPECT_EQ(PhysicalOffset(0, 20), results[2]->OffsetInContainerBlock());
  EXPECT_EQ("123", To<NGPhysicalTextFragment>(
                       results[2]->FirstChild()->PhysicalFragment())
                       .Text());
}

#define EXPECT_INK_OVERFLOWS(expected, fragment)         \
  do {                                                   \
    EXPECT_EQ(expected, fragment.InkOverflow());         \
    EXPECT_EQ(expected, fragment.SelfInkOverflow());     \
  } while (false)

TEST_F(NGPaintFragmentTest, InlineBox) {
  if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())
    return;
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

  const NGPaintFragment& inner_text1 = *line1_children[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inner_text1);

  const NGPaintFragment& line2 = *lines[1];
  EXPECT_EQ(1u, line2.Children().size());
  const NGPaintFragment& inner_text2 = *line2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inner_text2);
}

TEST_F(NGPaintFragmentTest, InlineBoxVerticalRL) {
  if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())
    return;
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

  const NGPaintFragment& inner_text1 = *line1_children[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 10, 30), inner_text1);

  const NGPaintFragment& line2 = *lines[1];
  EXPECT_EQ(1u, line2.Children().size());
  const NGPaintFragment& inner_text2 = *line2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 10, 30), inner_text2);
}

TEST_F(NGPaintFragmentTest, InlineBoxWithDecorations) {
  if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())
    return;
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

  // Inline boxes with box decorations generate box fragments.
  const NGPaintFragment& inline_box1 = *line1_children[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inline_box1);

  EXPECT_EQ(1u, inline_box1.Children().size());
  const NGPaintFragment& inner_text1 = *inline_box1.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inner_text1);

  const NGPaintFragment& line2 = *lines[1];
  EXPECT_EQ(1u, line2.Children().size());
  const NGPaintFragment& inline_box2 = *line2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inline_box2);

  const NGPaintFragment& inner_text2 = *inline_box2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inner_text2);
}

TEST_F(NGPaintFragmentTest, InlineBoxWithDecorationsVerticalRL) {
  if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())
    return;
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

  // Inline boxes with box decorations generate box fragments.
  const NGPaintFragment& inline_box1 = *line1_children[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 10, 30), inline_box1);

  EXPECT_EQ(1u, inline_box1.Children().size());
  const NGPaintFragment& inner_text1 = *inline_box1.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 10, 30), inner_text1);

  const NGPaintFragment& line2 = *lines[1];
  EXPECT_EQ(1u, line2.Children().size());
  const NGPaintFragment& inline_box2 = *line2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 10, 30), inline_box2);

  const NGPaintFragment& inner_text2 = *inline_box2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 10, 30), inner_text2);
}

TEST_F(NGPaintFragmentTest, InlineBlock) {
  if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())
    return;
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
}

TEST_F(NGPaintFragmentTest, InlineBlockVerticalRL) {
  if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())
    return;
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
}

TEST_F(NGPaintFragmentTest, RelativeBlock) {
  if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())
    return;
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

  const NGPaintFragment& inner_text1 = *ToList(line1.Children())[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inner_text1);

  const NGPaintFragment& line2 = *ToList(container->Children())[1];
  EXPECT_EQ(1u, line2.Children().size());
  const NGPaintFragment& inner_text2 = *line2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inner_text2);
}

TEST_F(NGPaintFragmentTest, RelativeInline) {
  if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())
    return;
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

  const NGPaintFragment& inline_box1 = *line1_children[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inline_box1);

  EXPECT_EQ(1u, inline_box1.Children().size());
  const NGPaintFragment& inner_text1 = *inline_box1.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inner_text1);

  const NGPaintFragment& line2 = *lines[1];
  EXPECT_EQ(1u, line2.Children().size());
  const NGPaintFragment& inline_box2 = *line2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inline_box2);

  const NGPaintFragment& inner_text2 = *inline_box2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inner_text2);
}

TEST_F(NGPaintFragmentTest, RelativeBlockAndInline) {
  if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())
    return;
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

  const NGPaintFragment& inline_box1 = *line1_children[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inline_box1);

  EXPECT_EQ(1u, inline_box1.Children().size());
  const NGPaintFragment& inner_text1 = *inline_box1.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text1.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inner_text1);

  const NGPaintFragment& line2 = *lines[1];
  EXPECT_EQ(1u, line2.Children().size());
  const NGPaintFragment& inline_box2 = *line2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inline_box2);

  const NGPaintFragment& inner_text2 = *inline_box2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text2.PhysicalFragment().Type());
  EXPECT_INK_OVERFLOWS(PhysicalRect(0, 0, 30, 10), inner_text2);
}

// Test that OOF should not create a NGPaintFragment.
TEST_F(NGPaintFragmentTest, OutOfFlow) {
  if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())
    return;
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
  if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())
    return;
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

}  // namespace blink
