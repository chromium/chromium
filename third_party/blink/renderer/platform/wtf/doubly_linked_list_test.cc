// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/doubly_linked_list.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf_test_helper.h"

namespace WTF {

namespace {

static size_t test_node_counter = 0;

class TestNode final : public DoublyLinkedListNode<TestNode> {
  USING_FAST_MALLOC(TestNode);
  friend class WTF::DoublyLinkedListNode<TestNode>;

 public:
  TestNode(int i) : i_(i) { ++test_node_counter; }
  ~TestNode() { --test_node_counter; }
  int i() { return i_; }

 private:
  int i_{0};
  TestNode* next_{nullptr};
  TestNode* prev_{nullptr};
};

class DoublyLinkedListTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

  static int CompareInt(TestNode*, TestNode*);
  bool IsSorted() const;
  DoublyLinkedList<TestNode>& List() { return list_; }
  DoublyLinkedList<TestNode>::AddResult CheckedInsert(int i);

 protected:
  DoublyLinkedList<TestNode> list_;
};

void DoublyLinkedListTest::SetUp() {
  EXPECT_TRUE(list_.empty());
  EXPECT_EQ(0ul, list_.size());
  EXPECT_EQ(0ul, test_node_counter);
}

void DoublyLinkedListTest::TearDown() {
  while (!list_.empty())
    delete list_.RemoveHead();
  EXPECT_EQ(0ul, test_node_counter);
}

int DoublyLinkedListTest::CompareInt(TestNode* first, TestNode* second) {
  return first->i() - second->i();
}

bool DoublyLinkedListTest::IsSorted() const {
  for (auto* node = list_.Head(); node && node->Next(); node = node->Next()) {
    if (node->i() >= node->Next()->i())
      return false;
  }
  return true;
}

DoublyLinkedList<TestNode>::AddResult DoublyLinkedListTest::CheckedInsert(
    int i) {
  size_t current_size = list_.size();

  auto result = list_.Insert(std::make_unique<TestNode>(i), CompareInt);
  EXPECT_EQ(list_.size(),
            result.is_new_entry ? current_size + 1 : current_size);
  EXPECT_EQ(test_node_counter,
            result.is_new_entry ? current_size + 1 : current_size);
  EXPECT_FALSE(list_.empty());
  return result;
}

TEST_F(DoublyLinkedListTest, InsertEmpty) {
  CheckedInsert(1);
  EXPECT_EQ(list_.Head(), list_.Tail());

  auto* node_heap = list_.RemoveHead();
  EXPECT_EQ(0ul, list_.size());
  EXPECT_EQ(1ul, test_node_counter);
  EXPECT_TRUE(list_.empty());

  delete node_heap;
  EXPECT_EQ(0ul, test_node_counter);

  list_.InsertAfter(std::make_unique<TestNode>(0), nullptr);
  EXPECT_EQ(1ul, list_.size());
  EXPECT_EQ(1ul, test_node_counter);
  EXPECT_FALSE(list_.empty());
  delete list_.RemoveHead();

  TestNode node_stack(-1);
  list_.Insert(&node_stack, CompareInt);
  EXPECT_EQ(1ul, list_.size());
  EXPECT_EQ(1ul, test_node_counter);
  EXPECT_EQ(list_.Head(), list_.Tail());
  EXPECT_FALSE(list_.empty());

  list_.Remove(&node_stack);
  EXPECT_EQ(0ul, list_.size());
  EXPECT_EQ(1ul, test_node_counter);
  EXPECT_TRUE(list_.empty());
}

TEST_F(DoublyLinkedListTest, InsertRandom) {
  const size_t num_items = 6;
  int items[6] = {2, -1, 3, 4, 0, 1};

  for (int item : items) {
    auto result = list_.Insert(std::make_unique<TestNode>(item), CompareInt);
    EXPECT_TRUE(result.is_new_entry);
  }
  EXPECT_EQ(num_items, list_.size());
  EXPECT_EQ(num_items, test_node_counter);
  EXPECT_NE(list_.Head(), list_.Tail());
  EXPECT_FALSE(list_.empty());

  EXPECT_TRUE(IsSorted());
}

TEST_F(DoublyLinkedListTest, InsertSorted) {
  const size_t num_items = 6;
  int items[6] = {0, 1, 2, 3, 4, 5};

  for (int item : items) {
    auto result = list_.Insert(std::make_unique<TestNode>(item), CompareInt);
    EXPECT_TRUE(result.is_new_entry);
  }
  EXPECT_EQ(num_items, list_.size());
  EXPECT_EQ(num_items, test_node_counter);
  EXPECT_NE(list_.Head(), list_.Tail());
  EXPECT_FALSE(list_.empty());

  EXPECT_TRUE(IsSorted());
}

TEST_F(DoublyLinkedListTest, InsertAfter) {
  auto begin_result = CheckedInsert(0);
  EXPECT_EQ(list_.Head(), list_.Tail());

  auto end_result =
      list_.InsertAfter(std::make_unique<TestNode>(10), begin_result.node);
  EXPECT_EQ(2ul, list_.size());
  EXPECT_EQ(2ul, test_node_counter);
  EXPECT_FALSE(list_.empty());
  EXPECT_TRUE(IsSorted());
  EXPECT_EQ(end_result.node, list_.Tail());

  auto center_result =
      list_.InsertAfter(std::make_unique<TestNode>(5), begin_result.node);
  EXPECT_EQ(3ul, list_.size());
  EXPECT_EQ(3ul, test_node_counter);
  EXPECT_TRUE(IsSorted());
  EXPECT_NE(center_result.node, list_.Head());
  EXPECT_NE(center_result.node, list_.Tail());

  auto new_end_result =
      list_.InsertAfter(std::make_unique<TestNode>(20), end_result.node);
  EXPECT_EQ(4ul, list_.size());
  EXPECT_EQ(4ul, test_node_counter);
  EXPECT_TRUE(IsSorted());
  EXPECT_EQ(new_end_result.node, list_.Tail());
}

TEST_F(DoublyLinkedListTest, InsertDup) {
  CheckedInsert(0);
  CheckedInsert(0);
  CheckedInsert(1);
  CheckedInsert(1);
  CheckedInsert(0);
}

TEST_F(DoublyLinkedListTest, InsertAfterDup) {
  // InsertAfter does not guarantee neither sorting nor uniqueness.
  auto result = list_.InsertAfter(std::make_unique<TestNode>(0), nullptr);
  EXPECT_EQ(1ul, list_.size());
  EXPECT_EQ(1ul, test_node_counter);
  EXPECT_FALSE(list_.empty());
  EXPECT_TRUE(IsSorted());
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(result.node, list_.Head());
  EXPECT_EQ(result.node, list_.Tail());

  result = list_.InsertAfter(std::make_unique<TestNode>(0), list_.Head());
  EXPECT_EQ(2ul, list_.size());
  EXPECT_EQ(2ul, test_node_counter);
  EXPECT_FALSE(list_.empty());
  EXPECT_FALSE(IsSorted());
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_NE(result.node, list_.Head());
  EXPECT_EQ(result.node, list_.Tail());

  result = list_.InsertAfter(std::make_unique<TestNode>(1), list_.Head());
  EXPECT_EQ(3ul, list_.size());
  EXPECT_EQ(3ul, test_node_counter);
  EXPECT_FALSE(list_.empty());
  EXPECT_FALSE(IsSorted());
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_NE(result.node, list_.Head());
  EXPECT_NE(result.node, list_.Tail());

  result = list_.InsertAfter(std::make_unique<TestNode>(1), list_.Tail());
  EXPECT_EQ(4ul, list_.size());
  EXPECT_EQ(4ul, test_node_counter);
  EXPECT_FALSE(list_.empty());
  EXPECT_FALSE(IsSorted());
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_NE(result.node, list_.Head());
  EXPECT_EQ(result.node, list_.Tail());
}

}  // anonymous namespace

}  // namespace WTF
