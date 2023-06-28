// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

#include "third_party/blink/renderer/core/editing/position.h"

namespace blink {

class EditingTestBaseTest : public EditingTestBase {};

TEST_F(EditingTestBaseTest, GetCaretTextFromBody) {
  SetBodyContent("<div>foo</div>");
  Element* const div = GetDocument().QuerySelector(AtomicString("div"));
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

// TODO(editing-dev): Add demos of other functions of EditingTestBase.

}  // namespace blink
