// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Position-based sentence tests are in position_units_sentence_test.cc.
// This file tests the VisiblePosition wrappers in visible_units_sentence.cc.

#include "third_party/blink/renderer/core/editing/visible_units.h"

#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"

namespace blink {

class VisibleUnitsSentenceTest : public EditingTestBase {};

TEST_F(VisibleUnitsSentenceTest, EndOfSentenceVisiblePosition) {
  SetBodyContent("<div id=sample>First sentence. Second sentence.</div>");
  Node* text =
      GetDocument().getElementById(AtomicString("sample"))->firstChild();

  VisiblePosition vp = CreateVisiblePosition(Position(*text, 0));
  VisiblePosition result = EndOfSentence(vp);
  EXPECT_FALSE(result.IsNull());
  // EndOfSentence with default kIncludeSpace includes the trailing space,
  // so "First sentence. " ends at offset 16.
  EXPECT_EQ(Position(*text, 16), result.DeepEquivalent());
}

TEST_F(VisibleUnitsSentenceTest, EndOfSentenceVisiblePositionInFlatTree) {
  SetBodyContent("<div id=sample>First sentence. Second sentence.</div>");
  Node* text =
      GetDocument().getElementById(AtomicString("sample"))->firstChild();

  VisiblePositionInFlatTree vp =
      CreateVisiblePosition(PositionInFlatTree(*text, 0));
  VisiblePositionInFlatTree result = EndOfSentence(vp);
  EXPECT_FALSE(result.IsNull());
  // EndOfSentence with default kIncludeSpace includes the trailing space,
  // so "First sentence. " ends at offset 16.
  EXPECT_EQ(PositionInFlatTree(*text, 16), result.DeepEquivalent());
}

}  // namespace blink
