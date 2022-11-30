// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/vector_backed_linked_list.h"

#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_test_helper.h"

namespace WTF {

TEST(VectorBackedLinkedListTest, Insert) {
  using List = VectorBackedLinkedList<int>;
  List list;

  EXPECT_TRUE(list.empty());
  EXPECT_TRUE(list.begin() == list.end());
  list.insert(list.end(), 1);
  list.insert(list.begin(), -2);
  list.insert(list.end(), 2);

  List::iterator it = list.begin();
  EXPECT_EQ(*it, -2);
  ++it;
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);

  it = list.insert(++list.begin(), 0);
  list.insert(it, -1);

  EXPECT_EQ(list.front(), -2);
  EXPECT_EQ(list.back(), 2);
  EXPECT_EQ(list.size(), 5u);

  int i = -2;
  for (auto element : list) {
    EXPECT_EQ(element, i);
    i++;
  }
}

TEST(VectorBackedLinkedListTest, PushFront) {
  using List = VectorBackedLinkedList<int>;
  List list;

  EXPECT_TRUE(list.empty());
  list.push_front(3);
  EXPECT_EQ(list.front(), 3);
  list.push_front(2);
  EXPECT_EQ(list.front(), 2);
  list.push_front(1);
  EXPECT_EQ(list.front(), 1);

  int i = 1;
  for (auto element : list) {
    EXPECT_EQ(element, i);
    i++;
  }
}

TEST(VectorBackedLinkedListTest, PushBack) {
  using List = VectorBackedLinkedList<int>;
  List list;

  EXPECT_TRUE(list.empty());
  list.push_back(1);
  EXPECT_EQ(list.back(), 1);
  list.push_back(2);
  EXPECT_EQ(list.back(), 2);
  list.push_back(3);
  EXPECT_EQ(list.back(), 3);

  int i = 1;
  for (auto element : list) {
    EXPECT_EQ(element, i);
    i++;
  }
}

TEST(VectorBackedLinkedListTest, MoveTo) {
  using List = VectorBackedLinkedList<int>;
  List list;

  list.push_back(1);
  list.MoveTo(list.begin(), list.end());
  List::iterator it = list.begin();
  EXPECT_EQ(*it, 1);
  list.push_back(2);
  list.push_back(3);

  List::iterator target = list.begin();
  list.MoveTo(target, list.end());  // {2, 3, 1}

  it = list.begin();
  EXPECT_EQ(*it, 2);
  ++it;
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_EQ(*it, 1);
  --it;

  target = it;
  list.MoveTo(target, list.begin());  // {3, 2, 1}
  it = list.begin();
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_EQ(*it, 2);
  ++it;
  EXPECT_EQ(*it, 1);

  target = it;
  list.MoveTo(target, --it);  // {3, 1, 2}
  it = list.begin();
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);

  list.MoveTo(list.begin(), list.begin());
  it = list.begin();
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);

  target = list.begin();
  List::iterator position = ++list.begin();
  list.MoveTo(target, position);
  it = list.begin();
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);
}

TEST(VectorBackedLinkedListTest, Erase) {
  using List = VectorBackedLinkedList<int>;
  List list;

  List::iterator it = list.insert(list.end(), 1);
  EXPECT_EQ(*it, 1);
  list.push_back(2);
  list.push_back(3);
  list.push_back(4);
  list.push_back(5);
  EXPECT_EQ(list.size(), 5u);

  int i = 1;
  for (auto element : list) {
    EXPECT_EQ(element, i);
    i++;
  }

  List::iterator target = list.begin();
  ++target;
  it = list.erase(target);  // list = {1, 3, 4, 5}
  EXPECT_EQ(*it, 3);
  EXPECT_EQ(list.size(), 4u);
  it = list.erase(++it);  // list = {1, 3, 5}
  EXPECT_EQ(*it, 5);
  EXPECT_EQ(list.size(), 3u);

  it = list.erase(list.begin());  // list = {3, 5}
  EXPECT_EQ(*it, 3);
  EXPECT_EQ(list.size(), 2u);

  it = list.begin();
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_EQ(*it, 5);
  ++it;
  EXPECT_TRUE(it == list.end());

  list.push_back(6);
  EXPECT_EQ(list.front(), 3);
  EXPECT_EQ(list.back(), 6);
}

TEST(VectorBackedLinkedListTest, PopFront) {
  using List = VectorBackedLinkedList<int>;
  List list;

  list.push_back(1);
  list.push_back(2);
  list.push_back(3);

  int i = 1;
  for (auto element : list) {
    EXPECT_EQ(element, i);
    i++;
  }

  list.pop_front();
  EXPECT_EQ(list.front(), 2);
  EXPECT_EQ(list.back(), 3);
  EXPECT_EQ(list.size(), 2u);

  list.pop_front();
  EXPECT_EQ(list.front(), 3);
  EXPECT_EQ(list.back(), 3);
  EXPECT_EQ(list.size(), 1u);

  list.pop_front();
  EXPECT_TRUE(list.empty());
}

TEST(VectorBackedLinkedListTest, PopBack) {
  using List = VectorBackedLinkedList<int>;
  List list;

  list.push_back(1);
  list.push_back(2);
  list.push_back(3);

  list.pop_back();
  EXPECT_EQ(list.front(), 1);
  EXPECT_EQ(list.back(), 2);
  EXPECT_EQ(list.size(), 2u);

  list.pop_back();
  EXPECT_EQ(list.front(), 1);
  EXPECT_EQ(list.back(), 1);
  EXPECT_EQ(list.size(), 1u);

  list.pop_back();
  EXPECT_TRUE(list.empty());
}

TEST(VectorBackedLinkedListTest, Clear) {
  using List = VectorBackedLinkedList<int>;
  List list;

  list.push_back(1);
  list.push_back(2);
  list.push_back(3);

  EXPECT_EQ(list.size(), 3u);

  list.clear();
  EXPECT_EQ(list.size(), 0u);
  EXPECT_TRUE(list.empty());

  EXPECT_TRUE(list.begin() == list.end());
  list.push_back(1);
  EXPECT_EQ(list.front(), 1);
  EXPECT_EQ(list.back(), 1);
  EXPECT_EQ(list.size(), 1u);
}

TEST(VectorBackedLinkedListTest, Iterator) {
  using List = VectorBackedLinkedList<int>;
  List list;

  list.push_back(1);
  list.push_back(2);
  list.push_back(3);

  List::iterator it = list.begin();

  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);
  ++it;
  EXPECT_EQ(*it, 3);
  *it = 4;  // list: {1, 2, 4}
  EXPECT_EQ(list.back(), 4);
  ++it;
  EXPECT_TRUE(it == list.end());
  --it;
  --it;
  --it;
  EXPECT_TRUE(it == list.begin());
  EXPECT_EQ(list.front(), 1);
  *it = 0;
  EXPECT_EQ(list.front(), 0);  // list: {0, 2, 4}

  List::reverse_iterator rit = list.rbegin();

  EXPECT_EQ(*rit, 4);
  ++rit;
  EXPECT_EQ(*rit, 2);
  ++rit;
  EXPECT_EQ(*rit, 0);
  EXPECT_FALSE(rit == list.rend());
  *rit = 1;  // list: {1, 2, 4}
  EXPECT_EQ(list.front(), 1);
  ++rit;
  EXPECT_TRUE(rit == list.rend());
  --rit;
  EXPECT_EQ(*rit, 1);
}

TEST(VectorBackedLinkedListTest, ConstIterator) {
  using List = VectorBackedLinkedList<int>;
  List list;

  list.push_back(1);
  list.push_back(2);
  list.push_back(3);

  List::const_iterator cit = list.cbegin();

  EXPECT_EQ(*cit, 1);
  ++cit;
  EXPECT_EQ(*cit, 2);
  ++cit;
  EXPECT_EQ(*cit, 3);
  ++cit;
  EXPECT_TRUE(cit == list.cend());
  --cit;
  --cit;
  --cit;
  EXPECT_TRUE(cit == list.cbegin());
  EXPECT_EQ(list.front(), 1);

  List::const_reverse_iterator crit = list.crbegin();

  EXPECT_EQ(*crit, 3);
  ++crit;
  EXPECT_EQ(*crit, 2);
  ++crit;
  EXPECT_EQ(*crit, 1);
  ++crit;
  EXPECT_TRUE(crit == list.crend());
  --crit;
  EXPECT_EQ(*crit, 1);
}

TEST(VectorBackedLinkedListTest, String) {
  using List = VectorBackedLinkedList<String>;
  List list;

  EXPECT_TRUE(list.empty());

  list.push_back("b");
  list.push_front("a");
  list.push_back("c");

  EXPECT_EQ(list.front(), "a");
  EXPECT_EQ(list.back(), "c");
  EXPECT_EQ(list.size(), 3u);

  List::iterator it = list.begin();
  EXPECT_EQ(*it, "a");
  ++it;
  EXPECT_EQ(*it, "b");
  List::iterator target = it;
  ++it;
  EXPECT_EQ(*it, "c");
  ++it;
  EXPECT_TRUE(it == list.end());
  --it;
  EXPECT_EQ(*it, "c");
  --it;
  --it;
  EXPECT_TRUE(it == list.begin());

  list.erase(target);
  it = list.begin();
  EXPECT_EQ(*it, "a");
  ++it;
  EXPECT_EQ(*it, "c");
  ++it;
  EXPECT_TRUE(it == list.end());

  list.pop_back();
  EXPECT_EQ(list.front(), "a");
  EXPECT_EQ(list.back(), "a");
  EXPECT_EQ(list.size(), 1u);

  list.push_front("c");
  it = list.begin();
  EXPECT_EQ(*it, "c");
  ++it;
  EXPECT_EQ(*it, "a");
  ++it;
  EXPECT_TRUE(it == list.end());

  list.clear();
  EXPECT_TRUE(list.empty());
  EXPECT_TRUE(list.begin() == list.end());

  list.push_front("a");
  EXPECT_EQ(list.size(), 1u);
  EXPECT_EQ(list.front(), "a");
  list.pop_back();
  EXPECT_TRUE(list.empty());
}

TEST(VectorBackedLinkedListTest, UniquePtr) {
  using List = VectorBackedLinkedList<std::unique_ptr<Dummy>>;
  List list;

  bool deleted1 = false, deleted2 = false, deleted3 = false;
  std::unique_ptr<Dummy> ptr1 = std::make_unique<Dummy>(deleted1);
  std::unique_ptr<Dummy> ptr2 = std::make_unique<Dummy>(deleted2);
  std::unique_ptr<Dummy> ptr3 = std::make_unique<Dummy>(deleted3);

  Dummy* raw_ptr1 = ptr1.get();
  Dummy* raw_ptr2 = ptr2.get();
  Dummy* raw_ptr3 = ptr3.get();

  list.push_front(std::move(ptr1));
  list.push_back(std::move(ptr3));
  List::iterator it = list.begin();
  ++it;
  it = list.insert(it, std::move(ptr2));
  EXPECT_EQ(it->get(), raw_ptr2);

  EXPECT_EQ(list.size(), 3u);
  EXPECT_EQ((list.front()).get(), raw_ptr1);
  EXPECT_EQ((list.back()).get(), raw_ptr3);

  it = list.begin();
  EXPECT_EQ(it->get(), raw_ptr1);
  ++it;
  EXPECT_EQ(it->get(), raw_ptr2);
  List::iterator target = it;
  ++it;
  EXPECT_EQ(it->get(), raw_ptr3);
  ++it;
  EXPECT_TRUE(it == list.end());
  --it;
  EXPECT_EQ(it->get(), raw_ptr3);
  --it;
  --it;
  EXPECT_TRUE(it == list.begin());

  list.erase(target);
  EXPECT_FALSE(deleted1);
  EXPECT_TRUE(deleted2);
  EXPECT_FALSE(deleted3);
  EXPECT_EQ(list.size(), 2u);
  it = list.begin();
  EXPECT_EQ(it->get(), raw_ptr1);
  ++it;
  EXPECT_EQ(it->get(), raw_ptr3);
  ++it;
  EXPECT_TRUE(it == list.end());

  list.pop_front();
  EXPECT_TRUE(deleted1);
  EXPECT_TRUE(deleted2);
  EXPECT_FALSE(deleted3);
  EXPECT_EQ(list.size(), 1u);
  it = list.begin();
  EXPECT_EQ(it->get(), raw_ptr3);
  ++it;
  EXPECT_TRUE(it == list.end());

  list.pop_back();
  EXPECT_TRUE(deleted1);
  EXPECT_TRUE(deleted2);
  EXPECT_TRUE(deleted3);
  EXPECT_TRUE(list.empty());

  bool deleted4 = false, deleted5 = false, deleted6 = false;
  std::unique_ptr<Dummy> ptr4 = std::make_unique<Dummy>(deleted4);
  std::unique_ptr<Dummy> ptr5 = std::make_unique<Dummy>(deleted5);
  std::unique_ptr<Dummy> ptr6 = std::make_unique<Dummy>(deleted6);

  Dummy* raw_ptr4 = ptr4.get();
  Dummy* raw_ptr5 = ptr5.get();
  Dummy* raw_ptr6 = ptr6.get();

  list.push_back(std::move(ptr4));
  list.push_back(std::move(ptr5));
  list.push_back(std::move(ptr6));

  it = list.end();
  --it;
  list.MoveTo(list.begin(), it);
  it = list.begin();
  EXPECT_EQ(it->get(), raw_ptr5);
  ++it;
  EXPECT_EQ(it->get(), raw_ptr4);
  ++it;
  EXPECT_EQ(it->get(), raw_ptr6);

  list.MoveTo(list.begin(), list.begin());
  it = list.begin();
  EXPECT_EQ(it->get(), raw_ptr5);
  ++it;
  EXPECT_EQ(it->get(), raw_ptr4);
  ++it;
  EXPECT_EQ(it->get(), raw_ptr6);

  EXPECT_FALSE(deleted4);
  EXPECT_FALSE(deleted5);
  EXPECT_FALSE(deleted6);

  list.clear();
  EXPECT_TRUE(list.empty());
  EXPECT_EQ(list.size(), 0u);

  EXPECT_TRUE(deleted4);
  EXPECT_TRUE(deleted5);
  EXPECT_TRUE(deleted6);
}

}  // namespace WTF
