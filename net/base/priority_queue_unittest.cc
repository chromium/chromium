// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/base/priority_queue.h"

#include <cstddef>

#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

typedef PriorityQueue<int>::Priority Priority;
// Queue 0 has empty lists for first and last priorities.
// Queue 1 has multiple empty lists in a row, and occupied first and last
// priorities.
// Queue 2 has multiple empty lists in a row at the first and last priorities.
//             Queue 0    Queue 1   Queue 2
// Priority 0: {}         {3, 7}    {}
// Priority 1: {2, 3, 7}  {2}       {}
// Priority 2: {1, 5}     {1, 5}    {1, 2, 3, 5, 7}
// Priority 3: {0}        {}        {0, 4, 6}
// Priority 4: {}         {}        {}
// Priority 5: {4, 6}     {6}       {}
// Priority 6: {}         {0, 4}    {}
constexpr Priority kNumPriorities = 7;
constexpr size_t kNumElements = 8;
constexpr size_t kNumQueues = 3;
constexpr Priority kPriorities[kNumQueues][kNumElements] = {
    {3, 2, 1, 1, 5, 2, 5, 1},
    {6, 2, 1, 0, 6, 2, 5, 0},
    {3, 2, 2, 2, 3, 2, 3, 2}};
constexpr int kFirstMinOrder[kNumQueues][kNumElements] = {
    {2, 3, 7, 1, 5, 0, 4, 6},
    {3, 7, 2, 1, 5, 6, 0, 4},
    {1, 2, 3, 5, 7, 0, 4, 6}};
constexpr int kLastMaxOrderErase[kNumQueues][kNumElements] = {
    {6, 4, 0, 5, 1, 7, 3, 2},
    {4, 0, 6, 5, 1, 2, 7, 3},
    {6, 4, 0, 7, 5, 3, 2, 1}};
constexpr int kFirstMaxOrder[kNumQueues][kNumElements] = {
    {4, 6, 0, 1, 5, 2, 3, 7},
    {0, 4, 6, 1, 5, 2, 3, 7},
    {0, 4, 6, 1, 2, 3, 5, 7}};
constexpr int kLastMinOrder[kNumQueues][kNumElements] = {
    {7, 3, 2, 5, 1, 0, 6, 4},
    {7, 3, 2, 5, 1, 6, 4, 0},
    {7, 5, 3, 2, 1, 6, 4, 0}};

class PriorityQueueTest : public testing::TestWithParam<size_t> {
 public:
  PriorityQueueTest() : queue_(kNumPriorities) {}

  void SetUp() override {
    CheckEmpty();
    for (size_t i = 0; i < kNumElements; ++i) {
      EXPECT_EQ(i, queue_.size());
      pointers_[i] =
          queue_.Insert(static_cast<int>(i), kPriorities[GetParam()][i]);
      EXPECT_FALSE(queue_.empty());
    }
    EXPECT_EQ(kNumElements, queue_.size());
  }

  void CheckEmpty() {
    EXPECT_TRUE(queue_.empty());
    EXPECT_EQ(0u, queue_.size());
    EXPECT_TRUE(queue_.FirstMin().is_null());
    EXPECT_TRUE(queue_.LastMin().is_null());
    EXPECT_TRUE(queue_.FirstMax().is_null());
    EXPECT_TRUE(queue_.LastMax().is_null());
  }

 protected:
  PriorityQueue<int> queue_;
  PriorityQueue<int>::Pointer pointers_[kNumElements];
};

TEST_P(PriorityQueueTest, AddAndClear) {
  for (size_t i = 0; i < kNumElements; ++i) {
    EXPECT_EQ(kPriorities[GetParam()][i], pointers_[i].priority());
    EXPECT_EQ(static_cast<int>(i), pointers_[i].value());
  }
  queue_.Clear();
  CheckEmpty();
}

TEST_P(PriorityQueueTest, PointerComparison) {
  for (PriorityQueue<int>::Pointer p = queue_.FirstMax();
       !p.Equals(queue_.LastMin()); p = queue_.GetNextTowardsLastMin(p)) {
    for (PriorityQueue<int>::Pointer q = queue_.GetNextTowardsLastMin(p);
         !q.is_null(); q = queue_.GetNextTowardsLastMin(q)) {
      EXPECT_TRUE(queue_.IsCloserToFirstMaxThan(p, q));
      EXPECT_FALSE(queue_.IsCloserToFirstMaxThan(q, p));
      EXPECT_FALSE(queue_.IsCloserToLastMinThan(p, q));
      EXPECT_TRUE(queue_.IsCloserToLastMinThan(q, p));
      EXPECT_FALSE(p.Equals(q));
    }
  }

  for (PriorityQueue<int>::Pointer p = queue_.LastMin();
       !p.Equals(queue_.FirstMax()); p = queue_.GetPreviousTowardsFirstMax(p)) {
    for (PriorityQueue<int>::Pointer q = queue_.GetPreviousTowardsFirstMax(p);
         !q.is_null(); q = queue_.GetPreviousTowardsFirstMax(q)) {
      EXPECT_FALSE(queue_.IsCloserToFirstMaxThan(p, q));
      EXPECT_TRUE(queue_.IsCloserToFirstMaxThan(q, p));
      EXPECT_TRUE(queue_.IsCloserToLastMinThan(p, q));
      EXPECT_FALSE(queue_.IsCloserToLastMinThan(q, p));
      EXPECT_FALSE(p.Equals(q));
    }
  }
}

TEST_P(PriorityQueueTest, FirstMinOrder) {
  for (size_t i = 0; i < kNumElements; ++i) {
    EXPECT_EQ(kNumElements - i, queue_.size());
    // Also check Equals.
    EXPECT_TRUE(
        queue_.FirstMin().Equals(pointers_[kFirstMinOrder[GetParam()][i]]));
    EXPECT_EQ(kFirstMinOrder[GetParam()][i], queue_.FirstMin().value());
    queue_.Erase(queue_.FirstMin());
  }
  CheckEmpty();
}

TEST_P(PriorityQueueTest, LastMinOrder) {
  for (size_t i = 0; i < kNumElements; ++i) {
    EXPECT_EQ(kLastMinOrder[GetParam()][i], queue_.LastMin().value());
    queue_.Erase(queue_.LastMin());
  }
  CheckEmpty();
}

TEST_P(PriorityQueueTest, FirstMaxOrder) {
  PriorityQueue<int>::Pointer p = queue_.FirstMax();
  size_t i = 0;
  for (; !p.is_null() && i < kNumElements;
       p = queue_.GetNextTowardsLastMin(p), ++i) {
    EXPECT_EQ(kFirstMaxOrder[GetParam()][i], p.value());
  }
  EXPECT_TRUE(p.is_null());
  EXPECT_EQ(kNumElements, i);
  queue_.Clear();
  CheckEmpty();
}

TEST_P(PriorityQueueTest, GetNextTowardsLastMinAndErase) {
  PriorityQueue<int>::Pointer current = queue_.FirstMax();
  for (size_t i = 0; i < kNumElements; ++i) {
    EXPECT_FALSE(current.is_null());
    EXPECT_EQ(kFirstMaxOrder[GetParam()][i], current.value());
    PriorityQueue<int>::Pointer next = queue_.GetNextTowardsLastMin(current);
    queue_.Erase(current);
    current = next;
  }
  EXPECT_TRUE(current.is_null());
  CheckEmpty();
}

TEST_P(PriorityQueueTest, GetPreviousTowardsFirstMaxAndErase) {
  PriorityQueue<int>::Pointer current = queue_.LastMin();
  for (size_t i = 0; i < kNumElements; ++i) {
    EXPECT_FALSE(current.is_null());
    EXPECT_EQ(kLastMinOrder[GetParam()][i], current.value());
    PriorityQueue<int>::Pointer next =
        queue_.GetPreviousTowardsFirstMax(current);
    queue_.Erase(current);
    current = next;
  }
  EXPECT_TRUE(current.is_null());
  CheckEmpty();
}

TEST_P(PriorityQueueTest, FirstMaxOrderErase) {
  for (size_t i = 0; i < kNumElements; ++i) {
    EXPECT_EQ(kFirstMaxOrder[GetParam()][i], queue_.FirstMax().value());
    queue_.Erase(queue_.FirstMax());
  }
  CheckEmpty();
}

TEST_P(PriorityQueueTest, LastMaxOrderErase) {
  for (size_t i = 0; i < kNumElements; ++i) {
    EXPECT_EQ(kLastMaxOrderErase[GetParam()][i], queue_.LastMax().value());
    queue_.Erase(queue_.LastMax());
  }
  CheckEmpty();
}

TEST_P(PriorityQueueTest, EraseFromMiddle) {
  queue_.Erase(pointers_[2]);
  queue_.Erase(pointers_[0]);

  const int expected_order[kNumQueues][kNumElements - 2] = {
      {3, 7, 1, 5, 4, 6}, {3, 7, 1, 5, 6, 4}, {1, 3, 5, 7, 4, 6}};

  for (const auto& value : expected_order[GetParam()]) {
    EXPECT_EQ(value, queue_.FirstMin().value());
    queue_.Erase(queue_.FirstMin());
  }
  CheckEmpty();
}

TEST_P(PriorityQueueTest, InsertAtFront) {
  queue_.InsertAtFront(8, 6);
  queue_.InsertAtFront(9, 2);
  queue_.InsertAtFront(10, 0);
  queue_.InsertAtFront(11, 1);
  queue_.InsertAtFront(12, 1);

  const int expected_order[kNumQueues][kNumElements + 5] = {
      {10, 12, 11, 2, 3, 7, 9, 1, 5, 0, 4, 6, 8},
      {10, 3, 7, 12, 11, 2, 9, 1, 5, 6, 8, 0, 4},
      {10, 12, 11, 9, 1, 2, 3, 5, 7, 0, 4, 6, 8}};

  for (const auto& value : expected_order[GetParam()]) {
    EXPECT_EQ(value, queue_.FirstMin().value());
    queue_.Erase(queue_.FirstMin());
  }
  CheckEmpty();
}

TEST_P(PriorityQueueTest, FindIf) {
  auto pred = [](size_t i, int value) -> bool {
    return value == static_cast<int>(i);
  };
  for (size_t i = 0; i < kNumElements; ++i) {
    PriorityQueue<int>::Pointer pointer =
        queue_.FindIf(base::BindRepeating(pred, i));
    EXPECT_FALSE(pointer.is_null());
    EXPECT_EQ(static_cast<int>(i), pointer.value());
    queue_.Erase(pointer);
    pointer = queue_.FindIf(base::BindRepeating(pred, i));
    EXPECT_TRUE(pointer.is_null());
  }
}

INSTANTIATE_TEST_SUITE_P(PriorityQueues,
                         PriorityQueueTest,
                         testing::Range(static_cast<size_t>(0), kNumQueues));

}  // namespace

}  // namespace net
