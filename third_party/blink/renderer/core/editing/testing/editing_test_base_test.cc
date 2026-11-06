// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/position.h"

namespace blink {

class EditingTestBaseTest : public EditingTestBase {};

TEST_F(EditingTestBaseTest, GetCaretTextFromBody) {
  SetBodyContent("<div>foo</div>");
  Element* const div = QuerySelector("div");
  Node* const foo = div->firstChild();
  EXPECT_EQ("|<div>foo</div>",
            GetCaretTextFromBody(Position::BeforeNode(*div)));

  // TODO(editing-dev): Consider different serialization for the following two
  // positions.
  EXPECT_EQ("<div>|foo</div>",
            GetCaretTextFromBody(Position::FirstPositionInNode(*div)));
  EXPECT_EQ("<div>|foo</div>", GetCaretTextFromBody(Position(foo, 0)));

  // TODO(editing-dev): Consider different serialization for the following two
  // positions.
  EXPECT_EQ("<div>foo|</div>", GetCaretTextFromBody(Position(foo, 3)));
  EXPECT_EQ("<div>foo|</div>",
            GetCaretTextFromBody(Position::LastPositionInNode(*div)));

  EXPECT_EQ("<div>foo</div>|", GetCaretTextFromBody(Position::AfterNode(*div)));
}

TEST_F(EditingTestBaseTest, GetRangeTextFromBody) {
  SetBodyContent("<div>foo</div>");
  Element* const div = QuerySelector("div");
  Text* const text = To<Text>(div->firstChild());

  Range* range = Range::Create(GetDocument());
  range->setStart(text, 1, IGNORE_EXCEPTION_FOR_TESTING);
  range->setEnd(text, 3, IGNORE_EXCEPTION_FOR_TESTING);
  EXPECT_EQ("<div>f^oo|</div>", GetRangeTextFromBody(range));
  EXPECT_EQ("<div>f^oo|</div>", GetRangeTextFromBody(EphemeralRange(range)));
}

// TODO(editing-dev): Add demos of other functions of EditingTestBase.

}  // namespace blink
