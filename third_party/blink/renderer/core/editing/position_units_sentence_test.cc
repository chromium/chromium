// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/position_units.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"

namespace blink {

class PositionUnitsSentenceTest : public EditingTestBase {};

TEST_F(PositionUnitsSentenceTest, StartOfSentenceWithShadowDOM) {
  const char* body_content =
      "<span id=host><b slot='#one' id=one>1</b><b slot='#two' "
      "id=two>22</b></span>";
  const char* shadow_content =
      "<p><i id=three>333</i> <slot name=#two></slot> <slot name=#one></slot> "
      "<i id=four>4444</i></p>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetElementById("one")->firstChild();
  Node* two = GetElementById("two")->firstChild();
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

TEST_F(PositionUnitsSentenceTest, SentenceBoundarySkipTextControl) {
  SetBodyContent("foo <input value=\"xx. xx.\"> bar.");
  const Node* foo = QuerySelector("input")->previousSibling();
  const Node* bar = QuerySelector("input")->nextSibling();

  EXPECT_EQ(Position(bar, 5), EndOfSentence(Position(foo, 1)).GetPosition());
  EXPECT_EQ(PositionInFlatTree(bar, 5),
            EndOfSentence(PositionInFlatTree(foo, 1)).GetPosition());

  EXPECT_EQ(Position(foo, 0),
            StartOfSentencePosition(Position(Position(bar, 3))));

  EXPECT_EQ(PositionInFlatTree(foo, 0),
            StartOfSentencePosition(PositionInFlatTree(bar, 3)));
}

TEST_F(PositionUnitsSentenceTest, NextSentencePositionWithPosition) {
  SetBodyContent("<div id=sample>First sentence. Second sentence.</div>");
  Node* text =
      GetElementById("sample")->firstChild();

  EXPECT_EQ(PositionInFlatTree(text, 16),
            NextSentencePosition(PositionInFlatTree(*text, 0)));
}

TEST_F(PositionUnitsSentenceTest, PreviousSentencePositionWithPositionInFlatTree) {
  SetBodyContent("<div id=sample>First sentence. Second sentence.</div>");
  Node* text =
      GetElementById("sample")->firstChild();

  // Offset 20 is inside "Second sentence." ICU sentence break preceding(20)
  // returns 16 (start of "Second sentence."), not 0.
  EXPECT_EQ(PositionInFlatTree(text, 16),
            PreviousSentencePosition(PositionInFlatTree(*text, 20)));
}

TEST_F(PositionUnitsSentenceTest, EndOfSentenceWithOmitSpacePosition) {
  SetBodyContent("<div id=sample>First sentence. Second sentence.</div>");
  Node* text = GetElementById("sample")->firstChild();

  // With kOmitSpace, end of sentence should be at the period (offset 15),
  // not after the trailing space (offset 16).
  EXPECT_EQ(Position(text, 15),
            EndOfSentence(Position(text, 0),
                          SentenceTrailingSpaceBehavior::kOmitSpace)
                .GetPosition());
}

TEST_F(PositionUnitsSentenceTest, EndOfSentenceWithOmitSpacePositionInFlatTree) {
  SetBodyContent("<div id=sample>First sentence. Second sentence.</div>");
  Node* text = GetElementById("sample")->firstChild();

  EXPECT_EQ(PositionInFlatTree(text, 15),
            EndOfSentence(PositionInFlatTree(text, 0),
                          SentenceTrailingSpaceBehavior::kOmitSpace)
                .GetPosition());
}

}  // namespace blink
