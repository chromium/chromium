// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/animation/priority_queue.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

class TestNode : public GarbageCollected<TestNode> {
 public:
  explicit TestNode() : handle_(kNotFound) {}

  wtf_size_t& PriorityQueueHandle() { return handle_; }

  void Trace(Visitor*) const {}

 private:
  wtf_size_t handle_;
};

using TestPriorityQueue = PriorityQueue<int, TestNode>;

void VerifyHeap(TestPriorityQueue& queue, int round = -1) {
  for (wtf_size_t index = 0; index < queue.size(); ++index) {
    const TestPriorityQueue::EntryType& entry = queue[index];

    wtf_size_t left_child_index = index * 2 + 1;
    if (left_child_index < queue.size())
      EXPECT_FALSE(queue[left_child_index].first < entry.first);

    wtf_size_t right_child_index = left_child_index + 1;
    if (right_child_index < queue.size())
      EXPECT_FALSE(queue[right_child_index].first < entry.first);

    EXPECT_EQ(entry.second->PriorityQueueHandle(), index);
  }
}

}  // namespace

TEST(PriorityQueueTest, Insertion) {
  test::TaskEnvironment task_environment;
  TestPriorityQueue queue;
  EXPECT_TRUE(queue.IsEmpty());
  queue.Insert(7, MakeGarbageCollected<TestNode>());
  EXPECT_FALSE(queue.IsEmpty());
  for (int n : {1, 2, 6, 4, 5, 3, 0})
    queue.Insert(n, MakeGarbageCollected<TestNode>());
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.size(), 8u);
  VerifyHeap(queue);
}

TEST(PriorityQueueTest, InsertionDuplicates) {
  test::TaskEnvironment task_environment;
  TestPriorityQueue queue;
  EXPECT_TRUE(queue.IsEmpty());
  for (int n : {7, 1, 5, 6, 5, 5, 1, 0})
    queue.Insert(n, MakeGarbageCollected<TestNode>());
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.size(), 8u);
  VerifyHeap(queue);
}

TEST(PriorityQueueTest, RemovalMin) {
  test::TaskEnvironment task_environment;
  TestPriorityQueue queue;
  EXPECT_TRUE(queue.IsEmpty());
  for (int n : {7, 1, 2, 6, 4, 5, 3, 0})
    queue.Insert(n, MakeGarbageCollected<TestNode>());
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.size(), 8u);
  VerifyHeap(queue);
  for (int n = 0; n < 8; ++n) {
    EXPECT_EQ(queue.Min(), n);
    TestNode* node = queue.MinElement();
    wtf_size_t expected_size = static_cast<wtf_size_t>(8 - n);
    EXPECT_EQ(queue.size(), expected_size);
    queue.Remove(node);
    EXPECT_EQ(queue.size(), expected_size - 1);
    VerifyHeap(queue);
  }
}

TEST(PriorityQueueTest, RemovalFilledFromOtherSubtree) {
  test::TaskEnvironment task_environment;
  TestPriorityQueue queue;
  using PairType = std::pair<int, Member<TestNode>>;
  HeapVector<PairType> vector;
  EXPECT_TRUE(queue.IsEmpty());
  // Build a heap/queue where the left subtree contains priority 3 and the right
  // contains priority 4:
  //
  //              /-{[6]=4}   {[index]=priority}
  //      /-{[2]=4}-{[5]=4}
  // {[0]=3}
  //      \-{[1]=3}-{[4]=3}
  //              \-{[3]=3}
  //                      \-{[7]=3}
  //
  for (int n : {3, 3, 4, 3, 3, 4, 4, 3}) {
    TestNode* node = MakeGarbageCollected<TestNode>();
    queue.Insert(n, node);
    vector.push_back<PairType>({n, node});
  }
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.size(), 8u);
  VerifyHeap(queue);

  queue.Remove(vector[6].second);
  EXPECT_EQ(queue.size(), 7u);
  VerifyHeap(queue);
}

TEST(PriorityQueueTest, RemovalReverse) {
  test::TaskEnvironment task_environment;
  TestPriorityQueue queue;
  using PairType = std::pair<int, Member<TestNode>>;
  HeapVector<PairType> vector;
  EXPECT_TRUE(queue.IsEmpty());
  for (int n : {7, 1, 2, 6, 4, 5, 3, 0}) {
    TestNode* node = MakeGarbageCollected<TestNode>();
    queue.Insert(n, node);
    vector.push_back<PairType>({n, node});
  }
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.size(), 8u);
  VerifyHeap(queue);
  std::sort(
      vector.begin(), vector.end(),
      [](const PairType& a, const PairType& b) { return a.first > b.first; });
  for (int n = 0; n < 8; ++n) {
    EXPECT_EQ(vector[n].first, 8 - (n + 1));
    wtf_size_t expected_size = static_cast<wtf_size_t>(8 - n);
    EXPECT_EQ(queue.size(), expected_size);
    queue.Remove(vector[n].second);
    EXPECT_EQ(queue.size(), expected_size - 1);
    VerifyHeap(queue);
  }
}

TEST(PriorityQueueTest, RemovalRandom) {
  test::TaskEnvironment task_environment;
  TestPriorityQueue queue;
  HeapVector<Member<TestNode>> vector;
  EXPECT_TRUE(queue.IsEmpty());
  for (int n : {7, 1, 2, 6, 4, 0, 5, 3}) {
    TestNode* node = MakeGarbageCollected<TestNode>();
    queue.Insert(n, node);
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
  test::TaskEnvironment task_environment;
  TestPriorityQueue queue;
  using PairType = std::pair<int, Member<TestNode>>;
  HeapVector<PairType> vector;
  EXPECT_TRUE(queue.IsEmpty());
  for (int n : {7, 1, 2, 6, 4, 0, 5, 3}) {
    TestNode* node = MakeGarbageCollected<TestNode>();
    queue.Insert(n, node);
    vector.push_back<PairType>({n, node});
  }
  EXPECT_FALSE(queue.IsEmpty());
  EXPECT_EQ(queue.size(), 8u);
  VerifyHeap(queue);

  // Increase/decrease priority for elements from even/odd slots in |vector|.
  for (int n = 0; n < 8; ++n) {
    int old_priority = vector[n].first;
    int adjust = ((n % 2) - 1) * 4;
    int new_priority = old_priority + adjust;
    EXPECT_EQ(queue.size(), 8u);
    queue.Update(new_priority, vector[n].second);
    EXPECT_EQ(queue.size(), 8u);
    VerifyHeap(queue, n);
  }

  // Decrease priority for the root node.
  TestNode* smallest = queue[0].second;
  queue.Update(queue[0].first - 10, smallest);
  EXPECT_EQ(smallest, queue[0].second);
  VerifyHeap(queue);

  // Increase priority for the root node.
  smallest = queue[0].second;
  queue.Update(queue[7].first + 1, smallest);
  EXPECT_EQ(smallest, queue[7].second);
  VerifyHeap(queue);

  // No-op update.
  TestNode* node = queue[3].second;
  queue.Update(queue[3].first, node);
  EXPECT_EQ(node, queue[3].second);
  VerifyHeap(queue);

  // Decrease priority for a non-root node.
  node = queue[3].second;
  int parent_prio = queue[TestPriorityQueue::ParentIndex(3)].first;
  queue.Update(parent_prio - 1, node);
  VerifyHeap(queue);

  // Matching priority of parent doesn't move the node.
  node = queue[3].second;
  parent_prio = queue[TestPriorityQueue::ParentIndex(3)].first;
  queue.Update(parent_prio, node);
  EXPECT_EQ(node, queue[3].second);
  VerifyHeap(queue);

  // Increase priority for a non-root node.
  node = queue[3].second;
  int left_child_prio = queue[TestPriorityQueue::LeftChildIndex(3)].first;
  queue.Update(left_child_prio + 1, node);
  VerifyHeap(queue);

  // Matching priority of smallest child doesn't move the node.
  node = queue[1].second;
  int left_child_index = TestPriorityQueue::LeftChildIndex(1);
  left_child_prio = queue[left_child_index].first;
  int right_child_prio = queue[left_child_index + 1].first;
  queue.Update(std::min(left_child_prio, right_child_prio), node);
  EXPECT_EQ(node, queue[1].second);
  VerifyHeap(queue);
}

}  // namespace blink
