// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/element.h"

#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class ElementInnerTest : public testing::WithParamInterface<bool>,
                         private ScopedLayoutNGForTest,
                         public EditingTestBase {
 protected:
  ElementInnerTest() : ScopedLayoutNGForTest(GetParam()) {}

  bool LayoutNGEnabled() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All, ElementInnerTest, testing::Bool());

// http://crbug.com/877498
TEST_P(ElementInnerTest, ListItemWithLeadingWhiteSpace) {
  SetBodyContent("<li id=target> abc</li>");
  Element& target = *GetDocument().getElementById("target");
  EXPECT_EQ("abc", target.innerText());
}

// http://crbug.com/877470
TEST_P(ElementInnerTest, SVGElementAsTableCell) {
  SetBodyContent(
      "<div id=target>abc"
      "<svg><rect style='display:table-cell'></rect></svg>"
      "</div>");
  Element& target = *GetDocument().getElementById("target");
  EXPECT_EQ("abc", target.innerText());
}

// http://crbug.com/878725
TEST_P(ElementInnerTest, SVGElementAsTableRow) {
  SetBodyContent(
      "<div id=target>abc"
      "<svg><rect style='display:table-row'></rect></svg>"
      "</div>");
  Element& target = *GetDocument().getElementById("target");
  EXPECT_EQ("abc", target.innerText());
}

// https://crbug.com/947422
TEST_P(ElementInnerTest, OverflowingListItemWithFloatFirstLetter) {
  InsertStyleElement(
      "div { display: list-item; overflow: hidden; }"
      "div::first-letter { float: right; }");
  SetBodyContent("<div id=target>foo</div>");
  Element& target = *GetDocument().getElementById("target");
  EXPECT_EQ("foo", target.innerText());
}

}  // namespace blink
