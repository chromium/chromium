// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"

#include <memory>
#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// Gathers all the nodes in `traversal_range` and returns them as a
// `HeapVector`.
HeapVector<Member<Node>> GatherFromTraversalRange(auto traversal_range) {
  HeapVector<Member<Node>> result;
  for (Node& node : traversal_range) {
    result.push_back(&node);
  }
  return result;
}

class FlatTreeTraversalTest : public PageTestBase {
 public:
  FlatTreeTraversalTest() {}

 protected:
  // Sets `mainHTML` to BODY element with `innerHTML` property and attaches
  // shadow root to child with `shadowHTML`, then update distribution for
  // calling member functions in `FlatTreeTraversal`.
  void SetupSampleHTML(std::string_view main_html,
                       std::string_view shadow_html,
                       unsigned index);

  void SetupDocumentTree(std::string_view main_html);

  void AttachOpenShadowRoot(Element& shadow_host,
                            std::string_view shadow_inner_html);
};

void FlatTreeTraversalTest::SetupSampleHTML(std::string_view main_html,
                                            std::string_view shadow_html,
                                            unsigned index) {
  Element* body = GetDocument().body();
  body->setInnerHTML(String::FromUTF8(main_html));
  auto* shadow_host = To<Element>(NodeTraversal::ChildAt(*body, index));
  AttachOpenShadowRoot(*shadow_host, shadow_html);
}

void FlatTreeTraversalTest::SetupDocumentTree(std::string_view main_html) {
  Element* body = GetDocument().body();
  body->setInnerHTML(String::FromUTF8(main_html));
}

void FlatTreeTraversalTest::AttachOpenShadowRoot(
    Element& shadow_host,
    std::string_view shadow_inner_html) {
  ShadowRoot& shadow_root =
      shadow_host.AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML(String::FromUTF8(shadow_inner_html));
}

namespace {

void TestCommonAncestor(Node* expected_result,
                        const Node& node_a,
                        const Node& node_b) {
  Node* result1 = FlatTreeTraversal::CommonAncestor(node_a, node_b);
  EXPECT_EQ(expected_result, result1)
      << "commonAncestor(" << node_a.textContent() << ","
      << node_b.textContent() << ")";
  Node* result2 = FlatTreeTraversal::CommonAncestor(node_b, node_a);
  EXPECT_EQ(expected_result, result2)
      << "commonAncestor(" << node_b.textContent() << ","
      << node_a.textContent() << ")";
}

}  // namespace

// Test case for
//  - childAt
//  - countChildren
//  - hasChildren
//  - index
//  - isDescendantOf
TEST_F(FlatTreeTraversalTest, childAt) {
  const char* main_html =
      "<div id='m0'>"
      "<span slot='#m00' id='m00'>m00</span>"
      "<span slot='#m01' id='m01'>m01</span>"
      "</div>";
  const char* shadow_html =
      "<a id='s00'>s00</a>"
      "<slot name='#m01'></slot>"
      "<a id='s02'>s02</a>"
      "<a id='s03'><slot name='#m00'></slot></a>"
      "<a id='s04'>s04</a>";
  SetupSampleHTML(main_html, shadow_html, 0);

  Element* body = GetDocument().body();
  Element* m0 = body->QuerySelector(AtomicString("#m0"));
  Element* m00 = m0->QuerySelector(AtomicString("#m00"));
  Element* m01 = m0->QuerySelector(AtomicString("#m01"));

  Element* shadow_host = m0;
  ShadowRoot* shadow_root = shadow_host->OpenShadowRoot();
  Element* s00 = shadow_root->QuerySelector(AtomicString("#s00"));
  Element* s02 = shadow_root->QuerySelector(AtomicString("#s02"));
  Element* s03 = shadow_root->QuerySelector(AtomicString("#s03"));
  Element* s04 = shadow_root->QuerySelector(AtomicString("#s04"));

  const unsigned kNumberOfChildNodes = 5;
  Node* expected_child_nodes[5] = {s00, m01, s02, s03, s04};

  ASSERT_EQ(kNumberOfChildNodes,
            FlatTreeTraversal::CountChildren(*shadow_host));
  EXPECT_TRUE(FlatTreeTraversal::HasChildren(*shadow_host));

  for (unsigned index = 0; index < kNumberOfChildNodes; ++index) {
    Node* child = FlatTreeTraversal::ChildAt(*shadow_host, index);
    EXPECT_EQ(index, FlatTreeTraversal::Index(*child))
        << "FlatTreeTraversal::index(FlatTreeTraversal(*shadowHost, " << index
        << "))";
    EXPECT_TRUE(FlatTreeTraversal::IsDescendantOf(*child, *shadow_host))
        << "FlatTreeTraversal::isDescendantOf(*FlatTreeTraversal(*"
           "shadowHost, "
        << index << "), *shadowHost)";
    bool is_slot_element = IsA<HTMLSlotElement>(child);
    if (is_slot_element) {
      child = FlatTreeTraversal::FirstChild(*child);
    }
    EXPECT_EQ(expected_child_nodes[index], child)
        << "FlatTreeTraversal::childAt(*shadowHost, " << index << ")";
    EXPECT_EQ(is_slot_element ? 0 : index, FlatTreeTraversal::Index(*child))
        << "FlatTreeTraversal::index(FlatTreeTraversal(*shadowHost, " << index
        << "))";
  }
  EXPECT_EQ(nullptr,
            FlatTreeTraversal::ChildAt(*shadow_host, kNumberOfChildNodes + 1))
      << "Out of bounds childAt() returns nullptr.";

  // Distributed node |m00| is child of slot in shadow tree |s03|.
  EXPECT_EQ(
      m00, FlatTreeTraversal::FirstChild(*FlatTreeTraversal::FirstChild(*s03)));
}

TEST_F(FlatTreeTraversalTest, DescendantsOf) {
  std::string_view main_html =
      R"(<div id='m0'>
        <span slot='#m00' id='m00'>m00</span>
        <span slot='#m01' id='m01'>m01</span>
      </div>)";
  std::string_view shadow_html =
      R"(<a id='s00'>s00</a>
      <slot name='#m01'></slot>
      <a id='s02'>s02</a>
      <a id='s03'>
        <slot name='#m00'></slot>
      </a>
      <a id='s04'>s04</a>)";
  SetupSampleHTML(main_html, shadow_html, 0);

  Element* body = GetDocument().body();
  Element* m0 = body->QuerySelector(AtomicString("#m0"));
  Element* shadow_host = m0;
  ShadowRoot* shadow_root = shadow_host->OpenShadowRoot();
  Element* s03 = shadow_root->QuerySelector(AtomicString("#s03"));

  {
    HeapVector<Member<Node>> expected_nodes;
    for (Node* child = FlatTreeTraversal::FirstChild(*body); child;
         child = FlatTreeTraversal::Next(*child)) {
      expected_nodes.push_back(child);
    }
    EXPECT_EQ(expected_nodes, GatherFromTraversalRange(
                                  FlatTreeTraversal::DescendantsOf(*body)));

    expected_nodes.push_front(body);
    EXPECT_EQ(expected_nodes,
              GatherFromTraversalRange(
                  FlatTreeTraversal::InclusiveDescendantsOf(*body)));
  }

  // Traversal of descendants of a node that is not the root node stays within
  // the sub tree.
  {
    HeapVector<Member<Node>> expected_nodes;
    for (Node* child = FlatTreeTraversal::FirstChild(*s03); child;
         child = FlatTreeTraversal::Next(*child, /*stay_within=*/s03)) {
      expected_nodes.push_back(child);
    }
    EXPECT_EQ(expected_nodes,
              GatherFromTraversalRange(FlatTreeTraversal::DescendantsOf(*s03)));

    expected_nodes.push_front(s03);
    EXPECT_EQ(expected_nodes,
              GatherFromTraversalRange(
                  FlatTreeTraversal::InclusiveDescendantsOf(*s03)));
  }
}

TEST_F(FlatTreeTraversalTest, StartsAtOrAfter) {
  std::string_view main_html =
      R"(<div id='m0'>
        <span slot='#m00' id='m00'>m00</span>
        <span slot='#m01' id='m01'>m01</span>
      </div>)";
  std::string_view shadow_html =
      R"(<a id='s00'>s00</a>
      <slot name='#m01'></slot>
      <a id='s02'>s02</a>
      <a id='s03'>
        <slot name='#m00'></slot>
      </a>
      <a id='s04'>s04</a>)";
  SetupSampleHTML(main_html, shadow_html, 0);

  Element* body = GetDocument().body();
  Element* m0 = body->QuerySelector(AtomicString("#m0"));
  Element* shadow_host = m0;
  ShadowRoot* shadow_root = shadow_host->OpenShadowRoot();
  Element* s03 = shadow_root->QuerySelector(AtomicString("#s03"));

  HeapVector<Member<Node>> expected_nodes;
  for (Node* child = FlatTreeTraversal::Next(*s03); child;
       child = FlatTreeTraversal::Next(*child)) {
    expected_nodes.push_back(child);
  }
  EXPECT_EQ(expected_nodes,
            GatherFromTraversalRange(FlatTreeTraversal::StartsAfter(*s03)));

  expected_nodes.push_front(*s03);
  EXPECT_EQ(expected_nodes,
            GatherFromTraversalRange(FlatTreeTraversal::StartsAt(*s03)));
}

TEST_F(FlatTreeTraversalTest, ChildrenOf) {
  SetupSampleHTML(
      "<p id=sample>ZERO<span slot=three>three</b><span "
      "slot=one>one</b>FOUR</p>",
      "zero<slot name=one></slot>two<slot name=three></slot>four", 0);
  Element* const sample = GetDocument().getElementById(AtomicString("sample"));

  HeapVector<Member<Node>> expected_nodes;
  for (Node* runner = FlatTreeTraversal::FirstChild(*sample); runner;
       runner = FlatTreeTraversal::NextSibling(*runner)) {
    expected_nodes.push_back(runner);
  }

  HeapVector<Member<Node>> actual_nodes;
  for (Node& child : FlatTreeTraversal::ChildrenOf(*sample))
    actual_nodes.push_back(&child);

  EXPECT_EQ(expected_nodes, actual_nodes);
}

// Test case for
//  - commonAncestor
//  - isDescendantOf
TEST_F(FlatTreeTraversalTest, commonAncestor) {
  // We build following flat tree:
  //             ____BODY___
  //             |    |     |
  //            m0    m1    m2       m1 is shadow host having m10, m11, m12.
  //            _|_   |   __|__
  //           |   |  |   |    |
  //          m00 m01 |   m20 m21
  //             _____|_____________
  //             |  |   |    |     |
  //            s10 s11 s12 s13  s14
  //                         |
  //                       __|__
  //                |      |    |
  //                m12    m10 m11 <-- distributed
  // where: each symbol consists with prefix, child index, child-child index.
  //  prefix "m" means node in main tree,
  //  prefix "d" means node in main tree and distributed
  //  prefix "s" means node in shadow tree
  const char* main_html =
      "<a id='m0'><b id='m00'>m00</b><b id='m01'>m01</b></a>"
      "<div id='m1'>"
      "<b slot='#m10' id='m10'>m10</b>"
      "<b slot='#m11' id='m11'>m11</b>"
      "<b slot='#m12' id='m12'>m12</b>"
      "</div>"
      "<a id='m2'><b id='m20'>m20</b><b id='m21'>m21</b></a>";
  const char* shadow_html =
      "<a id='s10'>s10</a>"
      "<a id='s11'><slot name='#m12'></slot></a>"
      "<a id='s12'>s12</a>"
      "<a id='s13'>"
      "<slot name='#m10'></slot>"
      "<slot name='#m11'></slot>"
      "</a>"
      "<a id='s14'>s14</a>";
  SetupSampleHTML(main_html, shadow_html, 1);
  Element* body = GetDocument().body();
  Element* m0 = body->QuerySelector(AtomicString("#m0"));
  Element* m1 = body->QuerySelector(AtomicString("#m1"));
  Element* m2 = body->QuerySelector(AtomicString("#m2"));

  Element* m00 = body->QuerySelector(AtomicString("#m00"));
  Element* m01 = body->QuerySelector(AtomicString("#m01"));
  Element* m10 = body->QuerySelector(AtomicString("#m10"));
  Element* m11 = body->QuerySelector(AtomicString("#m11"));
  Element* m12 = body->QuerySelector(AtomicString("#m12"));
  Element* m20 = body->QuerySelector(AtomicString("#m20"));
  Element* m21 = body->QuerySelector(AtomicString("#m21"));

  ShadowRoot* shadow_root = m1->OpenShadowRoot();
  Element* s10 = shadow_root->QuerySelector(AtomicString("#s10"));
  Element* s11 = shadow_root->QuerySelector(AtomicString("#s11"));
  Element* s12 = shadow_root->QuerySelector(AtomicString("#s12"));
  Element* s13 = shadow_root->QuerySelector(AtomicString("#s13"));
  Element* s14 = shadow_root->QuerySelector(AtomicString("#s14"));

  TestCommonAncestor(body, *m0, *m1);
  TestCommonAncestor(body, *m1, *m2);
  TestCommonAncestor(body, *m1, *m20);
  TestCommonAncestor(body, *s14, *m21);

  TestCommonAncestor(m0, *m0, *m0);
  TestCommonAncestor(m0, *m00, *m01);

  TestCommonAncestor(m1, *m1, *m1);
  TestCommonAncestor(m1, *s10, *s14);
  TestCommonAncestor(m1, *s10, *m12);
  TestCommonAncestor(m1, *s12, *m12);
  TestCommonAncestor(m1, *m10, *m12);

  TestCommonAncestor(m01, *m01, *m01);
  TestCommonAncestor(s11, *s11, *m12);
  TestCommonAncestor(s13, *m10, *m11);

  s12->remove(ASSERT_NO_EXCEPTION);
  TestCommonAncestor(s12, *s12, *s12);
  TestCommonAncestor(nullptr, *s12, *s11);
  TestCommonAncestor(nullptr, *s12, *m01);
  TestCommonAncestor(nullptr, *s12, *m20);

  m20->remove(ASSERT_NO_EXCEPTION);
  TestCommonAncestor(m20, *m20, *m20);
  TestCommonAncestor(nullptr, *m20, *s12);
  TestCommonAncestor(nullptr, *m20, *m1);
}

// Test case for
//  - NextSkippingChildren
//  - PreviousAbsoluteSibling
TEST_F(FlatTreeTraversalTest, SkippingChildrenFunctions) {
  const char* main_html =
      "<div id='m0'>m0</div>"
      "<div id='m1'>"
      "<span slot='#m10' id='m10'>m10</span>"
      "<span slot='#m11' id='m11'>m11</span>"
      "</div>"
      "<div id='m2'>m2</div>";
  const char* shadow_html =
      "<slot name='#m11'></slot>"
      "<a id='s11'>s11</a>"
      "<a id='s12'>"
      "<b id='s120'>s120</b>"
      "<slot name='#m10'></slot>"
      "</a>";
  SetupSampleHTML(main_html, shadow_html, 1);

  Element* body = GetDocument().body();
  Element* m0 = body->QuerySelector(AtomicString("#m0"));
  Element* m1 = body->QuerySelector(AtomicString("#m1"));
  Element* m2 = body->QuerySelector(AtomicString("#m2"));

  Element* m10 = body->QuerySelector(AtomicString("#m10"));
  Element* m10_slot_parent = To<Element>(FlatTreeTraversal::Parent(*m10));
  Element* m11 = body->QuerySelector(AtomicString("#m11"));
  Element* m11_slot_parent = To<Element>(FlatTreeTraversal::Parent(*m11));

  ShadowRoot* shadow_root = m1->OpenShadowRoot();
  Element* s11 = shadow_root->QuerySelector(AtomicString("#s11"));
  Element* s12 = shadow_root->QuerySelector(AtomicString("#s12"));
  Element* s120 = shadow_root->QuerySelector(AtomicString("#s120"));

  // Main tree node to main tree node
  EXPECT_EQ(*m1, FlatTreeTraversal::NextSkippingChildren(*m0));
  EXPECT_EQ(*m0, FlatTreeTraversal::PreviousAbsoluteSibling(*m1));

  // Distribute node to main tree node
  EXPECT_EQ(*m2, FlatTreeTraversal::NextSkippingChildren(*m10));
  EXPECT_EQ(*m1, FlatTreeTraversal::PreviousAbsoluteSibling(*m2));

  // Distribute node to node in shadow tree
  EXPECT_EQ(*s11, FlatTreeTraversal::NextSkippingChildren(*m11));
  EXPECT_EQ(*m11_slot_parent, FlatTreeTraversal::PreviousAbsoluteSibling(*s11));

  // Node in shadow tree to distributed node
  EXPECT_EQ(*s11, FlatTreeTraversal::NextSkippingChildren(*m11));
  EXPECT_EQ(*m11_slot_parent, FlatTreeTraversal::PreviousAbsoluteSibling(*s11));

  EXPECT_EQ(*m10_slot_parent, FlatTreeTraversal::NextSkippingChildren(*s120));
  EXPECT_EQ(*s120, FlatTreeTraversal::PreviousAbsoluteSibling(*m10));

  // Node in shadow tree to main tree
  EXPECT_EQ(*m2, FlatTreeTraversal::NextSkippingChildren(*s12));
  EXPECT_EQ(*m1, FlatTreeTraversal::PreviousAbsoluteSibling(*m2));
}

TEST_F(FlatTreeTraversalTest, AncestorsOf) {
  SetupDocumentTree("<div><div><div id=sample></div></div></div>");
  Element* const sample = GetDocument().getElementById(AtomicString("sample"));

  HeapVector<Member<Node>> expected_nodes;
  for (Node* parent = FlatTreeTraversal::Parent(*sample); parent;
       parent = FlatTreeTraversal::Parent(*parent)) {
    expected_nodes.push_back(parent);
  }

  HeapVector<Member<Node>> actual_nodes;
  for (Node& ancestor : FlatTreeTraversal::AncestorsOf(*sample))
    actual_nodes.push_back(&ancestor);

  EXPECT_EQ(expected_nodes, actual_nodes);
}

TEST_F(FlatTreeTraversalTest, InclusiveAncestorsOf) {
  SetupDocumentTree("<div><div><div id=sample></div></div></div>");
  Element* const sample = GetDocument().getElementById(AtomicString("sample"));

  HeapVector<Member<Node>> expected_nodes;
  for (Node* parent = sample; parent;
       parent = FlatTreeTraversal::Parent(*parent)) {
    expected_nodes.push_back(parent);
  }

  HeapVector<Member<Node>> actual_nodes;
  for (Node& ancestor : FlatTreeTraversal::InclusiveAncestorsOf(*sample))
    actual_nodes.push_back(&ancestor);

  EXPECT_EQ(expected_nodes, actual_nodes);
}

// Test case for
//  - lastWithin
//  - lastWithinOrSelf
TEST_F(FlatTreeTraversalTest, lastWithin) {
  const char* main_html =
      "<div id='m0'>m0</div>"
      "<div id='m1'>"
      "<span slot='#m10' id='m10'>m10</span>"
      "<span slot='#m11' id='m11'>m11</span>"
      "<span id='m12'>m12</span>"  // #m12 is not distributed.
      "</div>"
      "<div id='m2'></div>";
  const char* shadow_html =
      "<slot name='#m11'></slot>"
      "<a id='s11'>s11</a>"
      "<a id='s12'>"
      "<slot name='#m10'></slot>"
      "</a>";
  SetupSampleHTML(main_html, shadow_html, 1);

  Element* body = GetDocument().body();
  Element* m0 = body->QuerySelector(AtomicString("#m0"));
  Element* m1 = body->QuerySelector(AtomicString("#m1"));
  Element* m2 = body->QuerySelector(AtomicString("#m2"));

  Element* m10 = body->QuerySelector(AtomicString("#m10"));

  ShadowRoot* shadow_root = m1->OpenShadowRoot();
  Element* s11 = shadow_root->QuerySelector(AtomicString("#s11"));
  Element* s12 = shadow_root->QuerySelector(AtomicString("#s12"));

  EXPECT_EQ(m0->firstChild(), FlatTreeTraversal::LastWithin(*m0));
  EXPECT_EQ(*m0->firstChild(), FlatTreeTraversal::LastWithinOrSelf(*m0));

  EXPECT_EQ(m10->firstChild(), FlatTreeTraversal::LastWithin(*m1));
  EXPECT_EQ(*m10->firstChild(), FlatTreeTraversal::LastWithinOrSelf(*m1));

  EXPECT_EQ(nullptr, FlatTreeTraversal::LastWithin(*m2));
  EXPECT_EQ(*m2, FlatTreeTraversal::LastWithinOrSelf(*m2));

  EXPECT_EQ(s11->firstChild(), FlatTreeTraversal::LastWithin(*s11));
  EXPECT_EQ(*s11->firstChild(), FlatTreeTraversal::LastWithinOrSelf(*s11));

  EXPECT_EQ(m10->firstChild(), FlatTreeTraversal::LastWithin(*s12));
  EXPECT_EQ(*m10->firstChild(), FlatTreeTraversal::LastWithinOrSelf(*s12));
}

TEST_F(FlatTreeTraversalTest, previousPostOrder) {
  const char* main_html =
      "<div id='m0'>m0</div>"
      "<div id='m1'>"
      "<span slot='#m10' id='m10'>m10</span>"
      "<span slot='#m11' id='m11'>m11</span>"
      "</div>"
      "<div id='m2'>m2</div>";
  const char* shadow_html =
      "<slot name='#m11'></slot>"
      "<a id='s11'>s11</a>"
      "<a id='s12'>"
      "<b id='s120'>s120</b>"
      "<slot name='#m10'></slot>"
      "</a>";
  SetupSampleHTML(main_html, shadow_html, 1);

  Element* body = GetDocument().body();
  Element* m0 = body->QuerySelector(AtomicString("#m0"));
  Element* m1 = body->QuerySelector(AtomicString("#m1"));
  Element* m2 = body->QuerySelector(AtomicString("#m2"));

  Element* m10 = body->QuerySelector(AtomicString("#m10"));
  Element* m10_slot_parent = To<Element>(FlatTreeTraversal::Parent(*m10));
  Element* m11 = body->QuerySelector(AtomicString("#m11"));

  ShadowRoot* shadow_root = m1->OpenShadowRoot();
  Element* s11 = shadow_root->QuerySelector(AtomicString("#s11"));
  Element* s12 = shadow_root->QuerySelector(AtomicString("#s12"));
  Element* s120 = shadow_root->QuerySelector(AtomicString("#s120"));

  EXPECT_EQ(*m0->firstChild(), FlatTreeTraversal::PreviousPostOrder(*m0));
  EXPECT_EQ(*s12, FlatTreeTraversal::PreviousPostOrder(*m1));
  EXPECT_EQ(*m10->firstChild(), FlatTreeTraversal::PreviousPostOrder(*m10));
  EXPECT_EQ(*s120, FlatTreeTraversal::PreviousPostOrder(*m10->firstChild()));
  EXPECT_EQ(*s120,
            FlatTreeTraversal::PreviousPostOrder(*m10->firstChild(), s12));
  EXPECT_EQ(*m11->firstChild(), FlatTreeTraversal::PreviousPostOrder(*m11));
  EXPECT_EQ(*m0, FlatTreeTraversal::PreviousPostOrder(*m11->firstChild()));
  EXPECT_EQ(nullptr,
            FlatTreeTraversal::PreviousPostOrder(*m11->firstChild(), m11));
  EXPECT_EQ(*m2->firstChild(), FlatTreeTraversal::PreviousPostOrder(*m2));

  EXPECT_EQ(*s11->firstChild(), FlatTreeTraversal::PreviousPostOrder(*s11));
  EXPECT_EQ(*m10_slot_parent, FlatTreeTraversal::PreviousPostOrder(*s12));
  EXPECT_EQ(*s120->firstChild(), FlatTreeTraversal::PreviousPostOrder(*s120));
  EXPECT_EQ(*s11, FlatTreeTraversal::PreviousPostOrder(*s120->firstChild()));
  EXPECT_EQ(nullptr,
            FlatTreeTraversal::PreviousPostOrder(*s120->firstChild(), s12));
}

TEST_F(FlatTreeTraversalTest, nextSiblingNotInDocumentFlatTree) {
  const char* main_html =
      "<div id='m0'>m0</div>"
      "<div id='m1'>"
      "<span id='m10'>m10</span>"
      "<span id='m11'>m11</span>"
      "</div>"
      "<div id='m2'>m2</div>";
  const char* shadow_html = "<content select='#m11'></content>";
  SetupSampleHTML(main_html, shadow_html, 1);

  Element* body = GetDocument().body();
  Element* m10 = body->QuerySelector(AtomicString("#m10"));

  EXPECT_EQ(nullptr, FlatTreeTraversal::NextSibling(*m10));
  EXPECT_EQ(nullptr, FlatTreeTraversal::PreviousSibling(*m10));
}


TEST_F(FlatTreeTraversalTest, v1Simple) {
  const char* main_html =
      "<div id='host'>"
      "<div id='child1' slot='slot1'></div>"
      "<div id='child2' slot='slot2'></div>"
      "</div>";
  const char* shadow_html =
      "<div id='shadow-child1'></div>"
      "<slot name='slot1'></slot>"
      "<slot name='slot2'></slot>"
      "<div id='shadow-child2'></div>";

  SetupDocumentTree(main_html);
  Element* body = GetDocument().body();
  Element* host = body->QuerySelector(AtomicString("#host"));
  Element* child1 = body->QuerySelector(AtomicString("#child1"));
  Element* child2 = body->QuerySelector(AtomicString("#child2"));

  AttachOpenShadowRoot(*host, shadow_html);
  ShadowRoot* shadow_root = host->OpenShadowRoot();
  Element* slot1 = shadow_root->QuerySelector(AtomicString("[name=slot1]"));
  Element* slot2 = shadow_root->QuerySelector(AtomicString("[name=slot2]"));
  Element* shadow_child1 =
      shadow_root->QuerySelector(AtomicString("#shadow-child1"));
  Element* shadow_child2 =
      shadow_root->QuerySelector(AtomicString("#shadow-child2"));

  EXPECT_TRUE(slot1);
  EXPECT_TRUE(slot2);
  EXPECT_EQ(shadow_child1, FlatTreeTraversal::FirstChild(*host));
  EXPECT_EQ(slot1, FlatTreeTraversal::NextSibling(*shadow_child1));
  EXPECT_EQ(nullptr, FlatTreeTraversal::NextSibling(*child1));
  EXPECT_EQ(nullptr, FlatTreeTraversal::NextSibling(*child2));
  EXPECT_EQ(slot2, FlatTreeTraversal::NextSibling(*slot1));
  EXPECT_EQ(shadow_child2, FlatTreeTraversal::NextSibling(*slot2));
}

TEST_F(FlatTreeTraversalTest, v1Redistribution) {
  // composed tree:
  // d1
  // ├──/shadow-root
  // │   └── d1-1
  // │       ├──/shadow-root
  // │       │   ├── d1-1-1
  // │       │   ├── slot name=d1-1-s1
  // │       │   ├── slot name=d1-1-s2
  // │       │   └── d1-1-2
  // │       ├── d1-2
  // │       ├── slot id=d1-s0
  // │       ├── slot name=d1-s1 slot=d1-1-s1
  // │       ├── slot name=d1-s2
  // │       ├── d1-3
  // │       └── d1-4 slot=d1-1-s1
  // ├── d2 slot=d1-s1
  // ├── d3 slot=d1-s2
  // ├── d4 slot=nonexistent
  // └── d5

  // flat tree:
  // d1
  // └── d1-1
  //     ├── d1-1-1
  //     ├── slot name=d1-1-s1
  //     │   ├── slot name=d1-s1 slot=d1-1-s1
  //     │   │   └── d2 slot=d1-s1
  //     │   └── d1-4 slot=d1-1-s1
  //     ├── slot name=d1-1-s2
  //     └── d1-1-2
  const char* main_html =
      "<div id='d1'>"
      "<div id='d2' slot='d1-s1'></div>"
      "<div id='d3' slot='d1-s2'></div>"
      "<div id='d4' slot='nonexistent'></div>"
      "<div id='d5'></div>"
      "</div>"
      "<div id='d6'></div>";
  const char* shadow_html1 =
      "<div id='d1-1'>"
      "<div id='d1-2'></div>"
      "<slot id='d1-s0'></slot>"
      "<slot name='d1-s1' slot='d1-1-s1'></slot>"
      "<slot name='d1-s2'></slot>"
      "<div id='d1-3'></div>"
      "<div id='d1-4' slot='d1-1-s1'></div>"
      "</div>";
  const char* shadow_html2 =
      "<div id='d1-1-1'></div>"
      "<slot name='d1-1-s1'></slot>"
      "<slot name='d1-1-s2'></slot>"
      "<div id='d1-1-2'></div>";

  SetupDocumentTree(main_html);

  Element* body = GetDocument().body();
  Element* d1 = body->QuerySelector(AtomicString("#d1"));
  Element* d2 = body->QuerySelector(AtomicString("#d2"));
  Element* d3 = body->QuerySelector(AtomicString("#d3"));
  Element* d4 = body->QuerySelector(AtomicString("#d4"));
  Element* d5 = body->QuerySelector(AtomicString("#d5"));
  Element* d6 = body->QuerySelector(AtomicString("#d6"));

  AttachOpenShadowRoot(*d1, shadow_html1);
  ShadowRoot* shadow_root1 = d1->OpenShadowRoot();
  Element* d11 = shadow_root1->QuerySelector(AtomicString("#d1-1"));
  Element* d12 = shadow_root1->QuerySelector(AtomicString("#d1-2"));
  Element* d13 = shadow_root1->QuerySelector(AtomicString("#d1-3"));
  Element* d14 = shadow_root1->QuerySelector(AtomicString("#d1-4"));
  Element* d1s0 = shadow_root1->QuerySelector(AtomicString("#d1-s0"));
  Element* d1s1 = shadow_root1->QuerySelector(AtomicString("[name=d1-s1]"));
  Element* d1s2 = shadow_root1->QuerySelector(AtomicString("[name=d1-s2]"));

  AttachOpenShadowRoot(*d11, shadow_html2);
  ShadowRoot* shadow_root2 = d11->OpenShadowRoot();
  Element* d111 = shadow_root2->QuerySelector(AtomicString("#d1-1-1"));
  Element* d112 = shadow_root2->QuerySelector(AtomicString("#d1-1-2"));
  Element* d11s1 = shadow_root2->QuerySelector(AtomicString("[name=d1-1-s1]"));
  Element* d11s2 = shadow_root2->QuerySelector(AtomicString("[name=d1-1-s2]"));

  EXPECT_TRUE(d5);
  EXPECT_TRUE(d12);
  EXPECT_TRUE(d13);
  EXPECT_TRUE(d1s0);
  EXPECT_TRUE(d1s1);
  EXPECT_TRUE(d1s2);
  EXPECT_TRUE(d11s1);
  EXPECT_TRUE(d11s2);

  EXPECT_EQ(d11, FlatTreeTraversal::Next(*d1));
  EXPECT_EQ(d111, FlatTreeTraversal::Next(*d11));
  EXPECT_EQ(d11s1, FlatTreeTraversal::Next(*d111));
  EXPECT_EQ(d1s1, FlatTreeTraversal::Next(*d11s1));
  EXPECT_EQ(d2, FlatTreeTraversal::Next(*d1s1));
  EXPECT_EQ(d14, FlatTreeTraversal::Next(*d2));
  EXPECT_EQ(d11s2, FlatTreeTraversal::Next(*d14));
  EXPECT_EQ(d112, FlatTreeTraversal::Next(*d11s2));
  EXPECT_EQ(d6, FlatTreeTraversal::Next(*d112));

  EXPECT_EQ(d112, FlatTreeTraversal::Previous(*d6));

  EXPECT_EQ(d11, FlatTreeTraversal::Parent(*d111));
  EXPECT_EQ(d11, FlatTreeTraversal::Parent(*d112));
  EXPECT_EQ(d1s1, FlatTreeTraversal::Parent(*d2));
  EXPECT_EQ(d11s1, FlatTreeTraversal::Parent(*d14));
  EXPECT_EQ(d1s2, FlatTreeTraversal::Parent(*d3));
  EXPECT_EQ(nullptr, FlatTreeTraversal::Parent(*d4));
}

TEST_F(FlatTreeTraversalTest, v1SlotInDocumentTree) {
  const char* main_html =
      "<div id='parent'>"
      "<slot>"
      "<div id='child1'></div>"
      "<div id='child2'></div>"
      "</slot>"
      "</div>";

  SetupDocumentTree(main_html);
  Element* body = GetDocument().body();
  Element* parent = body->QuerySelector(AtomicString("#parent"));
  Element* slot = body->QuerySelector(AtomicString("slot"));
  Element* child1 = body->QuerySelector(AtomicString("#child1"));
  Element* child2 = body->QuerySelector(AtomicString("#child2"));

  EXPECT_EQ(slot, FlatTreeTraversal::FirstChild(*parent));
  EXPECT_EQ(child1, FlatTreeTraversal::FirstChild(*slot));
  EXPECT_EQ(child2, FlatTreeTraversal::NextSibling(*child1));
  EXPECT_EQ(nullptr, FlatTreeTraversal::NextSibling(*child2));
  EXPECT_EQ(slot, FlatTreeTraversal::Parent(*child1));
  EXPECT_EQ(slot, FlatTreeTraversal::Parent(*child2));
  EXPECT_EQ(parent, FlatTreeTraversal::Parent(*slot));
}

TEST_F(FlatTreeTraversalTest, v1FallbackContent) {
  const char* main_html = "<div id='d1'></div>";
  const char* shadow_html =
      "<div id='before'></div>"
      "<slot><p>fallback content</p></slot>"
      "<div id='after'></div>";

  SetupDocumentTree(main_html);

  Element* body = GetDocument().body();
  Element* d1 = body->QuerySelector(AtomicString("#d1"));

  AttachOpenShadowRoot(*d1, shadow_html);
  ShadowRoot* shadow_root = d1->OpenShadowRoot();
  Element* before = shadow_root->QuerySelector(AtomicString("#before"));
  Element* after = shadow_root->QuerySelector(AtomicString("#after"));
  Element* fallback_content = shadow_root->QuerySelector(AtomicString("p"));
  Element* slot = shadow_root->QuerySelector(AtomicString("slot"));

  EXPECT_EQ(before, FlatTreeTraversal::FirstChild(*d1));
  EXPECT_EQ(after, FlatTreeTraversal::LastChild(*d1));
  EXPECT_EQ(slot, FlatTreeTraversal::Parent(*fallback_content));

  EXPECT_EQ(slot, FlatTreeTraversal::NextSibling(*before));
  EXPECT_EQ(after, FlatTreeTraversal::NextSibling(*slot));
  EXPECT_EQ(nullptr, FlatTreeTraversal::NextSibling(*fallback_content));
  EXPECT_EQ(nullptr, FlatTreeTraversal::NextSibling(*after));

  EXPECT_EQ(slot, FlatTreeTraversal::PreviousSibling(*after));
  EXPECT_EQ(before, FlatTreeTraversal::PreviousSibling(*slot));
  EXPECT_EQ(nullptr, FlatTreeTraversal::PreviousSibling(*fallback_content));
  EXPECT_EQ(nullptr, FlatTreeTraversal::PreviousSibling(*before));
}

TEST_F(FlatTreeTraversalTest, v1FallbackContentSkippedInTraversal) {
  const char* main_html = "<div id='d1'><span></span></div>";
  const char* shadow_html =
      "<div id='before'></div>"
      "<slot><p>fallback content</p></slot>"
      "<div id='after'></div>";

  SetupDocumentTree(main_html);

  Element* body = GetDocument().body();
  Element* d1 = body->QuerySelector(AtomicString("#d1"));
  Element* span = body->QuerySelector(AtomicString("span"));

  AttachOpenShadowRoot(*d1, shadow_html);
  ShadowRoot* shadow_root = d1->OpenShadowRoot();
  Element* before = shadow_root->QuerySelector(AtomicString("#before"));
  Element* after = shadow_root->QuerySelector(AtomicString("#after"));
  Element* fallback_content = shadow_root->QuerySelector(AtomicString("p"));
  Element* slot = shadow_root->QuerySelector(AtomicString("slot"));

  EXPECT_EQ(before, FlatTreeTraversal::FirstChild(*d1));
  EXPECT_EQ(after, FlatTreeTraversal::LastChild(*d1));
  EXPECT_EQ(slot, FlatTreeTraversal::Parent(*span));
  EXPECT_EQ(d1, FlatTreeTraversal::Parent(*slot));

  EXPECT_EQ(slot, FlatTreeTraversal::NextSibling(*before));
  EXPECT_EQ(after, FlatTreeTraversal::NextSibling(*slot));
  EXPECT_EQ(nullptr, FlatTreeTraversal::NextSibling(*after));

  EXPECT_EQ(slot, FlatTreeTraversal::PreviousSibling(*after));
  EXPECT_EQ(before, FlatTreeTraversal::PreviousSibling(*slot));
  EXPECT_EQ(nullptr, FlatTreeTraversal::PreviousSibling(*before));

  EXPECT_EQ(nullptr, FlatTreeTraversal::Parent(*fallback_content));
  EXPECT_EQ(nullptr, FlatTreeTraversal::NextSibling(*fallback_content));
  EXPECT_EQ(nullptr, FlatTreeTraversal::PreviousSibling(*fallback_content));
}

TEST_F(FlatTreeTraversalTest, v1AllFallbackContent) {
  const char* main_html = "<div id='d1'></div>";
  const char* shadow_html =
      "<slot name='a'><p id='x'>fallback content X</p></slot>"
      "<slot name='b'><p id='y'>fallback content Y</p></slot>"
      "<slot name='c'><p id='z'>fallback content Z</p></slot>";

  SetupDocumentTree(main_html);

  Element* body = GetDocument().body();
  Element* d1 = body->QuerySelector(AtomicString("#d1"));

  AttachOpenShadowRoot(*d1, shadow_html);
  ShadowRoot* shadow_root = d1->OpenShadowRoot();
  Element* slot_a = shadow_root->QuerySelector(AtomicString("slot[name=a]"));
  Element* slot_b = shadow_root->QuerySelector(AtomicString("slot[name=b]"));
  Element* slot_c = shadow_root->QuerySelector(AtomicString("slot[name=c]"));
  Element* fallback_x = shadow_root->QuerySelector(AtomicString("#x"));
  Element* fallback_y = shadow_root->QuerySelector(AtomicString("#y"));
  Element* fallback_z = shadow_root->QuerySelector(AtomicString("#z"));

  EXPECT_EQ(slot_a, FlatTreeTraversal::FirstChild(*d1));
  EXPECT_EQ(slot_c, FlatTreeTraversal::LastChild(*d1));

  EXPECT_EQ(fallback_x, FlatTreeTraversal::FirstChild(*slot_a));
  EXPECT_EQ(fallback_y, FlatTreeTraversal::FirstChild(*slot_b));
  EXPECT_EQ(fallback_z, FlatTreeTraversal::FirstChild(*slot_c));

  EXPECT_EQ(slot_a, FlatTreeTraversal::Parent(*fallback_x));
  EXPECT_EQ(slot_b, FlatTreeTraversal::Parent(*fallback_y));
  EXPECT_EQ(slot_c, FlatTreeTraversal::Parent(*fallback_z));
  EXPECT_EQ(d1, FlatTreeTraversal::Parent(*slot_a));

  EXPECT_EQ(nullptr, FlatTreeTraversal::NextSibling(*fallback_x));
  EXPECT_EQ(nullptr, FlatTreeTraversal::NextSibling(*fallback_y));
  EXPECT_EQ(nullptr, FlatTreeTraversal::NextSibling(*fallback_z));

  EXPECT_EQ(nullptr, FlatTreeTraversal::PreviousSibling(*fallback_z));
  EXPECT_EQ(nullptr, FlatTreeTraversal::PreviousSibling(*fallback_y));
  EXPECT_EQ(nullptr, FlatTreeTraversal::PreviousSibling(*fallback_x));
}

}  // namespace
}  // namespace blink
