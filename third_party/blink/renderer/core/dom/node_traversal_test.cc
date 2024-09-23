// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/dom/node_traversal.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {
namespace node_traversal_test {

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

class NodeTraversalTest : public PageTestBase {
 public:
  NodeTraversalTest() {}

 protected:
  void SetupSampleHTML(const char* html);
};

void NodeTraversalTest::SetupSampleHTML(const char* html) {
  Element* body = GetDocument().body();
  SetBodyInnerHTML(String::FromUTF8(html));
  RemoveWhiteSpaceOnlyTextNodes(*body);
}

namespace {

void TestCommonAncestor(Node* expected_result,
                        const Node& node_a,
                        const Node& node_b) {
  Node* result1 = NodeTraversal::CommonAncestor(node_a, node_b);
  EXPECT_EQ(expected_result, result1)
      << "CommonAncestor(" << node_a.textContent() << ","
      << node_b.textContent() << ")";
  Node* result2 = NodeTraversal::CommonAncestor(node_b, node_a);
  EXPECT_EQ(expected_result, result2)
      << "CommonAncestor(" << node_b.textContent() << ","
      << node_a.textContent() << ")";
}

}  // namespace

// Test case for
//  - Next
//  - NextSkippingChildren
//  - NextPostOrder
TEST_F(NodeTraversalTest, NextFunctions) {
  SetupSampleHTML(R"(
      <div id='c0'>
        <div id='c00'></div>
        <div id='c01'></div>
      </div>
      <div id='c1'>
        <div id='c10'></div>
      </div>)");

  Element* html = GetDocument().documentElement();
  Element* body = GetDocument().body();
  Element* c0 = body->QuerySelector(AtomicString("#c0"));
  Element* c1 = body->QuerySelector(AtomicString("#c1"));
  Element* c00 = body->QuerySelector(AtomicString("#c00"));
  Element* c01 = body->QuerySelector(AtomicString("#c01"));
  Element* c10 = body->QuerySelector(AtomicString("#c10"));

  EXPECT_EQ(c0, NodeTraversal::Next(*body));
  EXPECT_EQ(c00, NodeTraversal::Next(*c0));
  EXPECT_EQ(c01, NodeTraversal::Next(*c00));
  EXPECT_EQ(c1, NodeTraversal::Next(*c01));
  EXPECT_EQ(c10, NodeTraversal::Next(*c1));
  EXPECT_EQ(nullptr, NodeTraversal::Next(*c10));

  EXPECT_EQ(nullptr, NodeTraversal::NextSkippingChildren(*body));
  EXPECT_EQ(c1, NodeTraversal::NextSkippingChildren(*c0));
  EXPECT_EQ(c01, NodeTraversal::NextSkippingChildren(*c00));
  EXPECT_EQ(c1, NodeTraversal::NextSkippingChildren(*c01));
  EXPECT_EQ(nullptr, NodeTraversal::NextSkippingChildren(*c1));
  EXPECT_EQ(nullptr, NodeTraversal::Next(*c10));

  EXPECT_EQ(html, NodeTraversal::NextPostOrder(*body));
  EXPECT_EQ(c10, NodeTraversal::NextPostOrder(*c0));
  EXPECT_EQ(body, NodeTraversal::NextPostOrder(*c1));
  EXPECT_EQ(c01, NodeTraversal::NextPostOrder(*c00));
  EXPECT_EQ(c0, NodeTraversal::NextPostOrder(*c01));
  EXPECT_EQ(c1, NodeTraversal::NextPostOrder(*c10));
}

// Test case for
//  - LastWithin
//  - LastWithinOrSelf
TEST_F(NodeTraversalTest, LastWithin) {
  SetupSampleHTML(R"(
      <div id='c0'>
        <div id='c00'></div>
      </div>
      <div id='c1'></div>)");

  Element* body = GetDocument().body();
  Element* c0 = body->QuerySelector(AtomicString("#c0"));
  Element* c1 = body->QuerySelector(AtomicString("#c1"));
  Element* c00 = body->QuerySelector(AtomicString("#c00"));

  EXPECT_EQ(c1, NodeTraversal::LastWithin(*body));
  EXPECT_EQ(c1, NodeTraversal::LastWithinOrSelf(*body));

  EXPECT_EQ(c00, NodeTraversal::LastWithin(*c0));
  EXPECT_EQ(c00, NodeTraversal::LastWithinOrSelf(*c0));

  EXPECT_EQ(nullptr, NodeTraversal::LastWithin(*c1));
  EXPECT_EQ(c1, NodeTraversal::LastWithinOrSelf(*c1));
}

// Test case for
//  - Previous
//  - PreviousAbsoluteSibling
//  - PreviousPostOrder
TEST_F(NodeTraversalTest, PreviousFunctions) {
  SetupSampleHTML(R"(
      <div id='c0'>
        <div id='c00'></div>
        <div id='c01'></div>
      </div>
      <div id='c1'>
        <div id='c10'></div>
      </div>)");

  Element* html = GetDocument().documentElement();
  Element* head = GetDocument().head();
  Element* body = GetDocument().body();
  Element* c0 = body->QuerySelector(AtomicString("#c0"));
  Element* c1 = body->QuerySelector(AtomicString("#c1"));
  Element* c00 = body->QuerySelector(AtomicString("#c00"));
  Element* c01 = body->QuerySelector(AtomicString("#c01"));
  Element* c10 = body->QuerySelector(AtomicString("#c10"));

  EXPECT_EQ(head, NodeTraversal::Previous(*body));
  EXPECT_EQ(body, NodeTraversal::Previous(*c0));
  EXPECT_EQ(c0, NodeTraversal::Previous(*c00));
  EXPECT_EQ(c00, NodeTraversal::Previous(*c01));
  EXPECT_EQ(c01, NodeTraversal::Previous(*c1));
  EXPECT_EQ(c1, NodeTraversal::Previous(*c10));

  EXPECT_EQ(nullptr, NodeTraversal::PreviousAbsoluteSibling(*html));
  EXPECT_EQ(head, NodeTraversal::PreviousAbsoluteSibling(*body));
  EXPECT_EQ(head, NodeTraversal::PreviousAbsoluteSibling(*c0));
  EXPECT_EQ(head, NodeTraversal::PreviousAbsoluteSibling(*c00));
  EXPECT_EQ(c00, NodeTraversal::PreviousAbsoluteSibling(*c01));
  EXPECT_EQ(c0, NodeTraversal::PreviousAbsoluteSibling(*c1));
  EXPECT_EQ(c0, NodeTraversal::PreviousAbsoluteSibling(*c10));

  EXPECT_EQ(c1, NodeTraversal::PreviousPostOrder(*body));
  EXPECT_EQ(c01, NodeTraversal::PreviousPostOrder(*c0));
  EXPECT_EQ(c10, NodeTraversal::PreviousPostOrder(*c1));
  EXPECT_EQ(head, NodeTraversal::PreviousPostOrder(*c00));
  EXPECT_EQ(c00, NodeTraversal::PreviousPostOrder(*c01));
  EXPECT_EQ(c0, NodeTraversal::PreviousPostOrder(*c10));
}

// Test case for
//  - ChildAt
//  - CountChildren
//  - HasChildren
//  - Index
//  - IsDescendantOf
TEST_F(NodeTraversalTest, ChildAt) {
  SetupSampleHTML(R"(
      <div id='c0'>
        <span id='c00'>c00</span>
      </div>
      <div id='c1'></div>
      <div id='c2'></div>)");

  Element* body = GetDocument().body();
  Element* c0 = body->QuerySelector(AtomicString("#c0"));
  Element* c1 = body->QuerySelector(AtomicString("#c1"));
  Element* c2 = body->QuerySelector(AtomicString("#c2"));
  Element* c00 = body->QuerySelector(AtomicString("#c00"));

  const unsigned kNumberOfChildNodes = 3;
  Node* expected_child_nodes[3] = {c0, c1, c2};

  ASSERT_EQ(kNumberOfChildNodes, NodeTraversal::CountChildren(*body));
  EXPECT_TRUE(NodeTraversal::HasChildren(*body));

  for (unsigned index = 0; index < kNumberOfChildNodes; ++index) {
    Node* child = NodeTraversal::ChildAt(*body, index);
    EXPECT_EQ(index, NodeTraversal::Index(*child))
        << "NodeTraversal::index(NodeTraversal(*body, " << index << "))";
    EXPECT_TRUE(NodeTraversal::IsDescendantOf(*child, *body))
        << "NodeTraversal::isDescendantOf(*NodeTraversal(*body, " << index
        << "), *body)";
    EXPECT_EQ(expected_child_nodes[index], child)
        << "NodeTraversal::childAt(*body, " << index << ")";
  }
  EXPECT_EQ(nullptr, NodeTraversal::ChildAt(*body, kNumberOfChildNodes + 1))
      << "Out of bounds childAt() returns nullptr.";

  EXPECT_EQ(c00, NodeTraversal::FirstChild(*c0));
}

// Test case for
//  - FirstChild
//  - LastChild
//  - NextSibling
//  - PreviousSibling
//  - Parent
TEST_F(NodeTraversalTest, Siblings) {
  SetupSampleHTML(R"(
      <div id='c0'></div>
      <div id='c1'></div>
      <div id='c2'></div>)");

  Element* body = GetDocument().body();
  Element* c0 = body->QuerySelector(AtomicString("#c0"));
  Element* c1 = body->QuerySelector(AtomicString("#c1"));
  Element* c2 = body->QuerySelector(AtomicString("#c2"));

  EXPECT_EQ(c0, NodeTraversal::FirstChild(*body));
  EXPECT_EQ(c2, NodeTraversal::LastChild(*body));

  EXPECT_EQ(body, NodeTraversal::Parent(*c0));
  EXPECT_EQ(body, NodeTraversal::Parent(*c1));
  EXPECT_EQ(body, NodeTraversal::Parent(*c2));

  EXPECT_EQ(c1, NodeTraversal::NextSibling(*c0));
  EXPECT_EQ(c2, NodeTraversal::NextSibling(*c1));
  EXPECT_EQ(nullptr, NodeTraversal::NextSibling(*c2));

  EXPECT_EQ(c1, NodeTraversal::PreviousSibling(*c2));
  EXPECT_EQ(c0, NodeTraversal::PreviousSibling(*c1));
  EXPECT_EQ(nullptr, NodeTraversal::PreviousSibling(*c0));
}

TEST_F(NodeTraversalTest, commonAncestor) {
  SetupSampleHTML(R"(
      <div id='c0'>
        <div id='c00'>
          <div id='c000'></div>
        </div>
        <div id='c01'></div>
      </div>
      <div id='c1'>
        <div id='c10'></div>
      </div>
      <div id='c2'></div>)");

  Element* body = GetDocument().body();
  Element* c0 = body->QuerySelector(AtomicString("#c0"));
  Element* c1 = body->QuerySelector(AtomicString("#c1"));
  Element* c2 = body->QuerySelector(AtomicString("#c2"));

  Element* c00 = body->QuerySelector(AtomicString("#c00"));
  Element* c01 = body->QuerySelector(AtomicString("#c01"));
  Element* c10 = body->QuerySelector(AtomicString("#c10"));
  Element* c000 = body->QuerySelector(AtomicString("#c000"));

  TestCommonAncestor(body, *c0, *c1);
  TestCommonAncestor(body, *c1, *c2);
  TestCommonAncestor(body, *c00, *c10);
  TestCommonAncestor(body, *c01, *c10);
  TestCommonAncestor(body, *c2, *c10);
  TestCommonAncestor(body, *c2, *c000);

  TestCommonAncestor(c0, *c00, *c01);
  TestCommonAncestor(c0, *c000, *c01);
  TestCommonAncestor(c1, *c1, *c10);
}

TEST_F(NodeTraversalTest, AncestorsOf) {
  SetupSampleHTML(R"(
      <div>
        <div>
          <div id='child'></div>
        </div>
      </div>)");

  Element* child = GetDocument().getElementById(AtomicString("child"));

  HeapVector<Member<Node>> expected_nodes;
  for (Node* parent = NodeTraversal::Parent(*child); parent;
       parent = NodeTraversal::Parent(*parent)) {
    expected_nodes.push_back(parent);
  }

  HeapVector<Member<Node>> actual_nodes;
  for (Node& ancestor : NodeTraversal::AncestorsOf(*child))
    actual_nodes.push_back(&ancestor);

  EXPECT_EQ(expected_nodes, actual_nodes);
}

TEST_F(NodeTraversalTest, InclusiveAncestorsOf) {
  SetupSampleHTML(R"(
      <div>
        <div>
          <div id='child'></div>
        </div>
      </div>)");

  Element* child = GetDocument().getElementById(AtomicString("child"));

  HeapVector<Member<Node>> expected_nodes;
  for (Node* parent = child; parent; parent = NodeTraversal::Parent(*parent)) {
    expected_nodes.push_back(parent);
  }

  HeapVector<Member<Node>> actual_nodes;
  for (Node& ancestor : NodeTraversal::InclusiveAncestorsOf(*child))
    actual_nodes.push_back(&ancestor);

  EXPECT_EQ(expected_nodes, actual_nodes);
}

TEST_F(NodeTraversalTest, ChildrenOf) {
  SetupSampleHTML(R"(
      <div id='c0'></div>
      <div id='c1'></div>
      <div id='c2'></div>)");

  Element* body = GetDocument().body();

  HeapVector<Member<Node>> expected_nodes;
  for (Node* child = NodeTraversal::FirstChild(*body); child;
       child = NodeTraversal::NextSibling(*child)) {
    expected_nodes.push_back(child);
  }

  HeapVector<Member<Node>> actual_nodes;
  for (Node& child : NodeTraversal::ChildrenOf(*body))
    actual_nodes.push_back(&child);

  EXPECT_EQ(expected_nodes, actual_nodes);
}

TEST_F(NodeTraversalTest, DescendantsOf) {
  SetupSampleHTML(R"(
      <div id='c0'>
        <div id='c00'></div>
        <div id='c01'></div>
      </div>
      <div id='c1'>
        <div id='c10'></div>
      </div>)");

  Element* body = GetDocument().body();

  HeapVector<Member<Node>> expected_nodes;
  for (Node* child = NodeTraversal::FirstChild(*body); child;
       child = NodeTraversal::Next(*child)) {
    expected_nodes.push_back(child);
  }

  HeapVector<Member<Node>> actual_nodes;
  for (Node& descendant : NodeTraversal::DescendantsOf(*body))
    actual_nodes.push_back(&descendant);

  EXPECT_EQ(expected_nodes, actual_nodes);
}

TEST_F(NodeTraversalTest, InclusiveDescendantsOf) {
  SetupSampleHTML(R"(
      <div id='c0'>
        <div id='c00'></div>
        <div id='c01'></div>
      </div>
      <div id='c1'>
        <div id='c10'></div>
      </div>)");

  Element* body = GetDocument().body();

  HeapVector<Member<Node>> expected_nodes;
  for (Node* child = body; child; child = NodeTraversal::Next(*child)) {
    expected_nodes.push_back(child);
  }

  HeapVector<Member<Node>> actual_nodes;
  for (Node& descendant : NodeTraversal::InclusiveDescendantsOf(*body))
    actual_nodes.push_back(&descendant);

  EXPECT_EQ(expected_nodes, actual_nodes);
}

}  // namespace node_traversal_test
}  // namespace blink
