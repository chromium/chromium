// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/select_menu_part_traversal.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

namespace {

template <class T>
HeapVector<Member<Node>> CollectFromIterable(T iterable) {
  HeapVector<Member<Node>> nodes;
  for (auto& node : iterable)
    nodes.push_back(&node);
  return nodes;
}

void RemoveWhiteSpaceOnlyTextNodes(ContainerNode& container) {
  for (Node* descendant :
       CollectFromIterable(NodeTraversal::InclusiveDescendantsOf(container))) {
    if (auto* text = DynamicTo<Text>(descendant)) {
      if (text->ContainsOnlyWhitespaceOrEmpty())
        text->remove();
    }
  }
}

}  // namespace

class SelectMenuPartTraversalTest : public PageTestBase {
 public:
  SelectMenuPartTraversalTest() = default;

 protected:
  void SetupSampleHTML(const char* main_html);
};

void SelectMenuPartTraversalTest::SetupSampleHTML(const char* html) {
  Element* body = GetDocument().body();
  SetBodyInnerHTML(String::FromUTF8(html));
  RemoveWhiteSpaceOnlyTextNodes(*body);
}

TEST_F(SelectMenuPartTraversalTest, Siblings) {
  SetupSampleHTML(R"(
    <select></select>
    <selectmenu></selectmenu>
    <div id='c0'></div>
    <select></select>
    <selectmenu></selectmenu>
    <div id='c1'></div>
    <div id='c2'></div>
    <select></select>
    <selectmenu></selectmenu>)");

  Element* body = GetDocument().body();
  Element* c0 = body->QuerySelector("#c0");
  Element* c1 = body->QuerySelector("#c1");
  Element* c2 = body->QuerySelector("#c2");

  EXPECT_EQ(c0, SelectMenuPartTraversal::FirstChild(*body));
  EXPECT_EQ(c2, SelectMenuPartTraversal::LastChild(*body));

  EXPECT_EQ(c1, SelectMenuPartTraversal::NextSibling(*c0));
  EXPECT_EQ(c2, SelectMenuPartTraversal::NextSibling(*c1));
  EXPECT_EQ(nullptr, SelectMenuPartTraversal::NextSibling(*c2));

  EXPECT_EQ(c1, SelectMenuPartTraversal::PreviousSibling(*c2));
  EXPECT_EQ(c0, SelectMenuPartTraversal::PreviousSibling(*c1));
  EXPECT_EQ(nullptr, SelectMenuPartTraversal::PreviousSibling(*c0));
}

TEST_F(SelectMenuPartTraversalTest, IsDescendantOf) {
  SetupSampleHTML(R"(
    <selectmenu>
        <div id='c0'></div>
    </selectmenu>
    <div id='c1'></div>
    <div id='c2'>
      <div id='c3'></div>
    </div>
    <select>
        <option id='c4'></option>
    </select>)");

  Element* body = GetDocument().body();
  Element* c0 = body->QuerySelector("#c0");
  Element* c1 = body->QuerySelector("#c1");
  Element* c2 = body->QuerySelector("#c2");
  Element* c3 = body->QuerySelector("#c3");
  Element* c4 = body->QuerySelector("#c4");

  EXPECT_FALSE(SelectMenuPartTraversal::IsDescendantOf(*body, *body));
  EXPECT_FALSE(SelectMenuPartTraversal::IsDescendantOf(*c0, *body));
  EXPECT_FALSE(SelectMenuPartTraversal::IsDescendantOf(*body, *c0));

  EXPECT_FALSE(SelectMenuPartTraversal::IsDescendantOf(*c0, *c1));
  EXPECT_FALSE(SelectMenuPartTraversal::IsDescendantOf(*c1, *c0));

  EXPECT_TRUE(SelectMenuPartTraversal::IsDescendantOf(*c1, *body));
  EXPECT_FALSE(SelectMenuPartTraversal::IsDescendantOf(*body, *c1));

  EXPECT_FALSE(SelectMenuPartTraversal::IsDescendantOf(*c1, *c2));
  EXPECT_FALSE(SelectMenuPartTraversal::IsDescendantOf(*c2, *c1));

  EXPECT_TRUE(SelectMenuPartTraversal::IsDescendantOf(*c3, *body));
  EXPECT_FALSE(SelectMenuPartTraversal::IsDescendantOf(*body, *c3));

  EXPECT_FALSE(SelectMenuPartTraversal::IsDescendantOf(*c4, *body));
  EXPECT_FALSE(SelectMenuPartTraversal::IsDescendantOf(*body, *c4));
}

TEST_F(SelectMenuPartTraversalTest, NextPrevious) {
  SetupSampleHTML(R"(
    <selectmenu>
        <div id='c0'></div>
    </selectmenu>
    <div id='c1'></div>
    <div id='c2'>
      <div id='c3'></div>
        <select>
          <option id='c4'></option>
        </select>
    </div>)");

  Element* body = GetDocument().body();
  Element* c1 = body->QuerySelector("#c1");
  Element* c2 = body->QuerySelector("#c2");
  Element* c3 = body->QuerySelector("#c3");

  EXPECT_EQ(nullptr, SelectMenuPartTraversal::Previous(*c1, body));
  EXPECT_EQ(body, SelectMenuPartTraversal::Previous(*c1, nullptr));
  EXPECT_EQ(*c2, SelectMenuPartTraversal::Next(*c1, body));
  EXPECT_EQ(*c1, SelectMenuPartTraversal::Previous(*c2, body));
  EXPECT_EQ(*c3, SelectMenuPartTraversal::Next(*c2, body));
  EXPECT_EQ(*c2, SelectMenuPartTraversal::Previous(*c3, body));
  EXPECT_EQ(nullptr, SelectMenuPartTraversal::Next(*c3, body));
  EXPECT_EQ(nullptr, SelectMenuPartTraversal::Next(*c3, nullptr));
}

}  // namespace blink
