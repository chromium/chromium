// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/select_list_part_traversal.h"

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

class SelectListPartTraversalTest : public PageTestBase {
 public:
  SelectListPartTraversalTest() = default;

 protected:
  void SetupSampleHTML(const char* main_html);
};

void SelectListPartTraversalTest::SetupSampleHTML(const char* html) {
  Element* body = GetDocument().body();
  SetBodyInnerHTML(String::FromUTF8(html));
  RemoveWhiteSpaceOnlyTextNodes(*body);
}

TEST_F(SelectListPartTraversalTest, Siblings) {
  SetupSampleHTML(R"(
    <select></select>
    <selectlist></selectlist>
    <div id='c0'></div>
    <select></select>
    <selectlist></selectlist>
    <div id='c1'></div>
    <div id='c2'></div>
    <select></select>
    <selectlist></selectlist>)");

  Element* body = GetDocument().body();
  Element* c0 = body->QuerySelector(AtomicString("#c0"));
  Element* c1 = body->QuerySelector(AtomicString("#c1"));
  Element* c2 = body->QuerySelector(AtomicString("#c2"));

  EXPECT_EQ(c0, SelectListPartTraversal::FirstChild(*body));
  EXPECT_EQ(c2, SelectListPartTraversal::LastChild(*body));

  EXPECT_EQ(c1, SelectListPartTraversal::NextSibling(*c0));
  EXPECT_EQ(c2, SelectListPartTraversal::NextSibling(*c1));
  EXPECT_EQ(nullptr, SelectListPartTraversal::NextSibling(*c2));

  EXPECT_EQ(c1, SelectListPartTraversal::PreviousSibling(*c2));
  EXPECT_EQ(c0, SelectListPartTraversal::PreviousSibling(*c1));
  EXPECT_EQ(nullptr, SelectListPartTraversal::PreviousSibling(*c0));
}

TEST_F(SelectListPartTraversalTest, IsDescendantOf) {
  SetupSampleHTML(R"(
    <selectlist>
        <div id='c0'></div>
    </selectlist>
    <div id='c1'></div>
    <div id='c2'>
      <div id='c3'></div>
    </div>
    <select>
        <option id='c4'></option>
    </select>)");

  Element* body = GetDocument().body();
  Element* c0 = body->QuerySelector(AtomicString("#c0"));
  Element* c1 = body->QuerySelector(AtomicString("#c1"));
  Element* c2 = body->QuerySelector(AtomicString("#c2"));
  Element* c3 = body->QuerySelector(AtomicString("#c3"));
  Element* c4 = body->QuerySelector(AtomicString("#c4"));

  EXPECT_FALSE(SelectListPartTraversal::IsDescendantOf(*body, *body));
  EXPECT_FALSE(SelectListPartTraversal::IsDescendantOf(*c0, *body));
  EXPECT_FALSE(SelectListPartTraversal::IsDescendantOf(*body, *c0));

  EXPECT_FALSE(SelectListPartTraversal::IsDescendantOf(*c0, *c1));
  EXPECT_FALSE(SelectListPartTraversal::IsDescendantOf(*c1, *c0));

  EXPECT_TRUE(SelectListPartTraversal::IsDescendantOf(*c1, *body));
  EXPECT_FALSE(SelectListPartTraversal::IsDescendantOf(*body, *c1));

  EXPECT_FALSE(SelectListPartTraversal::IsDescendantOf(*c1, *c2));
  EXPECT_FALSE(SelectListPartTraversal::IsDescendantOf(*c2, *c1));

  EXPECT_TRUE(SelectListPartTraversal::IsDescendantOf(*c3, *body));
  EXPECT_FALSE(SelectListPartTraversal::IsDescendantOf(*body, *c3));

  EXPECT_FALSE(SelectListPartTraversal::IsDescendantOf(*c4, *body));
  EXPECT_FALSE(SelectListPartTraversal::IsDescendantOf(*body, *c4));
}

TEST_F(SelectListPartTraversalTest, NextPrevious) {
  SetupSampleHTML(R"(
    <selectlist>
        <div id='c0'></div>
    </selectlist>
    <div id='c1'></div>
    <div id='c2'>
      <div id='c3'></div>
        <select>
          <option id='c4'></option>
        </select>
    </div>)");

  Element* body = GetDocument().body();
  Element* c1 = body->QuerySelector(AtomicString("#c1"));
  Element* c2 = body->QuerySelector(AtomicString("#c2"));
  Element* c3 = body->QuerySelector(AtomicString("#c3"));

  EXPECT_EQ(nullptr, SelectListPartTraversal::Previous(*c1, body));
  EXPECT_EQ(body, SelectListPartTraversal::Previous(*c1, nullptr));
  EXPECT_EQ(*c2, SelectListPartTraversal::Next(*c1, body));
  EXPECT_EQ(*c1, SelectListPartTraversal::Previous(*c2, body));
  EXPECT_EQ(*c3, SelectListPartTraversal::Next(*c2, body));
  EXPECT_EQ(*c2, SelectListPartTraversal::Previous(*c3, body));
  EXPECT_EQ(nullptr, SelectListPartTraversal::Next(*c3, body));
  EXPECT_EQ(nullptr, SelectListPartTraversal::Next(*c3, nullptr));
}

}  // namespace blink
