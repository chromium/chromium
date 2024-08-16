// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/dom/tag_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/layout/base_layout_algorithm_test.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/inline/inline_box_state.h"
#include "third_party/blink/renderer/core/layout/inline/inline_break_token.h"
#include "third_party/blink/renderer/core/layout/inline/inline_child_layout_context.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/inline/physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {
namespace {

// Compute the result of the effects of the `text-box-trim` property.
struct TextBoxTrimResult {
  explicit TextBoxTrimResult(const LayoutBox& layout_object) {
    const LayoutResult* result = layout_object.GetCachedLayoutResult(nullptr);
    const ConstraintSpace& space = result->GetConstraintSpaceForCaching();
    should_trim_start = space.ShouldTextBoxTrimStart();
    should_trim_end = space.ShouldTextBoxTrimEnd();
    is_trimmed = result->IsBlockStartTrimmed() || result->IsBlockEndTrimmed();
  }

  bool should_trim_start = false;
  bool should_trim_end = false;
  bool is_trimmed = false;
};

const PhysicalLineBoxFragment* FindBlockInInlineLineBoxFragment(
    Element* container) {
  InlineCursor cursor(*To<LayoutBlockFlow>(container->GetLayoutObject()));
  for (cursor.MoveToFirstLine(); cursor; cursor.MoveToNextLine()) {
    const PhysicalLineBoxFragment* fragment =
        cursor.Current()->LineBoxFragment();
    DCHECK(fragment);
    if (fragment->IsBlockInInline())
      return fragment;
  }
  return nullptr;
}

class InlineLayoutAlgorithmTest : public BaseLayoutAlgorithmTest {
 protected:
  static std::string AsFragmentItemsString(const LayoutBlockFlow& root) {
    std::ostringstream ostream;
    ostream << std::endl;
    for (InlineCursor cursor(root); cursor; cursor.MoveToNext()) {
      const auto& item = *cursor.CurrentItem();
      ostream << item << " " << item.RectInContainerFragment() << std::endl;
    }
    return ostream.str();
  }

  PhysicalRect TextAreaFirstLineRect(const char* id) {
    HTMLTextAreaElement* textarea = To<HTMLTextAreaElement>(GetElementById(id));
    DCHECK(textarea);

    InlineCursor cursor(*To<LayoutBlockFlow>(
        textarea->InnerEditorElement()->GetLayoutObject()));
    cursor.MoveToFirstLine();
    EXPECT_TRUE(cursor.IsNotNull());

    return PhysicalRect(cursor.Current().OffsetInContainerFragment(),
                        cursor.Current().Size());
  }
};

TEST_F(InlineLayoutAlgorithmTest, Types) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id="normal">normal</div>
    <div id="empty"><span></span></div>
  )HTML");
  InlineCursor normal(
      *To<LayoutBlockFlow>(GetLayoutObjectByElementId("normal")));
  normal.MoveToFirstLine();
  EXPECT_FALSE(normal.Current()->LineBoxFragment()->IsEmptyLineBox());

  InlineCursor empty(*To<LayoutBlockFlow>(GetLayoutObjectByElementId("empty")));
  empty.MoveToFirstLine();
  EXPECT_TRUE(empty.Current()->LineBoxFragment()->IsEmptyLineBox());
}

TEST_F(InlineLayoutAlgorithmTest, TypesForFirstLine) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div::first-line { font-size: 2em; }
    </style>
    <div id="normal">normal</div>
    <div id="empty"><span></span></div>
  )HTML");
  InlineCursor normal(
      *To<LayoutBlockFlow>(GetLayoutObjectByElementId("normal")));
  normal.MoveToFirstLine();
  EXPECT_FALSE(normal.Current()->LineBoxFragment()->IsEmptyLineBox());
  EXPECT_EQ(normal.Current().GetStyleVariant(), StyleVariant::kFirstLine);
  EXPECT_EQ(normal.Current()->LineBoxFragment()->GetStyleVariant(),
            StyleVariant::kFirstLine);

  InlineCursor empty(*To<LayoutBlockFlow>(GetLayoutObjectByElementId("empty")));
  empty.MoveToFirstLine();
  EXPECT_TRUE(empty.Current()->LineBoxFragment()->IsEmptyLineBox());
  EXPECT_EQ(empty.Current().GetStyleVariant(), StyleVariant::kFirstLine);
  EXPECT_EQ(empty.Current()->LineBoxFragment()->GetStyleVariant(),
            StyleVariant::kFirstLine);
}

TEST_F(InlineLayoutAlgorithmTest, TypesForBlockInInline) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id="block-in-inline">
      <span><div>normal</div></span>
    </div>
    <div id="block-in-inline-empty">
      <span><div></div></span>
    </div>
    <div id="block-in-inline-height">
      <span><div style="height: 100px"></div></span>
    </div>
  )HTML");
  // Regular block-in-inline.
  InlineCursor block_in_inline(
      *To<LayoutBlockFlow>(GetLayoutObjectByElementId("block-in-inline")));
  block_in_inline.MoveToFirstLine();
  EXPECT_TRUE(block_in_inline.Current()->LineBoxFragment()->IsEmptyLineBox());
  EXPECT_FALSE(block_in_inline.Current()->LineBoxFragment()->IsBlockInInline());
  block_in_inline.MoveToNextLine();
  EXPECT_FALSE(block_in_inline.Current()->LineBoxFragment()->IsEmptyLineBox());
  EXPECT_TRUE(block_in_inline.Current()->LineBoxFragment()->IsBlockInInline());
  int block_count = 0;
  for (InlineCursor children = block_in_inline.CursorForDescendants(); children;
       children.MoveToNext()) {
    if (children.Current()->BoxFragment() &&
        children.Current()->BoxFragment()->IsBlockInInline())
      ++block_count;
  }
  EXPECT_EQ(block_count, 1);
  block_in_inline.MoveToNextLine();
  EXPECT_TRUE(block_in_inline.Current()->LineBoxFragment()->IsEmptyLineBox());
  EXPECT_FALSE(block_in_inline.Current()->LineBoxFragment()->IsBlockInInline());

  // If the block is empty and self-collapsing, |IsEmptyLineBox| should be set.
  InlineCursor block_in_inline_empty(*To<LayoutBlockFlow>(
      GetLayoutObjectByElementId("block-in-inline-empty")));
  block_in_inline_empty.MoveToFirstLine();
  block_in_inline_empty.MoveToNextLine();
  EXPECT_TRUE(
      block_in_inline_empty.Current()->LineBoxFragment()->IsEmptyLineBox());
  EXPECT_TRUE(
      block_in_inline_empty.Current()->LineBoxFragment()->IsBlockInInline());

  // Test empty but non-self-collapsing block in an inline box.
  InlineCursor block_in_inline_height(*To<LayoutBlockFlow>(
      GetLayoutObjectByElementId("block-in-inline-height")));
  block_in_inline_height.MoveToFirstLine();
  block_in_inline_height.MoveToNextLine();
  EXPECT_FALSE(
      block_in_inline_height.Current()->LineBoxFragment()->IsEmptyLineBox());
  EXPECT_TRUE(
      block_in_inline_height.Current()->LineBoxFragment()->IsBlockInInline());
}

TEST_F(InlineLayoutAlgorithmTest, BreakToken) {
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
  InlineNode inline_node(block_flow);
  LogicalSize size(LayoutUnit(50), LayoutUnit(20));

  ConstraintSpaceBuilder builder(
      WritingMode::kHorizontalTb,
      {WritingMode::kHorizontalTb, TextDirection::kLtr},
      /* is_new_fc */ false);
  builder.SetAvailableSize(size);
  ConstraintSpace constraint_space = builder.ToConstraintSpace();

  BoxFragmentBuilder container_builder(
      block_flow, block_flow->Style(), constraint_space,
      block_flow->Style()->GetWritingDirection());
  SimpleInlineChildLayoutContext context(inline_node, &container_builder);
  const LayoutResult* layout_result =
      inline_node.Layout(constraint_space, nullptr, nullptr, &context);
  const auto& line1 = layout_result->GetPhysicalFragment();
  EXPECT_TRUE(line1.GetBreakToken());

  // Perform 2nd layout with the break token from the 1st line.
  const LayoutResult* layout_result2 = inline_node.Layout(
      constraint_space, line1.GetBreakToken(), nullptr, &context);
  const auto& line2 = layout_result2->GetPhysicalFragment();
  EXPECT_TRUE(line2.GetBreakToken());

  // Perform 3rd layout with the break token from the 2nd line.
  const LayoutResult* layout_result3 = inline_node.Layout(
      constraint_space, line2.GetBreakToken(), nullptr, &context);
  const auto& line3 = layout_result3->GetPhysicalFragment();
  EXPECT_FALSE(line3.GetBreakToken());
}

// This test ensures box fragments are generated when necessary, even when the
// line is empty. One such case is when the line contains a containing box of an
// out-of-flow object.
TEST_F(InlineLayoutAlgorithmTest,
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
      <oof-container id=target>
        <oof></oof>
      </oof-container>
    </div>
  )HTML");
  auto* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  const PhysicalBoxFragment* container = block_flow->GetPhysicalFragment(0);
  ASSERT_TRUE(container);
  EXPECT_EQ(LayoutUnit(), container->Size().height);

  InlineCursor line_box(*block_flow);
  ASSERT_TRUE(line_box);
  ASSERT_TRUE(line_box.Current().IsLineBox());
  EXPECT_EQ(PhysicalSize(), line_box.Current().Size());

  InlineCursor off_container(line_box);
  off_container.MoveToNext();
  ASSERT_TRUE(off_container);
  ASSERT_EQ(GetLayoutObjectByElementId("target"),
            off_container.Current().GetLayoutObject());
  EXPECT_EQ(PhysicalSize(), off_container.Current().Size());
}

// This test ensures that if an inline box generates (or does not generate) box
// fragments for a wrapped line, it should consistently do so for other lines
// too, when the inline box is fragmented to multiple lines.
TEST_F(InlineLayoutAlgorithmTest, BoxForEndMargin) {
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
    <div id=container>12 <span id=span>3 45</span> 6</div>
  )HTML");
  auto* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  InlineCursor line_box(*block_flow);
  ASSERT_TRUE(line_box) << "line_box is at start of first line.";
  ASSERT_TRUE(line_box.Current().IsLineBox());
  line_box.MoveToNextLine();
  ASSERT_TRUE(line_box) << "line_box is at start of second line.";
  InlineCursor cursor(line_box);
  ASSERT_TRUE(line_box.Current().IsLineBox());
  cursor.MoveToNext();
  ASSERT_TRUE(cursor);
  EXPECT_EQ(GetLayoutObjectByElementId("span"),
            cursor.Current().GetLayoutObject());

  // The <span> generates a box fragment for the 2nd line because it has a
  // right border. It should also generate a box fragment for the 1st line even
  // though there's no borders on the 1st line.
  const PhysicalBoxFragment* box_fragment = cursor.Current().BoxFragment();
  ASSERT_TRUE(box_fragment);
  EXPECT_EQ(PhysicalFragment::kFragmentBox, box_fragment->Type());

  line_box.MoveToNextLine();
  ASSERT_FALSE(line_box) << "block_flow has two lines.";
}

TEST_F(InlineLayoutAlgorithmTest, InlineBoxBorderPadding) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      font-size: 10px;
      line-height: 10px;
    }
    span {
      border-left: 1px solid blue;
      border-top: 2px solid blue;
      border-right: 3px solid blue;
      border-bottom: 4px solid blue;
      padding-left: 5px;
      padding-top: 6px;
      padding-right: 7px;
      padding-bottom: 8px;
    }
    </style>
    <div id="container">
      <span id="span">test<br>test</span>
    </div>
  )HTML");
  auto* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  InlineCursor cursor(*block_flow);
  const LayoutObject* span = GetLayoutObjectByElementId("span");
  cursor.MoveTo(*span);
  const FragmentItem& item1 = *cursor.Current();
  const PhysicalBoxFragment* box1 = item1.BoxFragment();
  ASSERT_TRUE(box1);
  const PhysicalBoxStrut borders1 = box1->Borders();
  const PhysicalBoxStrut padding1 = box1->Padding();
  int borders_and_padding1[] = {
      borders1.left.ToInt(),   borders1.top.ToInt(),   borders1.right.ToInt(),
      borders1.bottom.ToInt(), padding1.left.ToInt(),  padding1.top.ToInt(),
      padding1.right.ToInt(),  padding1.bottom.ToInt()};
  EXPECT_THAT(borders_and_padding1,
              testing::ElementsAre(1, 2, 0, 4, 5, 6, 0, 8));
  EXPECT_EQ(box1->ContentOffset(), PhysicalOffset(6, 8));
  EXPECT_EQ(item1.ContentOffsetInContainerFragment(),
            item1.OffsetInContainerFragment() + box1->ContentOffset());

  cursor.MoveToNextForSameLayoutObject();
  const FragmentItem& item2 = *cursor.Current();
  const PhysicalBoxFragment* box2 = item2.BoxFragment();
  ASSERT_TRUE(box2);
  const PhysicalBoxStrut borders2 = box2->Borders();
  const PhysicalBoxStrut padding2 = box2->Padding();
  int borders_and_padding2[] = {
      borders2.left.ToInt(),   borders2.top.ToInt(),   borders2.right.ToInt(),
      borders2.bottom.ToInt(), padding2.left.ToInt(),  padding2.top.ToInt(),
      padding2.right.ToInt(),  padding2.bottom.ToInt()};
  EXPECT_THAT(borders_and_padding2,
              testing::ElementsAre(0, 2, 3, 4, 0, 6, 7, 8));
  EXPECT_EQ(box2->ContentOffset(), PhysicalOffset(0, 8));
  EXPECT_EQ(item2.ContentOffsetInContainerFragment(),
            item2.OffsetInContainerFragment() + box2->ContentOffset());
}

// A block with inline children generates fragment tree as follows:
// - A box fragment created by BlockNode
//   - A wrapper box fragment created by InlineNode
//     - Line box fragments.
// This test verifies that borders/paddings are applied to the wrapper box.
TEST_F(InlineLayoutAlgorithmTest, ContainerBorderPadding) {
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

  const auto* layout_result =
      GetLayoutBoxByElementId("container")->GetSingleCachedLayoutResult();

  EXPECT_TRUE(layout_result->BfcBlockOffset().has_value());
  EXPECT_EQ(0, *layout_result->BfcBlockOffset());
  EXPECT_EQ(0, layout_result->BfcLineOffset());

  const auto& fragment =
      To<PhysicalBoxFragment>(layout_result->GetPhysicalFragment());
  EXPECT_EQ(fragment.ContentOffset(), PhysicalOffset(5, 10));
  PhysicalOffset line_offset = fragment.Children()[0].Offset();
  EXPECT_EQ(line_offset, PhysicalOffset(5, 10));
}

// The test leaks memory. crbug.com/721932
#if defined(ADDRESS_SANITIZER)
#define MAYBE_VerticalAlignBottomReplaced DISABLED_VerticalAlignBottomReplaced
#else
#define MAYBE_VerticalAlignBottomReplaced VerticalAlignBottomReplaced
#endif
TEST_F(InlineLayoutAlgorithmTest, MAYBE_VerticalAlignBottomReplaced) {
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
  InlineCursor cursor(*block_flow);
  ASSERT_TRUE(cursor);
  EXPECT_EQ(LayoutUnit(96), cursor.Current().Size().height);
  cursor.MoveToNext();
  ASSERT_TRUE(cursor);
  EXPECT_EQ(LayoutUnit(0), cursor.Current().OffsetInContainerFragment().top)
      << "Offset top of <img> should be zero.";
}

// Verifies that text can flow correctly around floats that were positioned
// before the inline block.
TEST_F(InlineLayoutAlgorithmTest, TextFloatsAroundFloatsBefore) {
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

  const auto& html_fragment =
      To<LayoutBox>(GetDocument()
                        .getElementsByTagName(AtomicString("html"))
                        ->item(0)
                        ->GetLayoutObject())
          ->GetSingleCachedLayoutResult()
          ->GetPhysicalFragment();

  auto* body_fragment =
      To<PhysicalBoxFragment>(html_fragment.Children()[0].get());
  auto* container_fragment =
      To<PhysicalBoxFragment>(body_fragment->Children()[0].get());
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
TEST_F(InlineLayoutAlgorithmTest, TextFloatsAroundInlineFloatThatFitsOnLine) {
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
  const PhysicalBoxFragment* block_box = block_flow->GetPhysicalFragment(0);
  ASSERT_TRUE(block_box);

  // Two lines.
  ASSERT_EQ(2u, block_box->Children().size());
  PhysicalOffset first_line_offset = block_box->Children()[1].Offset();

  // 30 == narrow-float's width.
  EXPECT_EQ(LayoutUnit(30), first_line_offset.left);

  Element* span = GetElementById("text");
  // 38 == narrow-float's width + body's margin.
  EXPECT_EQ(LayoutUnit(38), span->OffsetLeft());

  Element* narrow_float = GetElementById("narrow-float");
  // 8 == body's margin.
  EXPECT_EQ(8, narrow_float->OffsetLeft());
  EXPECT_EQ(8, narrow_float->OffsetTop());
}

// Verifies that the inline float got pushed to the next line if it doesn't
// fit the current line.
TEST_F(InlineLayoutAlgorithmTest,
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

  Element* wide_float = GetElementById("wide-float");
  // 8 == body's margin.
  EXPECT_EQ(8, wide_float->OffsetLeft());
}

// Verifies that if an inline float pushed to the next line then all others
// following inline floats positioned with respect to the float's top edge
// alignment rule.
TEST_F(InlineLayoutAlgorithmTest,
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
  Element* wide_float = GetElementById("left-wide");
  // 8 == body's margin.
  EXPECT_EQ(8, wide_float->OffsetLeft());

  Element* narrow_float = GetElementById("left-narrow");
  // 160 float-wide's width + 8 body's margin.
  EXPECT_EQ(160 + 8, narrow_float->OffsetLeft());

  // On the same line.
  EXPECT_EQ(wide_float->OffsetTop(), narrow_float->OffsetTop());
}

// Block-in-inline is not reusable. See |EndOfReusableItems|.
TEST_F(InlineLayoutAlgorithmTest, BlockInInlineAppend) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      :root {
        font-size: 10px;
      }
      #container {
        width: 10ch;
      }
    </style>
    <div id="container">
      <span id="span">
        12345678
        <div>block</div>
        12345678
      </span>
      12345678
    </div>
  )HTML");
  Element* container_element = GetElementById("container");
  const PhysicalLineBoxFragment* before_append =
      FindBlockInInlineLineBoxFragment(container_element);
  ASSERT_TRUE(before_append);

  Document& doc = GetDocument();
  container_element->appendChild(doc.createTextNode("12345678"));
  UpdateAllLifecyclePhasesForTest();
  const PhysicalLineBoxFragment* after_append =
      FindBlockInInlineLineBoxFragment(container_element);
  EXPECT_NE(before_append, after_append);
}

// Verifies that InlineLayoutAlgorithm positions floats with respect to their
// margins.
TEST_F(InlineLayoutAlgorithmTest, PositionFloatsWithMargins) {
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
TEST_F(InlineLayoutAlgorithmTest, InkOverflow) {
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
  const PhysicalBoxFragment& box_fragment = *block_flow->GetPhysicalFragment(0);
  EXPECT_EQ(LayoutUnit(10), box_fragment.Size().height);

  InlineCursor cursor(*block_flow);
  PhysicalRect ink_overflow = cursor.Current().InkOverflowRect();
  EXPECT_EQ(LayoutUnit(-5), ink_overflow.offset.top);
  EXPECT_EQ(LayoutUnit(20), ink_overflow.size.height);
}

// See also InlineLayoutAlgorithmTest.TextCombineFake
TEST_F(InlineLayoutAlgorithmTest, TextCombineBasic) {
  LoadAhem();
  InsertStyleElement(
      "body { margin: 0px; font: 100px/110px Ahem; }"
      "c { text-combine-upright: all; }"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>a<c id=target>01234</c>b</div>");

  EXPECT_EQ(R"DUMP(
{Line #descendants=5 LTR Standard} "0,0 110x300"
{Text 0-1 LTR Standard} "5,0 100x100"
{Box #descendants=2 Standard} "5,100 100x100"
{Box #descendants=1 AtomicInlineLTR Standard} "5,100 100x100"
{Text 2-3 LTR Standard} "5,200 100x100"
)DUMP",
            AsFragmentItemsString(
                *To<LayoutBlockFlow>(GetLayoutObjectByElementId("root"))));

  EXPECT_EQ(R"DUMP(
{Line #descendants=2 LTR Standard} "0,0 100x100"
{Text 0-5 LTR Standard} "0,0 500x100"
)DUMP",
            AsFragmentItemsString(*To<LayoutBlockFlow>(
                GetLayoutObjectByElementId("target")->SlowFirstChild())));
}

// See also InlineLayoutAlgorithmTest.TextCombineBasic
TEST_F(InlineLayoutAlgorithmTest, TextCombineFake) {
  LoadAhem();
  InsertStyleElement(
      "body { margin: 0px; font: 100px/110px Ahem; }"
      "c {"
      "  display: inline-block;"
      "  width: 1em; height: 1em;"
      "  writing-mode: horizontal-tb;"
      "}"
      "div { writing-mode: vertical-rl; }");
  SetBodyInnerHTML("<div id=root>a<c id=target>0</c>b</div>");

  EXPECT_EQ(R"DUMP(
{Line #descendants=4 LTR Standard} "0,0 110x300"
{Text 0-1 LTR Standard} "5,0 100x100"
{Box #descendants=1 AtomicInlineLTR Standard} "5,100 100x100"
{Text 2-3 LTR Standard} "5,200 100x100"
)DUMP",
            AsFragmentItemsString(
                *To<LayoutBlockFlow>(GetLayoutObjectByElementId("root"))));

  EXPECT_EQ(R"DUMP(
{Line #descendants=2 LTR Standard} "0,0 100x110"
{Text 0-1 LTR Standard} "0,5 100x100"
)DUMP",
            AsFragmentItemsString(
                *To<LayoutBlockFlow>(GetLayoutObjectByElementId("target"))));
}

// http://crbug.com/1413969
TEST_F(InlineLayoutAlgorithmTest, InitialLetterEmpty) {
  LoadAhem();
  InsertStyleElement(
      "body { font: 10px/15px Ahem; }"
      "#sample::first-letter { initial-letter: 3; }");
  SetBodyInnerHTML("<div id=sample><span> </span></div>");
  const char* const expected = R"DUMP(
{Line #descendants=2 LTR Standard} "0,0 0x0"
{Box #descendants=1 Standard} "0,0 0x0"
)DUMP";
  EXPECT_EQ(expected, AsFragmentItemsString(*To<LayoutBlockFlow>(
                          GetLayoutObjectByElementId("sample"))));
}

// http://crbug.com/1420168
TEST_F(InlineLayoutAlgorithmTest, InitialLetterWithEmptyInline) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  LoadAhem();
  InsertStyleElement(
      "body { font: 20px/24px Ahem; }"
      "div::first-letter { initial-letter: 3; }");
  SetBodyInnerHTML("<div id=sample>x<span></span></div>");
  const char* const expected = R"DUMP(
{Line #descendants=3 LTR Standard} "0,0 80x0"
{Box #descendants=1 AtomicInlineLTR Standard} "0,2 80x80"
{Box #descendants=1 Standard} "80,-16 0x20"
)DUMP";
  EXPECT_EQ(expected, AsFragmentItemsString(*To<LayoutBlockFlow>(
                          GetLayoutObjectByElementId("sample"))));
}

TEST_F(InlineLayoutAlgorithmTest, LineBoxWithHangingWidthRTLRightAligned) {
  LoadAhem();
  InsertStyleElement(
      "textarea {"
      "  width: 100px;"
      "  text-align: right;"
      "  font: 10px/10px Ahem;"
      "}");
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <textarea dir="rtl" id="a">abc  </textarea>
    <textarea dir="rtl" id="b">abc  nextLine</textarea>
    <textarea dir="rtl" id="c">abc        </textarea>
    <textarea dir="rtl" id="d">abc        nextLine</textarea>
  )HTML");

  // Trailing spaces conditionally hang since the line is followed by a line
  // break, and the line doesn't overflow, so they as treated as not hanging.
  EXPECT_EQ(PhysicalRect(50, 0, 50, 10), TextAreaFirstLineRect("a"));

  // The hanging width doesn't overflow, and it unconditionally hangs because
  // it's not followed by a line break.
  EXPECT_EQ(PhysicalRect(70, 0, 30, 10), TextAreaFirstLineRect("b"));

  // Trailing spaces conditionally hang since the line is followed by a line
  // break, and the line overflows, so only the overflowing width hangs.
  EXPECT_EQ(PhysicalRect(0, 0, 100, 10), TextAreaFirstLineRect("c"));

  // The hanging width overflows, and it unconditionally hangs because
  // it's not followed by a line break.
  EXPECT_EQ(PhysicalRect(70, 0, 30, 10), TextAreaFirstLineRect("d"));
}

TEST_F(InlineLayoutAlgorithmTest, LineBoxWithHangingWidthRTLCenterAligned) {
  LoadAhem();
  InsertStyleElement(
      "textarea {"
      "  width: 100px;"
      "  text-align: center;"
      "  font: 10px/10px Ahem;"
      "}");
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <textarea dir="rtl" id="a">abc  </textarea>
    <textarea dir="rtl" id="b">abc  nextLine</textarea>
    <textarea dir="rtl" id="c">abc      </textarea>
    <textarea dir="rtl" id="d">abc      nextLine</textarea>
    <textarea dir="rtl" id="e">abc        </textarea>
    <textarea dir="rtl" id="f">abc        nextLine</textarea>
  )HTML");

  // The line size is 30px and the trailing spaces are 20px. For a, those spaces
  // conditionally hang, and since the line doesn't overflow, they don't
  // actually hang. Therefore, the rectangle containing the line and trailing
  // spaces is centered, so its left edge is at (100 - 30 - 20)/2 = 25.
  // For b, those spaces hang unconditionally, so the rectangle containing the
  // line without the trailing spaces is centered, with its left edge at
  // (100 - 30)/2 = 35.
  EXPECT_EQ(PhysicalRect(25, 0, 50, 10), TextAreaFirstLineRect("a"));
  EXPECT_EQ(PhysicalRect(35, 0, 30, 10), TextAreaFirstLineRect("b"));

  // The line size is 30px and the trailing spaces are 60px. For c, those spaces
  // conditionally hang, and since the line doesn't overflow, they don't
  // actually hang. Therefore, the rectangle containing the line and trailing
  // spaces is centered, so its left edge is at (100 - 30 - 60)/2 = 5.
  // For d, those spaces hang unconditionally, so the rectangle containing the
  // line without the trailing spaces is centered, with its left edge at
  // (100 - 30)/2 = 35.
  EXPECT_EQ(PhysicalRect(5, 0, 90, 10), TextAreaFirstLineRect("c"));
  EXPECT_EQ(PhysicalRect(35, 0, 30, 10), TextAreaFirstLineRect("d"));

  // The line size is 30px and the trailing spaces are 80px. For e, those spaces
  // conditionally hang, so only the 10px that overflow the line actually hang.
  // Therefore, the rectangle containing the line and non-hanging spaces is
  // centered, so its left edge is at (100 - 30 - 70)/2 = 0.
  // For b, those spaces hang unconditionally, so the rectangle containing the
  // line without the trailing spaces is centered, with its left edge at
  // (100 - 30)/2 = 35.
  EXPECT_EQ(PhysicalRect(0, 0, 100, 10), TextAreaFirstLineRect("e"));
  EXPECT_EQ(PhysicalRect(35, 0, 30, 10), TextAreaFirstLineRect("f"));
}

TEST_F(InlineLayoutAlgorithmTest, TextBoxTrimConstraintSpace) {
  ScopedCSSTextBoxTrimForTest enable_text_box_trim(true);
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id="parent" style="text-box-trim: trim-both; position: relative">
      <div id="abs1" style="position: absolute">abs1</div>
      <div id="float1" style="float: left">float1</div>
      <div id="empty_before"> </div>
      <div id="nested_empty_before">
        <div id="nested_empty_before_child"> </div>
      </div>
      <div>
        <div id="first">first<br>first L2</div>
      </div>
      <div id="middle">middle</div>
      <div>
        <div id="last">last<br>last L2</div>
      </div>
      <div id="nested_empty_after">
        <div id="nested_empty_after_child"> </div>
      </div>
      <div id="empty_after"> </div>
      <div id="abs2" style="position: absolute">abs1</div>
      <div id="float2" style="float: left">float1</div>
    </div>
  )HTML");

  const TextBoxTrimResult parent{*GetLayoutBlockFlowByElementId("parent")};
  EXPECT_FALSE(parent.should_trim_start);
  EXPECT_FALSE(parent.should_trim_end);
  EXPECT_TRUE(parent.is_trimmed);

  // `ShouldTextBoxTrim*` should be set only to in-flow children.
  for (const char* id :
       {"abs1", "abs2", "float1", "float2", "middle", "empty_after",
        "nested_empty_after", "nested_empty_after_child"}) {
    const TextBoxTrimResult result{*GetLayoutBlockFlowByElementId(id)};
    EXPECT_FALSE(result.should_trim_start) << id;
    EXPECT_FALSE(result.should_trim_end) << id;
    EXPECT_FALSE(result.is_trimmed) << id;
  }

  for (const char* id :
       {"empty_before", "nested_empty_before", "nested_empty_before_child"}) {
    const TextBoxTrimResult result{*GetLayoutBlockFlowByElementId(id)};
    EXPECT_TRUE(result.should_trim_start) << id;
    EXPECT_FALSE(result.should_trim_end) << id;
    EXPECT_FALSE(result.is_trimmed) << id;
  }

  const TextBoxTrimResult first{*GetLayoutBlockFlowByElementId("first")};
  EXPECT_TRUE(first.should_trim_start);
  EXPECT_FALSE(first.should_trim_end);
  EXPECT_TRUE(first.is_trimmed);

  const TextBoxTrimResult last{*GetLayoutBlockFlowByElementId("last")};
  EXPECT_FALSE(last.should_trim_start);
  EXPECT_TRUE(last.should_trim_end);
  EXPECT_TRUE(last.is_trimmed);
}

TEST_F(InlineLayoutAlgorithmTest, TextBoxTrimConstraintSpaceSingle) {
  ScopedCSSTextBoxTrimForTest enable_text_box_trim(true);
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id="parent" style="text-box-trim: trim-both">
      <div id="single">single<br>single L2</div>
      <div id="empty_after"> </div>
    </div>
  )HTML");

  const TextBoxTrimResult parent{*GetLayoutBlockFlowByElementId("parent")};
  EXPECT_FALSE(parent.should_trim_start);
  EXPECT_FALSE(parent.should_trim_end);
  EXPECT_TRUE(parent.is_trimmed);

  const TextBoxTrimResult single{*GetLayoutBlockFlowByElementId("single")};
  EXPECT_TRUE(single.should_trim_start);
  EXPECT_TRUE(single.should_trim_end);
  EXPECT_TRUE(single.is_trimmed);

  const TextBoxTrimResult empty_after{
      *GetLayoutBlockFlowByElementId("empty_after")};
  EXPECT_FALSE(empty_after.should_trim_start);
  EXPECT_FALSE(empty_after.should_trim_end);
  EXPECT_FALSE(empty_after.is_trimmed);
}

TEST_F(InlineLayoutAlgorithmTest, TextBoxTrimConstraintSpaceEmptyOnly) {
  ScopedCSSTextBoxTrimForTest enable_text_box_trim(true);
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id="parent" style="text-box-trim: trim-both">
      <div id="empty"> </div>
    </div>
  )HTML");

  const TextBoxTrimResult parent{*GetLayoutBlockFlowByElementId("parent")};
  EXPECT_FALSE(parent.should_trim_start);
  EXPECT_FALSE(parent.should_trim_end);
  EXPECT_FALSE(parent.is_trimmed);

  // Note: the current implementation sets `should_trim_start` to `true` even if
  // it knows `empty` is empty. It can be either `true` or `false`. `EXPECT`
  // exists just to verify the current implementation works as intended.
  const TextBoxTrimResult empty{*GetLayoutBlockFlowByElementId("empty")};
  EXPECT_TRUE(empty.should_trim_start);
  EXPECT_FALSE(empty.should_trim_end);
  EXPECT_FALSE(empty.is_trimmed);
}

TEST_F(InlineLayoutAlgorithmTest, TextBoxTrimConstraintSpaceNone) {
  ScopedCSSTextBoxTrimForTest enable_text_box_trim(true);
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id="parent" style="text-box-trim: both">
    </div>
  )HTML");

  const TextBoxTrimResult parent{*GetLayoutBlockFlowByElementId("parent")};
  EXPECT_FALSE(parent.should_trim_start);
  EXPECT_FALSE(parent.should_trim_end);
  EXPECT_FALSE(parent.is_trimmed);
}

#undef MAYBE_VerticalAlignBottomReplaced

// crbug.com/341126037
TEST_F(InlineLayoutAlgorithmTest, BoxFragmentInRubyCrash) {
  SetBodyInnerHTML(R"HTML(
<table>
<caption>
<ruby>
<select></select>
<svg></svg>
<span dir="rtl">
</span>
foo
<rt>
<input></ruby>)HTML");
  // We had a crash in a case that the first base item in a kOpenRubyColumn
  // InlineItemResult creates a BoxFragment

  // This test passes if no crashes.
}

}  // namespace
}  // namespace blink
