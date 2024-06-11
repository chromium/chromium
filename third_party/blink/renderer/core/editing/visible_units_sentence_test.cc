// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/visible_units.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"

namespace blink {

class VisibleUnitsSentenceTest : public EditingTestBase {
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

TEST_F(VisibleUnitsSentenceTest, startOfSentence) {
  const char* body_content =
      "<span id=host><b slot='#one' id=one>1</b><b slot='#two' "
      "id=two>22</b></span>";
  const char* shadow_content =
      "<p><i id=three>333</i> <slot name=#two></slot> <slot name=#one></slot> "
      "<i id=four>4444</i></p>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById(AtomicString("one"))->firstChild();
  Node* two = GetDocument().getElementById(AtomicString("two"))->firstChild();
  Node* three =
      shadow_root->getElementById(AtomicString("three"))->firstChild();
  Node* four = shadow_root->getElementById(AtomicString("four"))->firstChild();

  EXPECT_EQ(Position(three, 0), StartOfSentencePosition(Position(*one, 0)));
  EXPECT_EQ(PositionInFlatTree(three, 0),
            StartOfSentencePosition(PositionInFlatTree(*one, 0)));

  EXPECT_EQ(Position(three, 0), StartOfSentencePosition(Position(*one, 1)));
  EXPECT_EQ(PositionInFlatTree(three, 0),
            StartOfSentencePosition(PositionInFlatTree(*one, 1)));

  EXPECT_EQ(Position(three, 0), StartOfSentencePosition(Position(*two, 0)));
  EXPECT_EQ(PositionInFlatTree(three, 0),
            StartOfSentencePosition(PositionInFlatTree(*two, 0)));

  EXPECT_EQ(Position(three, 0), StartOfSentencePosition(Position(*two, 1)));
  EXPECT_EQ(PositionInFlatTree(three, 0),
            StartOfSentencePosition(PositionInFlatTree(*two, 1)));

  EXPECT_EQ(Position(three, 0), StartOfSentencePosition(Position(*three, 1)));
  EXPECT_EQ(PositionInFlatTree(three, 0),
            StartOfSentencePosition(PositionInFlatTree(*three, 1)));

  EXPECT_EQ(Position(three, 0), StartOfSentencePosition(Position(*four, 1)));
  EXPECT_EQ(PositionInFlatTree(three, 0),
            StartOfSentencePosition(PositionInFlatTree(*four, 1)));
}

TEST_F(VisibleUnitsSentenceTest, SentenceBoundarySkipTextControl) {
  SetBodyContent("foo <input value=\"xx. xx.\"> bar.");
  const Node* foo =
      GetDocument().QuerySelector(AtomicString("input"))->previousSibling();
  const Node* bar =
      GetDocument().QuerySelector(AtomicString("input"))->nextSibling();

  EXPECT_EQ(Position(bar, 5), EndOfSentence(Position(foo, 1)).GetPosition());
  EXPECT_EQ(PositionInFlatTree(bar, 5),
            EndOfSentence(PositionInFlatTree(foo, 1)).GetPosition());

  EXPECT_EQ(Position(foo, 0),
            StartOfSentencePosition(Position(Position(bar, 3))));

  EXPECT_EQ(PositionInFlatTree(foo, 0),
            StartOfSentencePosition(PositionInFlatTree(bar, 3)));
}

}  // namespace blink
