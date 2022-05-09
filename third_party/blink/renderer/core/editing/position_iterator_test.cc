// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/position_iterator.h"

#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

class PositionIteratorTest : public EditingTestBase {
 protected:
  void TestDecrement(const char* markup);
  void TestIncrement(const char* markup);
};

void PositionIteratorTest::TestDecrement(const char* markup) {
  SetBodyContent(markup);
  const Element& element = *GetDocument().QuerySelector("#target");
  const Node* const text = element.previousSibling();
  const Element* const body = element.GetDocument().body();

  // Decrement until end of Text "123" from |element| on DOM tree.
  PositionIterator dom_iterator(Position::LastPositionInNode(*body));
  EXPECT_EQ(body, dom_iterator.GetNode());
  EXPECT_EQ(2, dom_iterator.OffsetInLeafNode());
  EXPECT_EQ(Position::LastPositionInNode(*body),
            dom_iterator.ComputePosition());

  dom_iterator.Decrement();
  EXPECT_EQ(element, dom_iterator.GetNode());
  EXPECT_EQ(1, dom_iterator.OffsetInLeafNode());
  EXPECT_EQ(Position::AfterNode(element), dom_iterator.ComputePosition());

  dom_iterator.Decrement();
  EXPECT_EQ(element, dom_iterator.GetNode());
  EXPECT_EQ(0, dom_iterator.OffsetInLeafNode());
  EXPECT_EQ(Position::BeforeNode(element), dom_iterator.ComputePosition());

  dom_iterator.Decrement();
  EXPECT_EQ(body, dom_iterator.GetNode());
  EXPECT_EQ(1, dom_iterator.OffsetInLeafNode());
  EXPECT_EQ(Position(body, 1), dom_iterator.ComputePosition());

  dom_iterator.Decrement();
  EXPECT_EQ(text, dom_iterator.GetNode());
  EXPECT_EQ(3, dom_iterator.OffsetInLeafNode());
  EXPECT_EQ(Position(text, 3), dom_iterator.ComputePosition());

  // Decrement until start of Text "123" from |element| on flat tree.
  PositionIteratorInFlatTree flat_iterator(
      PositionInFlatTree::LastPositionInNode(*body));
  EXPECT_EQ(body, flat_iterator.GetNode());
  EXPECT_EQ(2, flat_iterator.OffsetInLeafNode());
  EXPECT_EQ(PositionInFlatTree::LastPositionInNode(*body),
            flat_iterator.ComputePosition());

  flat_iterator.Decrement();
  EXPECT_EQ(element, flat_iterator.GetNode());
  EXPECT_EQ(1, flat_iterator.OffsetInLeafNode());
  EXPECT_EQ(PositionInFlatTree::AfterNode(element),
            flat_iterator.ComputePosition());

  flat_iterator.Decrement();
  EXPECT_EQ(element, flat_iterator.GetNode());
  EXPECT_EQ(0, flat_iterator.OffsetInLeafNode());
  EXPECT_EQ(PositionInFlatTree::BeforeNode(element),
            flat_iterator.ComputePosition());

  flat_iterator.Decrement();
  EXPECT_EQ(body, flat_iterator.GetNode());
  EXPECT_EQ(1, flat_iterator.OffsetInLeafNode());
  EXPECT_EQ(PositionInFlatTree(body, 1), flat_iterator.ComputePosition());

  flat_iterator.Decrement();
  EXPECT_EQ(text, flat_iterator.GetNode());
  EXPECT_EQ(3, flat_iterator.OffsetInLeafNode());
  EXPECT_EQ(PositionInFlatTree(text, 3), flat_iterator.ComputePosition());
}

void PositionIteratorTest::TestIncrement(const char* markup) {
  SetBodyContent(markup);
  const Element& element = *GetDocument().QuerySelector("#target");
  Node* const text = element.nextSibling();
  const Element* body = element.GetDocument().body();

  // Increment until start of Text "123" from |element| on DOM tree.
  PositionIterator dom_iterator(Position::FirstPositionInNode(*body));
  EXPECT_EQ(body, dom_iterator.GetNode());
  EXPECT_EQ(0, dom_iterator.OffsetInLeafNode());
  EXPECT_EQ(Position(body, 0), dom_iterator.ComputePosition());

  dom_iterator.Increment();
  EXPECT_EQ(element, dom_iterator.GetNode());
  EXPECT_EQ(0, dom_iterator.OffsetInLeafNode());
  EXPECT_EQ(Position::BeforeNode(element), dom_iterator.ComputePosition());

  dom_iterator.Increment();
  EXPECT_EQ(element, dom_iterator.GetNode());
  EXPECT_EQ(1, dom_iterator.OffsetInLeafNode());
  EXPECT_EQ(Position::AfterNode(element), dom_iterator.ComputePosition());

  dom_iterator.Increment();
  EXPECT_EQ(body, dom_iterator.GetNode());
  EXPECT_EQ(1, dom_iterator.OffsetInLeafNode());
  EXPECT_EQ(Position(body, 1), dom_iterator.ComputePosition());

  dom_iterator.Increment();
  EXPECT_EQ(text, dom_iterator.GetNode());
  EXPECT_EQ(0, dom_iterator.OffsetInLeafNode());
  EXPECT_EQ(Position(text, 0), dom_iterator.ComputePosition());

  // Increment until start of Text "123" from |element| on flat tree.
  PositionIteratorInFlatTree flat_iterator(
      PositionInFlatTree::FirstPositionInNode(*body));
  EXPECT_EQ(body, flat_iterator.GetNode());
  EXPECT_EQ(0, flat_iterator.OffsetInLeafNode());
  EXPECT_EQ(PositionInFlatTree(body, 0), flat_iterator.ComputePosition());

  flat_iterator.Increment();
  EXPECT_EQ(element, flat_iterator.GetNode());
  EXPECT_EQ(0, flat_iterator.OffsetInLeafNode());
  EXPECT_EQ(PositionInFlatTree::BeforeNode(element),
            flat_iterator.ComputePosition());

  flat_iterator.Increment();
  EXPECT_EQ(element, flat_iterator.GetNode());
  EXPECT_EQ(1, flat_iterator.OffsetInLeafNode());
  EXPECT_EQ(PositionInFlatTree::AfterNode(element),
            flat_iterator.ComputePosition());

  flat_iterator.Increment();
  EXPECT_EQ(body, flat_iterator.GetNode());
  EXPECT_EQ(1, flat_iterator.OffsetInLeafNode());
  EXPECT_EQ(PositionInFlatTree(body, 1), flat_iterator.ComputePosition());

  flat_iterator.Increment();
  EXPECT_EQ(text, flat_iterator.GetNode());
  EXPECT_EQ(0, flat_iterator.OffsetInLeafNode());
  EXPECT_EQ(PositionInFlatTree(text, 0), flat_iterator.ComputePosition());
}

// For http://crbug.com/695317
TEST_F(PositionIteratorTest, decrementWithInputElement) {
  TestDecrement("123<input id=target value='abc'>");
}

TEST_F(PositionIteratorTest, decrementWithSelectElement) {
  TestDecrement(
      "123<select id=target><option>1</option><option>2</option></select>");
}

// For http://crbug.com/695317
TEST_F(PositionIteratorTest, decrementWithTextAreaElement) {
  TestDecrement("123<textarea id=target>456</textarea>");
}

// For http://crbug.com/695317
TEST_F(PositionIteratorTest, incrementWithInputElement) {
  TestIncrement("<input id=target value='abc'>123");
}

TEST_F(PositionIteratorTest, incrementWithSelectElement) {
  TestIncrement(
      "<select id=target><option>1</option><option>2</option></select>123");
}

// For http://crbug.com/695317
TEST_F(PositionIteratorTest, incrementWithTextAreaElement) {
  TestIncrement("<textarea id=target>123</textarea>456");
}

// For http://crbug.com/1248744
TEST_F(PositionIteratorTest, nullPosition) {
  PositionIterator dom_iterator((Position()));
  PositionIteratorInFlatTree flat_iterator((PositionInFlatTree()));

  EXPECT_EQ(Position(), dom_iterator.ComputePosition());
  EXPECT_EQ(PositionInFlatTree(), flat_iterator.ComputePosition());

  EXPECT_EQ(Position(), dom_iterator.DeprecatedComputePosition());
  EXPECT_EQ(PositionInFlatTree(), flat_iterator.DeprecatedComputePosition());

  dom_iterator.Increment();
  flat_iterator.Increment();

  EXPECT_EQ(Position(), dom_iterator.ComputePosition());
  EXPECT_EQ(PositionInFlatTree(), flat_iterator.ComputePosition());

  dom_iterator.Decrement();
  flat_iterator.Decrement();

  EXPECT_EQ(Position(), dom_iterator.ComputePosition());
  EXPECT_EQ(PositionInFlatTree(), flat_iterator.ComputePosition());
}

}  // namespace blink
