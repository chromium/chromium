// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_test_helper.h"

namespace WTF {

static_assert(!WTF::IsTraceable<LinkedHashSet<int>>::value,
              "LinkedHashSet must not be traceable.");
static_assert(!WTF::IsTraceable<LinkedHashSet<String>>::value,
              "LinkedHashSet must not be traceable.");

template <typename T>
int* const ValueInstanceCount<T>::kDeletedValue =
    reinterpret_cast<int*>(static_cast<uintptr_t>(-1));

TEST(LinkedHashSetTest, CopyConstructAndAssignInt) {
  using Set = LinkedHashSet<ValueInstanceCount<int>>;
  // Declare the counters before the set, because they have to outlive teh set.
  int counter1 = 0;
  int counter2 = 0;
  int counter3 = 0;
  Set set1;
  EXPECT_EQ(set1.size(), 0u);
  EXPECT_TRUE(set1.empty());
  set1.insert(ValueInstanceCount<int>(&counter1, 1));
  set1.insert(ValueInstanceCount<int>(&counter2, 2));
  set1.insert(ValueInstanceCount<int>(&counter3, 3));
  EXPECT_EQ(set1.size(), 3u);
  Set set2(set1);
  EXPECT_EQ(set2.size(), 3u);
  Set set3;
  EXPECT_EQ(set3.size(), 0u);
  set3 = set2;
  EXPECT_EQ(set3.size(), 3u);
  auto it1 = set1.begin();
  auto it2 = set2.begin();
  auto it3 = set3.begin();
  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(it1->Value(), i + 1);
    EXPECT_EQ(it2->Value(), i + 1);
    EXPECT_EQ(it3->Value(), i + 1);
    ++it1;
    ++it2;
    ++it3;
  }

  // Each object is now in all 3 sets.
  // Count 2x because each set uses hash map and vector.
  EXPECT_EQ(counter1, 6);
  EXPECT_EQ(counter2, 6);
  EXPECT_EQ(counter3, 6);
}

TEST(LinkedHashSetTest, CopyConstructAndAssignIntPtr) {
  using Set = LinkedHashSet<int*>;
  Set set1;
  EXPECT_EQ(set1.size(), 0u);
  EXPECT_TRUE(set1.empty());
  std::unique_ptr<int> int1 = std::make_unique<int>(1);
  std::unique_ptr<int> int2 = std::make_unique<int>(2);
  std::unique_ptr<int> int3 = std::make_unique<int>(3);
  set1.insert(int1.get());
  set1.insert(int2.get());
  set1.insert(int3.get());
  EXPECT_EQ(set1.size(), 3u);
  Set set2(set1);
  EXPECT_EQ(set2.size(), 3u);
  Set set3;
  EXPECT_EQ(set3.size(), 0u);
  set3 = set2;
  EXPECT_EQ(set3.size(), 3u);
  auto it1 = set1.begin();
  auto it2 = set2.begin();
  auto it3 = set3.begin();
  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(**it1, i + 1);
    EXPECT_EQ(**it2, i + 1);
    EXPECT_EQ(**it3, i + 1);
    ++it1;
    ++it2;
    ++it3;
  }

  // Changing the pointed values in one set should change it in all sets.
  for (int* ptr : set1)
    *ptr += 1000;
  it1 = set1.begin();
  it2 = set2.begin();
  it3 = set3.begin();
  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(**it1, i + 1001);
    EXPECT_EQ(**it2, i + 1001);
    EXPECT_EQ(**it3, i + 1001);
    ++it1;
    ++it2;
    ++it3;
  }
}

TEST(LinkedHashSetTest, CopyConstructAndAssignString) {
  using Set = LinkedHashSet<String>;
  Set set1;
  EXPECT_EQ(set1.size(), 0u);
  EXPECT_TRUE(set1.empty());
  set1.insert("1");
  set1.insert("2");
  set1.insert("3");
  EXPECT_EQ(set1.size(), 3u);
  Set set2(set1);
  EXPECT_EQ(set2.size(), 3u);
  Set set3;
  EXPECT_EQ(set3.size(), 0u);
  set3 = set2;
  EXPECT_EQ(set3.size(), 3u);
  auto it1 = set1.begin();
  auto it2 = set2.begin();
  auto it3 = set3.begin();
  for (char16_t i = '1'; i < '4'; i++) {
    EXPECT_EQ(*it1, String(Vector<UChar>({i})));
    EXPECT_EQ(*it2, String(Vector<UChar>({i})));
    EXPECT_EQ(*it3, String(Vector<UChar>({i})));
    ++it1;
    ++it2;
    ++it3;
  }

  // Changing one set should not affect the others.
  set1.clear();
  set1.insert("11");
  set1.insert("12");
  set1.insert("13");
  it1 = set1.begin();
  it2 = set2.begin();
  it3 = set3.begin();
  for (char16_t i = '1'; i < '4'; i++) {
    EXPECT_EQ(*it1, String(Vector<UChar>({'1', i})));
    EXPECT_EQ(*it2, String(Vector<UChar>({i})));
    EXPECT_EQ(*it3, String(Vector<UChar>({i})));
    ++it1;
    ++it2;
    ++it3;
  }
}

TEST(LinkedHashSetTest, MoveConstructAndAssignInt) {
  using Set = LinkedHashSet<ValueInstanceCount<int>>;
  int counter1 = 0;
  int counter2 = 0;
  int counter3 = 0;
  Set set1;
  EXPECT_EQ(set1.size(), 0u);
  EXPECT_TRUE(set1.empty());
  set1.insert(ValueInstanceCount<int>(&counter1, 1));
  set1.insert(ValueInstanceCount<int>(&counter2, 2));
  set1.insert(ValueInstanceCount<int>(&counter3, 3));
  EXPECT_EQ(set1.size(), 3u);
  Set set2(std::move(set1));
  EXPECT_EQ(set2.size(), 3u);
  Set set3;
  EXPECT_EQ(set3.size(), 0u);
  set3 = std::move(set2);
  EXPECT_EQ(set3.size(), 3u);
  auto it = set3.begin();
  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(it->Value(), i + 1);
    ++it;
  }

  // Only move constructors were used, each object is only in set3.
  // Count 2x because each set uses hash map and vector.
  EXPECT_EQ(counter1, 2);
  EXPECT_EQ(counter2, 2);
  EXPECT_EQ(counter3, 2);

  Set set4(set3);
  // Copy constructor was used, each object is in set3 and set4.
  EXPECT_EQ(counter1, 4);
  EXPECT_EQ(counter2, 4);
  EXPECT_EQ(counter3, 4);
}

TEST(LinkedHashSetTest, MoveConstructAndAssignString) {
  using Set = LinkedHashSet<ValueInstanceCount<String>>;
  int counter1 = 0;
  int counter2 = 0;
  int counter3 = 0;
  Set set1;
  EXPECT_EQ(set1.size(), 0u);
  EXPECT_TRUE(set1.empty());
  set1.insert(ValueInstanceCount<String>(&counter1, "1"));
  set1.insert(ValueInstanceCount<String>(&counter2, "2"));
  set1.insert(ValueInstanceCount<String>(&counter3, "3"));
  EXPECT_EQ(set1.size(), 3u);
  Set set2(std::move(set1));
  EXPECT_EQ(set2.size(), 3u);
  Set set3;
  EXPECT_EQ(set3.size(), 0u);
  set3 = std::move(set2);
  EXPECT_EQ(set3.size(), 3u);
  auto it = set3.begin();
  for (char16_t i = '1'; i < '4'; i++) {
    EXPECT_EQ(it->Value(), String(Vector<UChar>({i})));
    ++it;
  }

  // Only move constructors were used, each object is only in set3.
  // Count 2x because each set uses hash map and vector.
  EXPECT_EQ(counter1, 2);
  EXPECT_EQ(counter2, 2);
  EXPECT_EQ(counter3, 2);

  Set set4(set3);
  // Copy constructor was used, each object is in set3 and set4.
  EXPECT_EQ(counter1, 4);
  EXPECT_EQ(counter2, 4);
  EXPECT_EQ(counter3, 4);
}

struct CustomHashTraitsForInt : public IntHashTraits<int, INT_MAX, INT_MIN> {};

TEST(LinkedHashSetTest, BeginEnd) {
  using Set = LinkedHashSet<int, CustomHashTraitsForInt>;
  Set set;
  EXPECT_EQ(set.begin(), set.end());
  EXPECT_EQ(set.rbegin(), set.rend());

  set.insert(1);
  EXPECT_EQ(*set.begin(), 1);
  EXPECT_NE(set.begin(), set.end());
  EXPECT_EQ(*set.rbegin(), 1);
  EXPECT_NE(set.rbegin(), set.rend());

  set.insert(2);
  EXPECT_EQ(*set.begin(), 1);
  EXPECT_NE(set.begin(), set.end());
  EXPECT_EQ(*set.rbegin(), 2);
  EXPECT_NE(set.rbegin(), set.rend());

  set.insert(3);
  EXPECT_EQ(*set.begin(), 1);
  EXPECT_NE(set.begin(), set.end());
  EXPECT_EQ(*set.rbegin(), 3);
  EXPECT_NE(set.rbegin(), set.rend());

  set.erase(2);
  EXPECT_EQ(*set.begin(), 1);
  EXPECT_NE(set.begin(), set.end());
  EXPECT_EQ(*set.rbegin(), 3);
  EXPECT_NE(set.rbegin(), set.rend());

  set.erase(1);
  EXPECT_EQ(*set.begin(), 3);
  EXPECT_NE(set.begin(), set.end());
  EXPECT_EQ(*set.rbegin(), 3);
  EXPECT_NE(set.rbegin(), set.rend());

  set.erase(3);
  EXPECT_EQ(set.begin(), set.end());
  EXPECT_EQ(set.rbegin(), set.rend());
}

TEST(LinkedHashSetTest, IteratorPre) {
  using Set = LinkedHashSet<int, CustomHashTraitsForInt>;
  Set set;

  set.insert(1);
  {
    auto it = set.begin();
    EXPECT_EQ(1, *it);
    EXPECT_EQ(set.end(), ++it);
  }
  {
    auto it = set.end();
    EXPECT_EQ(1, *--it);
    EXPECT_EQ(set.begin(), it);
  }

  set.insert(2);
  {
    auto it = set.begin();
    EXPECT_EQ(1, *it);
    EXPECT_EQ(2, *++it);
    EXPECT_EQ(set.end(), ++it);
  }
  {
    auto it = set.end();
    EXPECT_EQ(2, *--it);
    EXPECT_EQ(1, *--it);
    EXPECT_EQ(set.begin(), it);
  }

  set.insert(3);
  {
    auto it = set.begin();
    EXPECT_EQ(1, *it);
    EXPECT_EQ(2, *++it);
    EXPECT_EQ(3, *++it);
    EXPECT_EQ(set.end(), ++it);
  }
  {
    auto it = set.end();
    EXPECT_EQ(3, *--it);
    EXPECT_EQ(2, *--it);
    EXPECT_EQ(1, *--it);
    EXPECT_EQ(set.begin(), it);
  }
}

TEST(LinkedHashSetTest, ReverseIteratorPre) {
  using Set = LinkedHashSet<int, CustomHashTraitsForInt>;
  Set set;

  set.insert(1);
  {
    auto it = set.rbegin();
    EXPECT_EQ(1, *it);
    EXPECT_EQ(set.rend(), ++it);
  }
  {
    auto it = set.rend();
    EXPECT_EQ(1, *--it);
    EXPECT_EQ(set.rbegin(), it);
  }

  set.insert(2);
  {
    auto it = set.rbegin();
    EXPECT_EQ(2, *it);
    EXPECT_EQ(1, *++it);
    EXPECT_EQ(set.rend(), ++it);
  }
  {
    auto it = set.rend();
    EXPECT_EQ(1, *--it);
    EXPECT_EQ(2, *--it);
    EXPECT_EQ(set.rbegin(), it);
  }

  set.insert(3);
  {
    auto it = set.rbegin();
    EXPECT_EQ(3, *it);
    EXPECT_EQ(2, *++it);
    EXPECT_EQ(1, *++it);
    EXPECT_EQ(set.rend(), ++it);
  }
  {
    auto it = set.rend();
    EXPECT_EQ(1, *--it);
    EXPECT_EQ(2, *--it);
    EXPECT_EQ(3, *--it);
    EXPECT_EQ(set.rbegin(), it);
  }
}

TEST(LinkedHashSetTest, IteratorPost) {
  using Set = LinkedHashSet<int, CustomHashTraitsForInt>;
  Set set;

  set.insert(1);
  {
    auto it = set.begin();
    EXPECT_EQ(1, *it++);
    EXPECT_EQ(set.end(), it);
  }
  {
    auto it = set.end();
    it--;
    EXPECT_EQ(1, *it);
    EXPECT_EQ(set.begin(), it);
  }

  set.insert(2);
  {
    auto it = set.begin();
    EXPECT_EQ(1, *it++);
    EXPECT_EQ(2, *it++);
    EXPECT_EQ(set.end(), it);
  }
  {
    auto it = set.end();
    it--;
    EXPECT_EQ(2, *it--);
    EXPECT_EQ(1, *it);
    EXPECT_EQ(set.begin(), it);
  }

  set.insert(3);
  {
    auto it = set.begin();
    EXPECT_EQ(1, *it++);
    EXPECT_EQ(2, *it++);
    EXPECT_EQ(3, *it++);
    EXPECT_EQ(set.end(), it);
  }
  {
    auto it = set.end();
    it--;
    EXPECT_EQ(3, *it--);
    EXPECT_EQ(2, *it--);
    EXPECT_EQ(1, *it);
    EXPECT_EQ(set.begin(), it);
  }
}

TEST(LinkedHashSetTest, ReverseIteratorPost) {
  using Set = LinkedHashSet<int, CustomHashTraitsForInt>;
  Set set;

  set.insert(1);
  {
    auto it = set.rbegin();
    EXPECT_EQ(1, *it++);
    EXPECT_EQ(set.rend(), it);
  }
  {
    auto it = set.rend();
    it--;
    EXPECT_EQ(1, *it);
    EXPECT_EQ(set.rbegin(), it);
  }

  set.insert(2);
  {
    auto it = set.rbegin();
    EXPECT_EQ(2, *it++);
    EXPECT_EQ(1, *it++);
    EXPECT_EQ(set.rend(), it);
  }
  {
    auto it = set.rend();
    it--;
    EXPECT_EQ(1, *it--);
    EXPECT_EQ(2, *it);
    EXPECT_EQ(set.rbegin(), it);
  }

  set.insert(3);
  {
    auto it = set.rbegin();
    EXPECT_EQ(3, *it++);
    EXPECT_EQ(2, *it++);
    EXPECT_EQ(1, *it++);
    EXPECT_EQ(set.rend(), it);
  }
  {
    auto it = set.rend();
    it--;
    EXPECT_EQ(1, *it--);
    EXPECT_EQ(2, *it--);
    EXPECT_EQ(3, *it);
    EXPECT_EQ(set.rbegin(), it);
  }
}

TEST(LinkedHashSetTest, FrontAndBack) {
  using Set = LinkedHashSet<int, CustomHashTraitsForInt>;
  Set set;
  EXPECT_EQ(set.size(), 0u);
  EXPECT_TRUE(set.empty());

  set.PrependOrMoveToFirst(1);
  EXPECT_EQ(set.front(), 1);
  EXPECT_EQ(set.back(), 1);

  set.insert(2);
  EXPECT_EQ(set.front(), 1);
  EXPECT_EQ(set.back(), 2);

  set.AppendOrMoveToLast(3);
  EXPECT_EQ(set.front(), 1);
  EXPECT_EQ(set.back(), 3);

  set.PrependOrMoveToFirst(3);
  EXPECT_EQ(set.front(), 3);
  EXPECT_EQ(set.back(), 2);

  set.AppendOrMoveToLast(1);
  EXPECT_EQ(set.front(), 3);
  EXPECT_EQ(set.back(), 1);
}

TEST(LinkedHashSetTest, Contains) {
  using Set = LinkedHashSet<int, CustomHashTraitsForInt>;
  Set set;
  set.insert(-1);
  set.insert(0);
  set.insert(1);
  set.insert(2);
  set.insert(3);

  EXPECT_TRUE(set.Contains(-1));
  EXPECT_TRUE(set.Contains(0));
  EXPECT_TRUE(set.Contains(1));
  EXPECT_TRUE(set.Contains(2));
  EXPECT_TRUE(set.Contains(3));

  EXPECT_FALSE(set.Contains(10));
}

TEST(LinkedHashSetTest, Find) {
  using Set = LinkedHashSet<int, CustomHashTraitsForInt>;
  Set set;
  set.insert(-1);
  set.insert(0);
  set.insert(1);
  set.insert(2);
  set.insert(3);

  {
    const Set& ref = set;
    Set::const_iterator it = ref.find(2);
    EXPECT_EQ(2, *it);
    ++it;
    EXPECT_EQ(3, *it);
    --it;
    --it;
    EXPECT_EQ(1, *it);
  }
  {
    Set& ref = set;
    Set::iterator it = ref.find(2);
    EXPECT_EQ(2, *it);
    ++it;
    EXPECT_EQ(3, *it);
    --it;
    --it;
    EXPECT_EQ(1, *it);
  }
  Set::iterator it = set.find(10);
  EXPECT_TRUE(it == set.end());
}

TEST(LinkedHashSetTest, Insert) {
  using Set = LinkedHashSet<int, CustomHashTraitsForInt>;
  Set set;
  Set::AddResult result = set.insert(1);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);

  result = set.insert(1);
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);

  result = set.insert(2);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 2);

  result = set.insert(3);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 3);

  result = set.insert(2);
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 2);

  Set::const_iterator it = set.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);
  ++it;
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_TRUE(it == set.end());
}

TEST(LinkedHashSetTest, InsertBefore) {
  using Set = LinkedHashSet<int, CustomHashTraitsForInt>;
  Set set;
  set.insert(-1);
  set.insert(0);
  set.insert(2);
  set.insert(3);

  typename Set::iterator it = set.find(2);
  EXPECT_EQ(2, *it);
  set.InsertBefore(it, 1);
  ++it;
  EXPECT_EQ(3, *it);
  EXPECT_EQ(5u, set.size());
  --it;
  --it;
  EXPECT_EQ(1, *it);

  set.erase(-1);
  set.erase(0);
  set.erase(2);
  set.erase(3);
  EXPECT_EQ(1u, set.size());
  EXPECT_EQ(1, *it);
  ++it;
  EXPECT_EQ(it, set.end());
  --it;
  EXPECT_EQ(1, *it);
  set.InsertBefore(it, -1);
  set.InsertBefore(it, 0);
  set.insert(2);
  set.insert(3);

  set.InsertBefore(2, 42);
  set.InsertBefore(-1, 103);
  EXPECT_EQ(103, set.front());
  ++it;
  EXPECT_EQ(42, *it);
  EXPECT_EQ(7u, set.size());
}

TEST(LinkedHashSetTest, AppendOrMoveToLastNewItems) {
  using Set = LinkedHashSet<int, CustomHashTraitsForInt>;
  Set set;
  Set::AddResult result = set.AppendOrMoveToLast(1);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);
  result = set.insert(2);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 2);
  result = set.AppendOrMoveToLast(3);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 3);
  EXPECT_EQ(set.size(), 3UL);

  // The set should be in order 1, 2, 3.
  typename Set::iterator iterator = set.begin();
  EXPECT_EQ(1, *iterator);
  ++iterator;
  EXPECT_EQ(2, *iterator);
  ++iterator;
  EXPECT_EQ(3, *iterator);
  ++iterator;
}

TEST(LinkedHashSetTest, AppendOrMoveToLastWithDuplicates) {
  using Set = LinkedHashSet<int, CustomHashTraitsForInt>;
  Set set;

  // Add a single element twice.
  Set::AddResult result = set.insert(1);
  EXPECT_TRUE(result.is_new_entry);
  result = set.AppendOrMoveToLast(1);
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(1UL, set.size());

  set.insert(2);
  set.insert(3);
  EXPECT_EQ(3UL, set.size());

  // Appending 2 move it to the end.
  EXPECT_EQ(3, set.back());
  result = set.AppendOrMoveToLast(2);
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(2, set.back());

  // Inverse the list by moving each element to end end.
  result = set.AppendOrMoveToLast(3);
  EXPECT_FALSE(result.is_new_entry);
  result = set.AppendOrMoveToLast(2);
  EXPECT_FALSE(result.is_new_entry);
  result = set.AppendOrMoveToLast(1);
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(3UL, set.size());

  Set::iterator iterator = set.begin();
  EXPECT_EQ(3, *iterator);
  ++iterator;
  EXPECT_EQ(2, *iterator);
  ++iterator;
  EXPECT_EQ(1, *iterator);
  ++iterator;
}

TEST(LinkedHashSetTest, PrependOrMoveToFirstNewItems) {
  using Set = LinkedHashSet<int, CustomHashTraitsForInt>;
  Set set;
  Set::AddResult result = set.PrependOrMoveToFirst(1);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);

  result = set.insert(2);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 2);

  result = set.PrependOrMoveToFirst(3);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 3);

  EXPECT_EQ(set.size(), 3UL);

  // The set should be in order 3, 1, 2.
  typename Set::iterator iterator = set.begin();
  EXPECT_EQ(3, *iterator);
  ++iterator;
  EXPECT_EQ(1, *iterator);
  ++iterator;
  EXPECT_EQ(2, *iterator);
  ++iterator;
}

TEST(LinkedHashSetTest, PrependOrMoveToLastWithDuplicates) {
  using Set = LinkedHashSet<int, CustomHashTraitsForInt>;
  Set set;

  // Add a single element twice.
  typename Set::AddResult result = set.insert(1);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);
  result = set.PrependOrMoveToFirst(1);
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);
  EXPECT_EQ(1UL, set.size());

  set.insert(2);
  set.insert(3);
  EXPECT_EQ(3UL, set.size());

  // Prepending 2 move it to the beginning.
  EXPECT_EQ(1, set.front());
  result = set.PrependOrMoveToFirst(2);
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(2, set.front());

  // Inverse the set by moving each element to the first position.
  result = set.PrependOrMoveToFirst(1);
  EXPECT_FALSE(result.is_new_entry);
  result = set.PrependOrMoveToFirst(2);
  EXPECT_FALSE(result.is_new_entry);
  result = set.PrependOrMoveToFirst(3);
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(3UL, set.size());

  typename Set::iterator iterator = set.begin();
  EXPECT_EQ(3, *iterator);
  ++iterator;
  EXPECT_EQ(2, *iterator);
  ++iterator;
  EXPECT_EQ(1, *iterator);
  ++iterator;
}

TEST(LinkedHashSetTest, Erase) {
  using Set = LinkedHashSet<int, CustomHashTraitsForInt>;
  Set set;
  set.insert(1);
  set.insert(2);
  set.insert(3);
  set.insert(4);
  set.insert(5);

  Set::const_iterator it = set.begin();
  ++it;
  EXPECT_TRUE(set.Contains(2));
  set.erase(it);
  EXPECT_FALSE(set.Contains(2));
  it = set.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_EQ(*it, 4);
  ++it;
  EXPECT_EQ(*it, 5);

  EXPECT_TRUE(set.Contains(3));
  set.erase(3);
  EXPECT_FALSE(set.Contains(3));
  it = set.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 4);
  ++it;
  EXPECT_EQ(*it, 5);

  set.insert(6);
  it = set.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 4);
  ++it;
  EXPECT_EQ(*it, 5);
  ++it;
  EXPECT_EQ(*it, 6);
}

TEST(LinkedHashSetTest, RemoveFirst) {
  using Set = LinkedHashSet<int, CustomHashTraitsForInt>;
  Set set;
  set.insert(-1);
  set.insert(0);
  set.insert(1);
  set.insert(2);

  EXPECT_EQ(-1, set.front());
  EXPECT_EQ(2, set.back());

  set.RemoveFirst();
  Set::const_iterator it = set.begin();
  EXPECT_EQ(*it, 0);
  ++it;
  EXPECT_EQ(*it, 1);

  set.RemoveFirst();
  it = set.begin();
  EXPECT_EQ(*it, 1);

  set.RemoveFirst();
  it = set.begin();
  EXPECT_EQ(*it, 2);

  set.RemoveFirst();
  EXPECT_TRUE(set.empty());
}

TEST(LinkedHashSetTest, pop_back) {
  using Set = LinkedHashSet<int, CustomHashTraitsForInt>;
  Set set;
  set.insert(1);
  set.insert(2);
  set.insert(3);

  set.pop_back();
  Set::const_iterator it = set.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);

  set.pop_back();
  it = set.begin();
  EXPECT_EQ(*it, 1);

  set.pop_back();
  EXPECT_TRUE(set.begin() == set.end());
}

TEST(LinkedHashSetTest, Clear) {
  using Set = LinkedHashSet<int, CustomHashTraitsForInt>;
  Set set;
  set.insert(1);
  set.insert(2);
  set.insert(3);

  set.clear();
  EXPECT_TRUE(set.begin() == set.end());

  set.insert(1);
  Set::const_iterator it = set.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_TRUE(it == set.end());
}

// A unit type that has empty std::string value.
struct EmptyString {
  EmptyString() = default;
  explicit EmptyString(WTF::HashTableDeletedValueType) : deleted_(true) {}
  ~EmptyString() { CHECK(ok_); }

  bool operator==(const EmptyString& other) const {
    return str_ == other.str_ && deleted_ == other.deleted_ &&
           empty_ == other.empty_;
  }

  bool IsHashTableDeletedValue() const { return deleted_; }

  std::string str_;
  bool ok_ = true;
  bool deleted_ = false;
  bool empty_ = false;
};

template <>
struct HashTraits<EmptyString> : SimpleClassHashTraits<EmptyString> {
  static unsigned GetHash(const EmptyString&) { return 0; }
  static const bool kEmptyValueIsZero = false;

  // This overrides SimpleClassHashTraits<EmptyString>::EmptyValue() which
  // returns EmptyString().
  static EmptyString EmptyValue() {
    EmptyString empty;
    empty.empty_ = true;
    return empty;
  }
};

TEST(LinkedHashSetTest, Swap) {
  using Set = LinkedHashSet<int, CustomHashTraitsForInt>;
  int num = 10;
  Set set0;
  Set set1;
  Set set2;
  for (int i = 0; i < num; ++i) {
    set1.insert(i + 1);
    set2.insert(num - i);
  }

  typename Set::iterator it1 = set1.begin();
  typename Set::iterator it2 = set2.begin();
  for (int i = 0; i < num; ++i, ++it1, ++it2) {
    EXPECT_EQ(*it1, i + 1);
    EXPECT_EQ(*it2, num - i);
  }
  EXPECT_EQ(set0.begin(), set0.end());
  EXPECT_EQ(it1, set1.end());
  EXPECT_EQ(it2, set2.end());

  // Shift sets: 2->1, 1->0, 0->2
  set1.Swap(set2);  // Swap with non-empty sets.
  set0.Swap(set2);  // Swap with an empty set.

  it1 = set0.begin();
  it2 = set1.begin();
  for (int i = 0; i < num; ++i, ++it1, ++it2) {
    EXPECT_EQ(*it1, i + 1);
    EXPECT_EQ(*it2, num - i);
  }
  EXPECT_EQ(it1, set0.end());
  EXPECT_EQ(it2, set1.end());
  EXPECT_EQ(set2.begin(), set2.end());

  int removed_index = num >> 1;
  set0.erase(removed_index + 1);
  set1.erase(num - removed_index);

  it1 = set0.begin();
  it2 = set1.begin();
  for (int i = 0; i < num; ++i, ++it1, ++it2) {
    if (i == removed_index)
      ++i;
    EXPECT_EQ(*it1, i + 1);
    EXPECT_EQ(*it2, num - i);
  }
  EXPECT_EQ(it1, set0.end());
  EXPECT_EQ(it2, set1.end());
}

TEST(LinkedHashSetTest, IteratorsConvertToConstVersions) {
  using Set = LinkedHashSet<int, CustomHashTraitsForInt>;
  Set set;
  set.insert(42);
  typename Set::iterator it = set.begin();
  typename Set::const_iterator cit = it;
  typename Set::reverse_iterator rit = set.rbegin();
  typename Set::const_reverse_iterator crit = rit;
  // Use the variables to make the compiler happy.
  ASSERT_EQ(*cit, *crit);
}

TEST(LinkedHashSetRefPtrTest, WithRefPtr) {
  using Set = LinkedHashSet<scoped_refptr<DummyRefCounted>>;
  int expected = 1;
  // LinkedHashSet stores each object twice.
  if (std::is_same<Set, LinkedHashSet<scoped_refptr<DummyRefCounted>>>::value)
    expected = 2;
  bool is_deleted = false;
  DummyRefCounted::ref_invokes_count_ = 0;
  scoped_refptr<DummyRefCounted> object =
      base::AdoptRef(new DummyRefCounted(is_deleted));
  EXPECT_EQ(0, DummyRefCounted::ref_invokes_count_);

  Set set;
  set.insert(object);
  // Referenced only once (to store a copy in the container).
  EXPECT_EQ(expected, DummyRefCounted::ref_invokes_count_);
  EXPECT_EQ(object, set.front());
  EXPECT_EQ(expected, DummyRefCounted::ref_invokes_count_);

  DummyRefCounted* ptr = object.get();

  EXPECT_TRUE(set.Contains(object));
  EXPECT_TRUE(set.Contains(ptr));
  EXPECT_EQ(expected, DummyRefCounted::ref_invokes_count_);

  object = nullptr;
  EXPECT_FALSE(is_deleted);
  EXPECT_EQ(expected, DummyRefCounted::ref_invokes_count_);

  set.erase(ptr);
  EXPECT_TRUE(is_deleted);

  EXPECT_EQ(expected, DummyRefCounted::ref_invokes_count_);
}

TEST(LinkedHashSetRefPtrTest, ExerciseValuePeekInType) {
  using Set = LinkedHashSet<scoped_refptr<DummyRefCounted>>;
  Set set;
  bool is_deleted = false;
  bool is_deleted2 = false;

  scoped_refptr<DummyRefCounted> ptr =
      base::AdoptRef(new DummyRefCounted(is_deleted));
  scoped_refptr<DummyRefCounted> ptr2 =
      base::AdoptRef(new DummyRefCounted(is_deleted2));

  typename Set::AddResult add_result = set.insert(ptr);
  EXPECT_TRUE(add_result.is_new_entry);
  set.find(ptr);
  const Set& const_set(set);
  const_set.find(ptr);
  EXPECT_TRUE(set.Contains(ptr));
  set.insert(ptr);
  set.AppendOrMoveToLast(ptr);
  set.PrependOrMoveToFirst(ptr);
  set.InsertBefore(ptr, ptr);
  EXPECT_EQ(1u, set.size());
  set.insert(ptr2);
  ptr2 = nullptr;
  set.erase(ptr);

  EXPECT_FALSE(is_deleted);
  ptr = nullptr;
  EXPECT_TRUE(is_deleted);

  EXPECT_FALSE(is_deleted2);
  set.RemoveFirst();
  EXPECT_TRUE(is_deleted2);

  EXPECT_EQ(0u, set.size());
}

struct Simple {
  explicit Simple(int value) : value_(value) {}
  int value_;
};

struct Complicated {
  Complicated() : Complicated(0) {}
  explicit Complicated(int value) : simple_(value) {}
  Simple simple_;
  bool operator==(const Complicated& other) const {
    return simple_.value_ == other.simple_.value_;
  }
};

struct ComplicatedHashTraits : GenericHashTraits<Complicated> {
  static unsigned GetHash(const Complicated& key) { return key.simple_.value_; }
  static bool Equal(const Complicated& a, const Complicated& b) {
    return a.simple_.value_ == b.simple_.value_;
  }
  static constexpr bool kEmptyValueIsZero = false;
  static Complicated EmptyValue() { return static_cast<Complicated>(0); }
  static Complicated DeletedValue() { return static_cast<Complicated>(-1); }
};

struct ComplexityTranslator {
  static unsigned GetHash(const Simple& key) { return key.value_; }
  static bool Equal(const Complicated& a, const Simple& b) {
    return a.simple_.value_ == b.value_;
  }
};

TEST(LinkedHashSetHashFunctionsTest, CustomHashFunction) {
  using Set = LinkedHashSet<Complicated, ComplicatedHashTraits>;
  Set set;
  set.insert(Complicated(42));

  typename Set::iterator it = set.find(Complicated(42));
  EXPECT_NE(it, set.end());

  it = set.find(Complicated(103));
  EXPECT_EQ(it, set.end());

  const Set& const_set(set);

  typename Set::const_iterator const_iterator = const_set.find(Complicated(42));
  EXPECT_NE(const_iterator, const_set.end());

  const_iterator = const_set.find(Complicated(103));
  EXPECT_EQ(const_iterator, const_set.end());
}

TEST(LinkedHashSetTranslatorTest, ComplexityTranslator) {
  using Set = LinkedHashSet<Complicated, ComplicatedHashTraits>;
  Set set;
  set.insert(Complicated(42));

  EXPECT_TRUE(set.template Contains<ComplexityTranslator>(Simple(42)));

  typename Set::iterator it =
      set.template Find<ComplexityTranslator>(Simple(42));
  EXPECT_NE(it, set.end());

  it = set.template Find<ComplexityTranslator>(Simple(103));
  EXPECT_EQ(it, set.end());

  const Set& const_set(set);

  typename Set::const_iterator const_iterator =
      const_set.template Find<ComplexityTranslator>(Simple(42));
  EXPECT_NE(const_iterator, const_set.end());

  const_iterator = const_set.template Find<ComplexityTranslator>(Simple(103));
  EXPECT_EQ(const_iterator, const_set.end());
}

TEST(LinkedHashSetCountCopyTest, MoveConstructionShouldNotMakeCopy) {
  using Set = LinkedHashSet<CountCopy>;
  Set set;
  int counter = 0;
  set.insert(CountCopy(&counter));

  counter = 0;
  Set other(std::move(set));
  EXPECT_EQ(0, counter);
}

TEST(LinkedHashSetCountCopyTest, MoveAssignmentShouldNotMakeACopy) {
  using Set = LinkedHashSet<CountCopy>;
  Set set;
  int counter = 0;
  set.insert(CountCopy(&counter));

  Set other(set);
  counter = 0;
  set = std::move(other);
  EXPECT_EQ(0, counter);
}

// This ensures that LinkedHashSet can store a struct that needs
// HashTraits<>::kEmptyValueIsZero set to false. The default EmptyValue() of
// SimpleClassHashTraits<> returns a value created with the default constructor,
// so a custom HashTraits that sets kEmptyValueIsZero to false and also
// overrides EmptyValue() to provide another empty value is needed.
TEST(LinkedHashSetEmptyTest, EmptyString) {
  using Set = LinkedHashSet<EmptyString>;
  Set set;
  set.insert(EmptyString());
}

}  // namespace WTF
