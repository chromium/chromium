// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

using testing::ElementsAre;

namespace blink {

class NGFragmentItemTest : public NGLayoutTest {
 public:
  void ForceLayout() { RunDocumentLifecycle(); }

  LayoutBlockFlow* GetLayoutBlockFlowByElementId(const char* id) {
    return To<LayoutBlockFlow>(GetLayoutObjectByElementId(id));
  }

  Vector<NGInlineCursorPosition> GetLines(NGInlineCursor* cursor) {
    Vector<NGInlineCursorPosition> lines;
    for (cursor->MoveToFirstLine(); *cursor; cursor->MoveToNextLine())
      lines.push_back(cursor->Current());
    return lines;
  }

  wtf_size_t IndexOf(const Vector<NGInlineCursorPosition>& items,
                     const NGFragmentItem* target) {
    wtf_size_t index = 0;
    for (const auto& item : items) {
      if (item.Item() == target)
        return index;
      ++index;
    }
    return kNotFound;
  }

  void TestFirstDirtyLineIndex(const char* id, wtf_size_t expected_index) {
    LayoutBlockFlow* block_flow = GetLayoutBlockFlowByElementId(id);
    const NGPhysicalBoxFragment* fragment = block_flow->GetPhysicalFragment(0);
    const NGFragmentItems* items = fragment->Items();
    NGFragmentItems::DirtyLinesFromNeedsLayout(*block_flow);
    const NGFragmentItem* end_reusable_item =
        items->EndOfReusableItems(*fragment);

    NGInlineCursor cursor(*fragment, *items);
    const auto lines = GetLines(&cursor);
    EXPECT_EQ(IndexOf(lines, end_reusable_item), expected_index);
  }

  Vector<const NGFragmentItem*> ItemsForAsVector(
      const LayoutObject& layout_object) {
    Vector<const NGFragmentItem*> list;
    NGInlineCursor cursor;
    for (cursor.MoveTo(layout_object); cursor;
         cursor.MoveToNextForSameLayoutObject()) {
      DCHECK(cursor.Current().Item());
      const NGFragmentItem& item = *cursor.Current().Item();
      EXPECT_EQ(item.GetLayoutObject(), &layout_object);
      list.push_back(&item);
    }
    return list;
  }
};

TEST_F(NGFragmentItemTest, CopyMove) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      font-size: 20px;
      line-height: 10px;
    }
    </style>
    <div id="container">
      1234567
    </div>
  )HTML");
  LayoutBlockFlow* container =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  NGInlineCursor cursor(*container);

  // Test copying a line item.
  cursor.MoveToFirstLine();
  const NGFragmentItem* line_item = cursor.Current().Item();
  EXPECT_EQ(line_item->Type(), NGFragmentItem::kLine);
  EXPECT_NE(line_item->LineBoxFragment(), nullptr);
  NGFragmentItem copy_of_line(*line_item);
  EXPECT_EQ(copy_of_line.LineBoxFragment(), line_item->LineBoxFragment());
  EXPECT_TRUE(copy_of_line.IsInkOverflowComputed());

  // Test moving a line item.
  NGFragmentItem move_of_line(std::move(copy_of_line));
  EXPECT_EQ(move_of_line.LineBoxFragment(), line_item->LineBoxFragment());
  EXPECT_TRUE(move_of_line.IsInkOverflowComputed());

  // To test moving ink overflow, add an ink overflow to |move_of_line|.
  PhysicalRect not_small_ink_overflow_rect(0, 0, 5000, 100);
  move_of_line.ink_overflow_type_ =
      static_cast<int>(move_of_line.ink_overflow_.SetContents(
          move_of_line.InkOverflowType(), not_small_ink_overflow_rect,
          line_item->Size()));
  EXPECT_EQ(move_of_line.InkOverflowType(), NGInkOverflow::Type::kContents);
  NGFragmentItem move_of_line2(std::move(move_of_line));
  EXPECT_EQ(move_of_line2.InkOverflowType(), NGInkOverflow::Type::kContents);
  EXPECT_EQ(move_of_line2.InkOverflow(), not_small_ink_overflow_rect);

  // Test copying a text item.
  cursor.MoveToFirstChild();
  const NGFragmentItem* text_item = cursor.Current().Item();
  EXPECT_EQ(text_item->Type(), NGFragmentItem::kText);
  EXPECT_NE(text_item->TextShapeResult(), nullptr);
  NGFragmentItem copy_of_text(*text_item);
  EXPECT_EQ(copy_of_text.TextShapeResult(), text_item->TextShapeResult());
  // Ink overflow is copied for text items. See |NGFragmentItem| copy ctor.
  EXPECT_TRUE(copy_of_text.IsInkOverflowComputed());

  // Test moving a text item.
  NGFragmentItem move_of_text(std::move(copy_of_text));
  EXPECT_EQ(move_of_text.TextShapeResult(), text_item->TextShapeResult());
  // After the move, the source ShapeResult should be released.
  EXPECT_EQ(copy_of_text.TextShapeResult(), nullptr);
  EXPECT_TRUE(move_of_text.IsInkOverflowComputed());
}

TEST_F(NGFragmentItemTest, BasicText) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
      font-family: Ahem;
      font-size: 10px;
      line-height: 1;
    }
    div {
      width: 10ch;
    }
    </style>
    <div id="container">
      1234567 98765
    </div>
  )HTML");

  auto* container =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  auto* layout_text = To<LayoutText>(container->FirstChild());
  const NGPhysicalBoxFragment* box = container->GetPhysicalFragment(0);
  EXPECT_NE(box, nullptr);
  const NGFragmentItems* items = box->Items();
  EXPECT_NE(items, nullptr);
  EXPECT_EQ(items->Items().size(), 4u);

  // The text node wraps, produces two fragments.
  Vector<const NGFragmentItem*> items_for_text = ItemsForAsVector(*layout_text);
  EXPECT_EQ(items_for_text.size(), 2u);

  const NGFragmentItem& text1 = *items_for_text[0];
  EXPECT_EQ(text1.Type(), NGFragmentItem::kText);
  EXPECT_EQ(text1.GetLayoutObject(), layout_text);
  EXPECT_EQ(text1.OffsetInContainerFragment(), PhysicalOffset());
  EXPECT_TRUE(text1.IsFirstForNode());
  EXPECT_FALSE(text1.IsLastForNode());

  const NGFragmentItem& text2 = *items_for_text[1];
  EXPECT_EQ(text2.Type(), NGFragmentItem::kText);
  EXPECT_EQ(text2.GetLayoutObject(), layout_text);
  EXPECT_EQ(text2.OffsetInContainerFragment(), PhysicalOffset(0, 10));
  EXPECT_FALSE(text2.IsFirstForNode());
  EXPECT_TRUE(text2.IsLastForNode());
}

TEST_F(NGFragmentItemTest, RtlText) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      font-family: Ahem;
      font-size: 10px;
      width: 10ch;
      direction: rtl;
    }
    </style>
    <div id="container">
      <span id="span" style="background:hotpink;">
        11111. 22222.
      </span>
    </div>
  )HTML");

  auto* container =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  LayoutObject* span = GetLayoutObjectByElementId("span");
  auto* layout_text = To<LayoutText>(span->SlowFirstChild());
  const NGPhysicalBoxFragment* box = container->GetPhysicalFragment(0);
  EXPECT_NE(box, nullptr);
  const NGFragmentItems* items = box->Items();
  EXPECT_NE(items, nullptr);
  EXPECT_EQ(items->Items().size(), 8u);

  Vector<const NGFragmentItem*> items_for_span = ItemsForAsVector(*span);
  EXPECT_EQ(items_for_span.size(), 2u);
  const NGFragmentItem* item = items_for_span[0];
  EXPECT_TRUE(item->IsFirstForNode());
  EXPECT_FALSE(item->IsLastForNode());

  item = items_for_span[1];
  EXPECT_FALSE(item->IsFirstForNode());
  EXPECT_TRUE(item->IsLastForNode());

  Vector<const NGFragmentItem*> items_for_text = ItemsForAsVector(*layout_text);
  EXPECT_EQ(items_for_text.size(), 4u);

  item = items_for_text[0];
  EXPECT_EQ(item->Text(*items).ToString(), String("."));
  EXPECT_TRUE(item->IsFirstForNode());
  EXPECT_FALSE(item->IsLastForNode());

  item = items_for_text[1];
  EXPECT_EQ(item->Text(*items).ToString(), String("11111"));
  EXPECT_FALSE(item->IsFirstForNode());
  EXPECT_FALSE(item->IsLastForNode());

  item = items_for_text[2];
  EXPECT_EQ(item->Text(*items).ToString(), String("."));
  EXPECT_FALSE(item->IsFirstForNode());
  EXPECT_FALSE(item->IsLastForNode());

  item = items_for_text[3];
  EXPECT_EQ(item->Text(*items).ToString(), String("22222"));
  EXPECT_FALSE(item->IsFirstForNode());
  EXPECT_TRUE(item->IsLastForNode());
}

TEST_F(NGFragmentItemTest, BasicInlineBox) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
      font-family: Ahem;
      font-size: 10px;
      line-height: 1;
    }
    #container {
      width: 10ch;
    }
    #span1, #span2 {
      background: gray;
    }
    </style>
    <div id="container">
      000
      <span id="span1">1234 5678</span>
      999
      <span id="span2">12345678</span>
    </div>
  )HTML");

  // "span1" wraps, produces two fragments.
  const LayoutObject* span1 = GetLayoutObjectByElementId("span1");
  ASSERT_NE(span1, nullptr);
  Vector<const NGFragmentItem*> items_for_span1 = ItemsForAsVector(*span1);
  EXPECT_EQ(items_for_span1.size(), 2u);
  EXPECT_TRUE(items_for_span1[0]->IsFirstForNode());
  EXPECT_FALSE(items_for_span1[0]->IsLastForNode());
  EXPECT_EQ(PhysicalOffset(40, 0),
            items_for_span1[0]->OffsetInContainerFragment());
  EXPECT_EQ(PhysicalRect(0, 0, 40, 10), items_for_span1[0]->InkOverflow());
  EXPECT_FALSE(items_for_span1[1]->IsFirstForNode());
  EXPECT_TRUE(items_for_span1[1]->IsLastForNode());
  EXPECT_EQ(PhysicalOffset(0, 10),
            items_for_span1[1]->OffsetInContainerFragment());
  EXPECT_EQ(PhysicalRect(0, 0, 40, 10), items_for_span1[1]->InkOverflow());

  // "span2" doesn't wrap, produces only one fragment.
  const LayoutObject* span2 = GetLayoutObjectByElementId("span2");
  ASSERT_NE(span2, nullptr);
  Vector<const NGFragmentItem*> items_for_span2 = ItemsForAsVector(*span2);
  EXPECT_EQ(items_for_span2.size(), 1u);
  EXPECT_TRUE(items_for_span2[0]->IsFirstForNode());
  EXPECT_TRUE(items_for_span2[0]->IsLastForNode());
  EXPECT_EQ(PhysicalOffset(0, 20),
            items_for_span2[0]->OffsetInContainerFragment());
  EXPECT_EQ(PhysicalRect(0, 0, 80, 10), items_for_span2[0]->InkOverflow());
}

// Same as |BasicInlineBox| but `<span>`s do not have background.
// They will not produce fragment items, but all operations should work the
// same.
TEST_F(NGFragmentItemTest, CulledInlineBox) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    html, body {
      margin: 0;
      font-family: Ahem;
      font-size: 10px;
      line-height: 1;
    }
    #container {
      width: 10ch;
    }
    </style>
    <div id="container">
      000
      <span id="span1">1234 5678</span>
      999
      <span id="span2">12345678</span>
    </div>
  )HTML");

  // "span1" wraps, produces two fragments.
  const LayoutObject* span1 = GetLayoutObjectByElementId("span1");
  ASSERT_NE(span1, nullptr);
  Vector<const NGFragmentItem*> items_for_span1 = ItemsForAsVector(*span1);
  EXPECT_EQ(items_for_span1.size(), 0u);
  EXPECT_EQ(gfx::Rect(0, 0, 80, 20), span1->AbsoluteBoundingBoxRect());

  // "span2" doesn't wrap, produces only one fragment.
  const LayoutObject* span2 = GetLayoutObjectByElementId("span2");
  ASSERT_NE(span2, nullptr);
  Vector<const NGFragmentItem*> items_for_span2 = ItemsForAsVector(*span2);
  EXPECT_EQ(items_for_span2.size(), 0u);
  EXPECT_EQ(gfx::Rect(0, 20, 80, 10), span2->AbsoluteBoundingBoxRect());
}

TEST_F(NGFragmentItemTest, SelfPaintingInlineBox) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #self_painting_inline_box {
      opacity: .2;
    }
    </style>
    <div>
      <span id="self_painting_inline_box">self painting inline box</span>
    </div>
  )HTML");

  // Invalidate the ink overflow of a child in `#self_painting_inline_box`.
  auto* self_painting_inline_box =
      To<LayoutInline>(GetLayoutObjectByElementId("self_painting_inline_box"));
  ASSERT_TRUE(self_painting_inline_box->HasSelfPaintingLayer());
  auto* text = To<LayoutText>(self_painting_inline_box->FirstChild());
  text->InvalidateVisualOverflow();

  // Mark the |PaintLayer| to need to recalc visual overflow.
  self_painting_inline_box->Layer()->SetNeedsVisualOverflowRecalc();
  RunDocumentLifecycle();

  // Test if it recalculated the ink overflow.
  NGInlineCursor cursor;
  for (cursor.MoveTo(*text); cursor; cursor.MoveToNextForSameLayoutObject())
    EXPECT_TRUE(cursor.Current()->IsInkOverflowComputed());
}

TEST_F(NGFragmentItemTest, StartOffsetInContainer) {
  SetBodyInnerHTML(R"HTML(
    <style>
    atomic {
      display: inline-block;
      width: 3ch;
    }
    </style>
    <div id="container" style="font-size: 10px; width: 3ch">
      012&shy;456&shy;<span>8</span>90&shy;<atomic></atomic>
    </div>
  )HTML");
  auto* container =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  NGInlineCursor cursor(*container);
  while (!cursor.Current()->IsLayoutGeneratedText())
    cursor.MoveToNext();
  EXPECT_EQ(4u, cursor.Current()->StartOffsetInContainer(cursor));
  for (cursor.MoveToNext(); !cursor.Current()->IsLayoutGeneratedText();)
    cursor.MoveToNext();
  EXPECT_EQ(8u, cursor.Current()->StartOffsetInContainer(cursor));
  for (cursor.MoveToNext(); !cursor.Current()->IsLayoutGeneratedText();)
    cursor.MoveToNext();
  EXPECT_EQ(12u, cursor.Current()->StartOffsetInContainer(cursor));
}

TEST_F(NGFragmentItemTest, EllipsizedAtomicInline) {
  SetBodyInnerHTML(R"HTML(
    <style>
    #container {
      width: 100px;
      white-space: pre;
      text-overflow: ellipsis;
      overflow: hidden;
    }
    #atomic {
      display: inline-block;
      width: 200px;
    }
    </style>
    <div id="container"><span id="atomic"> </span>XXXXXX</div>
  )HTML");
  auto* container =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  auto* atomic = GetLayoutObjectByElementId("atomic");
  NGInlineCursor cursor(*container);
  cursor.MoveToNext();
  EXPECT_EQ(cursor.Current().GetLayoutObject(), atomic);
  EXPECT_EQ(cursor.Current()->Type(), NGFragmentItem::kBox);
  // When atomic inline is ellipsized, |IsLastForNode| should be set to the last
  // |kBox| item, even if ellipses follow.
  EXPECT_TRUE(cursor.Current()->IsLastForNode());
  cursor.MoveToNext();
  EXPECT_EQ(cursor.Current()->Type(), NGFragmentItem::kText);
  cursor.MoveToNext();
  EXPECT_EQ(cursor.Current().GetLayoutObject(), atomic);
  EXPECT_EQ(cursor.Current()->Type(), NGFragmentItem::kGeneratedText);
  EXPECT_TRUE(cursor.Current()->IsLastForNode());
}

TEST_F(NGFragmentItemTest, LineFragmentId) {
  ScopedLayoutNGBlockFragmentationForTest ng_block_frag(true);
  SetBodyInnerHTML(R"HTML(
    <style>
    #columns {
      columns: 2;
      column-fill: auto;
      line-height: 1em;
      height: 3em;
    }
    </style>
    <body>
      <div id="columns">
        <div id="target">
          1<br>
          2<br>
          3<br>
          4<br>
          5<br>
          6
        </div>
      </div>
    </body>
  )HTML");
  auto* target = To<LayoutBlockFlow>(GetLayoutObjectByElementId("target"));
  NGInlineCursor cursor(*target);
  wtf_size_t line_index = 0;
  for (cursor.MoveToFirstLine(); cursor;
       cursor.MoveToNextLineIncludingFragmentainer(), ++line_index) {
    EXPECT_EQ(cursor.Current()->FragmentId(),
              line_index + NGFragmentItem::kInitialLineFragmentId);
  }
  EXPECT_EQ(line_index, 6u);
}

TEST_F(NGFragmentItemTest, Outline) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
    #target {
      font-family: Ahem;
      font-size: 10px;
      width: 200px;
    }
    .inline-box {
      border: 5px solid blue;
    }
    .inline-block {
      display: inline-block;
    }
    </style>
    <div id="target">
      <span class="inline-box">
        <span class="inline-block">X<span>
      </span>
    </div>
  )HTML");
  auto* target = To<LayoutBlockFlow>(GetLayoutObjectByElementId("target"));
  Vector<PhysicalRect> rects = target->OutlineRects(
      nullptr, PhysicalOffset(), NGOutlineType::kIncludeBlockVisualOverflow);
  EXPECT_THAT(rects,
              testing::ElementsAre(
                  PhysicalRect(0, 0, 200, 10),   // <div id="target">
                  PhysicalRect(5, 0, 10, 10),    // <span class="inline-box">
                  PhysicalRect(5, 0, 10, 10)));  // <span class="inline-block">
}

// Various nodes/elements to test insertions.
using CreateNode = Node* (*)(Document&);
static CreateNode node_creators[] = {
    [](Document& document) -> Node* { return document.createTextNode("new"); },
    [](Document& document) -> Node* {
      return document.CreateRawElement(html_names::kSpanTag);
    },
    [](Document& document) -> Node* {
      Element* element = document.CreateRawElement(html_names::kSpanTag);
      element->classList().Add("abspos");
      return element;
    },
    [](Document& document) -> Node* {
      Element* element = document.CreateRawElement(html_names::kSpanTag);
      element->classList().Add("float");
      return element;
    }};

class FragmentItemInsertTest : public NGFragmentItemTest,
                               public testing::WithParamInterface<CreateNode> {
};

INSTANTIATE_TEST_SUITE_P(NGFragmentItemTest,
                         FragmentItemInsertTest,
                         testing::ValuesIn(node_creators));

// Various nodes/elements to test removals.
class FragmentItemRemoveTest : public NGFragmentItemTest,
                               public testing::WithParamInterface<const char*> {
};

INSTANTIATE_TEST_SUITE_P(
    NGFragmentItemTest,
    FragmentItemRemoveTest,
    testing::Values("text",
                    "<span>span</span>",
                    "<span>1234 12345678</span>",
                    "<span style='display: inline-block'>box</span>",
                    "<img>",
                    "<div style='float: left'>float</div>",
                    "<div style='position: absolute'>abs</div>"));

// Test marking line boxes when inserting a span before the first child.
TEST_P(FragmentItemInsertTest, MarkLineBoxesDirtyOnInsert) {
  SetBodyInnerHTML(R"HTML(
    <style>
    .abspos { position: absolute; }
    .float { float: left; }
    </style>
    <div id=container style="font-size: 10px; width: 10ch">
      12345678
    </div>
  )HTML");
  Node* insert = (*GetParam())(GetDocument());
  Element* container = GetElementById("container");
  container->insertBefore(insert, container->firstChild());
  TestFirstDirtyLineIndex("container", 0);
}

// Test marking line boxes when appending a span.
TEST_P(FragmentItemInsertTest, MarkLineBoxesDirtyOnAppend) {
  SetBodyInnerHTML(R"HTML(
    <style>
    .abspos { position: absolute; }
    .float { float: left; }
    </style>
    <div id=container style="font-size: 10px; width: 10ch">
      12345678
    </div>
  )HTML");
  Node* insert = (*GetParam())(GetDocument());
  Element* container = GetElementById("container");
  container->appendChild(insert);
  TestFirstDirtyLineIndex("container", 0);
}

// Test marking line boxes when appending a span on 2nd line.
TEST_P(FragmentItemInsertTest, MarkLineBoxesDirtyOnAppend2) {
  SetBodyInnerHTML(R"HTML(
    <style>
    .abspos { position: absolute; }
    .float { float: left; }
    </style>
    <div id=container style="font-size: 10px; width: 10ch">
      12345678
      2234
    </div>
  )HTML");
  Node* insert = (*GetParam())(GetDocument());
  Element* container = GetElementById("container");
  container->appendChild(insert);
  TestFirstDirtyLineIndex("container", 1);
}

// Test marking line boxes when appending a span on 2nd line.
TEST_P(FragmentItemInsertTest, MarkLineBoxesDirtyOnAppendAfterBR) {
  SetBodyInnerHTML(R"HTML(
    <style>
    .abspos { position: absolute; }
    .float { float: left; }
    </style>
    <div id=container style="font-size: 10px; width: 10ch">
      <br>
      <br>
    </div>
  )HTML");
  Node* insert = (*GetParam())(GetDocument());
  Element* container = GetElementById("container");
  container->appendChild(insert);
  TestFirstDirtyLineIndex("container", 1);
}

// Test marking line boxes when removing a span.
TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyOnRemove) {
  SetBodyInnerHTML(R"HTML(
    <div id=container style="font-size: 10px; width: 10ch">
      1234<span id=t>5678</span>
    </div>
  )HTML");
  Element* span = GetElementById("t");
  span->remove();
  // TODO(kojii): This can be optimized more.
  TestFirstDirtyLineIndex("container", 0);
}

// Test marking line boxes when removing a span.
TEST_P(FragmentItemRemoveTest, MarkLineBoxesDirtyOnRemoveFirst) {
  SetBodyInnerHTML(String(R"HTML(
    <div id=container style="font-size: 10px; width: 10ch">)HTML") +
                   GetParam() + R"HTML(<span>after</span>
    </div>
  )HTML");

  Element* container = GetElementById("container");
  Node* node = container->firstChild();
  ASSERT_TRUE(node);
  node->remove();
  TestFirstDirtyLineIndex("container", 0);
}

// Test marking line boxes when removing a span on 2nd line.
TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyOnRemove2) {
  SetBodyInnerHTML(R"HTML(
    <div id=container style="font-size: 10px; width: 10ch">
      12345678
      2234<span id=t>5678 3334</span>
    </div>
  )HTML");
  Element* span = GetElementById("t");
  span->remove();
  // TODO(kojii): This can be optimized more.
  TestFirstDirtyLineIndex("container", 0);
}

// Test marking line boxes when removing a text node on 2nd line.
TEST_P(FragmentItemRemoveTest, MarkLineBoxesDirtyOnRemoveAfterBR) {
  SetBodyInnerHTML(String(R"HTML(
    <div id=container style="font-size: 10px; width: 10ch">
      line 1
      <br>)HTML") + GetParam() +
                   "</div>");
  Element* container = GetElementById("container");
  Node* node = container->lastChild();
  ASSERT_TRUE(node);
  node->remove();
  // TODO(kojii): This can be optimized more.
  TestFirstDirtyLineIndex("container", 0);

  ForceLayout();  // Ensure running layout does not crash.
}

TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyOnEndSpaceCollapsed) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      font-size: 10px;
      width: 8ch;
    }
    #empty {
      background: yellow; /* ensure fragment is created */
    }
    #target {
      display: inline-block;
    }
    </style>
    <div id=container>
      1234567890
      1234567890
      <span id=empty> </span>
      <span id=target></span></div>
  )HTML");
  // Removing #target makes the spaces before it to be collapsed.
  Element* target = GetElementById("target");
  target->remove();
  // TODO(kojii): This can be optimized more.
  TestFirstDirtyLineIndex("container", 1);

  ForceLayout();  // Ensure running layout does not crash.
}

// Test marking line boxes when the first span has NeedsLayout. The span is
// culled.
TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyOnNeedsLayoutFirst) {
  SetBodyInnerHTML(R"HTML(
    <div id=container style="font-size: 10px; width: 10ch">
      <span id=t>1234</span>5678
    </div>
  )HTML");
  LayoutObject* span = GetLayoutObjectByElementId("t");
  span->SetNeedsLayout("");
  // TODO(kojii): This can be optimized more.
  TestFirstDirtyLineIndex("container", 0);
}

// Test marking line boxes when the first span has NeedsLayout. The span has a
// box fragment.
TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyOnNeedsLayoutFirstWithBox) {
  SetBodyInnerHTML(R"HTML(
    <div id=container style="font-size: 10px; width: 10ch">
      <span id=t style="background: blue">1234</span>5678
    </div>
  )HTML");
  LayoutObject* span = GetLayoutObjectByElementId("t");
  span->SetNeedsLayout("");
  // TODO(kojii): This can be optimized more.
  TestFirstDirtyLineIndex("container", 0);
}

// Test marking line boxes when a span has NeedsLayout. The span is culled.
TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyOnNeedsLayout) {
  SetBodyInnerHTML(R"HTML(
    <div id=container style="font-size: 10px; width: 10ch">
      12345678
      2234<span id=t>5678 3334</span>
    </div>
  )HTML");
  LayoutObject* span = GetLayoutObjectByElementId("t");
  span->SetNeedsLayout("");
  // TODO(kojii): This can be optimized more.
  TestFirstDirtyLineIndex("container", 0);
}

// Test marking line boxes when a span has NeedsLayout. The span has a box
// fragment.
TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyOnNeedsLayoutWithBox) {
  SetBodyInnerHTML(R"HTML(
    <div id=container style="font-size: 10px; width: 10ch">
      12345678
      2234<span id=t style="background: blue">5678 3334</span>
    </div>
  )HTML");
  LayoutObject* span = GetLayoutObjectByElementId("t");
  span->SetNeedsLayout("");
  // TODO(kojii): This can be optimized more.
  TestFirstDirtyLineIndex("container", 0);
}

// Test marking line boxes when a span inside a span has NeedsLayout.
// The parent span has a box fragment, and wraps, so that its fragment
// is seen earlier in pre-order DFS.
TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyOnChildOfWrappedBox) {
  SetBodyInnerHTML(R"HTML(
    <div id=container style="font-size: 10px">
      <span style="background: yellow">
        <span id=t>target</span>
        <br>
        12345678
      </span>
    </div>
  )HTML");
  LayoutObject* span = GetLayoutObjectByElementId("t");
  span->SetNeedsLayout("");
  TestFirstDirtyLineIndex("container", 0);
}

// Test marking line boxes when a span has NeedsLayout. The span has a box
// fragment.
TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyInInlineBlock) {
  SetBodyInnerHTML(R"HTML(
    <div id=container style="display: inline-block; font-size: 10px">
      12345678<br>
      12345678<br>
    </div>
  )HTML");
  Element* container = GetElementById("container");
  container->appendChild(GetDocument().createTextNode("append"));
  // TODO(kojii): This can be optimized more.
  TestFirstDirtyLineIndex("container", 1);
}

TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyByRemoveChildAfterForcedBreak) {
  SetBodyInnerHTML(R"HTML(
    <div id=container>
      line 1<br>
      <b id=target>line 2</b><br>
      line 3<br>
    </div>
  )HTML");
  Element& target = *GetDocument().getElementById("target");
  target.remove();
  // TODO(kojii): This can be optimized more.
  TestFirstDirtyLineIndex("container", 0);
}

TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyByRemoveForcedBreak) {
  SetBodyInnerHTML(R"HTML(
    <div id=container>
      line 1<br>
      line 2<br id=target>
      line 3<br>
    </div>"
  )HTML");
  Element& target = *GetDocument().getElementById("target");
  target.remove();
  // TODO(kojii): This can be optimized more.
  TestFirstDirtyLineIndex("container", 0);
}

TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyByRemoveSpanWithForcedBreak) {
  SetBodyInnerHTML(R"HTML(
    <div id=container>
      line 1<br>
      line 2<span id=target><br>
      </span>line 3<br>
    </div>
  )HTML");
  // |target| is a culled inline box. There is no fragment in fragment tree.
  Element& target = *GetDocument().getElementById("target");
  target.remove();
  // TODO(kojii): This can be optimized more.
  TestFirstDirtyLineIndex("container", 0);
}

TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyByInsertAtStart) {
  SetBodyInnerHTML(R"HTML(
    <div id=container>
      line 1<br>
      <b id=target>line 2</b><br>
      line 3<br>
    </div>
  )HTML");
  Element& target = *GetDocument().getElementById("target");
  target.parentNode()->insertBefore(Text::Create(GetDocument(), "XYZ"),
                                    &target);
  GetDocument().UpdateStyleAndLayoutTree();
  // TODO(kojii): This can be optimized more.
  TestFirstDirtyLineIndex("container", 0);
}

TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyByInsertAtLast) {
  SetBodyInnerHTML(R"HTML(
    <div id=container>
      line 1<br>
      <b id=target>line 2</b><br>
      line 3<br>
    </div>
  )HTML");
  Element& target = *GetDocument().getElementById("target");
  target.parentNode()->appendChild(Text::Create(GetDocument(), "XYZ"));
  GetDocument().UpdateStyleAndLayoutTree();
  TestFirstDirtyLineIndex("container", 1);
}

TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyByInsertAtMiddle) {
  SetBodyInnerHTML(R"HTML(
    <div id=container>
      line 1<br>
      <b id=target>line 2</b><br>
      line 3<br>
    </div>
  )HTML");
  Element& target = *GetDocument().getElementById("target");
  target.parentNode()->insertBefore(Text::Create(GetDocument(), "XYZ"),
                                    target.nextSibling());
  GetDocument().UpdateStyleAndLayoutTree();
  // TODO(kojii): This can be optimized more.
  TestFirstDirtyLineIndex("container", 0);
}

TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyByTextSetData) {
  SetBodyInnerHTML(R"HTML(
    <div id=container>
      line 1<br>
      <b id=target>line 2</b><br>
      line 3<br>
    </div>
  )HTML");
  Element& target = *GetDocument().getElementById("target");
  To<Text>(*target.firstChild()).setData("abc");
  // TODO(kojii): This can be optimized more.
  TestFirstDirtyLineIndex("container", 0);
}

TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyWrappedLine) {
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
    </div>
  )HTML");
  Element& target = *GetDocument().getElementById("target");
  target.remove();
  // TODO(kojii): This can be optimized more.
  TestFirstDirtyLineIndex("container", 0);
}

TEST_F(NGFragmentItemTest, MarkLineBoxesDirtyInsideInlineBlock) {
  SetBodyInnerHTML(R"HTML(
    <div id=container>
      <div id="inline-block" style="display: inline-block">
        <span id="target">DELETE ME</span>
      </div>
    </div>
  )HTML");
  Element& target = *GetDocument().getElementById("target");
  target.remove();
  TestFirstDirtyLineIndex("container", 0);
}

// This test creates various types of |NGFragmentItem| to check "natvis" (Native
// DebugVisualizers) for Windows Visual Studio.
TEST_F(NGFragmentItemTest, Disabled_DebugVisualizers) {
  SetBodyInnerHTML(R"HTML(
    <div id=container>
      text
      <span style="display: inline-block"></span>
    </div>
  )HTML");
  auto* container =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("container"));
  NGInlineCursor cursor(*container);
  cursor.MoveToFirstLine();
  const NGFragmentItem* line = cursor.Current().Item();
  EXPECT_NE(line, nullptr);
  cursor.MoveToNext();
  const NGFragmentItem* text = cursor.Current().Item();
  EXPECT_NE(text, nullptr);
  cursor.MoveToNext();
  const NGFragmentItem* box = cursor.Current().Item();
  EXPECT_NE(box, nullptr);
}

}  // namespace blink
