// Copyright 2020 The Chromium Authors. All rights reserved.
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
  EXPECT_TRUE(set1.IsEmpty());
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
  EXPECT_TRUE(set1.IsEmpty());
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
  EXPECT_TRUE(set1.IsEmpty());
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
  EXPECT_TRUE(set1.IsEmpty());
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
  EXPECT_TRUE(set1.IsEmpty());
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

TEST(LinkedHashSetTest, Iterator) {
  using Set = LinkedHashSet<int>;
  Set set;
  EXPECT_TRUE(set.begin() == set.end());
  EXPECT_TRUE(set.rbegin() == set.rend());
}

TEST(LinkedHashSetTest, FrontAndBack) {
  using Set = LinkedHashSet<int>;
  Set set;
  EXPECT_EQ(set.size(), 0u);
  EXPECT_TRUE(set.IsEmpty());

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

TEST(LinkedHashSetTest, FindAndContains) {
  using Set = LinkedHashSet<int>;
  Set set;
  set.insert(2);
  set.AppendOrMoveToLast(2);
  set.PrependOrMoveToFirst(1);
  set.insert(3);
  set.AppendOrMoveToLast(4);
  set.insert(5);

  int i = 1;
  for (auto element : set) {
    EXPECT_EQ(element, i);
    i++;
  }

  Set::const_iterator it = set.find(2);
  EXPECT_EQ(*it, 2);
  it = set.find(3);
  EXPECT_EQ(*it, 3);
  it = set.find(10);
  EXPECT_TRUE(it == set.end());

  EXPECT_TRUE(set.Contains(1));
  EXPECT_TRUE(set.Contains(2));
  EXPECT_TRUE(set.Contains(3));
  EXPECT_TRUE(set.Contains(4));
  EXPECT_TRUE(set.Contains(5));

  EXPECT_FALSE(set.Contains(10));
}

TEST(LinkedHashSetTest, Insert) {
  using Set = LinkedHashSet<int>;
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
  using Set = LinkedHashSet<int>;
  Set set;

  set.InsertBefore(set.begin(), 1);
  set.InsertBefore(10, 3);
  set.InsertBefore(3, 2);
  set.InsertBefore(set.end(), 6);
  set.InsertBefore(--set.end(), 5);
  set.InsertBefore(5, 4);

  Set::const_iterator it = set.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);
  ++it;
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_EQ(*it, 4);
  ++it;
  EXPECT_EQ(*it, 5);
  ++it;
  EXPECT_EQ(*it, 6);
  ++it;
  EXPECT_TRUE(it == set.end());
}

TEST(LinkedHashSetTest, AppendOrMoveToLast) {
  using Set = LinkedHashSet<int>;
  Set set;
  Set::AddResult result = set.AppendOrMoveToLast(1);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);

  result = set.AppendOrMoveToLast(2);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 2);

  result = set.AppendOrMoveToLast(1);
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);

  result = set.AppendOrMoveToLast(3);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 3);

  Set::const_iterator it = set.begin();
  EXPECT_EQ(*it, 2);
  ++it;
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 3);
}

TEST(LinkedHashSetTest, PrependOrMoveToFirst) {
  using Set = LinkedHashSet<int>;
  Set set;
  Set::AddResult result = set.PrependOrMoveToFirst(1);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);

  result = set.PrependOrMoveToFirst(2);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 2);

  result = set.PrependOrMoveToFirst(1);
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 1);

  result = set.PrependOrMoveToFirst(3);
  EXPECT_TRUE(result.is_new_entry);
  EXPECT_EQ(*result.stored_value, 3);

  Set::const_iterator it = set.begin();
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);
}

TEST(LinkedHashSetTest, Erase) {
  using Set = LinkedHashSet<int>;
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
  using Set = LinkedHashSet<int>;
  Set set;
  set.insert(1);
  set.insert(2);
  set.insert(3);

  set.RemoveFirst();
  Set::const_iterator it = set.begin();
  EXPECT_EQ(*it, 2);
  ++it;
  EXPECT_EQ(*it, 3);

  set.RemoveFirst();
  it = set.begin();
  EXPECT_EQ(*it, 3);

  set.RemoveFirst();
  EXPECT_TRUE(set.begin() == set.end());
}

TEST(LinkedHashSetTest, pop_back) {
  using Set = LinkedHashSet<int>;
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
  using Set = LinkedHashSet<int>;
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
  static const bool kEmptyValueIsZero = false;

  // This overrides SimpleClassHashTraits<EmptyString>::EmptyValue() which
  // returns EmptyString().
  static EmptyString EmptyValue() {
    EmptyString empty;
    empty.empty_ = true;
    return empty;
  }
};

template <>
struct DefaultHash<EmptyString> {
  struct Hash {
    static unsigned GetHash(const EmptyString&) { return 0; }
    static bool Equal(const EmptyString& value1, const EmptyString& value2) {
      return value1 == value2;
    }
    static const bool safe_to_compare_to_empty_or_deleted = true;
  };
};

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
