// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/visible_units.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"

namespace blink {
namespace visible_units_test {

PositionWithAffinity PositionWithAffinityInDOMTree(
    Node& anchor,
    int offset,
    TextAffinity affinity = TextAffinity::kDownstream) {
  return PositionWithAffinity(CanonicalPositionOf(Position(&anchor, offset)),
                              affinity);
}

VisiblePosition CreateVisiblePositionInDOMTree(
    Node& anchor,
    int offset,
    TextAffinity affinity = TextAffinity::kDownstream) {
  return CreateVisiblePosition(Position(&anchor, offset), affinity);
}

PositionInFlatTreeWithAffinity PositionWithAffinityInFlatTree(
    Node& anchor,
    int offset,
    TextAffinity affinity = TextAffinity::kDownstream) {
  return PositionInFlatTreeWithAffinity(
      CanonicalPositionOf(PositionInFlatTree(&anchor, offset)), affinity);
}

VisiblePositionInFlatTree CreateVisiblePositionInFlatTree(
    Node& anchor,
    int offset,
    TextAffinity affinity = TextAffinity::kDownstream) {
  return CreateVisiblePosition(PositionInFlatTree(&anchor, offset), affinity);
}

class VisibleUnitsTest : public EditingTestBase {
 protected:
  std::string TestSnapBackward(
      const std::string& selection_text,
      EditingBoundaryCrossingRule rule = kCannotCrossEditingBoundary) {
    const Position position = SetCaretTextToBody(selection_text);
    return GetCaretTextFromBody(MostBackwardCaretPosition(position, rule));
  }

  std::string TestSnapForward(
      const std::string& selection_text,
      EditingBoundaryCrossingRule rule = kCannotCrossEditingBoundary) {
    const Position position = SetCaretTextToBody(selection_text);
    return GetCaretTextFromBody(MostForwardCaretPosition(position, rule));
  }
};

TEST_F(VisibleUnitsTest, caretMinOffset) {
  const char* body_content = "<p id=one>one</p>";
  SetBodyContent(body_content);

  Element* one = GetDocument().getElementById(AtomicString("one"));

  EXPECT_EQ(0, CaretMinOffset(one->firstChild()));
}

TEST_F(VisibleUnitsTest, caretMinOffsetWithFirstLetter) {
  const char* body_content =
      "<style>#one:first-letter { font-size: 200%; }</style><p id=one>one</p>";
  SetBodyContent(body_content);

  Element* one = GetDocument().getElementById(AtomicString("one"));

  EXPECT_EQ(0, CaretMinOffset(one->firstChild()));
}

TEST_F(VisibleUnitsTest, characterAfter) {
  const char* body_content =
      "<p id='host'><b slot='#one' id='one'>1</b><b slot='#two' "
      "id='two'>22</b></p><b "
      "id='three'>333</b>";
  const char* shadow_content =
      "<b id='four'>4444</b><slot name='#two'></slot><slot name=#one></slot><b "
      "id='five'>5555</b>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* one = GetDocument().getElementById(AtomicString("one"));
  Element* two = GetDocument().getElementById(AtomicString("two"));

  EXPECT_EQ(
      0, CharacterAfter(CreateVisiblePositionInDOMTree(*one->firstChild(), 1)));
  EXPECT_EQ('5', CharacterAfter(
                     CreateVisiblePositionInFlatTree(*one->firstChild(), 1)));

  EXPECT_EQ('1', CharacterAfter(
                     CreateVisiblePositionInDOMTree(*two->firstChild(), 2)));
  EXPECT_EQ('1', CharacterAfter(
                     CreateVisiblePositionInFlatTree(*two->firstChild(), 2)));
}

// http://crbug.com/1176202
TEST_F(VisibleUnitsTest, CanonicalPositionOfWithBefore) {
  LoadAhem();
  InsertStyleElement(
      "body { font: 10px/15px Ahem; }"
      "b::before { content: '\\u200B'");
  // |LayoutInline::PhysicalLinesBoundingBox()| for <span></span> returns
  //    LayoutNG: (0,0)+(0x10)
  //    Legacy:   (0,0)+(0x0)
  //  because we don't cull empty <span> in LayoutNG.
  SetBodyContent("<div contenteditable id=target><span></span><b></b></div>");
  Element& target = *GetElementById("target");

  EXPECT_EQ(Position(target, 0), CanonicalPositionOf(Position(target, 0)));
  EXPECT_EQ(Position(target, 0), CanonicalPositionOf(Position(target, 1)));
  EXPECT_EQ(Position(target, 0), CanonicalPositionOf(Position(target, 2)));
}

TEST_F(VisibleUnitsTest, canonicalPositionOfWithHTMLHtmlElement) {
  const char* body_content =
      "<html><div id=one contenteditable>1</div><span id=two "
      "contenteditable=false>22</span><span id=three "
      "contenteditable=false>333</span><span id=four "
      "contenteditable=false>333</span></html>";
  SetBodyContent(body_content);

  Node* one = GetDocument().QuerySelector(AtomicString("#one"));
  Node* two = GetDocument().QuerySelector(AtomicString("#two"));
  Node* three = GetDocument().QuerySelector(AtomicString("#three"));
  Node* four = GetDocument().QuerySelector(AtomicString("#four"));
  Element* html = GetDocument().CreateRawElement(html_names::kHTMLTag);
  // Move two, three and four into second html element.
  html->AppendChild(two);
  html->AppendChild(three);
  html->AppendChild(four);
  one->appendChild(html);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(Position(),
            CanonicalPositionOf(Position(GetDocument().documentElement(), 0)));

  EXPECT_EQ(Position(one->firstChild(), 0),
            CanonicalPositionOf(Position(one, 0)));
  EXPECT_EQ(Position(one->firstChild(), 1),
            CanonicalPositionOf(Position(one, 1)));

  EXPECT_EQ(Position(one->firstChild(), 0),
            CanonicalPositionOf(Position(one->firstChild(), 0)));
  EXPECT_EQ(Position(one->firstChild(), 1),
            CanonicalPositionOf(Position(one->firstChild(), 1)));

  EXPECT_EQ(Position(html, 0), CanonicalPositionOf(Position(html, 0)));
  EXPECT_EQ(Position(html, 1), CanonicalPositionOf(Position(html, 1)));
  EXPECT_EQ(Position(html, 2), CanonicalPositionOf(Position(html, 2)));

  EXPECT_EQ(Position(two->firstChild(), 0),
            CanonicalPositionOf(Position(two, 0)));
  EXPECT_EQ(Position(two->firstChild(), 2),
            CanonicalPositionOf(Position(two, 1)));
}

// For http://crbug.com/695317
TEST_F(VisibleUnitsTest, canonicalPositionOfWithInputElement) {
  SetBodyContent("<input>123");
  Element* const input = GetDocument().QuerySelector(AtomicString("input"));

  EXPECT_EQ(Position::BeforeNode(*input),
            CanonicalPositionOf(Position::FirstPositionInNode(
                *GetDocument().documentElement())));

  EXPECT_EQ(PositionInFlatTree::BeforeNode(*input),
            CanonicalPositionOf(PositionInFlatTree::FirstPositionInNode(
                *GetDocument().documentElement())));
}

// http://crbug.com/1116214
TEST_F(VisibleUnitsTest, canonicalPositionOfWithCrossBlockFlowlement) {
  const char* body_content =
      "<div id=one>line1<span>X</span><div>line2</div></div>"
      "<div id=two>line3"
      "<span style='user-select: none'>X</span><div>line4</div></div>"
      "<div id=three>line5"
      "<span style='user-select: none'>X</span>333<div>line6</div></div>";
  SetBodyContent(body_content);

  UpdateAllLifecyclePhasesForTest();

  Element* const one = GetDocument().QuerySelector(AtomicString("#one"));
  Element* const two = GetDocument().QuerySelector(AtomicString("#two"));
  Element* const three = GetDocument().QuerySelector(AtomicString("#three"));
  Element* const one_span = one->QuerySelector(AtomicString("span"));
  Element* const two_span = two->QuerySelector(AtomicString("span"));
  Element* const three_span = three->QuerySelector(AtomicString("span"));
  Position one_text_pos(one_span->firstChild(), 1);
  Position two_text_pos(two_span->firstChild(), 1);
  Position three_text_pos(three_span->firstChild(), 1);

  EXPECT_EQ(one_text_pos, CanonicalPositionOf(one_text_pos));

  EXPECT_EQ(Position::LastPositionInNode(*two->firstChild()),
            CanonicalPositionOf(two_text_pos));

  EXPECT_EQ(Position(*three->lastChild()->previousSibling(), 0),
            CanonicalPositionOf(three_text_pos));
}

TEST_F(VisibleUnitsTest, characterBefore) {
  const char* body_content =
      "<p id=host><b slot='#one' id=one>1</b><b slot='#two' "
      "id=two>22</b></p><b id=three>333</b>";
  const char* shadow_content =
      "<b id=four>4444</b><slot name='#two'></slot><slot name=#one></slot><b "
      "id=five>5555</b>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById(AtomicString("one"))->firstChild();
  Node* two = GetDocument().getElementById(AtomicString("two"))->firstChild();
  Node* five = shadow_root->getElementById(AtomicString("five"))->firstChild();

  EXPECT_EQ('2', CharacterBefore(CreateVisiblePositionInDOMTree(*one, 0)));
  EXPECT_EQ('2', CharacterBefore(CreateVisiblePositionInFlatTree(*one, 0)));

  EXPECT_EQ('1', CharacterBefore(CreateVisiblePositionInDOMTree(*one, 1)));
  EXPECT_EQ('1', CharacterBefore(CreateVisiblePositionInFlatTree(*one, 1)));

  EXPECT_EQ(0, CharacterBefore(CreateVisiblePositionInDOMTree(*two, 0)));
  EXPECT_EQ('4', CharacterBefore(CreateVisiblePositionInFlatTree(*two, 0)));

  EXPECT_EQ(0, CharacterBefore(CreateVisiblePositionInDOMTree(*five, 0)));
  EXPECT_EQ('1', CharacterBefore(CreateVisiblePositionInFlatTree(*five, 0)));
}

TEST_F(VisibleUnitsTest, endOfDocument) {
  const char* body_content =
      "<span id=host><b slot='#one' id=one>1</b><b slot='#two' "
      "id=two>22</b></span>";
  const char* shadow_content =
      "<p><slot name='#two'></slot></p><p><slot name=#one></slot></p>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* one = GetDocument().getElementById(AtomicString("one"));
  Element* two = GetDocument().getElementById(AtomicString("two"));

  EXPECT_EQ(Position(two->firstChild(), 2),
            EndOfDocument(CreateVisiblePositionInDOMTree(*one->firstChild(), 0))
                .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(one->firstChild(), 1),
      EndOfDocument(CreateVisiblePositionInFlatTree(*one->firstChild(), 0))
          .DeepEquivalent());

  EXPECT_EQ(Position(two->firstChild(), 2),
            EndOfDocument(CreateVisiblePositionInDOMTree(*two->firstChild(), 1))
                .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(one->firstChild(), 1),
      EndOfDocument(CreateVisiblePositionInFlatTree(*two->firstChild(), 1))
          .DeepEquivalent());
}

TEST_F(VisibleUnitsTest,
       AdjustForwardPositionToAvoidCrossingEditingBoundariesNestedEditable) {
  const SelectionInDOMTree& selection = SetSelectionTextToBody(
      "<div contenteditable>"
      "abc"
      "<span contenteditable=\"false\">A^BC</span>"
      "d|ef"
      "</div>");
  const PositionWithAffinity& result =
      AdjustForwardPositionToAvoidCrossingEditingBoundaries(
          PositionWithAffinity(selection.Focus()), selection.Anchor());
  ASSERT_TRUE(result.IsNotNull());
  EXPECT_EQ(
      "<div contenteditable>"
      "abc"
      "<span contenteditable=\"false\">ABC|</span>"
      "def"
      "</div>",
      GetCaretTextFromBody(result.GetPosition()));
  EXPECT_EQ(TextAffinity::kDownstream, result.Affinity());
}

TEST_F(VisibleUnitsTest, isEndOfEditableOrNonEditableContent) {
  const char* body_content =
      "<span id=host><b slot='#one' id=one contenteditable>1</b><b slot='#two' "
      "id=two>22</b></span>";
  const char* shadow_content =
      "<slot name='#two'></slot></p><p><slot name='#one'></slot>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* one = GetDocument().getElementById(AtomicString("one"));
  Element* two = GetDocument().getElementById(AtomicString("two"));

  EXPECT_FALSE(IsEndOfEditableOrNonEditableContent(
      CreateVisiblePositionInDOMTree(*one->firstChild(), 1)));
  EXPECT_TRUE(IsEndOfEditableOrNonEditableContent(
      CreateVisiblePositionInFlatTree(*one->firstChild(), 1)));

  EXPECT_TRUE(IsEndOfEditableOrNonEditableContent(
      CreateVisiblePositionInDOMTree(*two->firstChild(), 2)));
  EXPECT_FALSE(IsEndOfEditableOrNonEditableContent(
      CreateVisiblePositionInFlatTree(*two->firstChild(), 2)));
}

TEST_F(VisibleUnitsTest, isEndOfEditableOrNonEditableContentWithInput) {
  const char* body_content = "<input id=sample value=ab>cde";
  SetBodyContent(body_content);

  Node* text =
      ToTextControl(GetDocument().getElementById(AtomicString("sample")))
          ->InnerEditorElement()
          ->firstChild();

  EXPECT_FALSE(IsEndOfEditableOrNonEditableContent(
      CreateVisiblePositionInDOMTree(*text, 0)));
  EXPECT_FALSE(IsEndOfEditableOrNonEditableContent(
      CreateVisiblePositionInFlatTree(*text, 0)));

  EXPECT_FALSE(IsEndOfEditableOrNonEditableContent(
      CreateVisiblePositionInDOMTree(*text, 1)));
  EXPECT_FALSE(IsEndOfEditableOrNonEditableContent(
      CreateVisiblePositionInFlatTree(*text, 1)));

  EXPECT_TRUE(IsEndOfEditableOrNonEditableContent(
      CreateVisiblePositionInDOMTree(*text, 2)));
  EXPECT_TRUE(IsEndOfEditableOrNonEditableContent(
      CreateVisiblePositionInFlatTree(*text, 2)));
}

TEST_F(VisibleUnitsTest, IsVisuallyEquivalentCandidateWithHTMLHtmlElement) {
  const char* body_content =
      "<html><div id=one contenteditable>1</div><span id=two "
      "contenteditable=false>22</span><span id=three "
      "contenteditable=false>333</span><span id=four "
      "contenteditable=false>333</span></html>";
  SetBodyContent(body_content);

  Node* one = GetDocument().QuerySelector(AtomicString("#one"));
  Node* two = GetDocument().QuerySelector(AtomicString("#two"));
  Node* three = GetDocument().QuerySelector(AtomicString("#three"));
  Node* four = GetDocument().QuerySelector(AtomicString("#four"));
  Element* html = GetDocument().CreateRawElement(html_names::kHTMLTag);
  // Move two, three and four into second html element.
  html->AppendChild(two);
  html->AppendChild(three);
  html->AppendChild(four);
  one->appendChild(html);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(IsVisuallyEquivalentCandidate(
      Position(GetDocument().documentElement(), 0)));

  EXPECT_FALSE(IsVisuallyEquivalentCandidate(Position(one, 0)));
  EXPECT_FALSE(IsVisuallyEquivalentCandidate(Position(one, 1)));

  EXPECT_TRUE(IsVisuallyEquivalentCandidate(Position(one->firstChild(), 0)));
  EXPECT_TRUE(IsVisuallyEquivalentCandidate(Position(one->firstChild(), 1)));

  EXPECT_TRUE(IsVisuallyEquivalentCandidate(Position(html, 0)));
  EXPECT_TRUE(IsVisuallyEquivalentCandidate(Position(html, 1)));
  EXPECT_TRUE(IsVisuallyEquivalentCandidate(Position(html, 2)));

  EXPECT_FALSE(IsVisuallyEquivalentCandidate(Position(two, 0)));
  EXPECT_FALSE(IsVisuallyEquivalentCandidate(Position(two, 1)));
}

TEST_F(VisibleUnitsTest, isVisuallyEquivalentCandidateWithHTMLBodyElement) {
  const char* body_content =
      "<div id=one contenteditable>1</div><span id=two "
      "contenteditable=false>22</span><span id=three "
      "contenteditable=false>333</span><span id=four "
      "contenteditable=false>333</span>";
  SetBodyContent(body_content);

  Node* one = GetDocument().QuerySelector(AtomicString("#one"));
  Node* two = GetDocument().QuerySelector(AtomicString("#two"));
  Node* three = GetDocument().QuerySelector(AtomicString("#three"));
  Node* four = GetDocument().QuerySelector(AtomicString("#four"));
  Element* body = GetDocument().CreateRawElement(html_names::kBodyTag);
  Element* empty_body = GetDocument().CreateRawElement(html_names::kBodyTag);
  Element* div = GetDocument().CreateRawElement(html_names::kDivTag);
  Element* br = GetDocument().CreateRawElement(html_names::kBrTag);
  empty_body->appendChild(div);
  empty_body->appendChild(br);
  one->appendChild(empty_body);
  // Move two, three and four into second body element.
  body->appendChild(two);
  body->AppendChild(three);
  body->AppendChild(four);
  one->appendChild(body);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  EXPECT_FALSE(IsVisuallyEquivalentCandidate(
      Position(GetDocument().documentElement(), 0)));

  EXPECT_FALSE(IsVisuallyEquivalentCandidate(Position(one, 0)));
  EXPECT_FALSE(IsVisuallyEquivalentCandidate(Position(one, 1)));

  EXPECT_TRUE(IsVisuallyEquivalentCandidate(Position(one->firstChild(), 0)));
  EXPECT_TRUE(IsVisuallyEquivalentCandidate(Position(one->firstChild(), 1)));

  EXPECT_TRUE(IsVisuallyEquivalentCandidate(Position(body, 0)));
  EXPECT_TRUE(IsVisuallyEquivalentCandidate(Position(body, 1)));
  EXPECT_TRUE(IsVisuallyEquivalentCandidate(Position(body, 2)));

  EXPECT_FALSE(IsVisuallyEquivalentCandidate(Position(two, 0)));
  EXPECT_FALSE(IsVisuallyEquivalentCandidate(Position(two, 1)));

  EXPECT_FALSE(IsVisuallyEquivalentCandidate(Position(empty_body, 0)));
  EXPECT_FALSE(IsVisuallyEquivalentCandidate(Position(empty_body, 1)));
}

TEST_F(VisibleUnitsTest, isVisuallyEquivalentCandidateWithDocument) {
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(IsVisuallyEquivalentCandidate(Position(&GetDocument(), 0)));
}

TEST_F(VisibleUnitsTest, mostBackwardCaretPositionAfterAnchor) {
  const char* body_content =
      "<p id='host'><b id='one'>1</b></p><b id='two'>22</b>";
  const char* shadow_content =
      "<b id='two'>22</b><slot name='#one'></slot><b id='three'>333</b>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* host = GetDocument().getElementById(AtomicString("host"));

  EXPECT_EQ(Position::LastPositionInNode(*host),
            MostForwardCaretPosition(Position::AfterNode(*host)));
  EXPECT_EQ(PositionInFlatTree::LastPositionInNode(*host),
            MostForwardCaretPosition(PositionInFlatTree::AfterNode(*host)));
}

TEST_F(VisibleUnitsTest, mostBackwardCaretPositionFirstLetter) {
  // Note: first-letter pseudo element contains letter and punctuations.
  const char* body_content =
      "<style>p:first-letter {color:red;}</style><p id=sample> (2)45 </p>";
  SetBodyContent(body_content);

  Node* sample =
      GetDocument().getElementById(AtomicString("sample"))->firstChild();

  EXPECT_EQ(Position(sample->parentNode(), 0),
            MostBackwardCaretPosition(Position(sample, 0)));
  EXPECT_EQ(Position(sample->parentNode(), 0),
            MostBackwardCaretPosition(Position(sample, 1)));
  EXPECT_EQ(Position(sample, 2),
            MostBackwardCaretPosition(Position(sample, 2)));
  EXPECT_EQ(Position(sample, 3),
            MostBackwardCaretPosition(Position(sample, 3)));
  EXPECT_EQ(Position(sample, 4),
            MostBackwardCaretPosition(Position(sample, 4)));
  EXPECT_EQ(Position(sample, 5),
            MostBackwardCaretPosition(Position(sample, 5)));
  EXPECT_EQ(Position(sample, 6),
            MostBackwardCaretPosition(Position(sample, 6)));
  EXPECT_EQ(Position(sample, 6),
            MostBackwardCaretPosition(Position(sample, 7)));
  EXPECT_EQ(Position(sample, 6),
            MostBackwardCaretPosition(
                Position::LastPositionInNode(*sample->parentNode())));
  EXPECT_EQ(
      Position(sample, 6),
      MostBackwardCaretPosition(Position::AfterNode(*sample->parentNode())));
  EXPECT_EQ(Position::LastPositionInNode(*GetDocument().body()),
            MostBackwardCaretPosition(
                Position::LastPositionInNode(*GetDocument().body())));
}

TEST_F(VisibleUnitsTest, mostBackwardCaretPositionFirstLetterSplit) {
  V8TestingScope scope;

  const char* body_content =
      "<style>p:first-letter {color:red;}</style><p id=sample>abc</p>";
  SetBodyContent(body_content);

  Node* sample = GetDocument().getElementById(AtomicString("sample"));
  Node* first_letter = sample->firstChild();
  // Split "abc" into "a" "bc"
  auto* remaining = To<Text>(first_letter)->splitText(1, ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(Position(sample, 0),
            MostBackwardCaretPosition(Position(first_letter, 0)));
  EXPECT_EQ(Position(first_letter, 1),
            MostBackwardCaretPosition(Position(first_letter, 1)));
  EXPECT_EQ(Position(first_letter, 1),
            MostBackwardCaretPosition(Position(remaining, 0)));
  EXPECT_EQ(Position(remaining, 1),
            MostBackwardCaretPosition(Position(remaining, 1)));
  EXPECT_EQ(Position(remaining, 2),
            MostBackwardCaretPosition(Position(remaining, 2)));
  EXPECT_EQ(Position(remaining, 2),
            MostBackwardCaretPosition(Position::LastPositionInNode(*sample)));
  EXPECT_EQ(Position(remaining, 2),
            MostBackwardCaretPosition(Position::AfterNode(*sample)));
}

TEST_F(VisibleUnitsTest, mostForwardCaretPositionAfterAnchor) {
  const char* body_content = "<p id='host'><b id='one'>1</b></p>";
  const char* shadow_content =
      "<b id='two'>22</b><slot name='#one'></slot><b id='three'>333</b>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");
  UpdateAllLifecyclePhasesForTest();

  Element* host = GetDocument().getElementById(AtomicString("host"));
  Element* three = shadow_root->getElementById(AtomicString("three"));

  EXPECT_EQ(Position::AfterNode(*host),
            MostBackwardCaretPosition(Position::AfterNode(*host)));
  EXPECT_EQ(PositionInFlatTree(three->firstChild(), 3),
            MostBackwardCaretPosition(PositionInFlatTree::AfterNode(*host)));
}

// http://crbug.com/1348816
TEST_F(VisibleUnitsTest, MostBackwardCaretPositionBeforeSvg) {
  EXPECT_EQ(
      "<div>A<svg><foreignObject height=\"10\" width=\"20\">| "
      "Z</foreignObject></svg></div>",
      TestSnapBackward("<div>A<svg><foreignObject height=10 width=20> "
                       "|Z</foreignObject></svg></div>"));
}

// http://crbug.com/1348816
TEST_F(VisibleUnitsTest, MostForwardCaretPositionBeforeSvg) {
  EXPECT_EQ(
      "<div>A|<svg><foreignObject height=\"10\" width=\"20\"> "
      "Z</foreignObject></svg></div>",
      TestSnapForward("<div>A|<svg><foreignObject height=10 width=20> "
                      "Z</foreignObject></svg></div>"));

  EXPECT_EQ(
      "<div>A<svg><foreignObject height=\"10\" width=\"20\"> "
      "|Z</foreignObject></svg></div>",
      TestSnapForward("<div>A<svg><foreignObject height=10 width=20>| "
                      "Z</foreignObject></svg></div>"));
}

TEST_F(VisibleUnitsTest, mostForwardCaretPositionFirstLetter) {
  // Note: first-letter pseudo element contains letter and punctuations.
  const char* body_content =
      "<style>p:first-letter {color:red;}</style><p id=sample> (2)45 </p>";
  SetBodyContent(body_content);

  Node* sample =
      GetDocument().getElementById(AtomicString("sample"))->firstChild();

  EXPECT_EQ(Position(GetDocument().body(), 0),
            MostForwardCaretPosition(
                Position::FirstPositionInNode(*GetDocument().body())));
  EXPECT_EQ(
      Position(sample, 1),
      MostForwardCaretPosition(Position::BeforeNode(*sample->parentNode())));
  EXPECT_EQ(Position(sample, 1),
            MostForwardCaretPosition(
                Position::FirstPositionInNode(*sample->parentNode())));
  EXPECT_EQ(Position(sample, 1), MostForwardCaretPosition(Position(sample, 0)));
  EXPECT_EQ(Position(sample, 1), MostForwardCaretPosition(Position(sample, 1)));
  EXPECT_EQ(Position(sample, 2), MostForwardCaretPosition(Position(sample, 2)));
  EXPECT_EQ(Position(sample, 3), MostForwardCaretPosition(Position(sample, 3)));
  EXPECT_EQ(Position(sample, 4), MostForwardCaretPosition(Position(sample, 4)));
  EXPECT_EQ(Position(sample, 5), MostForwardCaretPosition(Position(sample, 5)));
  EXPECT_EQ(Position(sample, 7), MostForwardCaretPosition(Position(sample, 6)));
  EXPECT_EQ(Position(sample, 7), MostForwardCaretPosition(Position(sample, 7)));
}

TEST_F(VisibleUnitsTest, nextPositionOf) {
  const char* body_content =
      "<b id=zero>0</b><p id=host><b slot='#one' id=one>1</b><b slot='#two' "
      "id=two>22</b></p><b "
      "id=three>333</b>";
  const char* shadow_content =
      "<b id=four>4444</b><slot name='#two'></slot><slot name=#one></slot><b "
      "id=five>55555</b>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Element* zero = GetDocument().getElementById(AtomicString("zero"));
  Element* one = GetDocument().getElementById(AtomicString("one"));
  Element* two = GetDocument().getElementById(AtomicString("two"));
  Element* three = GetDocument().getElementById(AtomicString("three"));
  Element* four = shadow_root->getElementById(AtomicString("four"));
  Element* five = shadow_root->getElementById(AtomicString("five"));

  EXPECT_EQ(Position(two->firstChild(), 2),
            NextPositionOf(CreateVisiblePosition(Position(zero, 1)))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(four->firstChild(), 0),
            NextPositionOf(CreateVisiblePosition(PositionInFlatTree(zero, 1)))
                .DeepEquivalent());

  EXPECT_EQ(Position(three->firstChild(), 0),
            NextPositionOf(CreateVisiblePosition(Position(one, 0),
                                                 TextAffinity::kUpstream))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(one->firstChild(), 1),
            NextPositionOf(CreateVisiblePosition(PositionInFlatTree(one, 0)))
                .DeepEquivalent());

  EXPECT_EQ(Position(two->firstChild(), 0),
            NextPositionOf(CreateVisiblePosition(Position(one, 1),
                                                 TextAffinity::kUpstream))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(five->firstChild(), 1),
            NextPositionOf(CreateVisiblePosition(PositionInFlatTree(one, 1)))
                .DeepEquivalent());

  EXPECT_EQ(
      Position(three->firstChild(), 0),
      NextPositionOf(CreateVisiblePosition(Position(two, 1))).DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(one->firstChild(), 1),
            NextPositionOf(CreateVisiblePosition(PositionInFlatTree(two, 1)))
                .DeepEquivalent());
}

TEST_F(VisibleUnitsTest, nextPositionOfTable) {
  SetBodyContent("<table id='table'></table>");
  Element* table = GetDocument().getElementById(AtomicString("table"));
  // Couldn't include the <br> in the HTML above since the parser would have
  // messed up the structure in the DOM.
  table->setInnerHTML("<br>", ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhasesForTest();

  Position position(table, 0);
  Position next =
      NextPositionOf(CreateVisiblePosition(position)).DeepEquivalent();
  EXPECT_NE(position, next);
  EXPECT_NE(MostBackwardCaretPosition(position),
            MostBackwardCaretPosition(next));
  EXPECT_NE(MostForwardCaretPosition(position), MostForwardCaretPosition(next));
}

TEST_F(VisibleUnitsTest, previousPositionOf) {
  const char* body_content =
      "<b id=zero>0</b><p id=host><b slot='#one' id=one>1</b><b slot='#two' "
      "id=two>22</b></p><b id=three>333</b>";
  const char* shadow_content =
      "<b id=four>4444</b><slot name='#two'></slot><slot name=#one></slot><b "
      "id=five>55555</b>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* zero = GetDocument().getElementById(AtomicString("zero"))->firstChild();
  Node* one = GetDocument().getElementById(AtomicString("one"))->firstChild();
  Node* two = GetDocument().getElementById(AtomicString("two"))->firstChild();
  Node* three =
      GetDocument().getElementById(AtomicString("three"))->firstChild();
  Node* four = shadow_root->getElementById(AtomicString("four"))->firstChild();
  Node* five = shadow_root->getElementById(AtomicString("five"))->firstChild();

  EXPECT_EQ(Position(zero, 0),
            PreviousPositionOf(CreateVisiblePosition(Position(zero, 1)))
                .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(zero, 0),
      PreviousPositionOf(CreateVisiblePosition(PositionInFlatTree(zero, 1)))
          .DeepEquivalent());

  EXPECT_EQ(Position(two, 1),
            PreviousPositionOf(CreateVisiblePosition(Position(one, 0)))
                .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(two, 1),
      PreviousPositionOf(CreateVisiblePosition(PositionInFlatTree(one, 0)))
          .DeepEquivalent());

  EXPECT_EQ(Position(two, 2),
            PreviousPositionOf(CreateVisiblePosition(Position(one, 1)))
                .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(two, 2),
      PreviousPositionOf(CreateVisiblePosition(PositionInFlatTree(one, 1)))
          .DeepEquivalent());

  EXPECT_EQ(Position(one, 1),
            PreviousPositionOf(CreateVisiblePosition(Position(two, 0)))
                .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(four, 3),
      PreviousPositionOf(CreateVisiblePosition(PositionInFlatTree(two, 0)))
          .DeepEquivalent());

  // DOM tree to shadow tree
  EXPECT_EQ(Position(two, 2),
            PreviousPositionOf(CreateVisiblePosition(Position(three, 0)))
                .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(five, 5),
      PreviousPositionOf(CreateVisiblePosition(PositionInFlatTree(three, 0)))
          .DeepEquivalent());

  // Shadow tree to DOM tree
  EXPECT_EQ(Position(),
            PreviousPositionOf(CreateVisiblePosition(Position(four, 0)))
                .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(zero, 1),
      PreviousPositionOf(CreateVisiblePosition(PositionInFlatTree(four, 0)))
          .DeepEquivalent());

  // Note: Canonicalization maps (five, 0) to (five, 0) in DOM tree and
  // (one, 1) in flat tree.
  EXPECT_EQ(Position(five, 0),
            PreviousPositionOf(CreateVisiblePosition(Position(five, 1)))
                .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(one, 1),
      PreviousPositionOf(CreateVisiblePosition(PositionInFlatTree(five, 1)))
          .DeepEquivalent());
}

TEST_F(VisibleUnitsTest, previousPositionOfOneCharPerLine) {
  const char* body_content =
      "<div id=sample style='font-size: 500px'>A&#x714a;&#xfa67;</div>";
  SetBodyContent(body_content);

  Node* sample =
      GetDocument().getElementById(AtomicString("sample"))->firstChild();

  // In case of each line has one character, VisiblePosition are:
  // [C,Dn]   [C,Up]  [B, Dn]   [B, Up]
  //  A        A       A         A|
  //  B        B|     |B         B
  // |C        C       C         C
  EXPECT_EQ(PositionWithAffinity(Position(sample, 1)),
            PreviousPositionOf(CreateVisiblePosition(Position(sample, 2)))
                .ToPositionWithAffinity());
  EXPECT_EQ(PositionWithAffinity(Position(sample, 1)),
            PreviousPositionOf(CreateVisiblePosition(Position(sample, 2),
                                                     TextAffinity::kUpstream))
                .ToPositionWithAffinity());
}

TEST_F(VisibleUnitsTest, previousPositionOfNoPreviousPosition) {
  SetBodyContent(
      "<span contenteditable='true'>"
      "<span> </span>"
      " "  // This whitespace causes no previous position.
      "<div id='anchor'> bar</div>"
      "</span>");
  const Position position(
      GetDocument().getElementById(AtomicString("anchor"))->firstChild(), 1);
  EXPECT_EQ(
      Position(),
      PreviousPositionOf(CreateVisiblePosition(position)).DeepEquivalent());
}

TEST_F(VisibleUnitsTest, rendersInDifferentPositionAfterAnchor) {
  const char* body_content = "<p id='sample'>00</p>";
  SetBodyContent(body_content);
  Element* sample = GetDocument().getElementById(AtomicString("sample"));

  EXPECT_FALSE(RendersInDifferentPosition(Position(), Position()));
  EXPECT_FALSE(
      RendersInDifferentPosition(Position(), Position::AfterNode(*sample)))
      << "if one of position is null, the reuslt is false.";
  EXPECT_FALSE(RendersInDifferentPosition(Position::AfterNode(*sample),
                                          Position(sample, 1)));
  EXPECT_FALSE(RendersInDifferentPosition(Position::LastPositionInNode(*sample),
                                          Position(sample, 1)));
}

TEST_F(VisibleUnitsTest, rendersInDifferentPositionAfterAnchorWithHidden) {
  const char* body_content =
      "<p><span id=one>11</span><span id=two style='display:none'>  "
      "</span></p>";
  SetBodyContent(body_content);
  Element* one = GetDocument().getElementById(AtomicString("one"));
  Element* two = GetDocument().getElementById(AtomicString("two"));

  EXPECT_TRUE(RendersInDifferentPosition(Position::LastPositionInNode(*one),
                                         Position(two, 0)))
      << "two doesn't have layout object";
}

TEST_F(VisibleUnitsTest,
       rendersInDifferentPositionAfterAnchorWithDifferentLayoutObjects) {
  const char* body_content =
      "<p><span id=one>11</span><span id=two>  </span></p>";
  SetBodyContent(body_content);
  Element* one = GetDocument().getElementById(AtomicString("one"));
  Element* two = GetDocument().getElementById(AtomicString("two"));

  EXPECT_FALSE(RendersInDifferentPosition(Position::LastPositionInNode(*one),
                                          Position(two, 0)));
  EXPECT_FALSE(RendersInDifferentPosition(Position::LastPositionInNode(*one),
                                          Position(two, 1)))
      << "width of two is zero since contents is collapsed whitespaces";
}

TEST_F(VisibleUnitsTest, renderedOffset) {
  const char* body_content =
      "<div contenteditable><span id='sample1'>1</span><span "
      "id='sample2'>22</span></div>";
  SetBodyContent(body_content);
  Element* sample1 = GetDocument().getElementById(AtomicString("sample1"));
  Element* sample2 = GetDocument().getElementById(AtomicString("sample2"));

  EXPECT_FALSE(
      RendersInDifferentPosition(Position::AfterNode(*sample1->firstChild()),
                                 Position(sample2->firstChild(), 0)));
  EXPECT_FALSE(RendersInDifferentPosition(
      Position::LastPositionInNode(*sample1->firstChild()),
      Position(sample2->firstChild(), 0)));
}

TEST_F(VisibleUnitsTest, startOfDocument) {
  const char* body_content =
      "<span id=host><b slot='#one' id=one>1</b><b slot='#two' "
      "id=two>22</b></span>";
  const char* shadow_content =
      "<p><slot name='#two'></slot></p><p><slot name=#one></slot></p>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById(AtomicString("one"))->firstChild();
  Node* two = GetDocument().getElementById(AtomicString("two"))->firstChild();

  EXPECT_EQ(Position(one, 0),
            CreateVisiblePosition(StartOfDocument(Position(*one, 0)))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(two, 0),
            CreateVisiblePosition(StartOfDocument(PositionInFlatTree(*one, 0)))
                .DeepEquivalent());

  EXPECT_EQ(Position(one, 0),
            CreateVisiblePosition(StartOfDocument(Position(*two, 1)))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(two, 0),
            CreateVisiblePosition(StartOfDocument(PositionInFlatTree(*two, 1)))
                .DeepEquivalent());
}

TEST_F(VisibleUnitsTest,
       endsOfNodeAreVisuallyDistinctPositionsWithInvisibleChild) {
  // Repro case of crbug.com/582247
  const char* body_content =
      "<button> </button><script>document.designMode = 'on'</script>";
  SetBodyContent(body_content);

  Node* button = GetDocument().QuerySelector(AtomicString("button"));
  EXPECT_TRUE(EndsOfNodeAreVisuallyDistinctPositions(button));
}

TEST_F(VisibleUnitsTest,
       endsOfNodeAreVisuallyDistinctPositionsWithEmptyLayoutChild) {
  // Repro case of crbug.com/584030
  const char* body_content =
      "<button><rt><script>document.designMode = 'on'</script></rt></button>";
  SetBodyContent(body_content);

  Node* button = GetDocument().QuerySelector(AtomicString("button"));
  EXPECT_TRUE(EndsOfNodeAreVisuallyDistinctPositions(button));
}

// Regression test for crbug.com/675429
TEST_F(VisibleUnitsTest,
       canonicalizationWithCollapsedSpaceAndIsolatedCombiningCharacter) {
  SetBodyContent("<p>  &#x20E3;</p>");  // Leading space is necessary

  Node* paragraph = GetDocument().QuerySelector(AtomicString("p"));
  Node* text = paragraph->firstChild();
  Position start = CanonicalPositionOf(Position::BeforeNode(*paragraph));
  EXPECT_EQ(Position(text, 2), start);
}

TEST_F(VisibleUnitsTest, MostForwardCaretPositionWithInvisibleFirstLetter) {
  InsertStyleElement("div::first-letter{visibility:hidden}");
  // Use special syntax to set input position DIV@0
  const Position position = SetCaretTextToBody("<div><!--|-->foo</div>");
  const Node* foo =
      GetDocument().QuerySelector(AtomicString("div"))->firstChild();
  EXPECT_EQ(Position(foo, 1), MostForwardCaretPosition(position));
}

// Regression test for crbug.com/1172091
TEST_F(VisibleUnitsTest, MostBackwardOrForwardCaretPositionWithBrInOptgroup) {
  SetBodyContent("<optgroup><br></optgroup>");
  Node* br = GetDocument().QuerySelector(AtomicString("br"));
  const Position& before = Position::BeforeNode(*br);
  EXPECT_EQ(before, MostBackwardCaretPosition(before));
  EXPECT_EQ(before, MostForwardCaretPosition(before));
  const Position& after = Position::AfterNode(*br);
  EXPECT_EQ(after, MostBackwardCaretPosition(after));
  EXPECT_EQ(after, MostForwardCaretPosition(after));
}

// http://crbug.com/1134470
TEST_F(VisibleUnitsTest, SnapBackwardWithZeroWidthSpace) {
  // Note: We should skip <wbr> otherwise caret stops before/after <wbr>.

  EXPECT_EQ("<p>ab|<wbr></p>", TestSnapBackward("<p>ab<wbr>|</p>"));
  EXPECT_EQ("<p>ab\u200B|</p>", TestSnapBackward("<p>ab\u200B|</p>"));
  EXPECT_EQ("<p>ab<!-- -->\u200B|</p>",
            TestSnapBackward("<p>ab<!-- -->\u200B|</p>"));

  EXPECT_EQ("<p>ab|<wbr><wbr></p>", TestSnapBackward("<p>ab<wbr><wbr>|</p>"));
  EXPECT_EQ("<p>ab\u200B\u200B|</p>",
            TestSnapBackward("<p>ab\u200B\u200B|</p>"));

  EXPECT_EQ("<p>ab|<wbr>cd</p>", TestSnapBackward("<p>ab<wbr>|cd</p>"));
  EXPECT_EQ("<p>ab\u200B|cd</p>", TestSnapBackward("<p>ab\u200B|cd</p>"));

  EXPECT_EQ("<p>ab|<wbr><wbr>cd</p>",
            TestSnapBackward("<p>ab<wbr><wbr>|cd</p>"));
  EXPECT_EQ("<p>ab\u200B\u200B|cd</p>",
            TestSnapBackward("<p>ab\u200B\u200B|cd</p>"));
}
TEST_F(VisibleUnitsTest, SnapForwardWithImg) {
  SetBodyContent("<img>");
  const auto& body = *GetDocument().body();
  const auto& img = *GetDocument().QuerySelector(AtomicString("img"));

  EXPECT_EQ(Position::BeforeNode(img),
            MostForwardCaretPosition(Position::FirstPositionInNode(body)));
  EXPECT_EQ(Position::BeforeNode(img),
            MostForwardCaretPosition(Position(body, 0)));
  EXPECT_EQ(Position::BeforeNode(img),
            MostForwardCaretPosition(Position::BeforeNode(img)));
  EXPECT_EQ(Position::BeforeNode(img),
            MostForwardCaretPosition(Position(img, 0)));
  EXPECT_EQ(Position::AfterNode(img),
            MostForwardCaretPosition(Position::LastPositionInNode(img)));
  EXPECT_EQ(Position::AfterNode(img),
            MostForwardCaretPosition(Position::AfterNode(img)));
}

TEST_F(VisibleUnitsTest, SnapForwardWithInput) {
  SetBodyContent("<input>");
  const auto& body = *GetDocument().body();
  const auto& input = *GetDocument().QuerySelector(AtomicString("input"));

  EXPECT_EQ(Position::BeforeNode(input),
            MostForwardCaretPosition(Position::FirstPositionInNode(body)));
  EXPECT_EQ(Position::BeforeNode(input),
            MostForwardCaretPosition(Position(body, 0)));
  EXPECT_EQ(Position::BeforeNode(input),
            MostForwardCaretPosition(Position::BeforeNode(input)));
  EXPECT_EQ(Position::BeforeNode(input),
            MostForwardCaretPosition(Position::FirstPositionInNode(input)));
  EXPECT_EQ(Position::BeforeNode(input),
            MostForwardCaretPosition(Position(input, 0)));
  EXPECT_EQ(Position::AfterNode(input),
            MostForwardCaretPosition(Position::LastPositionInNode(input)));
  EXPECT_EQ(Position::AfterNode(input),
            MostForwardCaretPosition(Position::AfterNode(input)));
}

TEST_F(VisibleUnitsTest, SnapForwardWithSelect) {
  SetBodyContent(
      "<select><option>1</option><option>2</option><option>3</option></"
      "select>");
  const auto& body = *GetDocument().body();
  const auto& select = *GetDocument().QuerySelector(AtomicString("select"));

  EXPECT_EQ(Position::BeforeNode(select),
            MostForwardCaretPosition(Position(body, 0)));
  EXPECT_EQ(Position::BeforeNode(select),
            MostForwardCaretPosition(Position::FirstPositionInNode(body)));
  EXPECT_EQ(Position::BeforeNode(select),
            MostForwardCaretPosition(Position::BeforeNode(select)));
  EXPECT_EQ(Position::BeforeNode(select),
            MostForwardCaretPosition(Position::FirstPositionInNode(select)));
  EXPECT_EQ(Position::BeforeNode(select),
            MostForwardCaretPosition(Position(select, 0)));

  // The internal version of `MostForwardCaretPosition()` is called with
  // `PositionInFlatTree(slot, 1)` and it scans at end of `<select>` then
  // returns `PositionInFlatTree(slot, 1)` and converts to
  // `Position(select, 1)`.
  EXPECT_EQ(Position(select, 1), MostForwardCaretPosition(Position(select, 1)));
  EXPECT_EQ(Position(select, 2), MostForwardCaretPosition(Position(select, 2)));
  EXPECT_EQ(Position::AfterNode(select),
            MostForwardCaretPosition(Position(select, 3)));
  EXPECT_EQ(Position::AfterNode(select),
            MostForwardCaretPosition(Position::LastPositionInNode(select)));
  EXPECT_EQ(Position::AfterNode(select),
            MostForwardCaretPosition(Position::AfterNode(select)));

  // Flat tree is
  //  <select>
  //    <div>""</div>
  //    <slot><option>1</option><option>2</option></slot>
  //  </select>
  EXPECT_EQ(PositionInFlatTree::BeforeNode(select),
            MostForwardCaretPosition(PositionInFlatTree(body, 0)));
  EXPECT_EQ(
      PositionInFlatTree::BeforeNode(select),
      MostForwardCaretPosition(PositionInFlatTree::FirstPositionInNode(body)));
  EXPECT_EQ(PositionInFlatTree::BeforeNode(select),
            MostForwardCaretPosition(PositionInFlatTree::BeforeNode(select)));

  // Note: `PositionIterator::DeprecatedComputePosition()` returns
  // `BeforeNode(<select>)` for <select>@n where n is 0 to 3, because
  // `EditingIgnoresContent(<select>)` is true.
  EXPECT_EQ(PositionInFlatTree::BeforeNode(select),
            MostForwardCaretPosition(
                PositionInFlatTree::FirstPositionInNode(select)));
  EXPECT_EQ(PositionInFlatTree::BeforeNode(select),
            MostForwardCaretPosition(PositionInFlatTree(select, 0)));
  EXPECT_EQ(PositionInFlatTree::BeforeNode(select),
            MostForwardCaretPosition(PositionInFlatTree(select, 1)));
  EXPECT_EQ(PositionInFlatTree::BeforeNode(select),
            MostForwardCaretPosition(PositionInFlatTree(select, 2)));
  EXPECT_EQ(PositionInFlatTree::BeforeNode(select),
            MostForwardCaretPosition(PositionInFlatTree(select, 3)));
  EXPECT_EQ(PositionInFlatTree::BeforeNode(select),
            MostForwardCaretPosition(PositionInFlatTree(select, 4)));
  EXPECT_EQ(PositionInFlatTree::AfterNode(select),
            MostForwardCaretPosition(PositionInFlatTree(select, 5)));

  EXPECT_EQ(
      PositionInFlatTree::AfterNode(select),
      MostForwardCaretPosition(PositionInFlatTree::LastPositionInNode(select)));
  EXPECT_EQ(PositionInFlatTree::AfterNode(select),
            MostForwardCaretPosition(PositionInFlatTree::AfterNode(select)));
}

// From ReplaceSelectionCommandTest.TableAndImages)
TEST_F(VisibleUnitsTest, SnapForwardWithTableAndImages) {
  SetBodyContent("<table> <tbody></tbody> </table>");
  const auto& table = *GetDocument().QuerySelector(AtomicString("table"));
  const auto& body = *GetDocument().body();
  auto& tbody = *GetDocument().QuerySelector(AtomicString("tbody"));
  auto& img1 = *GetDocument().CreateRawElement(html_names::kImgTag);
  tbody.AppendChild(&img1);
  auto& img2 = *GetDocument().CreateRawElement(html_names::kImgTag);
  tbody.AppendChild(&img2);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(Position(body, 0), MostForwardCaretPosition(Position(body, 0)));
  EXPECT_EQ(Position(body, 0),
            MostForwardCaretPosition(Position::FirstPositionInNode(body)));
  EXPECT_EQ(Position(table, 0),
            MostForwardCaretPosition(Position::BeforeNode(table)));
  EXPECT_EQ(Position(table, 0),
            MostForwardCaretPosition(Position::FirstPositionInNode(table)));
  EXPECT_EQ(Position(table, 0), MostForwardCaretPosition(Position(table, 0)));
  EXPECT_EQ(Position(table, 1), MostForwardCaretPosition(Position(table, 1)));
  EXPECT_EQ(Position::BeforeNode(img1),
            MostForwardCaretPosition(Position::BeforeNode(tbody)));
  EXPECT_EQ(Position::BeforeNode(img1),
            MostForwardCaretPosition(Position(tbody, 0)));
  EXPECT_EQ(Position::BeforeNode(img1),
            MostForwardCaretPosition(Position::FirstPositionInNode(tbody)));
  EXPECT_EQ(Position::BeforeNode(img2),
            MostForwardCaretPosition(Position(tbody, 1)));
  EXPECT_EQ(Position::LastPositionInNode(tbody),
            MostForwardCaretPosition(Position(tbody, 2)));
  EXPECT_EQ(Position::LastPositionInNode(tbody),
            MostForwardCaretPosition(Position::LastPositionInNode(tbody)));
  EXPECT_EQ(Position::LastPositionInNode(tbody),
            MostForwardCaretPosition(Position::AfterNode(tbody)));
  EXPECT_EQ(Position(table, 2), MostForwardCaretPosition(Position(table, 2)));
  EXPECT_EQ(Position::LastPositionInNode(table),
            MostForwardCaretPosition(Position(table, 3)));
  EXPECT_EQ(Position::LastPositionInNode(table),
            MostForwardCaretPosition(Position::LastPositionInNode(table)));
  EXPECT_EQ(Position::LastPositionInNode(table),
            MostForwardCaretPosition(Position::AfterNode(table)));
}

// http://crbug.com/1134470
TEST_F(VisibleUnitsTest, SnapForwardWithZeroWidthSpace) {
  // Note: We should skip <wbr> otherwise caret stops before/after <wbr>.

  EXPECT_EQ("<p>ab<wbr></p>", TestSnapForward("<p>ab|<wbr></p>"))
      << "We get <wbr>@0";
  EXPECT_EQ("<p>ab|\u200B</p>", TestSnapForward("<p>ab|\u200B</p>"));
  EXPECT_EQ("<p>ab<!-- -->|\u200B</p>",
            TestSnapForward("<p>ab<!-- -->|\u200B</p>"));

  EXPECT_EQ("<p>ab<wbr><wbr></p>", TestSnapForward("<p>ab|<wbr><wbr></p>"))
      << "We get <wbr>@0";
  EXPECT_EQ("<p>ab|\u200B\u200B</p>",
            TestSnapForward("<p>ab|\u200B\u200B</p>"));

  EXPECT_EQ("<p>ab<wbr>|cd</p>", TestSnapForward("<p>ab|<wbr>cd</p>"));
  EXPECT_EQ("<p>ab|\u200Bcd</p>", TestSnapForward("<p>ab|\u200Bcd</p>"));

  EXPECT_EQ("<p>ab<wbr><wbr>|cd</p>",
            TestSnapForward("<p>ab|<wbr><wbr>cd</p>"));
  EXPECT_EQ("<p>ab|\u200B\u200Bcd</p>",
            TestSnapForward("<p>ab|\u200B\u200Bcd</p>"));
}

TEST_F(VisibleUnitsTest, FirstRectForRangeHorizontal) {
  LoadAhem();
  InsertStyleElement("div { font:20px/20px Ahem;}");
  const SelectionInDOMTree selection =
      SetSelectionTextToBody("<div>^abcdef|</div>");
  const gfx::Rect rect = FirstRectForRange(selection.ComputeRange());
  EXPECT_EQ(gfx::Rect(8, 8, 120, 20), rect);
}

TEST_F(VisibleUnitsTest, FirstRectForRangeHorizontalWrap) {
  LoadAhem();
  InsertStyleElement("div { font:20px/20px Ahem; inline-size:60px;}");
  const SelectionInDOMTree selection =
      SetSelectionTextToBody("<div>^abc def|</div>");
  const gfx::Rect rect = FirstRectForRange(selection.ComputeRange());
  EXPECT_EQ(gfx::Rect(8, 8, 59, 20), rect);
}

TEST_F(VisibleUnitsTest, FirstRectForRangeVertical) {
  LoadAhem();
  InsertStyleElement("div { writing-mode:vertical-rl; font:20px/20px Ahem;}");
  const SelectionInDOMTree selection =
      SetSelectionTextToBody("<div>^abcdef|</div>");
  const gfx::Rect rect = FirstRectForRange(selection.ComputeRange());
  EXPECT_EQ(gfx::Rect(8, 8, 20, 119), rect);
}

TEST_F(VisibleUnitsTest, FirstRectForRangeVerticalWrap) {
  LoadAhem();
  InsertStyleElement(
      "div { writing-mode:vertical-rl; font:20px/20px Ahem; "
      "inline-size:60px;}");
  const SelectionInDOMTree selection =
      SetSelectionTextToBody("<div>^abc def|</div>");
  const gfx::Rect rect = FirstRectForRange(selection.ComputeRange());
  EXPECT_EQ(gfx::Rect(28, 8, 20, 59), rect);
}

}  // namespace visible_units_test
}  // namespace blink
