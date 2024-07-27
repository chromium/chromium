// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/visible_units.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"

namespace blink {

class VisibleUnitsParagraphTest : public EditingTestBase {
 protected:
  static PositionWithAffinity PositionWithAffinityInDOMTree(
      Node& anchor,
      int offset,
      TextAffinity affinity = TextAffinity::kDownstream) {
    return PositionWithAffinity(CanonicalPositionOf(Position(&anchor, offset)),
                                affinity);
  }

  static VisiblePosition CreateVisiblePositionInDOMTree(
      Node& anchor,
      int offset,
      TextAffinity affinity = TextAffinity::kDownstream) {
    return CreateVisiblePosition(Position(&anchor, offset), affinity);
  }

  static PositionInFlatTreeWithAffinity PositionWithAffinityInFlatTree(
      Node& anchor,
      int offset,
      TextAffinity affinity = TextAffinity::kDownstream) {
    return PositionInFlatTreeWithAffinity(
        CanonicalPositionOf(PositionInFlatTree(&anchor, offset)), affinity);
  }

  static VisiblePositionInFlatTree CreateVisiblePositionInFlatTree(
      Node& anchor,
      int offset,
      TextAffinity affinity = TextAffinity::kDownstream) {
    return CreateVisiblePosition(PositionInFlatTree(&anchor, offset), affinity);
  }
};

TEST_F(VisibleUnitsParagraphTest, endOfParagraphFirstLetter) {
  SetBodyContent(
      "<style>div::first-letter { color: red }</style><div "
      "id=sample>1ab\nde</div>");

  Node* sample = GetDocument().getElementById(AtomicString("sample"));
  Node* text = sample->firstChild();

  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 0))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 1))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 2))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 3))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 4))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 5))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 6))
                .DeepEquivalent());
}

TEST_F(VisibleUnitsParagraphTest, endOfParagraphFirstLetterPre) {
  SetBodyContent(
      "<style>pre::first-letter { color: red }</style><pre "
      "id=sample>1ab\nde</pre>");

  Node* sample = GetDocument().getElementById(AtomicString("sample"));
  Node* text = sample->firstChild();

  EXPECT_EQ(Position(text, 3),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 0))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 3),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 1))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 3),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 2))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 3))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 4))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 5))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 6))
                .DeepEquivalent());
}

TEST_F(VisibleUnitsParagraphTest, endOfParagraphShadow) {
  const char* body_content =
      "<span id=host><b slot='#one' id=one>1</b><b slot='#two' "
      "id=two>22</b></span><b id=three>333</b>";
  const char* shadow_content =
      "<p><slot name=#two></slot></p><p><slot name=#one></slot></p>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* one = GetDocument().getElementById(AtomicString("one"));
  Element* two = GetDocument().getElementById(AtomicString("two"));
  Element* three = GetDocument().getElementById(AtomicString("three"));

  EXPECT_EQ(
      Position(three->firstChild(), 3),
      EndOfParagraph(CreateVisiblePositionInDOMTree(*one->firstChild(), 1))
          .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(one->firstChild(), 1),
      EndOfParagraph(CreateVisiblePositionInFlatTree(*one->firstChild(), 1))
          .DeepEquivalent());

  EXPECT_EQ(
      Position(three->firstChild(), 3),
      EndOfParagraph(CreateVisiblePositionInDOMTree(*two->firstChild(), 2))
          .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(two->firstChild(), 2),
      EndOfParagraph(CreateVisiblePositionInFlatTree(*two->firstChild(), 2))
          .DeepEquivalent());
}

TEST_F(VisibleUnitsParagraphTest, endOfParagraphSimple) {
  SetBodyContent("<div id=sample>1ab\nde</div>");

  Node* sample = GetDocument().getElementById(AtomicString("sample"));
  Node* text = sample->firstChild();

  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 0))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 1))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 2))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 3))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 4))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 5))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 6))
                .DeepEquivalent());
}

TEST_F(VisibleUnitsParagraphTest, endOfParagraphSimplePre) {
  SetBodyContent("<pre id=sample>1ab\nde</pre>");

  Node* sample = GetDocument().getElementById(AtomicString("sample"));
  Node* text = sample->firstChild();

  EXPECT_EQ(Position(text, 3),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 0))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 3),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 1))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 3),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 2))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 3),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 3))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 4))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 5))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 6))
                .DeepEquivalent());
}

TEST_F(VisibleUnitsParagraphTest, isEndOfParagraph) {
  const char* body_content =
      "<span id=host><b slot='#one' id=one>1</b><b slot='#two' "
      "id=two>22</b></span><b id=three>333</b>";
  const char* shadow_content =
      "<p><slot name=#two></slot></p><p><slot name=#one></slot></p>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById(AtomicString("one"))->firstChild();
  Node* two = GetDocument().getElementById(AtomicString("two"))->firstChild();
  Node* three =
      GetDocument().getElementById(AtomicString("three"))->firstChild();

  EXPECT_FALSE(IsEndOfParagraph(CreateVisiblePositionInDOMTree(*one, 0)));
  EXPECT_FALSE(IsEndOfParagraph(CreateVisiblePositionInFlatTree(*one, 0)));

  EXPECT_FALSE(IsEndOfParagraph(CreateVisiblePositionInDOMTree(*one, 1)));
  EXPECT_TRUE(IsEndOfParagraph(CreateVisiblePositionInFlatTree(*one, 1)));

  EXPECT_FALSE(IsEndOfParagraph(CreateVisiblePositionInDOMTree(*two, 2)));
  EXPECT_TRUE(IsEndOfParagraph(CreateVisiblePositionInFlatTree(*two, 2)));

  EXPECT_FALSE(IsEndOfParagraph(CreateVisiblePositionInDOMTree(*three, 0)));
  EXPECT_FALSE(IsEndOfParagraph(CreateVisiblePositionInFlatTree(*three, 0)));

  EXPECT_TRUE(IsEndOfParagraph(CreateVisiblePositionInDOMTree(*three, 3)));
  EXPECT_TRUE(IsEndOfParagraph(CreateVisiblePositionInFlatTree(*three, 3)));
}

TEST_F(VisibleUnitsParagraphTest, isStartOfParagraph) {
  const char* body_content =
      "<b id=zero>0</b><span id=host><b slot='#one' id=one>1</b><b slot='#two' "
      "id=two>22</b></span><b id=three>333</b>";
  const char* shadow_content =
      "<p><slot name=#two></slot></p><p><slot name=#one></slot></p>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Node* zero = GetDocument().getElementById(AtomicString("zero"))->firstChild();
  Node* one = GetDocument().getElementById(AtomicString("one"))->firstChild();
  Node* two = GetDocument().getElementById(AtomicString("two"))->firstChild();
  Node* three =
      GetDocument().getElementById(AtomicString("three"))->firstChild();

  EXPECT_TRUE(IsStartOfParagraph(CreateVisiblePositionInDOMTree(*zero, 0)));
  EXPECT_TRUE(IsStartOfParagraph(CreateVisiblePositionInFlatTree(*zero, 0)));

  EXPECT_FALSE(IsStartOfParagraph(CreateVisiblePositionInDOMTree(*one, 0)));
  EXPECT_TRUE(IsStartOfParagraph(CreateVisiblePositionInFlatTree(*one, 0)));

  EXPECT_FALSE(IsStartOfParagraph(CreateVisiblePositionInDOMTree(*one, 1)));
  EXPECT_FALSE(IsStartOfParagraph(CreateVisiblePositionInFlatTree(*one, 1)));

  EXPECT_FALSE(IsStartOfParagraph(CreateVisiblePositionInDOMTree(*two, 0)));
  EXPECT_TRUE(IsStartOfParagraph(CreateVisiblePositionInFlatTree(*two, 0)));

  EXPECT_FALSE(IsStartOfParagraph(CreateVisiblePositionInDOMTree(*three, 0)));
  EXPECT_TRUE(IsStartOfParagraph(CreateVisiblePositionInFlatTree(*three, 0)));
}

TEST_F(VisibleUnitsParagraphTest, StartOfNextParagraphAfterTableCell) {
  SetBodyContent(
      "<input style='display: table-cell' type='file' "
      "maxlength='100'><select>");

  const Position& input =
      Position::BeforeNode(*GetDocument().QuerySelector(AtomicString("input")));
  const Position& select = Position::BeforeNode(
      *GetDocument().QuerySelector(AtomicString("select")));

  const VisiblePosition& input_position = CreateVisiblePosition(input);
  const VisiblePosition& after_input =
      VisiblePosition::AfterNode(*input.AnchorNode());
  const VisiblePosition& select_position = CreateVisiblePosition(select);

  const VisiblePosition& next_paragraph = StartOfNextParagraph(input_position);
  EXPECT_LT(input_position.DeepEquivalent(), next_paragraph.DeepEquivalent());
  EXPECT_LE(after_input.DeepEquivalent(), next_paragraph.DeepEquivalent());
  EXPECT_EQ(select_position.DeepEquivalent(), next_paragraph.DeepEquivalent());
}

TEST_F(VisibleUnitsParagraphTest,
       endOfParagraphWithDifferentUpAndDownVisiblePositions) {
  InsertStyleElement("span, div { display: inline-block; width: 50vw; }");
  SetBodyContent("x<span></span><div></div>");

  const Position& text_end =
      Position::LastPositionInNode(*GetDocument().body()->firstChild());
  const Position& before_div =
      Position::BeforeNode(*GetDocument().QuerySelector(AtomicString("div")));
  const VisiblePosition& upstream =
      CreateVisiblePosition(before_div, TextAffinity::kUpstream);
  const VisiblePosition& downstream =
      CreateVisiblePosition(before_div, TextAffinity::kDownstream);
  EXPECT_LT(upstream.DeepEquivalent(), downstream.DeepEquivalent());
  EXPECT_EQ(text_end, upstream.DeepEquivalent());
  EXPECT_EQ(before_div, downstream.DeepEquivalent());

  // The end of paragraph of a position shouldn't precede it (bug 1179113).
  const VisiblePosition& end_of_paragraph = EndOfParagraph(downstream);
  EXPECT_LE(downstream.DeepEquivalent(), end_of_paragraph.DeepEquivalent());

  // In in this case they are equal.
  EXPECT_EQ(downstream.DeepEquivalent(), end_of_paragraph.DeepEquivalent());
}

TEST_F(VisibleUnitsParagraphTest, endOfParagraphCannotBeBeforePosition) {
  SetBodyContent(
      "<span contenteditable>x<br contenteditable=false>"
      "<br contenteditable=false></span>");
  Element* span = GetDocument().QuerySelector(AtomicString("span"));
  const Position& p1 = Position(span, 2);
  const Position& p2 = Position::LastPositionInNode(*span);
  const Position& p3 = Position::AfterNode(*span);
  const VisiblePosition& vp1 = CreateVisiblePosition(p1);
  const VisiblePosition& vp2 = CreateVisiblePosition(p2);
  const VisiblePosition& vp3 = CreateVisiblePosition(p3);

  // The anchor should still be the span after the VisiblePosition
  // normalization, or the test would become useless.
  EXPECT_EQ(p1, vp1.DeepEquivalent());
  EXPECT_EQ(p2, vp2.DeepEquivalent());
  EXPECT_EQ(vp2.DeepEquivalent(), vp3.DeepEquivalent());

  // No need to test vp3 since it's equal to vp2.
  const VisiblePosition& end1 = EndOfParagraph(vp1);
  const VisiblePosition& end2 = EndOfParagraph(vp2);

  // EndOfParagraph() iterates nodes starting from the span, and "x"@1 would be
  // a suitable candidate. But it's skipped because it precedes the positions.
  EXPECT_LE(vp1.DeepEquivalent(), end1.DeepEquivalent());
  EXPECT_LE(vp2.DeepEquivalent(), end2.DeepEquivalent());

  // Test the actual values.
  EXPECT_EQ(p1, end1.DeepEquivalent());
  EXPECT_EQ(p2, end2.DeepEquivalent());
}

TEST_F(VisibleUnitsParagraphTest, endOfParagraphCannotCrossEditingRoot) {
  SetBodyContent(
      "<div><span contenteditable id=span>this </span>"
      "<a contenteditable=false>link</a>"
      "<span contenteditable> after</span></div>");

  Element* span = GetElementById("span");
  const Position& p1 = Position(span->firstChild(), 2);
  const Position& p2 = Position(span->firstChild(), 5);
  const VisiblePosition& vp1 = CreateVisiblePosition(p1);
  const VisiblePosition& vp2 = CreateVisiblePosition(p2);

  EXPECT_EQ(p1, vp1.DeepEquivalent());
  EXPECT_EQ(p2, vp2.DeepEquivalent());

  const VisiblePosition& end1 = EndOfParagraph(vp1);
  const VisiblePosition& end2 = EndOfParagraph(
      vp1, EditingBoundaryCrossingRule::kCanSkipOverEditingBoundary);

  EXPECT_LE(vp1.DeepEquivalent(), end1.DeepEquivalent());
  EXPECT_LE(vp1.DeepEquivalent(), end2.DeepEquivalent());

  EXPECT_EQ(p2, end1.DeepEquivalent());
  EXPECT_EQ(p2, end2.DeepEquivalent());
}

TEST_F(VisibleUnitsParagraphTest, startOfParagraphCannotBeAfterPosition) {
  SetBodyContent(
      "<span contenteditable><br contenteditable=false>"
      "<br contenteditable=false>x</span>");
  Element* span = GetDocument().QuerySelector(AtomicString("span"));
  const Position& p1 = Position(span, 1);
  const Position& p2 = Position::FirstPositionInNode(*span);
  const Position& p3 = Position::BeforeNode(*span);
  const VisiblePosition& vp1 = CreateVisiblePosition(p1);
  const VisiblePosition& vp2 = CreateVisiblePosition(p2);
  const VisiblePosition& vp3 = CreateVisiblePosition(p3);

  // The anchor should still be the span after the VisiblePosition
  // normalization, or the test would become useless.
  EXPECT_EQ(p1, vp1.DeepEquivalent());
  EXPECT_EQ(p2, vp2.DeepEquivalent());
  EXPECT_EQ(vp2.DeepEquivalent(), vp3.DeepEquivalent());

  // No need to test vp3 since it's equal to vp2.
  const VisiblePosition& start1 = StartOfParagraph(vp1);
  const VisiblePosition& start2 = StartOfParagraph(vp2);

  // StartOfParagraph() iterates nodes in post order starting from the span, and
  // "x"@0 would be a suitable candidate. But it's skipped because it's after
  // the positions.
  EXPECT_LE(start1.DeepEquivalent(), vp1.DeepEquivalent());
  EXPECT_LE(start2.DeepEquivalent(), vp2.DeepEquivalent());

  // Test the actual values.
  EXPECT_EQ(p1, start1.DeepEquivalent());
  EXPECT_EQ(p2, start2.DeepEquivalent());
}

}  // namespace blink
