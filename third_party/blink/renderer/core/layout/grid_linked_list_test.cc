// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid_linked_list.h"

#include <atomic>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

namespace {

class GridLinkedListTest : public testing::Test {
 public:
  template <typename NodeType>
  static NodeType* NthElement(GridLinkedList<NodeType>* gll, int n);
};

// Helper function for obtaining the nth element (starting from 0) of
// GridLinkedList. n should be smaller than the size of the list.
template <typename NodeType>
NodeType* GridLinkedListTest::NthElement(GridLinkedList<NodeType>* gll, int n) {
  DCHECK(gll);
  DCHECK_LT(n, gll->Size());
  NodeType* node = gll->Head();
  for (int i = 0; i < n; i++) {
    node = node->Next();
  }
  return node;
}

// IntNode defines a node class inheriting GridLinkedListNodeBase, with a member
// of value (int). It can be applied for NodeType in the GridLinkedList.
class IntNode : public GridLinkedListNodeBase<IntNode> {
 public:
  explicit IntNode(int value) : value_(value) {}
  ~IntNode() { destructor_calls.fetch_add(1, std::memory_order_relaxed); }

  int Value() const { return value_; }

  static std::atomic_int destructor_calls;

 private:
  int value_ = -1;
};

std::atomic_int IntNode::destructor_calls{0};

}  // namespace

TEST_F(GridLinkedListTest, IntNodeBasic) {
  IntNode* num1 = MakeGarbageCollected<IntNode>(1);
  IntNode* num2 = MakeGarbageCollected<IntNode>(2);
  IntNode* num3 = MakeGarbageCollected<IntNode>(3);

  Persistent<GridLinkedList<IntNode>> gll =
      MakeGarbageCollected<GridLinkedList<IntNode>>();

  EXPECT_EQ(gll->Size(), 0);
  EXPECT_TRUE(gll->IsEmpty());

  gll->Append(num1);
  EXPECT_EQ(gll->Size(), 1);
  EXPECT_EQ(NthElement<IntNode>(gll, 0)->Value(), 1);

  gll->Append(num2);
  EXPECT_EQ(gll->Size(), 2);
  EXPECT_EQ(NthElement<IntNode>(gll, 0)->Value(), 1);
  EXPECT_EQ(NthElement<IntNode>(gll, 1)->Value(), 2);

  gll->Push(num3);
  EXPECT_EQ(gll->Size(), 3);
  EXPECT_EQ(NthElement<IntNode>(gll, 0)->Value(), 3);
  EXPECT_EQ(NthElement<IntNode>(gll, 1)->Value(), 1);
  EXPECT_EQ(NthElement<IntNode>(gll, 2)->Value(), 2);

  gll->Remove(num1);
  EXPECT_EQ(gll->Size(), 2);
  EXPECT_EQ(NthElement<IntNode>(gll, 0)->Value(), 3);
  EXPECT_EQ(NthElement<IntNode>(gll, 1)->Value(), 2);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1, IntNode::destructor_calls);

  gll->Remove(num3);
  EXPECT_EQ(gll->Size(), 1);
  EXPECT_EQ(NthElement<IntNode>(gll, 0)->Value(), 2);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(2, IntNode::destructor_calls);

  gll->Remove(num2);
  EXPECT_EQ(gll->Size(), 0);
  EXPECT_TRUE(gll->IsEmpty());

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(3, IntNode::destructor_calls);
}

}  // namespace blink
