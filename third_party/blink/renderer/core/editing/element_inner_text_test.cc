// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/element.h"

#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/text_visitor.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

using ElementInnerTest = EditingTestBase;

// http://crbug.com/877498
TEST_F(ElementInnerTest, ListItemWithLeadingWhiteSpace) {
  SetBodyContent("<li id=target> abc</li>");
  Element& target = *GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ("abc", target.innerText());
}

// http://crbug.com/877470
TEST_F(ElementInnerTest, SVGElementAsTableCell) {
  SetBodyContent(
      "<div id=target>abc"
      "<svg><rect style='display:table-cell'></rect></svg>"
      "</div>");
  Element& target = *GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ("abc", target.innerText());
}

// http://crbug.com/878725
TEST_F(ElementInnerTest, SVGElementAsTableRow) {
  SetBodyContent(
      "<div id=target>abc"
      "<svg><rect style='display:table-row'></rect></svg>"
      "</div>");
  Element& target = *GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ("abc", target.innerText());
}

// https://crbug.com/947422
TEST_F(ElementInnerTest, OverflowingListItemWithFloatFirstLetter) {
  InsertStyleElement(
      "div { display: list-item; overflow: hidden; }"
      "div::first-letter { float: right; }");
  SetBodyContent("<div id=target>foo</div>");
  Element& target = *GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ("foo", target.innerText());
}

// https://crbug.com/1164747
TEST_F(ElementInnerTest, GetInnerTextWithoutUpdate) {
  SetBodyContent("<div id=target>ab<span>c</span></div>");
  Element& target = *GetDocument().getElementById(AtomicString("target"));
  EXPECT_EQ("abc", target.innerText());
  EXPECT_EQ("abc", target.GetInnerTextWithoutUpdate());
}

using VisitedNodes = HeapHashSet<Member<const Node>>;
class TextVisitorImpl : public TextVisitor {
  STACK_ALLOCATED();

 public:
  explicit TextVisitorImpl(VisitedNodes& nodes) : nodes_(nodes) {}

  // TextVisitor:
  void WillVisit(const Node& element, unsigned offset) override {
    nodes_.insert(&element);
  }

 private:
  VisitedNodes& nodes_;
};

// Ensures TextVisitor is called for all children of a <select>.
TEST_F(ElementInnerTest, VisitAllChildrenOfSelect) {
  SetBodyContent(
      "<select id='0'><optgroup id='1'><option "
      "id='2'></option></optgroup><option id='3'></option></select>");
  VisitedNodes visited_nodes;
  TextVisitorImpl visitor(visited_nodes);
  GetDocument().body()->getElementById(AtomicString("0"))->innerText(&visitor);

  // The select and all its descendants should be visited. Each one has an
  // id from 0-4.
  for (int i = 0; i < 4; ++i) {
    Element* element =
        GetDocument().getElementById(AtomicString(String::Number(i)));
    ASSERT_TRUE(element) << i;
    EXPECT_TRUE(visited_nodes.Contains(element)) << i;
    visited_nodes.erase(element);
  }

  // Nothing else should remain.
  EXPECT_TRUE(visited_nodes.empty());
}

}  // namespace blink
