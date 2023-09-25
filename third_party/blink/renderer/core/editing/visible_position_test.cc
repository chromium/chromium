// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/visible_position.h"

#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"

namespace blink {

class VisiblePositionTest : public EditingTestBase {};

TEST_F(VisiblePositionTest, EmptyEditable) {
  SetBodyContent("<div id=target contenteditable></div>");
  const Element& target = *GetElementById("target");

  EXPECT_EQ(Position(target, 0),
            CreateVisiblePosition(Position(target, 0)).DeepEquivalent());
  EXPECT_EQ(Position(target, 0),
            CreateVisiblePosition(Position::FirstPositionInNode(target))
                .DeepEquivalent());
  EXPECT_EQ(Position(target, 0),
            CreateVisiblePosition(Position::LastPositionInNode(target))
                .DeepEquivalent());
}

TEST_F(VisiblePositionTest, EmptyEditableWithBlockChild) {
  // Note: Placeholder <br> is needed to have non-zero editable.
  SetBodyContent("<div id=target contenteditable><div><br></div></div>");
  const Element& target = *GetElementById("target");
  const Node& div = *target.firstChild();
  const Node& br = *div.firstChild();

  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position(target, 0)).DeepEquivalent());
  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position::FirstPositionInNode(target))
                .DeepEquivalent());
  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position::LastPositionInNode(target))
                .DeepEquivalent());
  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position(target, 1)).DeepEquivalent());
  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position(div, 0)).DeepEquivalent());
  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position::BeforeNode(div)).DeepEquivalent());
  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position::AfterNode(div)).DeepEquivalent());
  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position::BeforeNode(br)).DeepEquivalent());
  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position::AfterNode(br)).DeepEquivalent());
}

TEST_F(VisiblePositionTest, EmptyEditableWithInlineChild) {
  SetBodyContent("<div id=target contenteditable><span></span></div>");
  const Element& target = *GetElementById("target");
  const Node& span = *target.firstChild();

  EXPECT_EQ(Position(target, 0),
            CreateVisiblePosition(Position(target, 0)).DeepEquivalent());
  EXPECT_EQ(Position(target, 0),
            CreateVisiblePosition(Position::FirstPositionInNode(target))
                .DeepEquivalent());
  EXPECT_EQ(Position(target, 0),
            CreateVisiblePosition(Position::LastPositionInNode(target))
                .DeepEquivalent());
  EXPECT_EQ(Position(target, 0),
            CreateVisiblePosition(Position(target, 1)).DeepEquivalent());
  EXPECT_EQ(Position(target, 0),
            CreateVisiblePosition(Position(span, 0)).DeepEquivalent());
  EXPECT_EQ(Position(target, 0),
            CreateVisiblePosition(Position::BeforeNode(span)).DeepEquivalent());
  EXPECT_EQ(Position(target, 0),
            CreateVisiblePosition(Position::AfterNode(span)).DeepEquivalent());
}

TEST_F(VisiblePositionTest, PlaceholderBR) {
  SetBodyContent("<div id=target><br id=br></div>");
  const Element& target = *GetElementById("target");
  const Element& br = *GetElementById("br");

  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position(target, 0)).DeepEquivalent());
  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position::FirstPositionInNode(target))
                .DeepEquivalent());
  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position::LastPositionInNode(target))
                .DeepEquivalent());
  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position(target, 1)).DeepEquivalent());
  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position(br, 0)).DeepEquivalent());
  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position::BeforeNode(br)).DeepEquivalent());
  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position::AfterNode(br)).DeepEquivalent());
}

TEST_F(VisiblePositionTest, PlaceholderBRWithCollapsedSpace) {
  SetBodyContent("<div id=target> <br id=br> </div>");
  const Element& target = *GetElementById("target");
  const Element& br = *GetElementById("br");

  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position(target, 0)).DeepEquivalent());
  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position::FirstPositionInNode(target))
                .DeepEquivalent());
  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position::LastPositionInNode(target))
                .DeepEquivalent());
  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position(target, 1)).DeepEquivalent());
  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position(target, 2)).DeepEquivalent());
  EXPECT_EQ(
      Position::BeforeNode(br),
      CreateVisiblePosition(Position(target.firstChild(), 0)).DeepEquivalent());
  EXPECT_EQ(
      Position::BeforeNode(br),
      CreateVisiblePosition(Position(target.firstChild(), 1)).DeepEquivalent());
  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position(br, 0)).DeepEquivalent());
  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position::BeforeNode(br)).DeepEquivalent());
  EXPECT_EQ(Position::BeforeNode(br),
            CreateVisiblePosition(Position::AfterNode(br)).DeepEquivalent());
  EXPECT_EQ(
      Position::BeforeNode(br),
      CreateVisiblePosition(Position(target.lastChild(), 0)).DeepEquivalent());
  EXPECT_EQ(
      Position::BeforeNode(br),
      CreateVisiblePosition(Position(target.lastChild(), 1)).DeepEquivalent());
}

#if DCHECK_IS_ON()

TEST_F(VisiblePositionTest, NullIsValid) {
  EXPECT_TRUE(VisiblePosition().IsValid());
}

TEST_F(VisiblePositionTest, NonNullIsValidBeforeMutation) {
  SetBodyContent("<p>one</p>");

  Element* paragraph = GetDocument().QuerySelector(AtomicString("p"));
  Position position(paragraph->firstChild(), 1);
  EXPECT_TRUE(CreateVisiblePosition(position).IsValid());
}

TEST_F(VisiblePositionTest, NonNullInvalidatedAfterDOMChange) {
  SetBodyContent("<p>one</p>");

  Element* paragraph = GetDocument().QuerySelector(AtomicString("p"));
  Position position(paragraph->firstChild(), 1);
  VisiblePosition null_visible_position;
  VisiblePosition non_null_visible_position = CreateVisiblePosition(position);

  Element* div = GetDocument().CreateRawElement(html_names::kDivTag);
  GetDocument().body()->AppendChild(div);

  EXPECT_TRUE(null_visible_position.IsValid());
  EXPECT_FALSE(non_null_visible_position.IsValid());

  UpdateAllLifecyclePhasesForTest();

  // Invalid VisiblePosition can never become valid again.
  EXPECT_FALSE(non_null_visible_position.IsValid());
}

TEST_F(VisiblePositionTest, NonNullInvalidatedAfterStyleChange) {
  SetBodyContent("<div>one</div><p>two</p>");

  Element* paragraph = GetDocument().QuerySelector(AtomicString("p"));
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Position position(paragraph->firstChild(), 1);

  VisiblePosition visible_position1 = CreateVisiblePosition(position);
  div->style()->setProperty(GetDocument().GetExecutionContext(), "color", "red",
                            "important", ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(visible_position1.IsValid());

  UpdateAllLifecyclePhasesForTest();

  VisiblePosition visible_position2 = CreateVisiblePosition(position);
  div->style()->setProperty(GetDocument().GetExecutionContext(), "display",
                            "none", "important", ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(visible_position2.IsValid());

  UpdateAllLifecyclePhasesForTest();

  // Invalid VisiblePosition can never become valid again.
  EXPECT_FALSE(visible_position1.IsValid());
  EXPECT_FALSE(visible_position2.IsValid());
}

#endif

TEST_F(VisiblePositionTest, NormalizationAroundLineBreak) {
  LoadAhem();
  InsertStyleElement(
      "div {"
      "width: 5.5ch;"
      "font: 10px/10px Ahem;"
      "word-wrap: break-word;"
      "}");
  SetBodyContent(
      "<div>line1line2</div>"
      "<div>line1<br>line2</div>"
      "<div>line1<wbr>line2</div>"
      "<div>line1<span></span>line2</div>"
      "<div>line1<span></span><span></span>line2</div>");

  StaticElementList* tests =
      GetDocument().QuerySelectorAll(AtomicString("div"));
  for (unsigned i = 0; i < tests->length(); ++i) {
    Element* test = tests->item(i);
    Node* node1 = test->firstChild();
    Node* node2 = test->lastChild();
    PositionWithAffinity line1_end(Position(node1, 5), TextAffinity::kUpstream);
    PositionWithAffinity line2_start(Position(node2, node1 == node2 ? 5 : 0),
                                     TextAffinity::kDownstream);
    PositionWithAffinity line1_end_normalized =
        CreateVisiblePosition(line1_end).ToPositionWithAffinity();
    PositionWithAffinity line2_start_normalized =
        CreateVisiblePosition(line2_start).ToPositionWithAffinity();

    EXPECT_FALSE(InSameLine(line1_end, line2_start));
    EXPECT_FALSE(InSameLine(line1_end_normalized, line2_start_normalized));
    EXPECT_TRUE(InSameLine(line1_end, line1_end_normalized));
    EXPECT_TRUE(InSameLine(line2_start, line2_start_normalized));
  }
}

TEST_F(VisiblePositionTest, SpacesAroundLineBreak) {
  // Narrow <body> forces "a" and "b" to be in different lines.
  InsertStyleElement("body { width: 1px }");
  {
    SetBodyContent("a b");
    Node* ab = GetDocument().body()->firstChild();
    EXPECT_EQ(Position(ab, 0),
              CreateVisiblePosition(Position(ab, 0)).DeepEquivalent());
    EXPECT_EQ(Position(ab, 1),
              CreateVisiblePosition(Position(ab, 1)).DeepEquivalent());
    EXPECT_EQ(Position(ab, 2),
              CreateVisiblePosition(Position(ab, 2)).DeepEquivalent());
  }
  {
    SetBodyContent("a<span> b</span>");
    Node* a = GetDocument().body()->firstChild();
    Node* b = a->nextSibling()->firstChild();
    EXPECT_EQ(Position(a, 0),
              CreateVisiblePosition(Position(a, 0)).DeepEquivalent());
    EXPECT_EQ(Position(a, 1),
              CreateVisiblePosition(Position(a, 1)).DeepEquivalent());
    EXPECT_EQ(Position(a, 1),
              CreateVisiblePosition(Position(b, 0)).DeepEquivalent());
    EXPECT_EQ(Position(b, 1),
              CreateVisiblePosition(Position(b, 1)).DeepEquivalent());
    EXPECT_EQ(Position(b, 2),
              CreateVisiblePosition(Position(b, 2)).DeepEquivalent());
  }
  {
    SetBodyContent("<span>a</span> b");
    Node* b = GetDocument().body()->lastChild();
    Node* a = b->previousSibling()->firstChild();
    EXPECT_EQ(Position(a, 0),
              CreateVisiblePosition(Position(a, 0)).DeepEquivalent());
    EXPECT_EQ(Position(a, 1),
              CreateVisiblePosition(Position(a, 1)).DeepEquivalent());
    EXPECT_EQ(Position(a, 1),
              CreateVisiblePosition(Position(b, 0)).DeepEquivalent());
    EXPECT_EQ(Position(b, 1),
              CreateVisiblePosition(Position(b, 1)).DeepEquivalent());
    EXPECT_EQ(Position(b, 2),
              CreateVisiblePosition(Position(b, 2)).DeepEquivalent());
  }
  {
    SetBodyContent("a <span>b</span>");
    Node* a = GetDocument().body()->firstChild();
    Node* b = a->nextSibling()->firstChild();
    EXPECT_EQ(Position(a, 0),
              CreateVisiblePosition(Position(a, 0)).DeepEquivalent());
    EXPECT_EQ(Position(a, 1),
              CreateVisiblePosition(Position(a, 1)).DeepEquivalent());
    EXPECT_EQ(Position(a, 2),
              CreateVisiblePosition(Position(a, 2)).DeepEquivalent());
    EXPECT_EQ(Position(a, 2),
              CreateVisiblePosition(Position(b, 0)).DeepEquivalent());
    EXPECT_EQ(Position(b, 1),
              CreateVisiblePosition(Position(b, 1)).DeepEquivalent());
  }
  {
    SetBodyContent("<span>a </span>b");
    Node* b = GetDocument().body()->lastChild();
    Node* a = b->previousSibling()->firstChild();
    EXPECT_EQ(Position(a, 0),
              CreateVisiblePosition(Position(a, 0)).DeepEquivalent());
    EXPECT_EQ(Position(a, 1),
              CreateVisiblePosition(Position(a, 1)).DeepEquivalent());
    EXPECT_EQ(Position(a, 2),
              CreateVisiblePosition(Position(a, 2)).DeepEquivalent());
    EXPECT_EQ(Position(a, 2),
              CreateVisiblePosition(Position(b, 0)).DeepEquivalent());
    EXPECT_EQ(Position(b, 1),
              CreateVisiblePosition(Position(b, 1)).DeepEquivalent());
  }
}

TEST_F(VisiblePositionTest, TextCombine) {
  InsertStyleElement(
      "div {"
      "  font: 100px/110px Ahem;"
      "  writing-mode: vertical-rl;"
      "}"
      "tcy { text-combine-upright: all; }");
  SetBodyInnerHTML("<div>a<tcy id=target>01234</tcy>b</div>");
  const auto& target = *GetElementById("target");
  const auto& text_a = *To<Text>(target.previousSibling());
  const auto& text_01234 = *To<Text>(target.firstChild());
  const auto& text_b = *To<Text>(target.nextSibling());

  EXPECT_EQ(Position(text_a, 0),
            CreateVisiblePosition(Position(text_a, 0)).DeepEquivalent());
  EXPECT_EQ(Position(text_a, 1),
            CreateVisiblePosition(Position(text_a, 1)).DeepEquivalent());

  if (text_01234.GetLayoutObject()->Parent()->IsLayoutTextCombine()) {
    EXPECT_EQ(Position(text_01234, 0),
              CreateVisiblePosition(Position(text_01234, 0)).DeepEquivalent());
  } else {
    EXPECT_EQ(Position(text_a, 1),
              CreateVisiblePosition(Position(text_01234, 0)).DeepEquivalent());
  }
  EXPECT_EQ(Position(text_01234, 1),
            CreateVisiblePosition(Position(text_01234, 1)).DeepEquivalent());
  EXPECT_EQ(Position(text_01234, 2),
            CreateVisiblePosition(Position(text_01234, 2)).DeepEquivalent());
  EXPECT_EQ(Position(text_01234, 3),
            CreateVisiblePosition(Position(text_01234, 3)).DeepEquivalent());
  EXPECT_EQ(Position(text_01234, 4),
            CreateVisiblePosition(Position(text_01234, 4)).DeepEquivalent());
  EXPECT_EQ(Position(text_01234, 5),
            CreateVisiblePosition(Position(text_01234, 5)).DeepEquivalent());

  if (text_01234.GetLayoutObject()->Parent()->IsLayoutTextCombine()) {
    EXPECT_EQ(Position(text_b, 0),
              CreateVisiblePosition(Position(text_b, 0)).DeepEquivalent());
  } else {
    EXPECT_EQ(Position(text_01234, 5),
              CreateVisiblePosition(Position(text_b, 0)).DeepEquivalent());
  }
  EXPECT_EQ(Position(text_b, 1),
            CreateVisiblePosition(Position(text_b, 1)).DeepEquivalent());
}

}  // namespace blink
