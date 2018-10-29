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
  LayoutBlockFlow* container =
      ToLayoutBlockFlow(GetLayoutObjectByElementId("container"));
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
  EXPECT_EQ(NGPhysicalOffset(), results[0]->InlineOffsetToContainerBox());

  results.clear();
  it = NGPaintFragment::InlineFragmentsFor(box);
  results.AppendRange(it.begin(), it.end());
  EXPECT_EQ(3u, results.size());
  EXPECT_EQ(box, results[0]->GetLayoutObject());
  EXPECT_EQ(box, results[1]->GetLayoutObject());
  EXPECT_EQ(box, results[2]->GetLayoutObject());

  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(60), LayoutUnit()),
            results[0]->InlineOffsetToContainerBox());
  EXPECT_EQ("789", ToNGPhysicalTextFragment(
                       results[0]->FirstChild()->PhysicalFragment())
                       .Text());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit(10)),
            results[1]->InlineOffsetToContainerBox());
  EXPECT_EQ("123456789", ToNGPhysicalTextFragment(
                             results[1]->FirstChild()->PhysicalFragment())
                             .Text());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(), LayoutUnit(20)),
            results[2]->InlineOffsetToContainerBox());
  EXPECT_EQ("123", ToNGPhysicalTextFragment(
                       results[2]->FirstChild()->PhysicalFragment())
                       .Text());
}

TEST_F(NGPaintFragmentTest, InlineBox) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    div { font: 10px Ahem; width: 10ch; }
    </style>
    <body>
      <div id="container">12345 <span id="box">XXX YYY<span></div>
    </body>
  )HTML");
  const NGPaintFragment* container = GetPaintFragmentByElementId("container");
  EXPECT_EQ(2u, container->Children().size());
  const NGPaintFragment& line1 = *container->FirstChild();
  EXPECT_EQ(2u, line1.Children().size());

  // Inline boxes without box decorations (border, background, etc.) do not
  // generate box fragments and that their child fragments are placed directly
  // under the line box.
  const NGPaintFragment& outer_text = *line1.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            outer_text.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(0, 0, 60, 10), outer_text.VisualRect());

  const NGPaintFragment& inner_text1 = *ToList(line1.Children())[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text1.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(60, 0, 30, 10), inner_text1.VisualRect());

  const NGPaintFragment& line2 = *ToList(container->Children())[1];
  EXPECT_EQ(1u, line2.Children().size());
  const NGPaintFragment& inner_text2 = *line2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text2.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(0, 10, 30, 10), inner_text2.VisualRect());
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
  const NGPaintFragment& line1 = *container->FirstChild();
  EXPECT_EQ(2u, line1.Children().size());

  const NGPaintFragment& outer_text = *line1.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            outer_text.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(0, 0, 60, 10), outer_text.VisualRect());

  // Inline boxes with box decorations generate box fragments.
  const NGPaintFragment& inline_box1 = *ToList(line1.Children())[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box1.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(60, 0, 30, 10), inline_box1.VisualRect());

  EXPECT_EQ(1u, inline_box1.Children().size());
  const NGPaintFragment& inner_text1 = *inline_box1.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text1.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(60, 0, 30, 10), inner_text1.VisualRect());

  const NGPaintFragment& line2 = *ToList(container->Children())[1];
  EXPECT_EQ(1u, line2.Children().size());
  const NGPaintFragment& inline_box2 = *line2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box2.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(0, 10, 30, 10), inline_box2.VisualRect());

  const NGPaintFragment& inner_text2 = *inline_box2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text2.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(0, 10, 30, 10), inner_text2.VisualRect());
}

TEST_F(NGPaintFragmentTest, InlineBlock) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    div { font: 10px Ahem; width: 10ch; }
    span { display: inline-block; }
    #box2 { position: relative; top: 10px; }
    </style>
    <body>
      <div id="container">12345
        <span id="box1">X</span><span id="box2">Y</span></div>
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
  EXPECT_EQ("12345 ", ToNGPhysicalTextFragment(outer_text.PhysicalFragment())
                          .Text()
                          .ToString());
  // TODO(kojii): This is still incorrect.
  EXPECT_EQ(LayoutRect(0, 0, 60, 10), outer_text.VisualRect());
  EXPECT_EQ(LayoutRect(), outer_text.SelectionVisualRect());

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
  EXPECT_EQ(LayoutRect(60, 0, 10, 10), box1.VisualRect());
  EXPECT_EQ(LayoutRect(), box1.SelectionVisualRect());

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
  EXPECT_EQ(LayoutRect(60, 0, 10, 10), inner_text.VisualRect());
  EXPECT_EQ(LayoutRect(), inner_text.SelectionVisualRect());

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
  EXPECT_EQ(LayoutRect(70, 10, 10, 10), box2.VisualRect());
  EXPECT_EQ(LayoutRect(), box2.SelectionVisualRect());

  GetDocument().GetFrame()->Selection().SelectAll();
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(LayoutRect(0, 0, 60, 10), outer_text.VisualRect());
  EXPECT_EQ(LayoutRect(0, 0, 60, 10), outer_text.SelectionVisualRect());
  EXPECT_EQ(LayoutRect(60, 0, 10, 10), box1.VisualRect());
  EXPECT_EQ(LayoutRect(), box1.SelectionVisualRect());
  EXPECT_EQ(LayoutRect(60, 0, 10, 10), inner_text.VisualRect());
  EXPECT_EQ(LayoutRect(60, 0, 10, 10), inner_text.SelectionVisualRect());
  EXPECT_EQ(LayoutRect(70, 10, 10, 10), box2.VisualRect());
  EXPECT_EQ(LayoutRect(), box2.SelectionVisualRect());
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
      <div id="container">12345 <span id="box">XXX YYY<span></div>
    </body>
  )HTML");
  const NGPaintFragment* container = GetPaintFragmentByElementId("container");
  EXPECT_EQ(2u, container->Children().size());
  const NGPaintFragment& line1 = *container->FirstChild();
  EXPECT_EQ(2u, line1.Children().size());

  const NGPaintFragment& outer_text = *line1.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            outer_text.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(0, 10, 60, 10), outer_text.VisualRect());

  const NGPaintFragment& inner_text1 = *ToList(line1.Children())[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text1.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(60, 10, 30, 10), inner_text1.VisualRect());

  const NGPaintFragment& line2 = *ToList(container->Children())[1];
  EXPECT_EQ(1u, line2.Children().size());
  const NGPaintFragment& inner_text2 = *line2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text2.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(0, 20, 30, 10), inner_text2.VisualRect());
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
  const NGPaintFragment& line1 = *container->FirstChild();
  EXPECT_EQ(2u, line1.Children().size());

  const NGPaintFragment& outer_text = *line1.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            outer_text.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(0, 0, 60, 10), outer_text.VisualRect());

  const NGPaintFragment& inline_box1 = *ToList(line1.Children())[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box1.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(60, 10, 30, 10), inline_box1.VisualRect());

  EXPECT_EQ(1u, inline_box1.Children().size());
  const NGPaintFragment& inner_text1 = *inline_box1.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text1.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(60, 10, 30, 10), inner_text1.VisualRect());

  const NGPaintFragment& line2 = *ToList(container->Children())[1];
  EXPECT_EQ(1u, line2.Children().size());
  const NGPaintFragment& inline_box2 = *line2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box2.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(0, 20, 30, 10), inline_box2.VisualRect());

  const NGPaintFragment& inner_text2 = *inline_box2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text2.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(0, 20, 30, 10), inner_text2.VisualRect());
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
  const NGPaintFragment& line1 = *container->FirstChild();
  EXPECT_EQ(2u, line1.Children().size());

  const NGPaintFragment& outer_text = *line1.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            outer_text.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(0, 10, 60, 10), outer_text.VisualRect());

  const NGPaintFragment& inline_box1 = *ToList(line1.Children())[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box1.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(60, 20, 30, 10), inline_box1.VisualRect());

  EXPECT_EQ(1u, inline_box1.Children().size());
  const NGPaintFragment& inner_text1 = *inline_box1.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text1.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(60, 20, 30, 10), inner_text1.VisualRect());

  const NGPaintFragment& line2 = *ToList(container->Children())[1];
  EXPECT_EQ(1u, line2.Children().size());
  const NGPaintFragment& inline_box2 = *line2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox,
            inline_box2.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(0, 30, 30, 10), inline_box2.VisualRect());

  const NGPaintFragment& inner_text2 = *inline_box2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText,
            inner_text2.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(0, 30, 30, 10), inner_text2.VisualRect());
}

TEST_F(NGPaintFragmentTest, FlippedBlock) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    div {
      writing-mode: vertical-rl;
      font: 10px Ahem;
      width: 20em;
      height: 10em;
    }
    span { background: yellow; }
    </style>
    <body>
      <div id="container">1234567890
        pppp<span>XXX</span></div>
    </body>
  )HTML");
  const NGPaintFragment* container = GetPaintFragmentByElementId("container");
  EXPECT_EQ(2u, container->Children().size());
  const NGPaintFragment& line1 = *container->FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentLineBox,
            line1.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(190, 0, 10, 100), line1.VisualRect());
  EXPECT_EQ(1u, line1.Children().size());

  const NGPaintFragment& text1 = *line1.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText, text1.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(190, 0, 10, 100), text1.VisualRect());

  const NGPaintFragment& line2 = *ToList(container->Children())[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentLineBox,
            line2.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(180, 0, 10, 70), line2.VisualRect());
  EXPECT_EQ(2u, line2.Children().size());

  const NGPaintFragment& text2 = *line2.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText, text2.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(180, 0, 10, 40), text2.VisualRect());

  const NGPaintFragment& box = *ToList(line2.Children())[1];
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox, box.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(180, 40, 10, 30), box.VisualRect());
  EXPECT_EQ(1u, box.Children().size());

  const NGPaintFragment& text3 = *box.FirstChild();
  EXPECT_EQ(NGPhysicalFragment::kFragmentText, text3.PhysicalFragment().Type());
  EXPECT_EQ(LayoutRect(180, 40, 10, 30), text3.VisualRect());
}

TEST_F(NGPaintFragmentTest, MarkLineBoxesDirtyByRemoveBr) {
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

TEST_F(NGPaintFragmentTest, MarkLineBoxesDirtyByRemoveChild) {
  SetBodyInnerHTML(
      "<div id=container>line 1<br><b id=target>line 2</b><br>line 3<br>"
      "</div>");
  Element& target = *GetDocument().getElementById("target");
  target.remove();
  const NGPaintFragment& container = *GetPaintFragmentByElementId("container");
  EXPECT_TRUE(container.FirstChild()->IsDirty());
  EXPECT_TRUE(ToList(container.Children())[1]->IsDirty());
  EXPECT_FALSE(ToList(container.Children())[2]->IsDirty());
}

TEST_F(NGPaintFragmentTest, MarkLineBoxesDirtyByRemoveSpanWithBr) {
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

TEST_F(NGPaintFragmentTest, MarkLineBoxesDirtyByInsertAtStart) {
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

TEST_F(NGPaintFragmentTest, MarkLineBoxesDirtyByInsertAtLast) {
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

TEST_F(NGPaintFragmentTest, MarkLineBoxesDirtyByInsertAtMiddle) {
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
  SetBodyInnerHTML(
      "<div id=container>line 1<br><b id=target>line 2</b><br>line "
      "3<br></div>");
  Element& target = *GetDocument().getElementById("target");
  ToText(*target.firstChild()).setData("abc");
  const NGPaintFragment& container = *GetPaintFragmentByElementId("container");
  EXPECT_FALSE(container.FirstChild()->IsDirty());
  EXPECT_TRUE(ToList(container.Children())[1]->IsDirty());
  EXPECT_FALSE(ToList(container.Children())[2]->IsDirty());
}

TEST_F(NGPaintFragmentTest, MarkLineBoxesDirtyWrappedLine) {
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
