// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_base_layout_algorithm_test.h"

#include "third_party/blink/renderer/core/dom/tag_collection.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_box_state.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_child_layout_context.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"

namespace blink {
namespace {

class NGInlineLayoutAlgorithmTest : public NGBaseLayoutAlgorithmTest {};

TEST_F(NGInlineLayoutAlgorithmTest, BreakToken) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      html {
        font: 10px/1 Ahem;
      }
      #container {
        width: 50px; height: 20px;
      }
    </style>
    <div id=container>123 456 789</div>
  )HTML");

  // Perform 1st Layout.
  auto* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  NGInlineNode inline_node(block_flow);
  LogicalSize size(LayoutUnit(50), LayoutUnit(20));

  NGConstraintSpaceBuilder builder(WritingMode::kHorizontalTb,
                                   WritingMode::kHorizontalTb,
                                   /* is_new_fc */ false);
  builder.SetAvailableSize(size);
  NGConstraintSpace constraint_space = builder.ToConstraintSpace();

  NGInlineChildLayoutContext context;
  scoped_refptr<const NGLayoutResult> layout_result =
      inline_node.Layout(constraint_space, nullptr, &context);
  const auto& line1 = layout_result->PhysicalFragment();
  EXPECT_FALSE(line1.BreakToken()->IsFinished());

  // Perform 2nd layout with the break token from the 1st line.
  scoped_refptr<const NGLayoutResult> layout_result2 =
      inline_node.Layout(constraint_space, line1.BreakToken(), &context);
  const auto& line2 = layout_result2->PhysicalFragment();
  EXPECT_FALSE(line2.BreakToken()->IsFinished());

  // Perform 3rd layout with the break token from the 2nd line.
  scoped_refptr<const NGLayoutResult> layout_result3 =
      inline_node.Layout(constraint_space, line2.BreakToken(), &context);
  const auto& line3 = layout_result3->PhysicalFragment();
  EXPECT_TRUE(line3.BreakToken()->IsFinished());
}

TEST_F(NGInlineLayoutAlgorithmTest, GenerateHyphen) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    #container {
      font: 10px/1 Ahem;
      width: 5ch;
    }
    </style>
    <div id=container>abc&shy;def</div>
  )HTML");
  scoped_refptr<const NGPhysicalBoxFragment> block =
      GetBoxFragmentByElementId("container");
  EXPECT_EQ(2u, block->Children().size());
  const NGPhysicalLineBoxFragment& line1 =
      To<NGPhysicalLineBoxFragment>(*block->Children()[0].get());

  // The hyphen is in its own NGPhysicalTextFragment.
  EXPECT_EQ(2u, line1.Children().size());
  EXPECT_EQ(NGPhysicalFragment::kFragmentText, line1.Children()[1]->Type());
  const auto& hyphen = To<NGPhysicalTextFragment>(*line1.Children()[1].get());
  EXPECT_EQ(String(u"\u2010"), hyphen.Text().ToString());
  // It should have the same LayoutObject as the hyphened word.
  EXPECT_EQ(line1.Children()[0]->GetLayoutObject(), hyphen.GetLayoutObject());
}

TEST_F(NGInlineLayoutAlgorithmTest, GenerateEllipsis) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    #container {
      font: 10px/1 Ahem;
      width: 5ch;
      overflow: hidden;
      text-overflow: ellipsis;
    }
    </style>
    <div id=container>123456</div>
  )HTML");
  scoped_refptr<const NGPhysicalBoxFragment> block =
      GetBoxFragmentByElementId("container");
  EXPECT_EQ(1u, block->Children().size());
  const auto& line1 =
      To<NGPhysicalLineBoxFragment>(*block->Children()[0].get());

  // The ellipsis is in its own NGPhysicalTextFragment.
  EXPECT_EQ(3u, line1.Children().size());
  const auto& ellipsis = To<NGPhysicalTextFragment>(*line1.Children().back());
  EXPECT_EQ(String(u"\u2026"), ellipsis.Text().ToString());
  // It should have the same LayoutObject as the clipped word.
  EXPECT_EQ(line1.Children()[0]->GetLayoutObject(), ellipsis.GetLayoutObject());
}

TEST_F(NGInlineLayoutAlgorithmTest, EllipsisInlineBoxOnly) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    #container {
      font: 10px/1 Ahem;
      width: 5ch;
      overflow: hidden;
      text-overflow: ellipsis;
    }
    span {
      border: solid 10ch blue;
    }
    </style>
    <div id=container><span></span></div>
  )HTML");
  scoped_refptr<const NGPhysicalBoxFragment> block =
      GetBoxFragmentByElementId("container");
  EXPECT_EQ(1u, block->Children().size());
  const auto& line1 =
      To<NGPhysicalLineBoxFragment>(*block->Children()[0].get());

  // There should not be ellipsis in this line.
  for (const auto& child : line1.Children()) {
    if (const auto* text = DynamicTo<NGPhysicalTextFragment>(child.get())) {
      EXPECT_FALSE(text->IsEllipsis());
    }
  }
}

// This test ensures box fragments are generated when necessary, even when the
// line is empty. One such case is when the line contains a containing box of an
// out-of-flow object.
TEST_F(NGInlineLayoutAlgorithmTest,
       EmptyLineWithOutOfFlowInInlineContainingBlock) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    oof-container {
      position: relative;
    }
    oof {
      position: absolute;
      width: 100px;
      height: 100px;
    }
    html, body { margin: 0; }
    html {
      font-size: 10px;
    }
    </style>
    <div id=container>
      <oof-container>
        <oof></oof>
      </oof-container>
    </div>
  )HTML");
  auto* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  const NGPhysicalBoxFragment* container = block_flow->CurrentFragment();
  ASSERT_TRUE(container);
  EXPECT_EQ(LayoutUnit(), container->Size().height);

  EXPECT_EQ(2u, container->Children().size());
  const auto& linebox =
      To<NGPhysicalLineBoxFragment>(*container->Children()[0]);

  EXPECT_EQ(1u, linebox.Children().size());
  EXPECT_EQ(PhysicalSize(), linebox.Size());

  const auto& oof_container = To<NGPhysicalBoxFragment>(*linebox.Children()[0]);
  EXPECT_EQ(PhysicalSize(), oof_container.Size());
}

// This test ensures that if an inline box generates (or does not generate) box
// fragments for a wrapped line, it should consistently do so for other lines
// too, when the inline box is fragmented to multiple lines.
TEST_F(NGInlineLayoutAlgorithmTest, BoxForEndMargin) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    #container {
      font: 10px/1 Ahem;
      width: 50px;
    }
    span {
      border-right: 10px solid blue;
    }
    </style>
    <!-- This line wraps, and only 2nd line has a border. -->
    <div id=container>12 <span>3 45</span> 6</div>
  )HTML");
  auto* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  const NGPhysicalBoxFragment* block_box = block_flow->CurrentFragment();
  ASSERT_TRUE(block_box);
  EXPECT_EQ(2u, block_box->Children().size());
  const auto& line_box1 =
      To<NGPhysicalLineBoxFragment>(*block_box->Children()[0].get());
  EXPECT_EQ(2u, line_box1.Children().size());

  // The <span> generates a box fragment for the 2nd line because it has a
  // right border. It should also generate a box fragment for the 1st line even
  // though there's no borders on the 1st line.
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox, line_box1.Children()[1]->Type());
}

// A block with inline children generates fragment tree as follows:
// - A box fragment created by NGBlockNode
//   - A wrapper box fragment created by NGInlineNode
//     - Line box fragments.
// This test verifies that borders/paddings are applied to the wrapper box.
TEST_F(NGInlineLayoutAlgorithmTest, ContainerBorderPadding) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html, body { margin: 0; }
    div {
      padding-left: 5px;
      padding-top: 10px;
      display: flow-root;
    }
    </style>
    <div id=container>test</div>
  )HTML");
  auto* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  NGBlockNode block_node(block_flow);
  NGConstraintSpace space = NGConstraintSpace::CreateFromLayoutObject(
      *block_flow, false /* is_layout_root */);
  scoped_refptr<const NGLayoutResult> layout_result = block_node.Layout(space);

  EXPECT_TRUE(layout_result->BfcBlockOffset().has_value());
  EXPECT_EQ(0, *layout_result->BfcBlockOffset());
  EXPECT_EQ(0, layout_result->BfcLineOffset());

  PhysicalOffset line_offset =
      layout_result->PhysicalFragment().Children()[0].Offset();
  EXPECT_EQ(5, line_offset.left);
  EXPECT_EQ(10, line_offset.top);
}

// The test leaks memory. crbug.com/721932
#if defined(ADDRESS_SANITIZER)
#define MAYBE_VerticalAlignBottomReplaced DISABLED_VerticalAlignBottomReplaced
#else
#define MAYBE_VerticalAlignBottomReplaced VerticalAlignBottomReplaced
#endif
TEST_F(NGInlineLayoutAlgorithmTest, MAYBE_VerticalAlignBottomReplaced) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    html { font-size: 10px; }
    img { vertical-align: bottom; }
    #container { display: flow-root; }
    </style>
    <div id=container><img src="#" width="96" height="96"></div>
  )HTML");
  auto* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  NGInlineNode inline_node(block_flow);
  NGInlineChildLayoutContext context;
  NGConstraintSpace space = NGConstraintSpace::CreateFromLayoutObject(
      *block_flow, false /* is_layout_root */);
  scoped_refptr<const NGLayoutResult> layout_result =
      inline_node.Layout(space, nullptr, &context);

  const auto& line = layout_result->PhysicalFragment();
  EXPECT_EQ(LayoutUnit(96), line.Size().height);
  PhysicalOffset img_offset = line.Children()[0].Offset();
  EXPECT_EQ(LayoutUnit(0), img_offset.top);
}

// Verifies that text can flow correctly around floats that were positioned
// before the inline block.
TEST_F(NGInlineLayoutAlgorithmTest, TextFloatsAroundFloatsBefore) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      * {
        font-family: "Arial", sans-serif;
        font-size: 20px;
      }
      #container {
        height: 200px; width: 200px; outline: solid blue;
      }
      #left-float1 {
        float: left; width: 30px; height: 30px; background-color: blue;
      }
      #left-float2 {
        float: left; width: 10px; height: 10px;
        background-color: purple;
      }
      #right-float {
        float: right; width: 40px; height: 40px; background-color: yellow;
      }
    </style>
    <div id="container">
      <div id="left-float1"></div>
      <div id="left-float2"></div>
      <div id="right-float"></div>
      <span id="text">The quick brown fox jumps over the lazy dog</span>
    </div>
  )HTML");
  // ** Run LayoutNG algorithm **
  NGConstraintSpace space;
  scoped_refptr<const NGPhysicalBoxFragment> html_fragment;
  std::tie(html_fragment, space) = RunBlockLayoutAlgorithmForElement(
      GetDocument().getElementsByTagName("html")->item(0));
  auto* body_fragment =
      To<NGPhysicalBoxFragment>(html_fragment->Children()[0].get());
  auto* container_fragment =
      To<NGPhysicalBoxFragment>(body_fragment->Children()[0].get());
  Vector<PhysicalOffset> line_offsets;
  for (const auto& child : container_fragment->Children()) {
    if (!child->IsLineBox())
      continue;

    line_offsets.push_back(child.Offset());
  }

  // Line break points may vary by minor differences in fonts.
  // The test is valid as long as we have 3 or more lines and their positions
  // are correct.
  EXPECT_GE(line_offsets.size(), 3UL);

  // 40 = #left-float1' width 30 + #left-float2 10
  EXPECT_EQ(LayoutUnit(40), line_offsets[0].left);

  // 40 = #left-float1' width 30
  EXPECT_EQ(LayoutUnit(30), line_offsets[1].left);
  EXPECT_EQ(LayoutUnit(), line_offsets[2].left);
}

// Verifies that text correctly flows around the inline float that fits on
// the same text line.
TEST_F(NGInlineLayoutAlgorithmTest, TextFloatsAroundInlineFloatThatFitsOnLine) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      * {
        font-family: "Arial", sans-serif;
        font-size: 18px;
      }
      #container {
        height: 200px; width: 200px; outline: solid orange;
      }
      #narrow-float {
        float: left; width: 30px; height: 30px; background-color: blue;
      }
    </style>
    <div id="container">
      <span id="text">
        The quick <div id="narrow-float"></div> brown fox jumps over the lazy
      </span>
    </div>
  )HTML");

  auto* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  const NGPhysicalBoxFragment* block_box = block_flow->CurrentFragment();
  ASSERT_TRUE(block_box);

  // Two lines.
  EXPECT_EQ(2u, block_box->Children().size());
  PhysicalOffset first_line_offset = block_box->Children()[1].Offset();

  // 30 == narrow-float's width.
  EXPECT_EQ(LayoutUnit(30), first_line_offset.left);

  Element* span = GetDocument().getElementById("text");
  // 38 == narrow-float's width + body's margin.
  EXPECT_EQ(LayoutUnit(38), span->OffsetLeft());

  Element* narrow_float = GetDocument().getElementById("narrow-float");
  // 8 == body's margin.
  EXPECT_EQ(8, narrow_float->OffsetLeft());
  EXPECT_EQ(8, narrow_float->OffsetTop());
}

// Verifies that the inline float got pushed to the next line if it doesn't
// fit the current line.
TEST_F(NGInlineLayoutAlgorithmTest,
       TextFloatsAroundInlineFloatThatDoesNotFitOnLine) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      * {
        font-family: "Arial", sans-serif;
        font-size: 19px;
      }
      #container {
        height: 200px; width: 200px; outline: solid orange;
      }
      #wide-float {
        float: left; width: 160px; height: 30px; background-color: red;
      }
    </style>
    <div id="container">
      <span id="text">
        The quick <div id="wide-float"></div> brown fox jumps over the lazy dog
      </span>
    </div>
  )HTML");

  Element* wide_float = GetDocument().getElementById("wide-float");
  // 8 == body's margin.
  EXPECT_EQ(8, wide_float->OffsetLeft());
}

// Verifies that if an inline float pushed to the next line then all others
// following inline floats positioned with respect to the float's top edge
// alignment rule.
TEST_F(NGInlineLayoutAlgorithmTest,
       FloatsArePositionedWithRespectToTopEdgeAlignmentRule) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      * {
        font-family: "Arial", sans-serif;
        font-size: 19px;
      }
      #container {
        height: 200px; width: 200px; outline: solid orange;
      }
      #left-narrow {
        float: left; width: 5px; height: 30px; background-color: blue;
      }
      #left-wide {
        float: left; width: 160px; height: 30px; background-color: red;
      }
    </style>
    <div id="container">
      <span id="text">
        The quick <div id="left-wide"></div> brown <div id="left-narrow"></div>
        fox jumps over the lazy dog
      </span>
    </div>
  )HTML");
  Element* wide_float = GetDocument().getElementById("left-wide");
  // 8 == body's margin.
  EXPECT_EQ(8, wide_float->OffsetLeft());

  Element* narrow_float = GetDocument().getElementById("left-narrow");
  // 160 float-wide's width + 8 body's margin.
  EXPECT_EQ(160 + 8, narrow_float->OffsetLeft());

  // On the same line.
  EXPECT_EQ(wide_float->OffsetTop(), narrow_float->OffsetTop());
}

// Verifies that InlineLayoutAlgorithm positions floats with respect to their
// margins.
TEST_F(NGInlineLayoutAlgorithmTest, PositionFloatsWithMargins) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #container {
        height: 200px; width: 200px; outline: solid orange;
      }
      #left {
        float: left; width: 5px; height: 30px; background-color: blue;
        margin: 10%;
      }
    </style>
    <div id="container">
      <span id="text">
        The quick <div id="left"></div> brown fox jumps over the lazy dog
      </span>
    </div>
  )HTML");
  Element* span = GetElementById("text");
  // 53 = sum of left's inline margins: 40 + left's width: 5 + body's margin: 8
  EXPECT_EQ(LayoutUnit(53), span->OffsetLeft());
}

// Test glyph bounding box causes ink overflow.
TEST_F(NGInlineLayoutAlgorithmTest, InkOverflow) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #container {
        font: 20px/.5 Ahem;
        display: flow-root;
      }
    </style>
    <div id="container">Hello</div>
  )HTML");
  auto* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  const NGPaintFragment* paint_fragment = block_flow->PaintFragment();
  ASSERT_TRUE(paint_fragment);
  const NGPhysicalFragment& box_fragment = paint_fragment->PhysicalFragment();

  EXPECT_EQ(LayoutUnit(10), box_fragment.Size().height);

  PhysicalRect ink_overflow = paint_fragment->InkOverflow();
  EXPECT_EQ(LayoutUnit(-5), ink_overflow.offset.top);
  EXPECT_EQ(LayoutUnit(20), ink_overflow.size.height);

  // |ContentsInkOverflow| should match to |InkOverflow|, except the width
  // because |<div id=container>| might be wider than the content.
  EXPECT_EQ(ink_overflow.offset, paint_fragment->ContentsInkOverflow().offset);
  EXPECT_EQ(ink_overflow.size.height,
            paint_fragment->ContentsInkOverflow().size.height);
}

#undef MAYBE_VerticalAlignBottomReplaced
}  // namespace
}  // namespace blink
