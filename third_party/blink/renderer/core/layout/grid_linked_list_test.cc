// Copyright 2022 The Chromium Authors
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

// IntNode defines a node class inheriting GridLinkedListNodeBase, with a member
// of value (int). It can be applied for NodeType in the GridLinkedList.
class IntNode : public GridLinkedListNodeBase<IntNode> {
 public:
  explicit IntNode(int value) : value_(value) {}
  ~IntNode() override {
    destructor_calls.fetch_add(1, std::memory_order_relaxed);
  }

  int Value() const { return value_; }

  static int Compare(IntNode* a, IntNode* b);

  static std::atomic_int destructor_calls;

 private:
  int value_ = -1;
};

int IntNode::Compare(IntNode* a, IntNode* b) {
  DCHECK(a);
  DCHECK(b);
  return a->Value() - b->Value();
}

std::atomic_int IntNode::destructor_calls{0};

class GridLinkedListTest : public testing::Test {
 public:
  void SetUp() override;

  template <typename NodeType>
  static NodeType* NthElement(GridLinkedList<NodeType>* gll, int n);

  template <typename NodeType, typename CompareFunc>
  static bool IsSorted(GridLinkedList<NodeType>* gll,
                       const CompareFunc& compare_func);
};

void GridLinkedListTest::SetUp() {
  IntNode::destructor_calls = 0;
}

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

template <typename NodeType, typename CompareFunc>
bool GridLinkedListTest::IsSorted(GridLinkedList<NodeType>* gll,
                                  const CompareFunc& compare_func) {
  DCHECK(gll);
  for (NodeType* node = gll->Head(); node && node->Next();
       node = node->Next()) {
    if (compare_func(node, node->Next()) >= 0)
      return false;
  }
  return true;
}

}  // namespace

TEST_F(GridLinkedListTest, IntNodeBasic) {
  IntNode* num1 = MakeGarbageCollected<IntNode>(1);
  IntNode* num2 = MakeGarbageCollected<IntNode>(2);
  IntNode* num3 = MakeGarbageCollected<IntNode>(3);

  Persistent<GridLinkedList<IntNode>> gll_persistent =
      MakeGarbageCollected<GridLinkedList<IntNode>>();
  GridLinkedList<IntNode>* gll = gll_persistent;

  EXPECT_EQ(gll->Size(), 0);
  EXPECT_TRUE(gll->IsEmpty());

  gll->Append(num1);
  EXPECT_EQ(gll->Size(), 1);
  EXPECT_EQ(NthElement(gll, 0)->Value(), 1);

  gll->Append(num2);
  EXPECT_EQ(gll->Size(), 2);
  EXPECT_EQ(NthElement(gll, 0)->Value(), 1);
  EXPECT_EQ(NthElement(gll, 1)->Value(), 2);

  gll->Push(num3);
  EXPECT_EQ(gll->Size(), 3);
  EXPECT_EQ(NthElement(gll, 0)->Value(), 3);
  EXPECT_EQ(NthElement(gll, 1)->Value(), 1);
  EXPECT_EQ(NthElement(gll, 2)->Value(), 2);

  gll->Remove(num1);
  EXPECT_EQ(gll->Size(), 2);
  EXPECT_EQ(NthElement(gll, 0)->Value(), 3);
  EXPECT_EQ(NthElement(gll, 1)->Value(), 2);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(1, IntNode::destructor_calls);

  gll->Remove(num3);
  EXPECT_EQ(gll->Size(), 1);
  EXPECT_EQ(NthElement(gll, 0)->Value(), 2);

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(2, IntNode::destructor_calls);

  gll->Remove(num2);
  EXPECT_EQ(gll->Size(), 0);
  EXPECT_TRUE(gll->IsEmpty());

  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(3, IntNode::destructor_calls);
}

TEST_F(GridLinkedListTest, Insert) {
  IntNode* num1 = MakeGarbageCollected<IntNode>(1);
  IntNode* num2 = MakeGarbageCollected<IntNode>(2);
  IntNode* num3 = MakeGarbageCollected<IntNode>(3);
  IntNode* num2_again = MakeGarbageCollected<IntNode>(2);

  Persistent<GridLinkedList<IntNode>> gll_persistent =
      MakeGarbageCollected<GridLinkedList<IntNode>>();
  GridLinkedList<IntNode>* gll = gll_persistent;
  EXPECT_TRUE(IsSorted(gll, IntNode::Compare));

  EXPECT_TRUE(gll->Insert(num2, IntNode::Compare));
  EXPECT_TRUE(IsSorted(gll, IntNode::Compare));

  EXPECT_TRUE(gll->Insert(num1, IntNode::Compare));
  EXPECT_TRUE(IsSorted(gll, IntNode::Compare));

  EXPECT_TRUE(gll->Insert(num3, IntNode::Compare));
  EXPECT_TRUE(IsSorted(gll, IntNode::Compare));

  EXPECT_FALSE(gll->Insert(num2_again, IntNode::Compare));
  EXPECT_EQ(gll->Insert(num2_again, IntNode::Compare).node, num2);
  EXPECT_TRUE(IsSorted(gll, IntNode::Compare));

  gll->Clear();
  DCHECK(gll->IsEmpty());
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(4, IntNode::destructor_calls);
}

}  // namespace blink
