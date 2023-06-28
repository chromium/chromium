// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/editing_strategy.h"

#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

class EditingStrategyTest : public EditingTestBase {};

TEST_F(EditingStrategyTest, caretMaxOffset) {
  const char* body_content =
      "<p id='host'>00<b slot='#one' id='one'>1</b><b slot='#two' "
      "id='two'>22</b>333</p>";
  const char* shadow_content = "<slot name=#two></slot><slot name=#one></slot>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");
  Node* host = GetDocument().getElementById(AtomicString("host"));
  Node* one = GetDocument().getElementById(AtomicString("one"));
  Node* two = GetDocument().getElementById(AtomicString("two"));

  EXPECT_EQ(4, EditingStrategy::CaretMaxOffset(*host));
  EXPECT_EQ(1, EditingStrategy::CaretMaxOffset(*one));
  EXPECT_EQ(1, EditingStrategy::CaretMaxOffset(*one->firstChild()));
  EXPECT_EQ(2, EditingStrategy::CaretMaxOffset(*two->firstChild()));

  EXPECT_EQ(2, EditingInFlatTreeStrategy::CaretMaxOffset(*host));
  EXPECT_EQ(1, EditingInFlatTreeStrategy::CaretMaxOffset(*one));
  EXPECT_EQ(1, EditingInFlatTreeStrategy::CaretMaxOffset(*one->firstChild()));
  EXPECT_EQ(2, EditingInFlatTreeStrategy::CaretMaxOffset(*two->firstChild()));
}

TEST_F(EditingStrategyTest, CaretMaxOffsetWithFirstLetter) {
  SetBodyContent(
      "<style>div::first-letter { text-transform: uppercase }</style>"
      "<div id='a'>a</div>"
      "<div id='b'>   b</div>"
      "<div id='c'>cde</div>");

  Node* a = GetDocument().getElementById(AtomicString("a"));
  Node* b = GetDocument().getElementById(AtomicString("b"));
  Node* c = GetDocument().getElementById(AtomicString("c"));

  EXPECT_EQ(1, EditingStrategy::CaretMaxOffset(*a->firstChild()));
  EXPECT_EQ(4, EditingStrategy::CaretMaxOffset(*b->firstChild()));
  EXPECT_EQ(3, EditingStrategy::CaretMaxOffset(*c->firstChild()));
}

}  // namespace blink
