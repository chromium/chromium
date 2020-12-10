// Copyright 2015 The Chromium Authors. All rights reserved.
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

  Node* sample = GetDocument().getElementById("sample");
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

  Node* sample = GetDocument().getElementById("sample");
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

  Element* one = GetDocument().getElementById("one");
  Element* two = GetDocument().getElementById("two");
  Element* three = GetDocument().getElementById("three");

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

  Node* sample = GetDocument().getElementById("sample");
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

  Node* sample = GetDocument().getElementById("sample");
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

  Node* one = GetDocument().getElementById("one")->firstChild();
  Node* two = GetDocument().getElementById("two")->firstChild();
  Node* three = GetDocument().getElementById("three")->firstChild();

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

  Node* zero = GetDocument().getElementById("zero")->firstChild();
  Node* one = GetDocument().getElementById("one")->firstChild();
  Node* two = GetDocument().getElementById("two")->firstChild();
  Node* three = GetDocument().getElementById("three")->firstChild();

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

}  // namespace blink
