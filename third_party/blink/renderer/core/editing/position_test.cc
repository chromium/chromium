// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/position.h"

#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

class PositionTest : public EditingTestBase {};

TEST_F(PositionTest, AtStartOfTreeWithAfterChildren) {
  ASSERT_TRUE(GetDocument().firstChild()) << "We should have <html>.";
  EXPECT_TRUE(Position(GetDocument(), 0).AtStartOfTree());
  EXPECT_FALSE(Position::LastPositionInNode(GetDocument()).AtStartOfTree());
}

TEST_F(PositionTest, IsEquivalent) {
  SetBodyContent("<a id=sample>0<b>1</b>2</a>");

  Element* sample = GetDocument().getElementById(AtomicString("sample"));

  EXPECT_TRUE(Position(sample, 0).IsEquivalent(Position(sample, 0)));

  EXPECT_TRUE(
      Position(sample, 0).IsEquivalent(Position::FirstPositionInNode(*sample)));
  EXPECT_TRUE(Position(sample, 0).IsEquivalent(
      Position::BeforeNode(*sample->firstChild())));
  EXPECT_TRUE(Position(sample, 1).IsEquivalent(
      Position::AfterNode(*sample->firstChild())));
  EXPECT_TRUE(Position(sample, 1).IsEquivalent(
      Position::BeforeNode(*sample->firstChild()->nextSibling())));
  EXPECT_TRUE(Position(sample, 2).IsEquivalent(
      Position::BeforeNode(*sample->lastChild())));
  EXPECT_TRUE(Position(sample, 3).IsEquivalent(
      Position::AfterNode(*sample->lastChild())));
  EXPECT_TRUE(
      Position(sample, 3).IsEquivalent(Position::LastPositionInNode(*sample)));

  EXPECT_FALSE(Position(sample, 0).IsEquivalent(Position(sample, 1)));
  EXPECT_FALSE(
      Position(sample, 0).IsEquivalent(Position::LastPositionInNode(*sample)));
}

TEST_F(PositionTest, NodeAsRangeLastNodeNull) {
  EXPECT_EQ(nullptr, Position().NodeAsRangeLastNode());
  EXPECT_EQ(nullptr, PositionInFlatTree().NodeAsRangeLastNode());
}

TEST_F(PositionTest, editingPositionOfWithEditingIgnoresContent) {
  const char* body_content =
      "<textarea id=textarea></textarea><a id=child1>1</a><b id=child2>2</b>";
  SetBodyContent(body_content);
  Node* textarea = GetDocument().getElementById(AtomicString("textarea"));

  EXPECT_EQ(Position::BeforeNode(*textarea),
            Position::EditingPositionOf(textarea, 0));
  EXPECT_EQ(Position::AfterNode(*textarea),
            Position::EditingPositionOf(textarea, 1));
  EXPECT_EQ(Position::AfterNode(*textarea),
            Position::EditingPositionOf(textarea, 2));

  // Change DOM tree to
  // <textarea id=textarea><a id=child1>1</a><b id=child2>2</b></textarea>
  Node* child1 = GetDocument().getElementById(AtomicString("child1"));
  Node* child2 = GetDocument().getElementById(AtomicString("child2"));
  textarea->appendChild(child1);
  textarea->appendChild(child2);

  EXPECT_EQ(Position::BeforeNode(*textarea),
            Position::EditingPositionOf(textarea, 0));
  EXPECT_EQ(Position::AfterNode(*textarea),
            Position::EditingPositionOf(textarea, 1));
  EXPECT_EQ(Position::AfterNode(*textarea),
            Position::EditingPositionOf(textarea, 2));
  EXPECT_EQ(Position::AfterNode(*textarea),
            Position::EditingPositionOf(textarea, 3));
}

// http://crbug.com/1248760
TEST_F(PositionTest, LastPositionInOrAfterNodeNotInFlatTree) {
  SetBodyContent("<option><select>A</select></option>");
  const Element& document_element = *GetDocument().documentElement();
  const Element& select = *GetDocument().QuerySelector(AtomicString("select"));

  EXPECT_EQ(Position::LastPositionInNode(document_element),
            Position::LastPositionInOrAfterNode(document_element));
  EXPECT_EQ(PositionInFlatTree::LastPositionInNode(document_element),
            PositionInFlatTree::LastPositionInOrAfterNode(document_element));

  // Note: <select> isn't appeared in flat tree, because <option> doesn't
  // take it as valid child node.
  EXPECT_EQ(Position::AfterNode(select),
            Position::LastPositionInOrAfterNode(select));
  EXPECT_EQ(PositionInFlatTree::LastPositionInNode(select),
            PositionInFlatTree::LastPositionInOrAfterNode(select));
}

TEST_F(PositionTest, NodeAsRangeLastNode) {
  const char* body_content =
      "<p id='p1'>11</p><p id='p2'></p><p id='p3'>33</p>";
  SetBodyContent(body_content);
  Node* p1 = GetDocument().getElementById(AtomicString("p1"));
  Node* p2 = GetDocument().getElementById(AtomicString("p2"));
  Node* p3 = GetDocument().getElementById(AtomicString("p3"));
  Node* body = EditingStrategy::Parent(*p1);
  Node* t1 = EditingStrategy::FirstChild(*p1);
  Node* t3 = EditingStrategy::FirstChild(*p3);

  EXPECT_EQ(body, Position::InParentBeforeNode(*p1).NodeAsRangeLastNode());
  EXPECT_EQ(t1, Position::InParentBeforeNode(*p2).NodeAsRangeLastNode());
  EXPECT_EQ(p2, Position::InParentBeforeNode(*p3).NodeAsRangeLastNode());
  EXPECT_EQ(t1, Position::InParentAfterNode(*p1).NodeAsRangeLastNode());
  EXPECT_EQ(p2, Position::InParentAfterNode(*p2).NodeAsRangeLastNode());
  EXPECT_EQ(t3, Position::InParentAfterNode(*p3).NodeAsRangeLastNode());
  EXPECT_EQ(t3, Position::AfterNode(*p3).NodeAsRangeLastNode());

  EXPECT_EQ(body,
            PositionInFlatTree::InParentBeforeNode(*p1).NodeAsRangeLastNode());
  EXPECT_EQ(t1,
            PositionInFlatTree::InParentBeforeNode(*p2).NodeAsRangeLastNode());
  EXPECT_EQ(p2,
            PositionInFlatTree::InParentBeforeNode(*p3).NodeAsRangeLastNode());
  EXPECT_EQ(t1,
            PositionInFlatTree::InParentAfterNode(*p1).NodeAsRangeLastNode());
  EXPECT_EQ(p2,
            PositionInFlatTree::InParentAfterNode(*p2).NodeAsRangeLastNode());
  EXPECT_EQ(t3,
            PositionInFlatTree::InParentAfterNode(*p3).NodeAsRangeLastNode());
  EXPECT_EQ(t3, PositionInFlatTree::AfterNode(*p3).NodeAsRangeLastNode());
}

// TODO(crbug.com/1157146): This test breaks without Shadow DOM v0.
TEST_F(PositionTest, DISABLED_NodeAsRangeLastNodeShadow) {
  const char* body_content =
      "<p id='host'>00<b slot='#one' id='one'>11</b><b slot='#two' "
      "id='two'>22</b>33</p>";
  const char* shadow_content =
      "<a id='a'><slot name='#two'></slot><slot name='#one'></slot></a>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* host = GetDocument().getElementById(AtomicString("host"));
  Node* n1 = GetDocument().getElementById(AtomicString("one"));
  Node* n2 = GetDocument().getElementById(AtomicString("two"));
  Node* t0 = EditingStrategy::FirstChild(*host);
  Node* t1 = EditingStrategy::FirstChild(*n1);
  Node* t2 = EditingStrategy::FirstChild(*n2);
  Node* t3 = EditingStrategy::LastChild(*host);
  Node* a = shadow_root->getElementById(AtomicString("a"));

  EXPECT_EQ(t0, Position::InParentBeforeNode(*n1).NodeAsRangeLastNode());
  EXPECT_EQ(t1, Position::InParentBeforeNode(*n2).NodeAsRangeLastNode());
  EXPECT_EQ(t1, Position::InParentAfterNode(*n1).NodeAsRangeLastNode());
  EXPECT_EQ(t2, Position::InParentAfterNode(*n2).NodeAsRangeLastNode());
  EXPECT_EQ(t3, Position::AfterNode(*host).NodeAsRangeLastNode());

  // TODO(crbug.com/1157146): This returns <slot name='#one'> instead of t2:
  EXPECT_EQ(t2,
            PositionInFlatTree::InParentBeforeNode(*n1).NodeAsRangeLastNode());
  // TODO(crbug.com/1157146): This returns <slot name='#two'> instead of a:
  EXPECT_EQ(a,
            PositionInFlatTree::InParentBeforeNode(*n2).NodeAsRangeLastNode());
  EXPECT_EQ(t1,
            PositionInFlatTree::InParentAfterNode(*n1).NodeAsRangeLastNode());
  EXPECT_EQ(t2,
            PositionInFlatTree::InParentAfterNode(*n2).NodeAsRangeLastNode());
  EXPECT_EQ(t1, PositionInFlatTree::AfterNode(*host).NodeAsRangeLastNode());
}

TEST_F(PositionTest, OperatorBool) {
  SetBodyContent("foo");
  EXPECT_FALSE(static_cast<bool>(Position()));
  EXPECT_TRUE(static_cast<bool>(Position(GetDocument().body(), 0)));
}

TEST_F(PositionTest, ToPositionInFlatTreeWithActiveInsertionPoint) {
  const char* body_content =
      "<p id='host'>00<b slot='#one' id='one'>11</b>22</p>";
  const char* shadow_content =
      "<a id='a'><slot name=#one id='content'></slot><slot></slot></a>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");
  Element* anchor = shadow_root->getElementById(AtomicString("a"));

  EXPECT_EQ(PositionInFlatTree(anchor, 0),
            ToPositionInFlatTree(Position(anchor, 0)));
  EXPECT_EQ(PositionInFlatTree(anchor, 1),
            ToPositionInFlatTree(Position(anchor, 1)));
  EXPECT_EQ(PositionInFlatTree::LastPositionInNode(*anchor),
            ToPositionInFlatTree(Position(anchor, 2)));
}

TEST_F(PositionTest, ToPositionInFlatTreeWithInactiveInsertionPoint) {
  const char* body_content = "<p id='p'><slot></slot></p>";
  SetBodyContent(body_content);
  Element* anchor = GetDocument().getElementById(AtomicString("p"));

  EXPECT_EQ(PositionInFlatTree(anchor, 0),
            ToPositionInFlatTree(Position(anchor, 0)));
  EXPECT_EQ(PositionInFlatTree::LastPositionInNode(*anchor),
            ToPositionInFlatTree(Position(anchor, 1)));
}

// This test comes from "editing/style/block-style-progress-crash.html".
TEST_F(PositionTest, ToPositionInFlatTreeWithNotDistributed) {
  SetBodyContent("<progress id=sample>foo</progress>");
  Element* sample = GetDocument().getElementById(AtomicString("sample"));

  EXPECT_EQ(PositionInFlatTree::FirstPositionInNode(*sample),
            ToPositionInFlatTree(Position(sample, 0)));
}

TEST_F(PositionTest, ToPositionInFlatTreeWithShadowRoot) {
  const char* body_content =
      "<p id='host'>00<b slot='#one' id='one'>11</b>22</p>";
  const char* shadow_content = "<a><slot name=#one></slot></a>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");
  Element* host = GetDocument().getElementById(AtomicString("host"));

  EXPECT_EQ(PositionInFlatTree(host, 0),
            ToPositionInFlatTree(Position(shadow_root, 0)));
  EXPECT_EQ(PositionInFlatTree::LastPositionInNode(*host),
            ToPositionInFlatTree(Position(shadow_root, 1)));
  EXPECT_EQ(PositionInFlatTree::LastPositionInNode(*host),
            ToPositionInFlatTree(Position::LastPositionInNode(*shadow_root)));
  EXPECT_EQ(PositionInFlatTree::FirstPositionInNode(*host),
            ToPositionInFlatTree(Position::FirstPositionInNode(*shadow_root)));
}

TEST_F(PositionTest,
       ToPositionInFlatTreeWithShadowRootContainingSingleContent) {
  const char* body_content =
      "<p id='host'>00<b slot='#one' id='one'>11</b>22</p>";
  const char* shadow_content = "<slot name=#one></slot>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");
  Element* host = GetDocument().getElementById(AtomicString("host"));

  EXPECT_EQ(PositionInFlatTree(host, 0),
            ToPositionInFlatTree(Position(shadow_root, 0)));
  EXPECT_EQ(PositionInFlatTree::LastPositionInNode(*host),
            ToPositionInFlatTree(Position(shadow_root, 1)));
}

TEST_F(PositionTest, ToPositionInFlatTreeWithEmptyShadowRoot) {
  const char* body_content = "<p id='host'>00<b id='one'>11</b>22</p>";
  const char* shadow_content = "";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");
  Element* host = GetDocument().getElementById(AtomicString("host"));

  EXPECT_EQ(PositionInFlatTree::FirstPositionInNode(*host),
            ToPositionInFlatTree(Position(shadow_root, 0)));
}

TEST_F(PositionTest, NullPositionNotConnected) {
  EXPECT_FALSE(Position().IsConnected());
  EXPECT_FALSE(PositionInFlatTree().IsConnected());
}

TEST_F(PositionTest, IsConnectedBasic) {
  Position position = SetCaretTextToBody("<div>f|oo</div>");
  EXPECT_TRUE(position.IsConnected());
  EXPECT_TRUE(ToPositionInFlatTree(position).IsConnected());

  position.AnchorNode()->remove();
  EXPECT_FALSE(position.IsConnected());
  EXPECT_FALSE(ToPositionInFlatTree(position).IsConnected());
}

TEST_F(PositionTest, IsConnectedInFlatTree) {
  Position position = SetCaretTextToBody(
      "<div>f|oo<template data-mode=open>bar</template></div>");
  EXPECT_TRUE(position.IsConnected());
  EXPECT_FALSE(ToPositionInFlatTree(position).IsConnected());
}

TEST_F(PositionTest, FirstPositionInShadowHost) {
  SetBodyContent("<p id=host>foo</p>");
  SetShadowContent("bar", "host");
  Element* host = GetDocument().getElementById(AtomicString("host"));

  Position dom = Position::FirstPositionInNode(*host);
  PositionInFlatTree flat = PositionInFlatTree::FirstPositionInNode(*host);
  EXPECT_EQ(dom, ToPositionInDOMTree(flat));
  EXPECT_EQ(flat, ToPositionInFlatTree(dom));
}

TEST_F(PositionTest, LastPositionInShadowHost) {
  SetBodyContent("<p id=host>foo</p>");
  SetShadowContent("bar", "host");
  Element* host = GetDocument().getElementById(AtomicString("host"));

  Position dom = Position::LastPositionInNode(*host);
  PositionInFlatTree flat = PositionInFlatTree::LastPositionInNode(*host);
  EXPECT_EQ(dom, ToPositionInDOMTree(flat));
  EXPECT_EQ(flat, ToPositionInFlatTree(dom));
}

TEST_F(PositionTest, ComparePositionsAcrossShadowBoundary) {
  SetBodyContent("<p id=host>foo</p>");
  ShadowRoot* shadow_root = SetShadowContent("<div>bar</div>", "host");
  Element* body = GetDocument().body();
  Element* host = GetDocument().getElementById(AtomicString("host"));
  Node* child = shadow_root->firstChild();
  Node* grandchild = child->firstChild();
  std::array<Node*, 4> nodes = {body, host, child, grandchild};
  unsigned size = nodes.size();
  for (unsigned i = 0; i < size; ++i) {
    for (unsigned j = 0; j < i; ++j) {
      EXPECT_LT(Position(nodes[j], 0), Position(nodes[i], 0));
      EXPECT_LT(PositionInFlatTree(nodes[j], 0),
                PositionInFlatTree(nodes[i], 0));
    }
    EXPECT_EQ(Position(nodes[i], 0), Position(nodes[i], 0));
    EXPECT_EQ(PositionInFlatTree(nodes[i], 0), PositionInFlatTree(nodes[i], 0));
    for (unsigned j = i + 1; j < size; ++j) {
      EXPECT_GT(Position(nodes[j], 0), Position(nodes[i], 0));
      EXPECT_GT(PositionInFlatTree(nodes[j], 0),
                PositionInFlatTree(nodes[i], 0));
    }
  }
}

}  // namespace blink
