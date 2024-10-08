// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/local_caret_rect.h"

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

bool operator==(const LocalCaretRect& rect1, const LocalCaretRect& rect2) {
  return rect1.layout_object == rect2.layout_object && rect1.rect == rect2.rect;
}

std::ostream& operator<<(std::ostream& out, const LocalCaretRect& caret_rect) {
  return out << "layout_object = " << caret_rect.layout_object
             << ", rect = " << caret_rect.rect;
}

class LocalCaretRectTest : public EditingTestBase {
 protected:
  LocalCaretRect LocalCaretRectOf(
      const Position& position,
      EditingBoundaryCrossingRule rule = kCanCrossEditingBoundary) {
    return LocalCaretRectOfPosition(PositionWithAffinity(position), rule);
  }
};

TEST_F(LocalCaretRectTest, DOMAndFlatTrees) {
  const char* body_content =
      "<p id='host'><b slot='#one' id='one'>1</b></p><b id='two'>22</b>";
  const char* shadow_content =
      "<b id='two'>22</b><slot name=#one></slot><b id='three'>333</b>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* one = GetDocument().getElementById(AtomicString("one"));

  const LocalCaretRect& caret_rect_from_dom_tree = LocalCaretRectOfPosition(
      PositionWithAffinity(Position(one->firstChild(), 0)));

  const LocalCaretRect& caret_rect_from_flat_tree = LocalCaretRectOfPosition(
      PositionInFlatTreeWithAffinity(PositionInFlatTree(one->firstChild(), 0)));

  EXPECT_FALSE(caret_rect_from_dom_tree.IsEmpty());
  EXPECT_EQ(caret_rect_from_dom_tree, caret_rect_from_flat_tree);
}

// http://crbug.com/1174101
TEST_F(LocalCaretRectTest, EmptyInlineFlex) {
  LoadAhem();
  InsertStyleElement(R"CSS(
    div { font: 10px/15px Ahem; width: 100px; }
    i {
        display: inline-flex;
        width: 30px; height: 30px;
        border: solid 10px red;
    })CSS");
  // |ComputeInlinePosition(AfterChildren:<div>)=AfterChildren:<b>
  // When removing <i>, we have <b>@0
  SetBodyContent(
      "<div id=target contenteditable>"
      "ab<i contenteditable=false><b></b></i></div>");
  const auto& target = *GetElementById("target");
  const auto& ab = *To<Text>(target.firstChild());
  const auto& inline_flex = *ab.nextSibling();
  const LocalCaretRect before_ab =
      LocalCaretRect(ab.GetLayoutObject(), {0, 32, 1, 10});
  const LocalCaretRect before_inline_flex =
      LocalCaretRect(ab.GetLayoutObject(), {20, 32, 1, 10});
  const LocalCaretRect after_inline_flex =
      LocalCaretRect(inline_flex.GetLayoutObject(), {49, 0, 1, 50});

  EXPECT_EQ(before_ab, LocalCaretRectOf(Position(target, 0)));
  EXPECT_EQ(before_inline_flex, LocalCaretRectOf(Position(target, 1)));
  EXPECT_EQ(after_inline_flex, LocalCaretRectOf(Position(target, 2)));
  EXPECT_EQ(before_ab, LocalCaretRectOf(Position::BeforeNode(target)));
  EXPECT_EQ(after_inline_flex, LocalCaretRectOf(Position::AfterNode(target)));
  EXPECT_EQ(after_inline_flex,
            LocalCaretRectOf(Position::LastPositionInNode(target)));
}

TEST_F(LocalCaretRectTest, SimpleText) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=div style='font: 10px/10px Ahem; width: 30px'>XXX</div>");
  const Node* foo = GetElementById("div")->firstChild();

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(29, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, MixedHeightText) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=div style='font: 10px/10px Ahem; width: 30px'>Xpp</div>");
  const Node* foo = GetElementById("div")->firstChild();

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(29, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, RtlText) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<bdo dir=rtl id=bdo style='display: block; "
      "font: 10px/10px Ahem; width: 30px'>XXX</bdo>");
  const Node* foo = GetElementById("bdo")->firstChild();

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(29, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, ClampingAndRounding) {
  // crbug.com/1228620
  LoadAhem();
  SetBodyContent(R"HTML(
      <style>
      #root {
        margin-left: 0.6px;
        width: 150.6px;
        text-align: right;
        font: 30px/30px Ahem;
      }
      </style>
      <div id=root>def</div>)HTML");
  const Node* text = GetElementById("root")->firstChild();
  EXPECT_EQ(
      LocalCaretRect(text->GetLayoutObject(), PhysicalRect(149, 0, 1, 30)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(Position(text, 3), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, OverflowTextLtr) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=root style='font: 10px/10px Ahem; width: 30px'>"
      "XXXX"
      "</div>");
  const Node* text = GetElementById("root")->firstChild();
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(39, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 4), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, UnderflowTextLtr) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=root style='font: 10px/10px Ahem; width: 30px'>"
      "XX"
      "</div>");
  const Node* text = GetElementById("root")->firstChild();
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(20, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 2), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, OverflowTextRtl) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<bdo id=root style='display:block; font: 10px/10px Ahem; width: 30px' "
      "dir=rtl>"
      "XXXX"
      "</bdo>");
  const Node* text = GetElementById("root")->firstChild();
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(29, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(
      LocalCaretRect(text->GetLayoutObject(), PhysicalRect(-10, 0, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(Position(text, 4), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, UnderflowTextRtl) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<bdo id=root style='display:block; font: 10px/10px Ahem; width: 30px' "
      "dir=rtl>"
      "XX"
      "</bdo>");
  const Node* text = GetElementById("root")->firstChild();
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(29, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 2), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, VerticalRLText) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=div style='writing-mode: vertical-rl; word-break: break-all; "
      "font: 10px/10px Ahem; width: 30px; height: 30px'>XXXYYYZZZ</div>");
  const Node* foo = GetElementById("div")->firstChild();

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 10, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 20, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kUpstream)));

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 10, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 4), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 20, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 5), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 6), TextAffinity::kUpstream)));

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 6), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 10, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 7), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 20, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 8), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 9), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, VerticalLRText) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=div style='writing-mode: vertical-lr; word-break: break-all; "
      "font: 10px/10px Ahem; width: 30px; height: 30px'>XXXYYYZZZ</div>");
  const Node* foo = GetElementById("div")->firstChild();

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 10, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 20, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kUpstream)));

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 10, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 4), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 20, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 5), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 6), TextAffinity::kUpstream)));

  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 6), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 10, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 7), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 20, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 8), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 9), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, OverflowTextVerticalLtr) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=root style='font: 10px/10px Ahem; height: 30px; writing-mode: "
      "vertical-lr'>"
      "XXXX"
      "</div>");
  const Node* text = GetElementById("root")->firstChild();
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(0, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(0, 39, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 4), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, UnderflowTextVerticalLtr) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=root style='font: 10px/10px Ahem; height: 30px; writing-mode: "
      "vertical-lr'>"
      "XX"
      "</div>");
  const Node* text = GetElementById("root")->firstChild();
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(0, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(0, 20, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 2), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, OverflowTextVerticalRtl) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<bdo id=root style='display:block; font: 10px/10px Ahem; height: 30px; "
      "writing-mode: vertical-lr' dir=rtl>"
      "XXXX"
      "</bdo>");
  const Node* text = GetElementById("root")->firstChild();
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(0, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(
      LocalCaretRect(text->GetLayoutObject(), PhysicalRect(0, -10, 10, 1)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(Position(text, 4), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, UnderflowTextVerticalRtl) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<bdo id=root style='display:block; font: 10px/10px Ahem; height: 30px; "
      "writing-mode: vertical-lr' dir=rtl>"
      "XX"
      "</bdo>");
  const Node* text = GetElementById("root")->firstChild();
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(0, 29, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 0), TextAffinity::kDownstream)));
  // LocalCaretRect may be outside the containing block.
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(0, 10, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text, 2), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, TwoLinesOfTextWithSoftWrap) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=div style='font: 10px/10px Ahem; width: 30px; "
      "word-break: break-all'>XXXXXX</div>");
  const Node* foo = GetElementById("div")->firstChild();

  // First line
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(29, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kUpstream)));

  // Second line
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(10, 10, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 4), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(20, 10, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 5), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(29, 10, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 6), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, SoftLineWrapBetweenMultipleTextNodes) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div style='font: 10px/10px Ahem; width: 30px; word-break: break-all'>"
      "<span>A</span>"
      "<span>B</span>"
      "<span id=span-c>C</span>"
      "<span id=span-d>D</span>"
      "<span>E</span>"
      "<span>F</span>"
      "</div>");
  const Node* text_c = GetElementById("span-c")->firstChild();
  const Node* text_d = GetElementById("span-d")->firstChild();

  const Position after_c(text_c, 1);
  EXPECT_EQ(
      LocalCaretRect(text_c->GetLayoutObject(), PhysicalRect(29, 0, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(after_c, TextAffinity::kUpstream)));
  EXPECT_EQ(
      LocalCaretRect(text_d->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(after_c, TextAffinity::kDownstream)));

  const Position before_d(text_d, 0);
  EXPECT_EQ(
      LocalCaretRect(text_d->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(before_d, TextAffinity::kUpstream)));
  EXPECT_EQ(
      LocalCaretRect(text_d->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(before_d, TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, SoftLineWrapBetweenMultipleTextNodesRtl) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<bdo dir=rtl style='font: 10px/10px Ahem; width: 30px; "
      "word-break: break-all; display: block'>"
      "<span>A</span>"
      "<span>B</span>"
      "<span id=span-c>C</span>"
      "<span id=span-d>D</span>"
      "<span>E</span>"
      "<span>F</span>"
      "</bdo>");
  const Node* text_c = GetElementById("span-c")->firstChild();
  const Node* text_d = GetElementById("span-d")->firstChild();

  const Position after_c(text_c, 1);
  EXPECT_EQ(
      LocalCaretRect(text_c->GetLayoutObject(), PhysicalRect(0, 0, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(after_c, TextAffinity::kUpstream)));
  EXPECT_EQ(
      LocalCaretRect(text_d->GetLayoutObject(), PhysicalRect(29, 10, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(after_c, TextAffinity::kDownstream)));

  const Position before_d(text_d, 0);
  EXPECT_EQ(
      LocalCaretRect(text_d->GetLayoutObject(), PhysicalRect(29, 10, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(before_d, TextAffinity::kUpstream)));
  EXPECT_EQ(
      LocalCaretRect(text_d->GetLayoutObject(), PhysicalRect(29, 10, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(before_d, TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, CaretRectAtBR) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div style='font: 10px/10px Ahem; width: 30px'><br>foo</div>");
  const Element& br = *GetDocument().QuerySelector(AtomicString("br"));

  EXPECT_EQ(LocalCaretRect(br.GetLayoutObject(), PhysicalRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::BeforeNode(br), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, CaretRectAtRtlBR) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<bdo dir=rtl style='display: block; font: 10px/10px Ahem; width: 30px'>"
      "<br>foo</bdo>");
  const Element& br = *GetDocument().QuerySelector(AtomicString("br"));

  EXPECT_EQ(LocalCaretRect(br.GetLayoutObject(), PhysicalRect(29, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::BeforeNode(br), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, Images) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=div style='font: 10px/10px Ahem; width: 30px'>"
      "<img id=img1 width=10px height=10px>"
      "<img id=img2 width=10px height=10px>"
      "</div>");

  const Element& img1 = *GetElementById("img1");

  EXPECT_EQ(LocalCaretRect(img1.GetLayoutObject(), PhysicalRect(0, 0, 1, 12)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::BeforeNode(img1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(img1.GetLayoutObject(), PhysicalRect(9, 0, 1, 12)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(img1), TextAffinity::kDownstream)));

  const Element& img2 = *GetElementById("img2");

  // Box-anchored LocalCaretRect is local to the box itself, instead of its
  // containing block.
  EXPECT_EQ(LocalCaretRect(img1.GetLayoutObject(), PhysicalRect(9, 0, 1, 12)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::BeforeNode(img2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(img2.GetLayoutObject(), PhysicalRect(9, 0, 1, 12)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(img2), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, RtlImages) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<bdo dir=rtl style='font: 10px/10px Ahem; width: 30px; display: block'>"
      "<img id=img1 width=10px height=10px>"
      "<img id=img2 width=10px height=10px>"
      "</bdo>");

  const Element& img1 = *GetElementById("img1");
  const Element& img2 = *GetElementById("img2");

  // Box-anchored LocalCaretRect is local to the box itself, instead of its
  // containing block.
  EXPECT_EQ(LocalCaretRect(img1.GetLayoutObject(), PhysicalRect(9, 0, 1, 12)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::BeforeNode(img1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(img2.GetLayoutObject(), PhysicalRect(9, 0, 1, 12)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(img1), TextAffinity::kDownstream)));

  EXPECT_EQ(LocalCaretRect(img2.GetLayoutObject(), PhysicalRect(9, 0, 1, 12)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::BeforeNode(img2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(img2.GetLayoutObject(), PhysicalRect(0, 0, 1, 12)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(img2), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, VerticalImage) {
  // This test only records the current behavior. Future changes are allowed.

  SetBodyContent(
      "<div style='writing-mode: vertical-rl'>"
      "<img id=img width=10px height=20px>"
      "</div>");

  const Element& img = *GetElementById("img");

  // Box-anchored LocalCaretRect is local to the box itself, instead of its
  // containing block.
  EXPECT_EQ(LocalCaretRect(img.GetLayoutObject(), PhysicalRect(0, 0, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::BeforeNode(img), TextAffinity::kDownstream)));

  EXPECT_EQ(LocalCaretRect(img.GetLayoutObject(), PhysicalRect(0, 19, 10, 1)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(img), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, TextAndImageMixedHeight) {
  // This test only records the current behavior. Future changes are allowed.

  LoadAhem();
  SetBodyContent(
      "<div id=div style='font: 10px/10px Ahem; width: 30px'>"
      "X"
      "<img id=img width=10px height=5px style='vertical-align: text-bottom'>"
      "p</div>");

  const Element& img = *GetElementById("img");
  const Node* text1 = img.previousSibling();
  const Node* text2 = img.nextSibling();

  EXPECT_EQ(LocalCaretRect(text1->GetLayoutObject(), PhysicalRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(text1, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(
      LocalCaretRect(text1->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(Position(text1, 1), TextAffinity::kDownstream)));

  EXPECT_EQ(
      LocalCaretRect(text1->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
      LocalCaretRectOfPosition(PositionWithAffinity(
          Position::BeforeNode(img), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(img.GetLayoutObject(), PhysicalRect(9, -5, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(img), TextAffinity::kDownstream)));

  EXPECT_EQ(
      LocalCaretRect(text2->GetLayoutObject(), PhysicalRect(20, 0, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(Position(text2, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(
      LocalCaretRect(text2->GetLayoutObject(), PhysicalRect(29, 0, 1, 10)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(Position(text2, 1), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, FloatFirstLetter) {
  LoadAhem();
  InsertStyleElement("#container::first-letter{float:right}");
  SetBodyContent(
      "<div id=container style='font: 10px/10px Ahem; width: 40px'>foo</div>");
  const Node* foo = GetElementById("container")->firstChild();
  const LayoutObject* first_letter = AssociatedLayoutObjectOf(*foo, 0);
  const LayoutObject* remaining_text = AssociatedLayoutObjectOf(*foo, 1);

  EXPECT_EQ(LocalCaretRect(first_letter, PhysicalRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(remaining_text, PhysicalRect(0, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(remaining_text, PhysicalRect(10, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 2), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(remaining_text, PhysicalRect(20, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, AfterLineBreak) {
  LoadAhem();
  SetBodyContent("<div style='font: 10px/10px Ahem;'>foo<br><br></div>");
  const Node* div = GetDocument().body()->firstChild();
  const Node* foo = div->firstChild();
  const Node* first_br = foo->nextSibling();
  const Node* second_br = first_br->nextSibling();
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(30, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(*foo), TextAffinity::kDownstream)));
  EXPECT_EQ(
      LocalCaretRect(second_br->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
      LocalCaretRectOfPosition(PositionWithAffinity(
          Position::AfterNode(*first_br), TextAffinity::kDownstream)));
  EXPECT_EQ(
      LocalCaretRect(second_br->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
      LocalCaretRectOfPosition(PositionWithAffinity(
          Position::AfterNode(*second_br), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, AfterLineBreakInPre) {
  LoadAhem();
  SetBodyContent("<pre style='font: 10px/10px Ahem;'>foo\n\n</pre>");
  const Node* pre = GetDocument().body()->firstChild();
  const Node* foo = pre->firstChild();
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(30, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 4), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 5), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, AfterLineBreakInPre2) {
  LoadAhem();
  // This test case simulates the rendering of the inner editor of
  // <textarea>foo\n</textarea> without using text control element.
  SetBodyContent("<pre style='font: 10px/10px Ahem;'>foo\n<br></pre>");
  const Node* pre = GetDocument().body()->firstChild();
  const Node* foo = pre->firstChild();
  const Node* br = foo->nextSibling();
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(30, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(br->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 4), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(br->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(*br), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, AfterLineBreakTextArea) {
  LoadAhem();
  SetBodyContent("<textarea style='font: 10px/10px Ahem; '>foo\n\n</textarea>");
  const auto* textarea = ToTextControl(GetDocument().body()->firstChild());
  const Node* inner_text = textarea->InnerEditorElement()->firstChild();
  EXPECT_EQ(
      LocalCaretRect(inner_text->GetLayoutObject(), PhysicalRect(30, 0, 1, 10)),
      LocalCaretRectOfPosition(PositionWithAffinity(
          Position(inner_text, 3), TextAffinity::kDownstream)));
  EXPECT_EQ(
      LocalCaretRect(inner_text->GetLayoutObject(), PhysicalRect(0, 10, 1, 10)),
      LocalCaretRectOfPosition(PositionWithAffinity(
          Position(inner_text, 4), TextAffinity::kDownstream)));
  const Node* hidden_br = inner_text->nextSibling();
  EXPECT_EQ(
      LocalCaretRect(hidden_br->GetLayoutObject(), PhysicalRect(0, 20, 1, 10)),
      LocalCaretRectOfPosition(PositionWithAffinity(
          Position(inner_text, 5), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, CollapsedSpace) {
  LoadAhem();
  SetBodyContent(
      "<div style='font: 10px/10px Ahem;'>"
      "<span>foo</span><span>  </span></div>");
  const Node* first_span = GetDocument().body()->firstChild()->firstChild();
  const Node* foo = first_span->firstChild();
  const Node* second_span = first_span->nextSibling();
  const Node* white_spaces = second_span->firstChild();
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(30, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(foo, 3), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(30, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position::AfterNode(*foo), TextAffinity::kDownstream)));
  // TODO(yoichio): Following should return valid rect: crbug.com/812535.
  EXPECT_EQ(
      LocalCaretRect(first_span->GetLayoutObject(), PhysicalRect(0, 0, 0, 0)),
      LocalCaretRectOfPosition(
          PositionWithAffinity(Position::LastPositionInNode(*first_span),
                               TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(30, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(white_spaces, 0), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(30, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(white_spaces, 1), TextAffinity::kDownstream)));
  EXPECT_EQ(LocalCaretRect(foo->GetLayoutObject(), PhysicalRect(30, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(
                Position(white_spaces, 2), TextAffinity::kDownstream)));
}

TEST_F(LocalCaretRectTest, AbsoluteCaretBoundsOfWithShadowDOM) {
  const char* body_content =
      "<p id='host'><b slot='#one' id='one'>11</b><b name='#two' "
      "id='two'>22</b></p>";
  const char* shadow_content =
      "<div><slot name=#two></slot><slot name=#one></slot></div>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* body = GetDocument().body();
  Element* one = body->QuerySelector(AtomicString("#one"));

  gfx::Rect bounds_in_dom_tree = AbsoluteCaretBoundsOf(
      CreateVisiblePosition(Position(one, 0)).ToPositionWithAffinity());
  gfx::Rect bounds_in_flat_tree =
      AbsoluteCaretBoundsOf(CreateVisiblePosition(PositionInFlatTree(one, 0))
                                .ToPositionWithAffinity());

  EXPECT_FALSE(bounds_in_dom_tree.IsEmpty());
  EXPECT_EQ(bounds_in_dom_tree, bounds_in_flat_tree);
}

// Repro case of crbug.com/680428
TEST_F(LocalCaretRectTest, AbsoluteSelectionBoundsOfWithImage) {
  SetBodyContent("<div>foo<img></div>");

  Node* node = GetDocument().QuerySelector(AtomicString("img"));
  gfx::Rect rect = AbsoluteSelectionBoundsOf(VisiblePosition::Create(
      PositionWithAffinity(Position::LastPositionInNode(*node))));
  EXPECT_FALSE(rect.IsEmpty());
}

static std::pair<PhysicalRect, PhysicalRect> GetPhysicalRects(
    const Position& caret) {
  const PositionWithAffinity position(caret);
  const PhysicalRect& position_rect = LocalCaretRectOfPosition(position).rect;
  const PositionWithAffinity visible_position(
      CreateVisiblePosition(position).DeepEquivalent());
  const PhysicalRect& visible_position_rect =
      LocalCaretRectOfPosition(visible_position).rect;
  return {position_rect, visible_position_rect};
}

TEST_F(LocalCaretRectTest, AfterLineBreakInPreBlockLTRLineLTR) {
  LoadAhem();
  InsertStyleElement("pre{ font: 10px/10px Ahem; width: 300px }");
  const Position& caret =
      SetCaretTextToBody("<pre dir='ltr'>foo\n|<bdo dir='ltr'>abc</bdo></pre>");
  auto [position_rect, visible_position_rect] = GetPhysicalRects(caret);
  EXPECT_EQ(PhysicalRect(0, 10, 1, 10), position_rect);
  EXPECT_EQ(PhysicalRect(0, 10, 1, 10), visible_position_rect);
}

TEST_F(LocalCaretRectTest, AfterLineBreakInPreBlockLTRLineRTL) {
  LoadAhem();
  InsertStyleElement("pre{ font: 10px/10px Ahem; width: 300px }");
  const Position& caret =
      SetCaretTextToBody("<pre dir='ltr'>foo\n|<bdo dir='rtl'>abc</bdo></pre>");
  auto [position_rect, visible_position_rect] = GetPhysicalRects(caret);
  EXPECT_EQ(PhysicalRect(0, 10, 1, 10), position_rect);
  EXPECT_EQ(PhysicalRect(0, 10, 1, 10), visible_position_rect);
}

TEST_F(LocalCaretRectTest, AfterLineBreakInPreBlockRTLLineLTR) {
  LoadAhem();
  InsertStyleElement("pre{ font: 10px/10px Ahem; width: 300px }");
  const Position& caret =
      SetCaretTextToBody("<pre dir='rtl'>foo\n|<bdo dir='ltr'>abc</bdo></pre>");
  auto [position_rect, visible_position_rect] = GetPhysicalRects(caret);
  EXPECT_EQ(PhysicalRect(299, 10, 1, 10), position_rect);
  EXPECT_EQ(PhysicalRect(299, 10, 1, 10), visible_position_rect);
}

TEST_F(LocalCaretRectTest, AfterLineBreakInPreBlockRTLLineRTL) {
  LoadAhem();
  InsertStyleElement("pre{ font: 10px/10px Ahem; width: 300px }");
  const Position& caret =
      SetCaretTextToBody("<pre dir='rtl'>foo\n|<bdo dir='rtl'>abc</bdo></pre>");
  auto [position_rect, visible_position_rect] = GetPhysicalRects(caret);
  EXPECT_EQ(PhysicalRect(299, 10, 1, 10), position_rect);
  EXPECT_EQ(PhysicalRect(299, 10, 1, 10), visible_position_rect);
}

// crbug.com/834686
TEST_F(LocalCaretRectTest, AfterTrimedLineBreak) {
  LoadAhem();
  InsertStyleElement("body { font: 10px/10px Ahem; width: 300px }");
  const Position& caret = SetCaretTextToBody("<div>foo\n|</div>");
  auto [position_rect, visible_position_rect] = GetPhysicalRects(caret);
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10), position_rect);
  EXPECT_EQ(PhysicalRect(30, 0, 1, 10), visible_position_rect);
}

// See also CaretPositionTest.MultiColumnSingleText
TEST_F(LocalCaretRectTest, MultiColumnSingleText) {
  LoadAhem();
  InsertStyleElement(
      "div { font: 10px/15px Ahem; column-count: 3; width: 20ch; }");
  SetBodyInnerHTML("<div id=target>abc def ghi jkl mno pqr</div>");
  // This HTML is rendered as:
  //    abc ghi mno
  //    def jkl
  const auto& target = *GetElementById("target");
  const Text& text = *To<Text>(target.firstChild());

  // Note: Legacy layout caret rect is in stitch coordinate space == as if
  // columns are laid out vertically.
  // NG caret rect is in relative to containing box fragment.

  // "abc " in column 1
  EXPECT_EQ(PhysicalRect(0, 2, 1, 10),
            LocalCaretRectOf(Position(text, 0)).rect);
  EXPECT_EQ(PhysicalRect(10, 2, 1, 10),
            LocalCaretRectOf(Position(text, 1)).rect);
  EXPECT_EQ(PhysicalRect(20, 2, 1, 10),
            LocalCaretRectOf(Position(text, 2)).rect);
  EXPECT_EQ(PhysicalRect(30, 2, 1, 10),
            LocalCaretRectOf(Position(text, 3)).rect);

  // "def " in column 1
  EXPECT_EQ(PhysicalRect(0, 17, 1, 10),
            LocalCaretRectOf(Position(text, 4)).rect);
  EXPECT_EQ(PhysicalRect(10, 17, 1, 10),
            LocalCaretRectOf(Position(text, 5)).rect);
  EXPECT_EQ(PhysicalRect(20, 17, 1, 10),
            LocalCaretRectOf(Position(text, 6)).rect);
  EXPECT_EQ(PhysicalRect(30, 17, 1, 10),
            LocalCaretRectOf(Position(text, 7)).rect);

  // "ghi " in column 2
  EXPECT_EQ(PhysicalRect(0, 2, 1, 10),
            LocalCaretRectOf(Position(text, 8)).rect);
  EXPECT_EQ(PhysicalRect(10, 2, 1, 10),
            LocalCaretRectOf(Position(text, 9)).rect);
  EXPECT_EQ(PhysicalRect(20, 2, 1, 10),
            LocalCaretRectOf(Position(text, 10)).rect);
  EXPECT_EQ(PhysicalRect(30, 2, 1, 10),
            LocalCaretRectOf(Position(text, 11)).rect);

  // "jkl " in column 2
  EXPECT_EQ(PhysicalRect(0, 17, 1, 10),
            LocalCaretRectOf(Position(text, 12)).rect);
  EXPECT_EQ(PhysicalRect(10, 17, 1, 10),
            LocalCaretRectOf(Position(text, 13)).rect);
  EXPECT_EQ(PhysicalRect(20, 17, 1, 10),
            LocalCaretRectOf(Position(text, 14)).rect);
  EXPECT_EQ(PhysicalRect(30, 17, 1, 10),
            LocalCaretRectOf(Position(text, 15)).rect);

  // "mno " in column 3
  EXPECT_EQ(PhysicalRect(0, 2, 1, 10),
            LocalCaretRectOf(Position(text, 16)).rect);
  EXPECT_EQ(PhysicalRect(10, 2, 1, 10),
            LocalCaretRectOf(Position(text, 17)).rect);
  EXPECT_EQ(PhysicalRect(20, 2, 1, 10),
            LocalCaretRectOf(Position(text, 18)).rect);
  EXPECT_EQ(PhysicalRect(30, 2, 1, 10),
            LocalCaretRectOf(Position(text, 19)).rect);

  // "pqr" in column 3
  EXPECT_EQ(PhysicalRect(0, 17, 1, 10),
            LocalCaretRectOf(Position(text, 20)).rect);
  EXPECT_EQ(PhysicalRect(10, 17, 1, 10),
            LocalCaretRectOf(Position(text, 21)).rect);
  EXPECT_EQ(PhysicalRect(20, 17, 1, 10),
            LocalCaretRectOf(Position(text, 22)).rect);
  EXPECT_EQ(PhysicalRect(30, 17, 1, 10),
            LocalCaretRectOf(Position(text, 23)).rect);
}

TEST_F(LocalCaretRectTest, UnicodeBidiPlaintextWithDifferentBlockDirection) {
  LoadAhem();
  InsertStyleElement("div { font: 10px/10px Ahem; unicode-bidi: plaintext }");
  const Position position = SetCaretTextToBody("<div dir='rtl'>|abc</div>");
  const PhysicalRect caret_rect =
      LocalCaretRectOfPosition(PositionWithAffinity(position)).rect;
  EXPECT_EQ(PhysicalRect(0, 0, 1, 10), caret_rect);
}

// http://crbug.com/835779
TEST_F(LocalCaretRectTest, NextLineWithoutLeafChild) {
  LoadAhem();
  InsertStyleElement("div { font: 10px/10px Ahem; width: 30px }");
  SetBodyContent(
      "<div>"
      "<br>"
      "<span style=\"border-left: 50px solid\"></span>"
      "foo"
      "</div>");

  const Element& br = *GetDocument().QuerySelector(AtomicString("br"));
  EXPECT_EQ(
      PhysicalRect(50, 10, 1, 10),
      LocalCaretRectOfPosition(PositionWithAffinity(Position::AfterNode(br)))
          .rect);
}

TEST_F(LocalCaretRectTest, BidiTextWithImage) {
  LoadAhem();
  InsertStyleElement(
      "div { font: 10px/10px Ahem; width: 30px }"
      "img { width: 10px; height: 10px; vertical-align: bottom }");
  SetBodyContent("<div dir=rtl>X<img id=image>Y</div>");
  const Element& image = *GetElementById("image");
  const LayoutObject* image_layout = image.GetLayoutObject();
  const LayoutObject* text_before = image.previousSibling()->GetLayoutObject();
  EXPECT_EQ(LocalCaretRect(text_before, PhysicalRect(10, 0, 1, 10)),
            LocalCaretRectOfPosition(
                PositionWithAffinity(Position::BeforeNode(image))));
  EXPECT_EQ(LocalCaretRect(image_layout, PhysicalRect(9, 0, 1, 10)),
            LocalCaretRectOfPosition(
                PositionWithAffinity(Position::AfterNode(image))));
}

// https://crbug.com/876044
TEST_F(LocalCaretRectTest, RtlMeterNoCrash) {
  SetBodyContent("foo<meter dir=rtl></meter>");
  const Position position = Position::LastPositionInNode(*GetDocument().body());
  // Shouldn't crash inside
  const LocalCaretRect local_caret_rect =
      LocalCaretRectOfPosition(PositionWithAffinity(position));
  EXPECT_EQ(
      GetDocument().QuerySelector(AtomicString("meter"))->GetLayoutObject(),
      local_caret_rect.layout_object);
}

// https://crbug.com/883044
TEST_F(LocalCaretRectTest, AfterCollapsedWhiteSpaceInRTLText) {
  LoadAhem();
  InsertStyleElement(
      "bdo { display: block; font: 10px/10px Ahem; width: 100px }");
  const Position position =
      SetCaretTextToBody("<bdo dir=rtl>AAA  |BBB<span>CCC</span></bdo>");
  const Node* text = position.AnchorNode();
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(60, 0, 1, 10)),
            LocalCaretRectOfPosition(
                PositionWithAffinity(position, TextAffinity::kDownstream)));
}

// https://crbug.com/936988
TEST_F(LocalCaretRectTest, AfterIneditableInline) {
  LoadAhem();
  InsertStyleElement("div { font: 10px/10px Ahem }");
  SetBodyContent(
      "<div contenteditable><span contenteditable=\"false\">foo</span></div>");
  const Element* div = GetDocument().QuerySelector(AtomicString("div"));
  const Node* text = div->firstChild()->firstChild();

  const Position position = Position::LastPositionInNode(*div);
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(30, 0, 1, 10)),
            LocalCaretRectOfPosition(PositionWithAffinity(position)));
}

// http://crbug.com/688015
TEST_F(LocalCaretRectTest, LocalCaretAtBeginningOfNonEditable) {
  LoadAhem();
  InsertStyleElement(
      "div { width: 70px; padding-left: 10px; font: 10px/10px Ahem }"
      "span { padding-left: 15px }");
  SetBodyContent(
      "<div contenteditable><span contenteditable=\"false\">foo</span></div>");
  const Element* div = GetDocument().QuerySelector(AtomicString("div"));
  const Element* span = To<Element>(div->firstChild());
  const Node* text = span->firstChild();

  const Position& position = Position::FirstPositionInNode(*div);
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(25, 0, 1, 10)),
            LocalCaretRectOf(position, kCanCrossEditingBoundary));
  EXPECT_EQ(LocalCaretRect(span->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
            LocalCaretRectOf(position, kCannotCrossEditingBoundary));
}

// http://crbug.com/1456664
TEST_F(LocalCaretRectTest,
       LocalCaretAtBeginningOfNonEditableWithCollapsedWhitespace) {
  LoadAhem();
  InsertStyleElement(
      "div { width: 70px; padding-left: 10px; font: 10px/10px Ahem }"
      "span { padding-left: 15px }");
  // Note the space before the span!
  SetBodyContent(
      "<div contenteditable> <span contenteditable=\"false\">foo</span></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  const Element* span = div->firstElementChild();
  const Node* text = span->firstChild();

  const Position& position = Position::FirstPositionInNode(*div);
  EXPECT_EQ(LocalCaretRect(text->GetLayoutObject(), PhysicalRect(25, 0, 1, 10)),
            LocalCaretRectOf(position, kCanCrossEditingBoundary));
  EXPECT_EQ(LocalCaretRect(span->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
            LocalCaretRectOf(position, kCannotCrossEditingBoundary));
}

// http://crbug.com/1384470
TEST_F(LocalCaretRectTest, LocalCaretAtBeginningOfNonEditableWithSvg) {
  LoadAhem();
  InsertStyleElement(
      "div { width: 70px; padding-left: 10px; font: 10px/10px Ahem }"
      "span { padding-left: 15px }"
      "svg { vertical-align: text-bottom; }");
  // Note the space before the span!
  SetBodyContent(
      "<div contenteditable> <span contenteditable=\"false\">"
      "<svg width=\"30\" height=\"10\"></svg></span></div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  const Element* span = div->firstElementChild();

  const Position& position = Position::FirstPositionInNode(*div);
  // TODO(abotella): Should be (0, 0), with the svg as the layout object.
  EXPECT_EQ(LocalCaretRect(div->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
            LocalCaretRectOf(position, kCanCrossEditingBoundary));
  EXPECT_EQ(LocalCaretRect(span->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
            LocalCaretRectOf(position, kCannotCrossEditingBoundary));
}

// http://crbug.com/688015
TEST_F(LocalCaretRectTest, LocalCaretAtBeginningOfNonEditableInFlatTree) {
  LoadAhem();
  InsertStyleElement(
      "div { width: 70px; padding-left: 10px; font: 10px/10px Ahem }"
      "span { padding-left: 15px }");
  const char* body_content =
      "<div contenteditable><div id='host'>"
      "<span slot='#one'>foo1</span><span>foo3</span></div></div>";
  const char* shadow_content =
      "<slot name=#one></slot>"
      "<span contenteditable='false' id='foo2'>foo2</span>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");
  const Element* target = GetDocument().getElementById(AtomicString("host"));
  const Element* foo1_span = To<Element>(target->firstChild());
  const Node* foo1 = foo1_span->firstChild();

  const Position& position = Position::FirstPositionInNode(*target);
  EXPECT_EQ(LocalCaretRect(foo1->GetLayoutObject(), PhysicalRect(25, 0, 1, 10)),
            LocalCaretRectOf(position, kCanCrossEditingBoundary));
  // TODO(abotella): The layout object should probably be |foo1_span|'s.
  // TODO(abotella): We should avoid using LayoutBox::LocalCaretRect in
  // LayoutNG.
  EXPECT_EQ(
      LocalCaretRect(target->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
      LocalCaretRectOf(position, kCannotCrossEditingBoundary));

  const PositionInFlatTree& position_in_flat_tree =
      PositionInFlatTree::FirstPositionInNode(*target);
  EXPECT_EQ(LocalCaretRect(foo1->GetLayoutObject(), PhysicalRect(25, 0, 1, 10)),
            LocalCaretRectOfPosition(
                PositionInFlatTreeWithAffinity(position_in_flat_tree),
                kCanCrossEditingBoundary));
  // TODO(abotella): The layout object should probably be |foo1_span|'s.
  // TODO(abotella): We should avoid using LayoutBox::LocalCaretRect in
  // LayoutNG.
  EXPECT_EQ(
      LocalCaretRect(target->GetLayoutObject(), PhysicalRect(10, 0, 1, 10)),
      LocalCaretRectOfPosition(
          PositionInFlatTreeWithAffinity(position_in_flat_tree),
          kCannotCrossEditingBoundary));
}

// http://crbug.com/688015
TEST_F(LocalCaretRectTest, LocalCaretAtEndOfNonEditable) {
  LoadAhem();
  InsertStyleElement(
      "div { width: 70px; padding: 10px; font: 10px/10px Ahem }"
      "span { padding: 15px }");
  SetBodyContent(
      "<div contenteditable><span contenteditable=\"false\">foo</span></div>");
  const Element* div = GetDocument().QuerySelector(AtomicString("div"));
  const Element* span = To<Element>(div->firstChild());
  const Node* text = span->firstChild();

  const Position& position = Position::LastPositionInNode(*div);
  EXPECT_EQ(
      LocalCaretRect(text->GetLayoutObject(), PhysicalRect(55, 10, 1, 10)),
      LocalCaretRectOf(position, kCanCrossEditingBoundary));
  EXPECT_EQ(
      LocalCaretRect(span->GetLayoutObject(), PhysicalRect(69, 10, 1, 10)),
      LocalCaretRectOf(position, kCannotCrossEditingBoundary));
}

// http://crbug.com/1456664
TEST_F(LocalCaretRectTest,
       LocalCaretAtEndOfNonEditableWithCollapsedWhitespace) {
  LoadAhem();
  InsertStyleElement(
      "div { width: 70px; padding: 10px; font: 10px/10px Ahem }"
      "span { padding: 15px }");
  // Note the space after the span!
  SetBodyContent(
      "<div contenteditable><span contenteditable=\"false\">foo</span> </div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  const Element* span = To<Element>(div->firstChild());
  const Node* text = span->firstChild();

  const Position& position = Position::LastPositionInNode(*div);
  EXPECT_EQ(
      LocalCaretRect(text->GetLayoutObject(), PhysicalRect(55, 10, 1, 10)),
      LocalCaretRectOf(position, kCanCrossEditingBoundary));
  EXPECT_EQ(
      LocalCaretRect(span->GetLayoutObject(), PhysicalRect(69, 10, 1, 10)),
      LocalCaretRectOf(position, kCannotCrossEditingBoundary));
}

// http://crbug.com/1384470
TEST_F(LocalCaretRectTest, LocalCaretAtEndOfNonEditableWithSvg) {
  LoadAhem();
  InsertStyleElement(
      "div { width: 70px; padding: 10px; font: 10px/10px Ahem }"
      "span { padding: 15px }"
      "svg { vertical-align: text-bottom; }");
  // Note the space after the span!
  SetBodyContent(
      "<div contenteditable><span contenteditable=\"false\">"
      "<svg width=\"30\" height=\"10\"></svg></span> </div>");
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  const Element* span = To<Element>(div->firstChild());
  const Element* svg = To<Element>(span->firstChild());

  const Position& position = Position::LastPositionInNode(*div);
  EXPECT_EQ(LocalCaretRect(svg->GetLayoutObject(), PhysicalRect(29, 0, 1, 10)),
            LocalCaretRectOf(position, kCanCrossEditingBoundary));
  EXPECT_EQ(
      LocalCaretRect(span->GetLayoutObject(), PhysicalRect(69, 10, 1, 10)),
      LocalCaretRectOf(position, kCannotCrossEditingBoundary));
}

// http://crbug.com/688015
TEST_F(LocalCaretRectTest, LocalCaretAtEndOfNonEditableInFlatTree) {
  LoadAhem();
  InsertStyleElement(
      "div { width: 70px; padding: 10px; font: 10px/10px Ahem }"
      "span { padding: 15px }");
  const char* body_content =
      "<div contenteditable><div id='host'>"
      "<span slot='#one'>foo1</span><span>foo3</span></div></div>";
  const char* shadow_content =
      "<slot name=#one></slot>"
      "<span contenteditable='false' id='foo2'>foo2</span>";
  SetBodyContent(body_content);
  auto* shadow_root = SetShadowContent(shadow_content, "host");
  const Element* target = GetDocument().getElementById(AtomicString("host"));
  const Element* foo2_span = shadow_root->getElementById(AtomicString("foo2"));
  const Node* foo2 = foo2_span->firstChild();

  const Position& position = Position::LastPositionInNode(*target);
  // The old code had a bug that the caret inline offset was greater than the
  // inline size of `target`.  The inline size of `target` is 10px + 70px + 10px
  // => 90px, and the caret should be between 10 to 80.
  PhysicalRect expected = RuntimeEnabledFeatures::SidewaysWritingModesEnabled()
                              ? PhysicalRect(79, 10, 1, 10)
                              : PhysicalRect(99, 10, 1, 10);
  // TODO(abotella): The coordinates should be (50, 20) and the layout object
  // should probably be |foo2|'s.
  // TODO(abotella): We should avoid using LayoutBox::LocalCaretRect in
  // LayoutNG.
  EXPECT_EQ(LocalCaretRect(target->GetLayoutObject(), expected),
            LocalCaretRectOf(position, kCanCrossEditingBoundary));
  // TODO(abotella): The coordinates should be (49, 20) and the layout object
  // should probably be |foo2_span|'s.
  // TODO(abotella): We should avoid using LayoutBox::LocalCaretRect in
  // LayoutNG.
  EXPECT_EQ(LocalCaretRect(target->GetLayoutObject(), expected),
            LocalCaretRectOf(position, kCannotCrossEditingBoundary));

  const PositionInFlatTree& position_in_flat_tree =
      PositionInFlatTree::LastPositionInNode(*target);
  EXPECT_EQ(
      LocalCaretRect(foo2->GetLayoutObject(), PhysicalRect(50, 20, 1, 10)),
      LocalCaretRectOfPosition(
          PositionInFlatTreeWithAffinity(position_in_flat_tree),
          kCanCrossEditingBoundary));
  // 50 rather than 49 and a |foo2|'s layout object rather than |foo2_span|'s
  // because |foo2_span| is a culled inline. This is probably fine, because
  // the 1px difference isn't appreciable in practice.
  EXPECT_EQ(
      LocalCaretRect(foo2->GetLayoutObject(), PhysicalRect(50, 20, 1, 10)),
      LocalCaretRectOfPosition(
          PositionInFlatTreeWithAffinity(position_in_flat_tree),
          kCannotCrossEditingBoundary));
}

// http://crbug.com/688015
TEST_F(LocalCaretRectTest, AbsoluteCaretAtEndOfNonEditable) {
  LoadAhem();
  InsertStyleElement(
      "body { margin: 5px; }"
      "div { width: 70px; padding-left: 10px; font: 10px/10px Ahem }"
      "span { padding-left: 15px }");
  SetBodyContent(
      "<div contenteditable><span contenteditable=\"false\">foo</span></div>");

  const Element* div = GetDocument().QuerySelector(AtomicString("div"));
  const Position& position = Position::LastPositionInNode(*div);
  EXPECT_EQ("60,5 1x10", AbsoluteCaretBoundsOf(PositionWithAffinity(position),
                                               kCanCrossEditingBoundary)
                             .ToString());
  EXPECT_EQ("59,5 1x10", AbsoluteCaretBoundsOf(PositionWithAffinity(position),
                                               kCannotCrossEditingBoundary)
                             .ToString());
}

// http://crbug.com/688015
TEST_F(LocalCaretRectTest, AbsoluteCaretAtBeginningOfNonEditable) {
  LoadAhem();
  InsertStyleElement(
      "body { margin: 5px; }"
      "div { width: 70px; padding-left: 10px; font: 10px/10px Ahem }"
      "span { padding-left: 15px }");
  SetBodyContent(
      "<div contenteditable><span contenteditable=\"false\">foo</span></div>");

  const Element* div = GetDocument().QuerySelector(AtomicString("div"));
  const Position& position = Position::FirstPositionInNode(*div);
  EXPECT_EQ("30,5 1x10", AbsoluteCaretBoundsOf(PositionWithAffinity(position),
                                               kCanCrossEditingBoundary)
                             .ToString());
  EXPECT_EQ("15,5 1x10", AbsoluteCaretBoundsOf(PositionWithAffinity(position),
                                               kCannotCrossEditingBoundary)
                             .ToString());
}

// https://crbug.com/1155399
TEST_F(LocalCaretRectTest, OptionWithDisplayContents) {
  LoadAhem();
  InsertStyleElement(
      "body { font: 10px/10px Ahem; width: 300px }"
      "option { display: contents; }");
  SetBodyContent("<option>a</option>");
  const Element* body = GetDocument().body();
  const Element* option = GetDocument().QuerySelector(AtomicString("option"));
  LocalCaretRect empty;
  LocalCaretRect start(body->GetLayoutObject(), PhysicalRect(0, 0, 1, 10));
  LocalCaretRect end(body->GetLayoutObject(), PhysicalRect(299, 0, 1, 10));

  // LocalCaretRectOfPosition shouldn't crash
  for (const Position& p : {Position::BeforeNode(*body), Position(body, 0)})
    EXPECT_EQ(start, LocalCaretRectOfPosition(PositionWithAffinity(p)));
  for (const Position& p :
       {Position::BeforeNode(*option), Position(option, 0), Position(option, 1),
        Position::LastPositionInNode(*option), Position::AfterNode(*option)})
    EXPECT_EQ(empty, LocalCaretRectOfPosition(PositionWithAffinity(p)));
  for (const Position& p :
       {Position(body, 1), Position::LastPositionInNode(*body),
        Position::AfterNode(*body)})
    EXPECT_EQ(end, LocalCaretRectOfPosition(PositionWithAffinity(p)));
}

TEST_F(LocalCaretRectTest, TextCombineOneTextNode) {
  LoadAhem();
  InsertStyleElement(
      "div {"
      "  font: 100px/110px Ahem;"
      "  writing-mode: vertical-rl;"
      "}"
      "tcy { text-combine-upright: all; }");
  SetBodyInnerHTML("<div>a<tcy id=target>01234</tcy>b</div>");
  //   LayoutBlockFlow {HTML} at (0,0) size 800x600
  //     LayoutBlockFlow {BODY} at (8,8) size 784x584
  //       LayoutBlockFlow {DIV} at (0,0) size 110x300
  //         LayoutText {#text} at (5,0) size 100x100
  //           text run at (5,0) width 100: "a"
  //         LayoutInline {TCY} at (5,100) size 100x100
  //           LayoutTextCombine (anonymous) at (5,100) size 100x100
  //             LayoutText {#text} at (-5,0) size 110x100
  //               text run at (0,0) width 500: "01234"
  //         LayoutText {#text} at (5,200) size 100x100
  //           text run at (5,200) width 100: "b"
  const auto& target = *GetElementById("target");
  const auto& text_a = *To<Text>(target.previousSibling());
  const auto& text_01234 = *To<Text>(target.firstChild());
  const auto& text_b = *To<Text>(target.nextSibling());

  // text_a
  EXPECT_EQ(
      LocalCaretRect(text_a.GetLayoutObject(), PhysicalRect(5, 0, 100, 1)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_a, 0))));
  EXPECT_EQ(
      LocalCaretRect(text_a.GetLayoutObject(), PhysicalRect(5, 100, 100, 1)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_a, 1))));

  // text_01234
  EXPECT_EQ(
      LocalCaretRect(text_01234.GetLayoutObject(), PhysicalRect(0, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_01234, 0))));
  EXPECT_EQ(
      LocalCaretRect(text_01234.GetLayoutObject(), PhysicalRect(17, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_01234, 1))));
  EXPECT_EQ(
      LocalCaretRect(text_01234.GetLayoutObject(), PhysicalRect(39, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_01234, 2))));
  EXPECT_EQ(
      LocalCaretRect(text_01234.GetLayoutObject(), PhysicalRect(61, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_01234, 3))));
  EXPECT_EQ(
      LocalCaretRect(text_01234.GetLayoutObject(), PhysicalRect(83, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_01234, 4))));
  EXPECT_EQ(
      LocalCaretRect(text_01234.GetLayoutObject(), PhysicalRect(99, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_01234, 5))));

  // text_b
  EXPECT_EQ(
      LocalCaretRect(text_b.GetLayoutObject(), PhysicalRect(5, 200, 100, 1)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_b, 0))));
  EXPECT_EQ(
      LocalCaretRect(text_b.GetLayoutObject(), PhysicalRect(5, 299, 100, 1)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_b, 1))));
}

TEST_F(LocalCaretRectTest, TextCombineTwoTextNodes) {
  LoadAhem();
  InsertStyleElement(
      "div {"
      "  font: 100px/110px Ahem;"
      "  writing-mode: vertical-rl;"
      "}"
      "tcy { text-combine-upright: all; }");
  SetBodyInnerHTML("<div>a<tcy id=target>012<!-- -->34</tcy>b</div>");
  //   LayoutBlockFlow {HTML} at (0,0) size 800x600
  //     LayoutBlockFlow {BODY} at (8,8) size 784x584
  //       LayoutBlockFlow {DIV} at (0,0) size 110x300
  //         LayoutText {#text} at (5,0) size 100x100
  //           text run at (5,0) width 100: "a"
  //         LayoutInline {TCY} at (5,100) size 100x100
  //           LayoutTextCombine (anonymous) at (5,100) size 100x100
  //             LayoutText {#text} at (-5,0) size 66x100
  //               text run at (0,0) width 300: "012"
  //             LayoutText {#text} at (61,0) size 44x100
  //               text run at (300,0) width 200: "34"
  //         LayoutText {#text} at (5,200) size 100x100
  //           text run at (5,200) width 100: "b"
  const auto& target = *GetElementById("target");
  const auto& text_a = *To<Text>(target.previousSibling());
  const auto& text_012 = *To<Text>(target.firstChild());
  const auto& text_34 = *To<Text>(target.lastChild());
  const auto& text_b = *To<Text>(target.nextSibling());

  // text_a
  EXPECT_EQ(
      LocalCaretRect(text_a.GetLayoutObject(), PhysicalRect(5, 0, 100, 1)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_a, 0))));
  EXPECT_EQ(
      LocalCaretRect(text_a.GetLayoutObject(), PhysicalRect(5, 100, 100, 1)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_a, 1))));

  // text_012
  EXPECT_EQ(
      LocalCaretRect(text_012.GetLayoutObject(), PhysicalRect(0, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_012, 0))));
  EXPECT_EQ(
      LocalCaretRect(text_012.GetLayoutObject(), PhysicalRect(17, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_012, 1))));
  EXPECT_EQ(
      LocalCaretRect(text_012.GetLayoutObject(), PhysicalRect(39, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_012, 2))));
  EXPECT_EQ(
      LocalCaretRect(text_012.GetLayoutObject(), PhysicalRect(61, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_012, 3))));

  // text_34
  EXPECT_EQ(
      LocalCaretRect(text_34.GetLayoutObject(), PhysicalRect(61, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_34, 0))));
  EXPECT_EQ(
      LocalCaretRect(text_34.GetLayoutObject(), PhysicalRect(83, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_34, 1))));
  EXPECT_EQ(
      LocalCaretRect(text_34.GetLayoutObject(), PhysicalRect(99, 0, 1, 100)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_34, 2))));

  // text_b
  EXPECT_EQ(
      LocalCaretRect(text_b.GetLayoutObject(), PhysicalRect(5, 200, 100, 1)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_b, 0))));
  EXPECT_EQ(
      LocalCaretRect(text_b.GetLayoutObject(), PhysicalRect(5, 299, 100, 1)),
      LocalCaretRectOfPosition(PositionWithAffinity(Position(text_b, 1))));
}

TEST_F(LocalCaretRectTest,
       LocalCaretAtStartOfNonEditableWithDifferentFontSizes) {
  LoadAhem();
  InsertStyleElement(
      "div { width: 110px; padding: 15px; font: 15px/15px Ahem }"
      "span { padding: 10px; font: 10px/10px Ahem }");
  SetBodyContent(
      "<div contenteditable><span contenteditable=false>foo</span> bar</div>");
  const Element& div = *GetDocument().QuerySelector(AtomicString("div"));
  const Element& span = *To<Element>(div.firstChild());
  const Text& text = *To<Text>(span.firstChild());

  const Position& position = Position::FirstPositionInNode(div);
  EXPECT_EQ(LocalCaretRect(text.GetLayoutObject(), PhysicalRect(25, 19, 1, 10)),
            LocalCaretRectOf(position, kCanCrossEditingBoundary));
  EXPECT_EQ(LocalCaretRect(span.GetLayoutObject(), PhysicalRect(15, 15, 1, 15)),
            LocalCaretRectOf(position, kCannotCrossEditingBoundary));
}

TEST_F(LocalCaretRectTest, LocalCaretAtEndOfNonEditableWithDifferentFontSizes) {
  LoadAhem();
  InsertStyleElement(
      "div { width: 120px; padding: 10px; font: 10px/10px Ahem }"
      "span { padding: 15px; font: 15px/15px Ahem }");
  SetBodyContent(
      "<div contenteditable>foo <span contenteditable=false>bar</span></div>");
  const Element& div = *GetDocument().QuerySelector(AtomicString("div"));
  const Element& span = *To<Element>(div.lastChild());
  const Text& text = *To<Text>(span.firstChild());

  const Position& position = Position::LastPositionInNode(div);
  EXPECT_EQ(
      LocalCaretRect(text.GetLayoutObject(), PhysicalRect(110, 10, 1, 15)),
      LocalCaretRectOf(position, kCanCrossEditingBoundary));
  EXPECT_EQ(
      LocalCaretRect(span.GetLayoutObject(), PhysicalRect(124, 10, 1, 15)),
      LocalCaretRectOf(position, kCannotCrossEditingBoundary));
}

TEST_F(LocalCaretRectTest, LocalCaretInSvgTextWithFontScaling) {
  LoadAhem();
  InsertStyleElement(
      "body { margin: 0 }"
      "svg { width: 100% }"
      "text { font: 10px/10px Ahem }");
  SetBodyContent(
      "<svg viewBox='0 0 160 120'><text x='10' y='10'>Text</text></svg>");

  const Text& text = To<Text>(
      *GetDocument().QuerySelector(AtomicString("text"))->firstChild());
  EXPECT_EQ(LocalCaretRect(text.GetLayoutObject(), PhysicalRect(10, 2, 1, 10)),
            LocalCaretRectOf(Position(text, 0)));
  EXPECT_EQ(LocalCaretRect(text.GetLayoutObject(), PhysicalRect(20, 2, 1, 10)),
            LocalCaretRectOf(Position(text, 1)));
  EXPECT_EQ(LocalCaretRect(text.GetLayoutObject(), PhysicalRect(30, 2, 1, 10)),
            LocalCaretRectOf(Position(text, 2)));
}

TEST_F(LocalCaretRectTest, AbsoluteCaretAtStartOrEndOfNonEditableBidi) {
  LoadAhem();
  InsertStyleElement(
      "body { margin: 0 }"
      "div { width: 100px; padding: 10px; text-align: center; font: 10px/10px "
      "Ahem }"
      "span { background-color: red }");
  SetBodyContent(
      "<div dir=rtl contenteditable><span contenteditable=false>"
      "<bdo dir=ltr>abc</bdo> <bdo dir=rtl>ABC</bdo></span></div>");
  const Element& div = *GetDocument().QuerySelector(AtomicString("div"));

  const Position& startPosition = Position::FirstPositionInNode(div);
  EXPECT_EQ("95,10 1x10",
            AbsoluteCaretBoundsOf(PositionWithAffinity(startPosition),
                                  kCanCrossEditingBoundary)
                .ToString());
  EXPECT_EQ("94,10 1x10",
            AbsoluteCaretBoundsOf(PositionWithAffinity(startPosition),
                                  kCannotCrossEditingBoundary)
                .ToString());

  const Position& endPosition = Position::LastPositionInNode(div);
  EXPECT_EQ("25,10 1x10",
            AbsoluteCaretBoundsOf(PositionWithAffinity(endPosition),
                                  kCanCrossEditingBoundary)
                .ToString());
  EXPECT_EQ("25,10 1x10",
            AbsoluteCaretBoundsOf(PositionWithAffinity(endPosition),
                                  kCannotCrossEditingBoundary)
                .ToString());
}

TEST_F(LocalCaretRectTest, AbsoluteCaretAtStartOrEndOfNonEditableBidiCulled) {
  LoadAhem();
  InsertStyleElement(
      "body { margin: 0 }"
      "div { width: 100px; padding: 10px; text-align: center; font: 10px/10px "
      "Ahem }");
  SetBodyContent(
      "<div dir=rtl contenteditable><span contenteditable=false>"
      "<bdo dir=ltr>abc</bdo> <bdo dir=rtl>ABC</bdo></span></div>");
  const Element& div = *GetDocument().QuerySelector(AtomicString("div"));

  const Position& startPosition = Position::FirstPositionInNode(div);
  EXPECT_EQ("95,10 1x10",
            AbsoluteCaretBoundsOf(PositionWithAffinity(startPosition),
                                  kCanCrossEditingBoundary)
                .ToString());
  // TODO(abotella): Should this be 95,10?
  EXPECT_EQ("65,10 1x10",
            AbsoluteCaretBoundsOf(PositionWithAffinity(startPosition),
                                  kCannotCrossEditingBoundary)
                .ToString());

  const Position& endPosition = Position::LastPositionInNode(div);
  EXPECT_EQ("25,10 1x10",
            AbsoluteCaretBoundsOf(PositionWithAffinity(endPosition),
                                  kCanCrossEditingBoundary)
                .ToString());
  EXPECT_EQ("25,10 1x10",
            AbsoluteCaretBoundsOf(PositionWithAffinity(endPosition),
                                  kCannotCrossEditingBoundary)
                .ToString());
}
}  // namespace blink
