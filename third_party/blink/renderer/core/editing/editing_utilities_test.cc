// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/editing_utilities.h"

#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class EditingUtilitiesTest : public EditingTestBase {};

TEST_F(EditingUtilitiesTest, ComputePositionForNodeRemovalAfterChildren) {
  SetBodyContent("<div id=a><p id=b><img id=c></p></div>");
  const Position position = Position::LastPositionInNode(*GetElementById("c"));
  // Simulate <p> will be removed.
  EXPECT_EQ(Position(*GetElementById("a"), 0),
            ComputePositionForNodeRemoval(position, *GetElementById("b")));
}

TEST_F(EditingUtilitiesTest, ComputePositionForNodeRemovalAfterNode) {
  // "editing/deleting/delete-start-block.html" hits this case.
  SetBodyContent("<div id=a><p id=b><img id=c></p></div>");
  const Position position = Position::AfterNode(*GetElementById("c"));
  // Simulate <p> will be removed.
  EXPECT_EQ(Position(*GetElementById("a"), 0),
            ComputePositionForNodeRemoval(position, *GetElementById("b")));
}

TEST_F(EditingUtilitiesTest, DirectionOfEnclosingBlockOf) {
  const char* body_content =
      "<p id='host'><b slot='#one' id='one'></b><b slot='#two' "
      "id='two'>22</b></p>";
  const char* shadow_content =
      "<slot name=#two></slot><p dir=rtl><slot name=#one></slot><p>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");
  Node* one = GetDocument().getElementById(AtomicString("one"));

  EXPECT_EQ(TextDirection::kLtr, DirectionOfEnclosingBlockOf(Position(one, 0)));
  EXPECT_EQ(TextDirection::kRtl,
            DirectionOfEnclosingBlockOf(PositionInFlatTree(one, 0)));
}

// TODO(crbug.com/1157146): This test breaks without Shadow DOM v0.
TEST_F(EditingUtilitiesTest,
       DISABLED_firstEditablePositionAfterPositionInRoot) {
  const char* body_content =
      "<p id='host' contenteditable><b slot='#one' id='one'>1</b><b "
      "slot='#two' id='two'>22</b></p>";
  const char* shadow_content =
      "<slot name=#two></slot><slot name=#one></slot><b id='three'>333</b>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");
  Element* host = GetDocument().getElementById(AtomicString("host"));
  Node* one = GetDocument().getElementById(AtomicString("one"));
  Node* two = GetDocument().getElementById(AtomicString("two"));
  Node* three = shadow_root->getElementById(AtomicString("three"));

  EXPECT_EQ(Position(one, 0),
            FirstEditablePositionAfterPositionInRoot(Position(one, 0), *host));
  // TODO(crbug.com/1157146): This returns the beginning of "1" instead of the
  // end of "22".
  EXPECT_EQ(Position(two->firstChild(), 2),
            CreateVisiblePosition(FirstEditablePositionAfterPositionInRoot(
                                      Position(one, 0), *host))
                .DeepEquivalent());

  EXPECT_EQ(PositionInFlatTree(one, 0),
            FirstEditablePositionAfterPositionInRoot(PositionInFlatTree(one, 0),
                                                     *host));
  // TODO(crbug.com/1157146): This returns the beginning of "1" instead of the
  // end of "22".
  EXPECT_EQ(PositionInFlatTree(two->firstChild(), 2),
            CreateVisiblePosition(FirstEditablePositionAfterPositionInRoot(
                                      PositionInFlatTree(one, 0), *host))
                .DeepEquivalent());

  EXPECT_EQ(
      Position::FirstPositionInNode(*host),
      FirstEditablePositionAfterPositionInRoot(Position(three, 0), *host));
  // TODO(crbug.com/1157146): This returns the beginning of "1" instead of the
  // beginning of "22".
  EXPECT_EQ(Position(two->firstChild(), 0),
            CreateVisiblePosition(FirstEditablePositionAfterPositionInRoot(
                                      Position(three, 0), *host))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree::AfterNode(*host),
            FirstEditablePositionAfterPositionInRoot(
                PositionInFlatTree(three, 0), *host));
  EXPECT_EQ(PositionInFlatTree::LastPositionInNode(*host),
            CreateVisiblePosition(FirstEditablePositionAfterPositionInRoot(
                                      PositionInFlatTree(three, 0), *host))
                .DeepEquivalent());
}

TEST_F(EditingUtilitiesTest, enclosingBlock) {
  const char* body_content = "<p id='host'><b slot='#one' id='one'>11</b></p>";
  const char* shadow_content =
      "<slot name=#two></slot><div id='three'><slot name=#one></slot></div>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");
  Node* host = GetDocument().getElementById(AtomicString("host"));
  Node* one = GetDocument().getElementById(AtomicString("one"));
  Node* three = shadow_root->getElementById(AtomicString("three"));

  EXPECT_EQ(host,
            EnclosingBlock(Position(one, 0), kCannotCrossEditingBoundary));
  EXPECT_EQ(three, EnclosingBlock(PositionInFlatTree(one, 0),
                                  kCannotCrossEditingBoundary));
}

TEST_F(EditingUtilitiesTest, enclosingNodeOfType) {
  const char* body_content = "<p id='host'><b slot='#one' id='one'>11</b></p>";
  const char* shadow_content =
      "<slot name=#two></slot><div id='three'><slot name=#one></div></slot>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");
  Node* host = GetDocument().getElementById(AtomicString("host"));
  Node* one = GetDocument().getElementById(AtomicString("one"));
  Node* three = shadow_root->getElementById(AtomicString("three"));

  EXPECT_EQ(host, EnclosingNodeOfType(Position(one, 0), IsEnclosingBlock));
  EXPECT_EQ(three,
            EnclosingNodeOfType(PositionInFlatTree(one, 0), IsEnclosingBlock));
}

// http://crbug.com/873088
TEST_F(EditingUtilitiesTest, IsEditablePositionWithHr) {
  SetBodyContent("<hr contenteditable id=target>");
  Element& target = *GetDocument().getElementById(AtomicString("target"));
  EXPECT_FALSE(IsEditablePosition(Position::BeforeNode(target)));
  EXPECT_TRUE(IsEditablePosition(Position(target, 0)));
}

// http://crbug.com/873088
TEST_F(EditingUtilitiesTest, IsEditablePositionWithSpan) {
  SetBodyContent("<span contenteditable id=target>abc</span>");
  Element& target = *GetDocument().getElementById(AtomicString("target"));
  EXPECT_FALSE(IsEditablePosition(Position::BeforeNode(target)));
  EXPECT_TRUE(IsEditablePosition(Position(target, 0)));
}

// https://issues.chromium.org/issues/41490809
TEST_F(EditingUtilitiesTest, IsEditablePositionWithVisibleInertElement) {
  SetBodyContent(
      R"(<div contenteditable><span inert id="target">abc</span></div>)");
  Element& target = *GetElementById("target");
  EXPECT_TRUE(IsEditablePosition(Position::BeforeNode(target)));
  EXPECT_FALSE(IsEditablePosition(Position(target.firstChild(), 0)));
}

// https://issues.chromium.org/issues/41490809
TEST_F(EditingUtilitiesTest, IsEditablePositionWithInVisibleInertElement) {
  SetBodyContent(
      R"(<div contenteditable>
        <span inert id="target" style="display: none;">abc</span></div>)");
  Element& target = *GetElementById("target");
  EXPECT_TRUE(IsEditablePosition(Position::BeforeNode(target)));
  EXPECT_FALSE(IsEditablePosition(Position(target.firstChild(), 0)));
}

TEST_F(EditingUtilitiesTest, isEditablePositionWithTable) {
  // We would like to have below DOM tree without HTML, HEAD and BODY element.
  //   <table id=table><caption>foo</caption></table>
  // However, |setBodyContent()| automatically creates HTML, HEAD and BODY
  // element. So, we build DOM tree manually.
  // Note: This is unusual HTML taken from http://crbug.com/574230
  Element* table = GetDocument().CreateRawElement(html_names::kTableTag);
  table->setInnerHTML("<caption>foo</caption>");
  while (GetDocument().firstChild())
    GetDocument().firstChild()->remove();
  GetDocument().AppendChild(table);
  GetDocument().setDesignMode("on");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(IsEditablePosition(Position(table, 0)));
}

// http://crbug.com/1185089
// See also PositionRespectingEditingBoundaryWithInputReadWrite
TEST_F(EditingUtilitiesTest,
       PositionRespectingEditingBoundaryWithInputReadOnly) {
  LoadAhem();
  InsertStyleElement("body { font: 10px/15px Ahem; margin: 0px; }");
  SetBodyContent(
      "<div id=sample>012"
      "<input value=abc placeholder=xyz readonly>"
      "345</div>");
  const auto& sample = *GetElementById("sample");
  const auto& text_012 = *To<Text>(sample.firstChild());
  const auto& input =
      ToTextControl(*GetDocument().QuerySelector(AtomicString("input")));
  const auto& inner_editor = *input.InnerEditorElement();
  const auto& text_abc = *To<Text>(inner_editor.firstChild());

  const HitTestRequest hit_request(HitTestRequest::kActive);
  const HitTestLocation hit_location(PhysicalOffset(0, 0));
  HitTestResult hit_result(hit_request, hit_location);
  ASSERT_TRUE(
      GetDocument().View()->GetLayoutView()->HitTest(hit_location, hit_result));
  ASSERT_EQ(PositionWithAffinity(Position(text_012, 0)),
            hit_result.GetPosition());

  EXPECT_EQ(
      PositionWithAffinity(Position(text_abc, 0)),
      PositionRespectingEditingBoundary(Position(text_abc, 1), hit_result))
      << "Adjust to outside of <input> to inside of <input>";
  EXPECT_EQ(PositionWithAffinity(Position(text_012, 0)),
            PositionRespectingEditingBoundary(Position::BeforeNode(input),
                                              hit_result))
      << "No adjustment because both position are in same enclosing element";
}

// http://crbug.com/1185089
TEST_F(EditingUtilitiesTest,
       PositionRespectingEditingBoundaryWithInputReadOnlyInEditable) {
  LoadAhem();
  InsertStyleElement("body { font: 10px/15px Ahem; margin: 0px; }");
  SetBodyContent(
      "<div contenteditable id=sample>012"
      "<input value=abc placeholder=xyz readonly>"
      "345</div>");
  const auto& sample = *GetElementById("sample");
  const auto& text_012 = *To<Text>(sample.firstChild());
  const auto& input =
      ToTextControl(*GetDocument().QuerySelector(AtomicString("input")));
  const auto& inner_editor = *input.InnerEditorElement();
  const auto& text_abc = *To<Text>(inner_editor.firstChild());

  const HitTestRequest hit_request(HitTestRequest::kActive);
  const HitTestLocation hit_location(PhysicalOffset(5, 5));
  HitTestResult hit_result(hit_request, hit_location);
  ASSERT_TRUE(
      GetDocument().View()->GetLayoutView()->HitTest(hit_location, hit_result));
  ASSERT_EQ(PositionWithAffinity(Position(text_012, 0)),
            hit_result.GetPosition());

  EXPECT_EQ(
      PositionWithAffinity(Position(text_abc, 0)),
      PositionRespectingEditingBoundary(Position(text_abc, 1), hit_result))
      << "Adjust to outside of <input> to inside of <input>";
  EXPECT_EQ(PositionWithAffinity(Position(text_012, 0)),
            PositionRespectingEditingBoundary(Position::BeforeNode(input),
                                              hit_result))
      << "No adjustment because both position are in same enclosing element";
}

// http://crbug.com/1185089
// See also PositionRespectingEditingBoundaryWithInputReadOnly
TEST_F(EditingUtilitiesTest,
       PositionRespectingEditingBoundaryWithInputReadWrite) {
  LoadAhem();
  InsertStyleElement("body { font: 10px/15px Ahem; margin: 0px; }");
  SetBodyContent(
      "<div id=sample>012"
      "<input value=abc placeholder=xyz>"
      "345</div>");
  const auto& sample = *GetElementById("sample");
  const auto& text_012 = *To<Text>(sample.firstChild());
  const auto& input =
      ToTextControl(*GetDocument().QuerySelector(AtomicString("input")));
  const auto& inner_editor = *input.InnerEditorElement();
  const auto& text_abc = *To<Text>(inner_editor.firstChild());

  const HitTestRequest hit_request(HitTestRequest::kActive);
  const HitTestLocation hit_location(PhysicalOffset(5, 5));
  HitTestResult hit_result(hit_request, hit_location);
  ASSERT_TRUE(
      GetDocument().View()->GetLayoutView()->HitTest(hit_location, hit_result));
  ASSERT_EQ(PositionWithAffinity(Position(text_012, 0)),
            hit_result.GetPosition());

  EXPECT_EQ(
      PositionWithAffinity(Position(text_abc, 0)),
      PositionRespectingEditingBoundary(Position(text_abc, 1), hit_result))
      << "Adjust to outside of <input> to inside of <input>";
  EXPECT_EQ(PositionWithAffinity(Position(text_012, 0)),
            PositionRespectingEditingBoundary(Position::BeforeNode(input),
                                              hit_result))
      << "No adjustment because both position are in same enclosing element";
}

// http://crbug.com/1249655 and http://crbug.com/1252377
TEST_F(EditingUtilitiesTest,
       PositionRespectingEditingBoundaryWithPointerEventNone) {
  LoadAhem();
  InsertStyleElement(
      "body { font: 10px/15px Ahem; margin: 0px; }"
      "input { pointer-events: none; }");
  SetBodyContent("<div>012 <input id=target placeholder=abc> 345</div>");
  const auto& target = *To<TextControlElement>(GetElementById("target"));
  const auto& text_abc = *To<Text>(target.PlaceholderElement()->firstChild());

  const HitTestRequest hit_request(HitTestRequest::kActive);
  const HitTestLocation hit_location(PhysicalOffset(50, 5));
  HitTestResult hit_result(hit_request, hit_location);
  ASSERT_TRUE(
      GetDocument().View()->GetLayoutView()->HitTest(hit_location, hit_result));
  ASSERT_EQ(PositionWithAffinity(Position(text_abc, 1)),
            hit_result.GetPosition());
  // Simulates drag from "abc"@2 to "abc@1"
  EXPECT_EQ(
      PositionWithAffinity(Position(text_abc, 1)),
      PositionRespectingEditingBoundary(Position(text_abc, 2), hit_result));
}

TEST_F(EditingUtilitiesTest, RepeatString) {
  EXPECT_EQ("", RepeatString("xyz", 0));
  EXPECT_EQ("xyz", RepeatString("xyz", 1));
  EXPECT_EQ("xyzxyz", RepeatString("xyz", 2));
  EXPECT_EQ("xyzxyzxyz", RepeatString("xyz", 3));
}

// TODO(crbug.com/1157146): This test breaks without Shadow DOM v0.
TEST_F(EditingUtilitiesTest, DISABLED_tableElementJustBefore) {
  const char* body_content =
      "<div contenteditable id=host><table slot=#table "
      "id=table><tr><td>1</td></tr></table><b slot=#two id=two>22</b></div>";
  const char* shadow_content =
      "<slot name=#two></slot><slot name=#table></slot>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");
  Node* host = GetDocument().getElementById(AtomicString("host"));
  Node* table = GetDocument().getElementById(AtomicString("table"));

  EXPECT_EQ(table, TableElementJustBefore(VisiblePosition::AfterNode(*table)));
  // TODO(crbug.com/1157146): This returns null instead of the table.
  EXPECT_EQ(table, TableElementJustBefore(
                       VisiblePositionInFlatTree::AfterNode(*table)));

  EXPECT_EQ(table, TableElementJustBefore(
                       VisiblePosition::LastPositionInNode(*table)));
  // TODO(crbug.com/1157146): This returns null instead of the table.
  EXPECT_EQ(table, TableElementJustBefore(CreateVisiblePosition(
                       PositionInFlatTree::LastPositionInNode(*table))));

  EXPECT_EQ(nullptr,
            TableElementJustBefore(CreateVisiblePosition(Position(host, 2))));
  // TODO(crbug.com/1157146): This returns null instead of the table.
  EXPECT_EQ(table, TableElementJustBefore(
                       CreateVisiblePosition(PositionInFlatTree(host, 2))));

  EXPECT_EQ(nullptr, TableElementJustBefore(VisiblePosition::AfterNode(*host)));
  EXPECT_EQ(nullptr, TableElementJustBefore(
                         VisiblePositionInFlatTree::AfterNode(*host)));

  EXPECT_EQ(nullptr,
            TableElementJustBefore(VisiblePosition::LastPositionInNode(*host)));
  // TODO(crbug.com/1157146): This returns null instead of the table.
  EXPECT_EQ(table, TableElementJustBefore(CreateVisiblePosition(
                       PositionInFlatTree::LastPositionInNode(*host))));
}

// TODO(crbug.com/1157146): This test breaks without Shadow DOM v0.
TEST_F(EditingUtilitiesTest,
       DISABLED_lastEditablePositionBeforePositionInRoot) {
  const char* body_content =
      "<p id='host' contenteditable><b slot=#one id='one'>1</b><b slot=#two "
      "id='two'>22</b></p>";
  const char* shadow_content =
      "<slot name=#two></slot><slot name=#one></slot><b "
      "id='three'>333</b>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");
  Element* host = GetDocument().getElementById(AtomicString("host"));
  Node* one = GetDocument().getElementById(AtomicString("one"));
  Node* two = GetDocument().getElementById(AtomicString("two"));
  Node* three = shadow_root->getElementById(AtomicString("three"));

  EXPECT_EQ(Position(one, 0),
            LastEditablePositionBeforePositionInRoot(Position(one, 0), *host));
  // TODO(crbug.com/1157146): This returns the beginning of "1" instead of the
  // end of "22".
  EXPECT_EQ(Position(two->firstChild(), 2),
            CreateVisiblePosition(LastEditablePositionBeforePositionInRoot(
                                      Position(one, 0), *host))
                .DeepEquivalent());

  EXPECT_EQ(PositionInFlatTree(one, 0),
            LastEditablePositionBeforePositionInRoot(PositionInFlatTree(one, 0),
                                                     *host));
  // TODO(crbug.com/1157146): This returns the beginning of "1" instead of the
  // end of "22".
  EXPECT_EQ(PositionInFlatTree(two->firstChild(), 2),
            CreateVisiblePosition(LastEditablePositionBeforePositionInRoot(
                                      PositionInFlatTree(one, 0), *host))
                .DeepEquivalent());

  EXPECT_EQ(
      Position::FirstPositionInNode(*host),
      LastEditablePositionBeforePositionInRoot(Position(three, 0), *host));
  // TODO(crbug.com/1157146): This returns the beginning of "1" instead of the
  // beginning of "22".
  EXPECT_EQ(Position(two->firstChild(), 0),
            CreateVisiblePosition(LastEditablePositionBeforePositionInRoot(
                                      Position(three, 0), *host))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree::FirstPositionInNode(*host),
            LastEditablePositionBeforePositionInRoot(
                PositionInFlatTree(three, 0), *host));
  // TODO(crbug.com/1157146): This returns nullptr instead of the beginning of
  // "22".
  EXPECT_EQ(PositionInFlatTree(two->firstChild(), 0),
            CreateVisiblePosition(LastEditablePositionBeforePositionInRoot(
                                      PositionInFlatTree(three, 0), *host))
                .DeepEquivalent());
}

// TODO(crbug.com/1157146): This test breaks without Shadow DOM v0.
TEST_F(EditingUtilitiesTest, DISABLED_NextNodeIndex) {
  const char* body_content =
      "<p id='host'>00<b slot='#one' id='one'>11</b><b slot='#two' "
      "id='two'>22</b>33</p>";
  const char* shadow_content = "<slot name=#two></slot><slot name=#one></slot>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");
  Node* host = GetDocument().getElementById(AtomicString("host"));
  Node* two = GetDocument().getElementById(AtomicString("two"));

  EXPECT_EQ(
      Position(host, 3),
      NextPositionOf(Position(two, 1), PositionMoveType::kGraphemeCluster));
  // TODO(crbug.com/1157146): This returns the slot instead of the <p>.
  EXPECT_EQ(PositionInFlatTree(host, 1),
            NextPositionOf(PositionInFlatTree(two, 1),
                           PositionMoveType::kGraphemeCluster));
}

TEST_F(EditingUtilitiesTest, NextVisuallyDistinctCandidate) {
  const char* body_content =
      "<p id='host'>00<b slot='#one' id='one'>11</b><b slot='#two' "
      "id='two'>22</b><b slot='#three' id='three'>33</b></p>";
  const char* shadow_content =
      "<slot name=#two></slot><slot name=#one></slot><slot name=#three></slot>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");
  Node* one = GetDocument().getElementById(AtomicString("one"));
  Node* two = GetDocument().getElementById(AtomicString("two"));
  Node* three = GetDocument().getElementById(AtomicString("three"));

  EXPECT_EQ(Position(two->firstChild(), 0),
            NextVisuallyDistinctCandidate(Position(one, 1)));
  EXPECT_EQ(PositionInFlatTree(three->firstChild(), 1),
            NextVisuallyDistinctCandidate(PositionInFlatTree(one, 1)));
}

TEST_F(EditingUtilitiesTest, uncheckedPreviousNextOffset_FirstLetter) {
  SetBodyContent(
      "<style>p::first-letter {color:red;}</style><p id='target'>abc</p>");
  const Node& node =
      *GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(2, PreviousGraphemeBoundaryOf(node, 3));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(node, 2));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(node, 0));
  EXPECT_EQ(2, NextGraphemeBoundaryOf(node, 1));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(node, 2));

  UpdateAllLifecyclePhasesForTest();
  EXPECT_NE(nullptr, node.GetLayoutObject());
  EXPECT_EQ(2, PreviousGraphemeBoundaryOf(node, 3));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(node, 2));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(node, 0));
  EXPECT_EQ(2, NextGraphemeBoundaryOf(node, 1));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(node, 2));
}

TEST_F(EditingUtilitiesTest, uncheckedPreviousNextOffset_textTransform) {
  SetBodyContent(
      "<style>p {text-transform:uppercase}</style><p id='target'>abc</p>");
  const Node& node =
      *GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(2, PreviousGraphemeBoundaryOf(node, 3));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(node, 2));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(node, 0));
  EXPECT_EQ(2, NextGraphemeBoundaryOf(node, 1));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(node, 2));

  UpdateAllLifecyclePhasesForTest();
  EXPECT_NE(nullptr, node.GetLayoutObject());
  EXPECT_EQ(2, PreviousGraphemeBoundaryOf(node, 3));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(node, 2));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(node, 0));
  EXPECT_EQ(2, NextGraphemeBoundaryOf(node, 1));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(node, 2));
}

// Following breaking rules come from http://unicode.org/reports/tr29/
// Note that some of rules are in draft. Also see
// http://www.unicode.org/reports/tr29/proposed.html
TEST_F(EditingUtilitiesTest, uncheckedPreviousNextOffset) {
  // GB1: Break at the start of text.
  SetBodyContent("<p id='target'>a</p>");
  Node* node =
      GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));

  // GB2: Break at the end of text.
  SetBodyContent("<p id='target'>a</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));

  // GB3: Do not break between CR and LF.
  SetBodyContent("<p id='target'>a&#x0D;&#x0A;b</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(3, PreviousGraphemeBoundaryOf(*node, 4));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(4, NextGraphemeBoundaryOf(*node, 3));

  // GB4,GB5: Break before and after CR/LF/Control.
  SetBodyContent("<p id='target'>a&#x0D;b</p>");  // CR
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(2, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(2, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 2));
  SetBodyContent("<p id='target'>a&#x0A;b</p>");  // LF
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(2, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(2, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 2));
  // U+00AD(SOFT HYPHEN) has Control property.
  SetBodyContent("<p id='target'>a&#xAD;b</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(2, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(2, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 2));

  // GB6: Don't break Hangul sequence.
  const std::string l =
      "&#x1100;";  // U+1100 (HANGUL CHOSEONG KIYEOK) has L property.
  const std::string v =
      "&#x1160;";  // U+1160 (HANGUL JUNGSEONG FILLER) has V property.
  const std::string lv =
      "&#xAC00;";  // U+AC00 (HANGUL SYLLABLE GA) has LV property.
  const std::string lvt =
      "&#xAC01;";  // U+AC01 (HANGUL SYLLABLE GAG) has LVT property.
  const std::string t =
      "&#x11A8;";  // U+11A8 (HANGUL JONGSEONG KIYEOK) has T property.
  SetBodyContent("<p id='target'>a" + l + l + "b</p>");  // L x L
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(3, PreviousGraphemeBoundaryOf(*node, 4));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(4, NextGraphemeBoundaryOf(*node, 3));
  SetBodyContent("<p id='target'>a" + l + v + "b</p>");  // L x V
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(3, PreviousGraphemeBoundaryOf(*node, 4));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(4, NextGraphemeBoundaryOf(*node, 3));
  SetBodyContent("<p id='target'>a" + l + lv + "b</p>");  // L x LV
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(3, PreviousGraphemeBoundaryOf(*node, 4));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(4, NextGraphemeBoundaryOf(*node, 3));
  SetBodyContent("<p id='target'>a" + l + lvt + "b</p>");  // L x LVT
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(3, PreviousGraphemeBoundaryOf(*node, 4));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(4, NextGraphemeBoundaryOf(*node, 3));

  // GB7: Don't break Hangul sequence.
  SetBodyContent("<p id='target'>a" + lv + v + "b</p>");  // LV x V
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(3, PreviousGraphemeBoundaryOf(*node, 4));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(4, NextGraphemeBoundaryOf(*node, 3));
  SetBodyContent("<p id='target'>a" + lv + t + "b</p>");  // LV x T
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(3, PreviousGraphemeBoundaryOf(*node, 4));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(4, NextGraphemeBoundaryOf(*node, 3));
  SetBodyContent("<p id='target'>a" + v + v + "b</p>");  // V x V
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(3, PreviousGraphemeBoundaryOf(*node, 4));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(4, NextGraphemeBoundaryOf(*node, 3));
  SetBodyContent("<p id='target'>a" + v + t + "b</p>");  // V x T
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(3, PreviousGraphemeBoundaryOf(*node, 4));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(4, NextGraphemeBoundaryOf(*node, 3));

  // GB8: Don't break Hangul sequence.
  SetBodyContent("<p id='target'>a" + lvt + t + "b</p>");  // LVT x T
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(3, PreviousGraphemeBoundaryOf(*node, 4));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(4, NextGraphemeBoundaryOf(*node, 3));
  SetBodyContent("<p id='target'>a" + t + t + "b</p>");  // T x T
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(3, PreviousGraphemeBoundaryOf(*node, 4));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(4, NextGraphemeBoundaryOf(*node, 3));

  // Break other Hangul syllable combination. See test of GB999.

  // GB12: Don't break between regional indicator if there are even numbered
  // regional indicator symbols before.
  // U+1F1FA is REGIONAL INDICATOR SYMBOL LETTER U.
  // U+1F1F8 is REGIONAL INDICATOR SYMBOL LETTER S.
  const std::string flag = "&#x1F1FA;&#x1F1F8;";  // US flag.
  // ^(RI RI)* RI x RI
  SetBodyContent("<p id='target'>" + flag + flag + flag + flag + "a</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(16, PreviousGraphemeBoundaryOf(*node, 17));
  EXPECT_EQ(12, PreviousGraphemeBoundaryOf(*node, 16));
  EXPECT_EQ(8, PreviousGraphemeBoundaryOf(*node, 12));
  EXPECT_EQ(4, PreviousGraphemeBoundaryOf(*node, 8));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 4));
  EXPECT_EQ(4, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(8, NextGraphemeBoundaryOf(*node, 4));
  EXPECT_EQ(12, NextGraphemeBoundaryOf(*node, 8));
  EXPECT_EQ(16, NextGraphemeBoundaryOf(*node, 12));
  EXPECT_EQ(17, NextGraphemeBoundaryOf(*node, 16));

  // GB8c: Don't break between regional indicator if there are even numbered
  // regional indicator symbols before.
  // [^RI] (RI RI)* RI x RI
  SetBodyContent("<p id='target'>a" + flag + flag + flag + flag + "b</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(17, PreviousGraphemeBoundaryOf(*node, 18));
  EXPECT_EQ(13, PreviousGraphemeBoundaryOf(*node, 17));
  EXPECT_EQ(9, PreviousGraphemeBoundaryOf(*node, 13));
  EXPECT_EQ(5, PreviousGraphemeBoundaryOf(*node, 9));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 5));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(5, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(9, NextGraphemeBoundaryOf(*node, 5));
  EXPECT_EQ(13, NextGraphemeBoundaryOf(*node, 9));
  EXPECT_EQ(17, NextGraphemeBoundaryOf(*node, 13));
  EXPECT_EQ(18, NextGraphemeBoundaryOf(*node, 17));

  // GB8c: Break if there is an odd number of regional indicator symbols before.
  SetBodyContent("<p id='target'>a" + flag + flag + flag + flag +
                 "&#x1F1F8;b</p>");  // RI ÷ RI
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(19, PreviousGraphemeBoundaryOf(*node, 20));
  EXPECT_EQ(17, PreviousGraphemeBoundaryOf(*node, 19));
  EXPECT_EQ(13, PreviousGraphemeBoundaryOf(*node, 17));
  EXPECT_EQ(9, PreviousGraphemeBoundaryOf(*node, 13));
  EXPECT_EQ(5, PreviousGraphemeBoundaryOf(*node, 9));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 5));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(5, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(9, NextGraphemeBoundaryOf(*node, 5));
  EXPECT_EQ(13, NextGraphemeBoundaryOf(*node, 9));
  EXPECT_EQ(17, NextGraphemeBoundaryOf(*node, 13));
  EXPECT_EQ(19, NextGraphemeBoundaryOf(*node, 17));
  EXPECT_EQ(20, NextGraphemeBoundaryOf(*node, 19));

  // GB9: Do not break before extending characters or ZWJ.
  // U+0300(COMBINING GRAVE ACCENT) has Extend property.
  SetBodyContent("<p id='target'>a&#x0300;b</p>");  // x Extend
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(2, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(2, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 2));
  // U+200D is ZERO WIDTH JOINER.
  SetBodyContent("<p id='target'>a&#x200D;b</p>");  // x ZWJ
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(2, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(2, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 2));

  // GB9a: Do not break before SpacingMarks.
  // U+0903(DEVANAGARI SIGN VISARGA) has SpacingMark property.
  SetBodyContent("<p id='target'>a&#x0903;b</p>");  // x SpacingMark
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(2, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(2, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 2));

  // GB9b: Do not break after Prepend.
  // TODO(nona): Introduce Prepend test case once ICU grabs Unicode 9.0.

  // For https://bugs.webkit.org/show_bug.cgi?id=24342
  // The break should happens after Thai character.
  SetBodyContent("<p id='target'>a&#x0E40;b</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(2, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(2, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 2));

  // Blink customization: Don't break before Japanese half-width katakana voiced
  // marks.
  SetBodyContent("<p id='target'>a&#xFF76;&#xFF9E;b</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(3, PreviousGraphemeBoundaryOf(*node, 4));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(4, NextGraphemeBoundaryOf(*node, 3));

  // Additional rule for IndicSyllabicCategory=Virama: Do not break after that.
  // See
  // http://www.unicode.org/Public/9.0.0/ucd/IndicSyllabicCategory-9.0.0d2.txt
  // U+0905 is DEVANAGARI LETTER A. This has Extend property.
  // U+094D is DEVANAGARI SIGN VIRAMA. This has Virama property.
  // U+0915 is DEVANAGARI LETTER KA.
  SetBodyContent("<p id='target'>a&#x0905;&#x094D;&#x0915;b</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(4, PreviousGraphemeBoundaryOf(*node, 5));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 4));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(4, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(5, NextGraphemeBoundaryOf(*node, 4));
  // U+0E01 is THAI CHARACTER KO KAI
  // U+0E3A is THAI CHARACTER PHINTHU
  // Should break after U+0E3A since U+0E3A has Virama property but not listed
  // in IndicSyllabicCategory=Virama.
  SetBodyContent("<p id='target'>a&#x0E01;&#x0E3A;&#x0E01;b</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(4, PreviousGraphemeBoundaryOf(*node, 5));
  EXPECT_EQ(3, PreviousGraphemeBoundaryOf(*node, 4));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(4, NextGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(5, NextGraphemeBoundaryOf(*node, 4));

  // GB10: Do not break within emoji modifier.
  // GB10 is deleted in Unicode 11, but it's subsumed by GB9 by
  // extending the definition of Extend to include E_Base, E_Modifier,
  // etc. E_Base, E_Modifier and E_Base_GAZ are obsolete.
  // U+1F385(FATHER CHRISTMAS) used to have E_Base property.
  // U+1F3FB(EMOJI MODIFIER FITZPATRICK TYPE-1-2) used to have
  // E_Modifier property.
  SetBodyContent(
      "<p id='target'>a&#x1F385;&#x1F3FB;b</p>");  // E_Base x E_Modifier
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(5, PreviousGraphemeBoundaryOf(*node, 6));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 5));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(5, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(6, NextGraphemeBoundaryOf(*node, 5));
  // U+1F466(BOY) used to have EBG property, but now has Extend property.
  SetBodyContent(
      "<p id='target'>a&#x1F466;&#x1F3FB;b</p>");  // EBG x E_Modifier
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(5, PreviousGraphemeBoundaryOf(*node, 6));
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 5));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(5, NextGraphemeBoundaryOf(*node, 1));
  EXPECT_EQ(6, NextGraphemeBoundaryOf(*node, 5));

  // GB11: Do not break within ZWJ emoji sequence.
  // U+2764(HEAVY BLACK HEART) has Extended_Pictographic=True.
  // So does U+1F466.
  SetBodyContent(
      "<p id='target'>a&#x200D;&#x2764;b</p>");  // ZWJ x Glue_After_Zwj
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(3, PreviousGraphemeBoundaryOf(*node, 4));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(4, NextGraphemeBoundaryOf(*node, 3));
  SetBodyContent("<p id='target'>a&#x200D;&#x1F466;b</p>");  // ZWJ x EBG
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(4, PreviousGraphemeBoundaryOf(*node, 5));
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 4));
  EXPECT_EQ(4, NextGraphemeBoundaryOf(*node, 0));
  EXPECT_EQ(5, NextGraphemeBoundaryOf(*node, 4));

  // U+1F5FA(World Map) has Extended_Pictographic=True.
  SetBodyContent("<p id='target'>&#x200D;&#x1F5FA;</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 0));

  // GB999: Otherwise break everywhere.
  // Breaks between Hangul syllable except for GB6, GB7, GB8.
  SetBodyContent("<p id='target'>" + l + t + "</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  SetBodyContent("<p id='target'>" + v + l + "</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  SetBodyContent("<p id='target'>" + v + lv + "</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  SetBodyContent("<p id='target'>" + v + lvt + "</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  SetBodyContent("<p id='target'>" + lv + l + "</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  SetBodyContent("<p id='target'>" + lv + lv + "</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  SetBodyContent("<p id='target'>" + lv + lvt + "</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  SetBodyContent("<p id='target'>" + lvt + l + "</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  SetBodyContent("<p id='target'>" + lvt + v + "</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  SetBodyContent("<p id='target'>" + lvt + lv + "</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  SetBodyContent("<p id='target'>" + lvt + lvt + "</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  SetBodyContent("<p id='target'>" + t + l + "</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  SetBodyContent("<p id='target'>" + t + v + "</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  SetBodyContent("<p id='target'>" + t + lv + "</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
  SetBodyContent("<p id='target'>" + t + lvt + "</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));

  // Per GB8, do not break before Extended|ZWJ. E_Modifier is obsolete
  // in Unicode 11 and is now a part of Extended.
  SetBodyContent("<p id='target'>a&#x1F3FB;</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 3));
  EXPECT_EQ(3, NextGraphemeBoundaryOf(*node, 0));
  SetBodyContent("<p id='target'>&#x1F5FA;&#x1F3FB;</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(0, PreviousGraphemeBoundaryOf(*node, 4));
  EXPECT_EQ(4, NextGraphemeBoundaryOf(*node, 0));

  // For GB11, if trailing character is not Glue_After_Zwj or EBG, break happens
  // after ZWJ.
  // U+1F5FA(WORLD MAP) doesn't have either Glue_After_Zwj or EBG.
  SetBodyContent("<p id='target'>&#x200D;a</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(1, PreviousGraphemeBoundaryOf(*node, 2));
  EXPECT_EQ(1, NextGraphemeBoundaryOf(*node, 0));
}

TEST_F(EditingUtilitiesTest, previousPositionOf_Backspace) {
  // BMP characters. Only one code point should be deleted.
  SetBodyContent("<p id='target'>abc</p>");
  Node* node =
      GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(Position(node, 2),
            PreviousPositionOf(Position(node, 3),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 1),
            PreviousPositionOf(Position(node, 2),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 0),
            PreviousPositionOf(Position(node, 1),
                               PositionMoveType::kBackwardDeletion));
}

TEST_F(EditingUtilitiesTest, previousPositionOf_Backspace_FirstLetter) {
  SetBodyContent(
      "<style>p::first-letter {color:red;}</style><p id='target'>abc</p>");
  Node* node =
      GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(Position(node, 2),
            PreviousPositionOf(Position(node, 3),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 1),
            PreviousPositionOf(Position(node, 2),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 0),
            PreviousPositionOf(Position(node, 1),
                               PositionMoveType::kBackwardDeletion));

  SetBodyContent(
      "<style>p::first-letter {color:red;}</style><p id='target'>(a)bc</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(Position(node, 4),
            PreviousPositionOf(Position(node, 5),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 3),
            PreviousPositionOf(Position(node, 4),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 2),
            PreviousPositionOf(Position(node, 3),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 1),
            PreviousPositionOf(Position(node, 2),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 0),
            PreviousPositionOf(Position(node, 1),
                               PositionMoveType::kBackwardDeletion));
}

TEST_F(EditingUtilitiesTest, previousPositionOf_Backspace_TextTransform) {
  // Uppercase of &#x00DF; will be transformed to SS.
  SetBodyContent(
      "<style>p {text-transform:uppercase}</style><p "
      "id='target'>&#x00DF;abc</p>");
  Node* node =
      GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(Position(node, 3),
            PreviousPositionOf(Position(node, 4),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 2),
            PreviousPositionOf(Position(node, 3),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 1),
            PreviousPositionOf(Position(node, 2),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 0),
            PreviousPositionOf(Position(node, 1),
                               PositionMoveType::kBackwardDeletion));
}

TEST_F(EditingUtilitiesTest, IsTabHTMLSpanElementOnDisplayNone) {
  SetBodyContent("<span style=\"display:none\">\t</span>");
  const Node* const node = GetDocument().QuerySelector(AtomicString("span"));
  EXPECT_EQ(false, IsTabHTMLSpanElement(node));
}

TEST_F(EditingUtilitiesTest, previousPositionOf_Backspace_SurrogatePairs) {
  // Supplementary plane characters. Only one code point should be deleted.
  // &#x1F441; is EYE.
  SetBodyContent("<p id='target'>&#x1F441;&#x1F441;&#x1F441;</p>");
  Node* node =
      GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(Position(node, 4),
            PreviousPositionOf(Position(node, 6),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 2),
            PreviousPositionOf(Position(node, 4),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 0),
            PreviousPositionOf(Position(node, 2),
                               PositionMoveType::kBackwardDeletion));

  // BMP and Supplementary plane case.
  SetBodyContent("<p id='target'>&#x1F441;a&#x1F441;a</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(Position(node, 5),
            PreviousPositionOf(Position(node, 6),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 3),
            PreviousPositionOf(Position(node, 5),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 2),
            PreviousPositionOf(Position(node, 3),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 0),
            PreviousPositionOf(Position(node, 2),
                               PositionMoveType::kBackwardDeletion));

  // Edge case: broken surrogate pairs.
  SetBodyContent(
      "<p id='target'>&#xD83D;</p>");  // &#xD83D; is unpaired lead surrogate.
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(Position(node, 0),
            PreviousPositionOf(Position(node, 1),
                               PositionMoveType::kBackwardDeletion));

  // &#xD83D; is unpaired lead surrogate.
  SetBodyContent("<p id='target'>&#x1F441;&#xD83D;&#x1F441;</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(Position(node, 3),
            PreviousPositionOf(Position(node, 5),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 2),
            PreviousPositionOf(Position(node, 3),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 0),
            PreviousPositionOf(Position(node, 2),
                               PositionMoveType::kBackwardDeletion));

  SetBodyContent(
      "<p id='target'>a&#xD83D;a</p>");  // &#xD83D; is unpaired lead surrogate.
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(Position(node, 2),
            PreviousPositionOf(Position(node, 3),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 1),
            PreviousPositionOf(Position(node, 2),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 0),
            PreviousPositionOf(Position(node, 1),
                               PositionMoveType::kBackwardDeletion));

  SetBodyContent(
      "<p id='target'>&#xDC41;</p>");  // &#xDC41; is unpaired trail surrogate.
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(Position(node, 0),
            PreviousPositionOf(Position(node, 1),
                               PositionMoveType::kBackwardDeletion));

  // &#xDC41; is unpaired trail surrogate.
  SetBodyContent("<p id='target'>&#x1F441;&#xDC41;&#x1F441;</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(Position(node, 3),
            PreviousPositionOf(Position(node, 5),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 2),
            PreviousPositionOf(Position(node, 3),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 0),
            PreviousPositionOf(Position(node, 2),
                               PositionMoveType::kBackwardDeletion));

  // &#xDC41; is unpaired trail surrogate.
  SetBodyContent("<p id='target'>a&#xDC41;a</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(Position(node, 2),
            PreviousPositionOf(Position(node, 3),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 1),
            PreviousPositionOf(Position(node, 2),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 0),
            PreviousPositionOf(Position(node, 1),
                               PositionMoveType::kBackwardDeletion));

  // Edge case: specify middle of surrogate pairs.
  SetBodyContent("<p id='target'>&#x1F441;&#x1F441;&#x1F441</p>");
  node = GetDocument().getElementById(AtomicString("target"))->firstChild();
  EXPECT_EQ(Position(node, 4),
            PreviousPositionOf(Position(node, 5),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 2),
            PreviousPositionOf(Position(node, 3),
                               PositionMoveType::kBackwardDeletion));
  EXPECT_EQ(Position(node, 0),
            PreviousPositionOf(Position(node, 1),
                               PositionMoveType::kBackwardDeletion));
}

// crbug.com/1503530
TEST_F(EditingUtilitiesTest, IsRenderedAsNonInlineTableImageOrHR) {
  SetBodyContent("<p id='target' hidden></p>");
  IsRenderedAsNonInlineTableImageOrHR(GetElementById("target"));
  // Pass if no crash.
}

}  // namespace blink
