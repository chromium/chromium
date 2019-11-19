// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/animation/priority_queue.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

class TestNode : public GarbageCollected<TestNode> {
 public:
  explicit TestNode(int priority) : priority_(priority), handle_(kNotFound) {}

  int Priority() const { return priority_; }
  void SetPriority(int priority) { priority_ = priority; }

  wtf_size_t& PriorityQueueHandle() { return handle_; }

  void Trace(Visitor*) {}

 private:
  int priority_;
  wtf_size_t handle_;
};

struct TestNodeLess {
  bool operator()(const TestNode& a, const TestNode& b) {
    return a.Priority() < b.Priority();
  }
};

using TestPriorityQueue = PriorityQueue<TestNode, TestNodeLess>;

void VerifyHeap(TestPriorityQueue& queue, int round = -1) {
  for (wtf_size_t index = 0; index < queue.size(); ++index) {
    TestNode& node = *queue[index];

    wtf_size_t left_child_index = index * 2 + 1;
    if (left_child_index < queue.size())
      EXPECT_FALSE(TestNodeLess()(*queue[left_child_index], node));

    wtf_size_t right_child_index = left_child_index + 1;
    if (right_child_index < queue.size())
      EXPECT_FALSE(TestNodeLess()(*queue[right_child_index], node));

    EXPECT_EQ(node.PriorityQueueHandle(), index);
  }
}

}  // namespace

TEST(PriorityQueueTest, Insertion) {
  TestPriorityQueue queue;
  EXPECT_TRUE(queue.IsEmpty());
  queue.Insert(MakeGarbageCollected<TestNode>(7));
  EXPECT_FALSE(queue.IsEmpty());
  for (int n : {1, 2, 6, 4, 5, 3, 0})
    queue.Insert(MakeGarbageCollected<TestNode>(n));
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.size(), 8u);
  VerifyHeap(queue);
}

TEST(PriorityQueueTest, InsertionDuplicates) {
  TestPriorityQueue queue;
  EXPECT_TRUE(queue.IsEmpty());
  for (int n : {7, 1, 5, 6, 5, 5, 1, 0})
    queue.Insert(MakeGarbageCollected<TestNode>(n));
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.size(), 8u);
  VerifyHeap(queue);
}

TEST(PriorityQueueTest, RemovalMin) {
  TestPriorityQueue queue;
  EXPECT_TRUE(queue.IsEmpty());
  for (int n : {7, 1, 2, 6, 4, 5, 3, 0})
    queue.Insert(MakeGarbageCollected<TestNode>(n));
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.size(), 8u);
  VerifyHeap(queue);
  for (int n = 0; n < 8; ++n) {
    TestNode* node = queue.MinElement();
    EXPECT_EQ(node->Priority(), n);
    wtf_size_t expected_size = static_cast<wtf_size_t>(8 - n);
    EXPECT_EQ(queue.size(), expected_size);
    queue.Remove(node);
    EXPECT_EQ(queue.size(), expected_size - 1);
    VerifyHeap(queue);
  }
}

TEST(PriorityQueueTest, RemovalReverse) {
  TestPriorityQueue queue;
  HeapVector<Member<TestNode>> vector;
  EXPECT_TRUE(queue.IsEmpty());
  for (int n : {7, 1, 2, 6, 4, 5, 3, 0}) {
    TestNode* node = MakeGarbageCollected<TestNode>(n);
    queue.Insert(node);
    vector.push_back(node);
  }
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.size(), 8u);
  VerifyHeap(queue);
  std::sort(vector.begin(), vector.end(),
            [](const TestNode* a, const TestNode* b) {
              return a->Priority() > b->Priority();
            });
  for (int n = 0; n < 8; ++n) {
    TestNode* node = vector[n];
    EXPECT_EQ(node->Priority(), 8 - (n + 1));
    wtf_size_t expected_size = static_cast<wtf_size_t>(8 - n);
    EXPECT_EQ(queue.size(), expected_size);
    queue.Remove(node);
    EXPECT_EQ(queue.size(), expected_size - 1);
    VerifyHeap(queue);
  }
}

TEST(PriorityQueueTest, RemovalRandom) {
  TestPriorityQueue queue;
  HeapVector<Member<TestNode>> vector;
  EXPECT_TRUE(queue.IsEmpty());
  for (int n : {7, 1, 2, 6, 4, 0, 5, 3}) {
    TestNode* node = MakeGarbageCollected<TestNode>(n);
    queue.Insert(node);
    vector.push_back(node);
  }
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.size(), 8u);
  VerifyHeap(queue);
  for (int n = 0; n < 8; ++n) {
    wtf_size_t expected_size = static_cast<wtf_size_t>(8 - n);
    EXPECT_EQ(queue.size(), expected_size);
    queue.Remove(vector[n]);
    EXPECT_EQ(queue.size(), expected_size - 1);
    VerifyHeap(queue);
  }
}

TEST(PriorityQueueTest, Updates) {
  TestPriorityQueue queue;
  HeapVector<Member<TestNode>> vector;
  EXPECT_TRUE(queue.IsEmpty());
  for (int n : {7, 1, 2, 6, 4, 0, 5, 3}) {
    TestNode* node = MakeGarbageCollected<TestNode>(n);
    queue.Insert(node);
    vector.push_back(node);
  }
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.size(), 8u);
  VerifyHeap(queue);

  // Increase/decrease priority for elements from even/odd slots in |vector|.
  for (int n = 0; n < 8; ++n) {
    TestNode* node = vector[n];
    int old_priority = node->Priority();
    int adjust = ((n % 2) - 1) * 4;
    node->SetPriority(old_priority + adjust);
    EXPECT_EQ(queue.size(), 8u);
    queue.Update(node);
    EXPECT_EQ(queue.size(), 8u);
    VerifyHeap(queue, n);
  }

  // Decrease priority for the root node.
  TestNode* smallest = queue[0];
  smallest->SetPriority(smallest->Priority() - 10);
  queue.Update(smallest);
  EXPECT_EQ(smallest, queue[0]);
  VerifyHeap(queue);

  // Increase priority for the root node.
  smallest = queue[0];
  smallest->SetPriority(queue[7]->Priority() + 1);
  queue.Update(smallest);
  EXPECT_EQ(smallest, queue[7]);
  VerifyHeap(queue);

  // No-op update.
  TestNode* node = queue[3];
  queue.Update(node);
  EXPECT_EQ(node, queue[3]);
  VerifyHeap(queue);

  // Decrease priority for a non-root node.
  node = queue[3];
  node->SetPriority(queue[TestPriorityQueue::ParentIndex(3)]->Priority() - 1);
  queue.Update(node);
  VerifyHeap(queue);

  // Matching priority of parent doesn't move the node.
  node = queue[3];
  node->SetPriority(queue[TestPriorityQueue::ParentIndex(3)]->Priority());
  queue.Update(node);
  EXPECT_EQ(node, queue[3]);
  VerifyHeap(queue);

  // Increase priority for a non-root node.
  node = queue[3];
  node->SetPriority(queue[TestPriorityQueue::LeftChildIndex(3)]->Priority() +
                    1);
  queue.Update(node);
  VerifyHeap(queue);

  // Matching priority of smallest child doesn't move the node.
  node = queue[1];
  node->SetPriority(
      std::min(queue[TestPriorityQueue::LeftChildIndex(1)]->Priority(),
               queue[TestPriorityQueue::LeftChildIndex(1) + 1]->Priority()));
  queue.Update(node);
  EXPECT_EQ(node, queue[1]);
  VerifyHeap(queue);
}

}  // namespace blink
