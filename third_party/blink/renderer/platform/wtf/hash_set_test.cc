/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/wtf/hash_set.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/wtf_test_helper.h"

namespace WTF {

int* const CountCopy::kDeletedValue =
    reinterpret_cast<int*>(static_cast<uintptr_t>(-1));

namespace {

TEST(HashSetTest, IteratorComparison) {
  HashSet<int> set;
  set.insert(1);
  EXPECT_TRUE(set.begin() != set.end());
  EXPECT_FALSE(set.begin() == set.end());

  HashSet<int>::const_iterator begin = set.begin();
  EXPECT_TRUE(begin == set.begin());
  EXPECT_TRUE(set.begin() == begin);
  EXPECT_TRUE(begin != set.end());
  EXPECT_TRUE(set.end() != begin);
  EXPECT_FALSE(begin != set.begin());
  EXPECT_FALSE(set.begin() != begin);
  EXPECT_FALSE(begin == set.end());
  EXPECT_FALSE(set.end() == begin);
}

TEST(HashSetTest, Iteration) {
  HashSet<int> set;
  for (int i = 0; i < 10; ++i)
    set.insert(1 << i);

  int encountered_keys = 0, count = 0;
  for (auto it = set.begin(); it != set.end(); ++it) {
    encountered_keys |= *it;
    count++;
  }
  EXPECT_EQ(10, count);
  EXPECT_EQ((1 << 10) - 1, encountered_keys);

  encountered_keys = count = 0;
  for (auto it = set.end(); it != set.begin();) {
    --it;
    encountered_keys |= *it;
    count++;
  }
  EXPECT_EQ(10, count);
  EXPECT_EQ((1 << 10) - 1, encountered_keys);
}

template <unsigned size>
void TestReserveCapacity();
template <>
void TestReserveCapacity<0>() {}
template <unsigned size>
void TestReserveCapacity() {
  HashSet<int> test_set;

  // Initial capacity is zero.
  EXPECT_EQ(0UL, test_set.Capacity());

  test_set.ReserveCapacityForSize(size);
  const unsigned initial_capacity = test_set.Capacity();
  const unsigned kMinimumTableSize = HashTraits<int>::kMinimumTableSize;

  // reserveCapacityForSize should respect minimumTableSize.
  EXPECT_GE(initial_capacity, kMinimumTableSize);

  // Adding items up to size should never change the capacity.
  for (wtf_size_t i = 0; i < size; ++i) {
    test_set.insert(i + 1);  // Avoid adding '0'.
    EXPECT_EQ(initial_capacity, test_set.Capacity());
  }

  // Adding items up to less than half the capacity should not change the
  // capacity.
  unsigned capacity_limit = initial_capacity / 2 - 1;
  for (wtf_size_t i = size; i < capacity_limit; ++i) {
    test_set.insert(i + 1);
    EXPECT_EQ(initial_capacity, test_set.Capacity());
  }

  // Adding one more item increases the capacity.
  test_set.insert(capacity_limit + 1);
  EXPECT_GT(test_set.Capacity(), initial_capacity);

  TestReserveCapacity<size - 1>();
}

TEST(HashSetTest, ReserveCapacity) {
  TestReserveCapacity<128>();
}

TEST(HashSetTest, HashSetOwnPtr) {
  bool deleted1 = false, deleted2 = false;

  typedef HashSet<std::unique_ptr<Dummy>> OwnPtrSet;
  OwnPtrSet set;

  Dummy* ptr1 = new Dummy(deleted1);
  {
    // AddResult in a separate scope to avoid assertion hit,
    // since we modify the container further.
    HashSet<std::unique_ptr<Dummy>>::AddResult res1 =
        set.insert(base::WrapUnique(ptr1));
    EXPECT_EQ(ptr1, res1.stored_value->get());
  }

  EXPECT_FALSE(deleted1);
  EXPECT_EQ(1UL, set.size());
  OwnPtrSet::iterator it1 = set.find(ptr1);
  EXPECT_NE(set.end(), it1);
  EXPECT_EQ(ptr1, (*it1).get());

  Dummy* ptr2 = new Dummy(deleted2);
  {
    HashSet<std::unique_ptr<Dummy>>::AddResult res2 =
        set.insert(base::WrapUnique(ptr2));
    EXPECT_EQ(res2.stored_value->get(), ptr2);
  }

  EXPECT_FALSE(deleted2);
  EXPECT_EQ(2UL, set.size());
  OwnPtrSet::iterator it2 = set.find(ptr2);
  EXPECT_NE(set.end(), it2);
  EXPECT_EQ(ptr2, (*it2).get());

  set.erase(ptr1);
  EXPECT_TRUE(deleted1);

  set.clear();
  EXPECT_TRUE(deleted2);
  EXPECT_TRUE(set.IsEmpty());

  deleted1 = false;
  deleted2 = false;
  {
    OwnPtrSet set;
    set.insert(std::make_unique<Dummy>(deleted1));
    set.insert(std::make_unique<Dummy>(deleted2));
  }
  EXPECT_TRUE(deleted1);
  EXPECT_TRUE(deleted2);

  deleted1 = false;
  deleted2 = false;
  std::unique_ptr<Dummy> own_ptr1;
  std::unique_ptr<Dummy> own_ptr2;
  ptr1 = new Dummy(deleted1);
  ptr2 = new Dummy(deleted2);
  {
    OwnPtrSet set;
    set.insert(base::WrapUnique(ptr1));
    set.insert(base::WrapUnique(ptr2));
    own_ptr1 = set.Take(ptr1);
    EXPECT_EQ(1UL, set.size());
    own_ptr2 = set.TakeAny();
    EXPECT_TRUE(set.IsEmpty());
  }
  EXPECT_FALSE(deleted1);
  EXPECT_FALSE(deleted2);

  EXPECT_EQ(ptr1, own_ptr1.get());
  EXPECT_EQ(ptr2, own_ptr2.get());
}

TEST(HashSetTest, HashSetRefPtr) {
  bool is_deleted = false;
  DummyRefCounted::ref_invokes_count_ = 0;

  scoped_refptr<DummyRefCounted> ptr =
      base::AdoptRef(new DummyRefCounted(is_deleted));
  EXPECT_EQ(0, DummyRefCounted::ref_invokes_count_);
  HashSet<scoped_refptr<DummyRefCounted>> set;
  set.insert(ptr);
  // Referenced only once (to store a copy in the container).
  EXPECT_EQ(1, DummyRefCounted::ref_invokes_count_);

  DummyRefCounted* raw_ptr = ptr.get();

  EXPECT_TRUE(set.Contains(raw_ptr));
  EXPECT_NE(set.end(), set.find(raw_ptr));
  EXPECT_TRUE(set.Contains(ptr));
  EXPECT_NE(set.end(), set.find(ptr));

  ptr = nullptr;
  EXPECT_FALSE(is_deleted);

  set.erase(raw_ptr);
  EXPECT_TRUE(is_deleted);
  EXPECT_TRUE(set.IsEmpty());
  EXPECT_EQ(1, DummyRefCounted::ref_invokes_count_);
}

TEST(HashSetTest, MoveShouldNotMakeCopy) {
  HashSet<CountCopy> set;
  int counter = 0;
  set.insert(CountCopy(&counter));

  HashSet<CountCopy> other(set);
  counter = 0;
  set = std::move(other);
  EXPECT_EQ(0, counter);

  counter = 0;
  HashSet<CountCopy> yet_another(std::move(set));
  EXPECT_EQ(0, counter);
}

TEST(HashSetTest, MoveOnlyValue) {
  using TheSet = HashSet<MoveOnlyHashValue>;
  TheSet set;
  {
    TheSet::AddResult add_result = set.insert(MoveOnlyHashValue(1, 1));
    EXPECT_TRUE(add_result.is_new_entry);
    EXPECT_EQ(1, add_result.stored_value->Value());
    EXPECT_EQ(1, add_result.stored_value->Id());
  }
  auto iter = set.find(MoveOnlyHashValue(1));
  ASSERT_TRUE(iter != set.end());
  EXPECT_EQ(1, iter->Value());

  iter = set.find(MoveOnlyHashValue(2));
  EXPECT_TRUE(iter == set.end());

  for (int i = 2; i < 32; ++i) {
    TheSet::AddResult add_result = set.insert(MoveOnlyHashValue(i, i));
    EXPECT_TRUE(add_result.is_new_entry);
    EXPECT_EQ(i, add_result.stored_value->Value());
    EXPECT_EQ(i, add_result.stored_value->Id());
  }

  iter = set.find(MoveOnlyHashValue(1));
  ASSERT_TRUE(iter != set.end());
  EXPECT_EQ(1, iter->Value());
  EXPECT_EQ(1, iter->Id());

  iter = set.find(MoveOnlyHashValue(7));
  ASSERT_TRUE(iter != set.end());
  EXPECT_EQ(7, iter->Value());
  EXPECT_EQ(7, iter->Id());

  {
    TheSet::AddResult add_result = set.insert(
        MoveOnlyHashValue(7, 777));  // With different ID for identification.
    EXPECT_FALSE(add_result.is_new_entry);
    EXPECT_EQ(7, add_result.stored_value->Value());
    EXPECT_EQ(7, add_result.stored_value->Id());
  }

  set.erase(MoveOnlyHashValue(11));
  iter = set.find(MoveOnlyHashValue(11));
  EXPECT_TRUE(iter == set.end());

  MoveOnlyHashValue thirteen(set.Take(MoveOnlyHashValue(13)));
  EXPECT_EQ(13, thirteen.Value());
  EXPECT_EQ(13, thirteen.Id());
  iter = set.find(MoveOnlyHashValue(13));
  EXPECT_TRUE(iter == set.end());

  set.clear();
}

TEST(HashSetTest, UniquePtr) {
  using Pointer = std::unique_ptr<int>;
  using Set = HashSet<Pointer>;
  Set set;
  int* one_pointer = new int(1);
  {
    Set::AddResult add_result = set.insert(Pointer(one_pointer));
    EXPECT_TRUE(add_result.is_new_entry);
    EXPECT_EQ(one_pointer, add_result.stored_value->get());
    EXPECT_EQ(1, **add_result.stored_value);
  }
  auto iter = set.find(one_pointer);
  ASSERT_TRUE(iter != set.end());
  EXPECT_EQ(one_pointer, iter->get());

  Pointer nonexistent(new int(42));
  iter = set.find(nonexistent.get());
  EXPECT_TRUE(iter == set.end());

  // Insert more to cause a rehash.
  for (int i = 2; i < 32; ++i) {
    Set::AddResult add_result = set.insert(Pointer(new int(i)));
    EXPECT_TRUE(add_result.is_new_entry);
    EXPECT_EQ(i, **add_result.stored_value);
  }

  iter = set.find(one_pointer);
  ASSERT_TRUE(iter != set.end());
  EXPECT_EQ(one_pointer, iter->get());

  Pointer one(set.Take(one_pointer));
  ASSERT_TRUE(one);
  EXPECT_EQ(one_pointer, one.get());

  Pointer empty(set.Take(nonexistent.get()));
  EXPECT_TRUE(!empty);

  iter = set.find(one_pointer);
  EXPECT_TRUE(iter == set.end());

  // Re-insert to the deleted slot.
  {
    Set::AddResult add_result = set.insert(std::move(one));
    EXPECT_TRUE(add_result.is_new_entry);
    EXPECT_EQ(one_pointer, add_result.stored_value->get());
    EXPECT_EQ(1, **add_result.stored_value);
  }
}

bool IsOneTwoThreeSet(const HashSet<int>& set) {
  return set.size() == 3 && set.Contains(1) && set.Contains(2) &&
         set.Contains(3);
}

HashSet<int> ReturnOneTwoThreeSet() {
  return {1, 2, 3};
}

TEST(HashSetTest, InitializerList) {
  HashSet<int> empty({});
  EXPECT_TRUE(empty.IsEmpty());

  HashSet<int> one({1});
  EXPECT_EQ(1u, one.size());
  EXPECT_TRUE(one.Contains(1));

  HashSet<int> one_two_three({1, 2, 3});
  EXPECT_EQ(3u, one_two_three.size());
  EXPECT_TRUE(one_two_three.Contains(1));
  EXPECT_TRUE(one_two_three.Contains(2));
  EXPECT_TRUE(one_two_three.Contains(3));

  // Put some jank so we can check if the assignments later can clear them.
  empty.insert(9999);
  one.insert(9999);
  one_two_three.insert(9999);

  empty = {};
  EXPECT_TRUE(empty.IsEmpty());

  one = {1};
  EXPECT_EQ(1u, one.size());
  EXPECT_TRUE(one.Contains(1));

  one_two_three = {1, 2, 3};
  EXPECT_EQ(3u, one_two_three.size());
  EXPECT_TRUE(one_two_three.Contains(1));
  EXPECT_TRUE(one_two_three.Contains(2));
  EXPECT_TRUE(one_two_three.Contains(3));

  one_two_three = {3, 1, 1, 2, 1, 1, 3};
  EXPECT_EQ(3u, one_two_three.size());
  EXPECT_TRUE(one_two_three.Contains(1));
  EXPECT_TRUE(one_two_three.Contains(2));
  EXPECT_TRUE(one_two_three.Contains(3));

  // Other ways of construction: as a function parameter and in a return
  // statement.
  EXPECT_TRUE(IsOneTwoThreeSet({1, 2, 3}));
  EXPECT_TRUE(IsOneTwoThreeSet(ReturnOneTwoThreeSet()));
}

enum TestEnum {
  kItem0,
};

enum class TestEnumClass : unsigned char {
  kItem0,
};

TEST(HashSetTest, HasTraitsForEnum) {
  // Ensure that enum hash keys are buildable.
  HashSet<TestEnum> set1;
  set1.insert(TestEnum::kItem0);
  HashSet<TestEnumClass> set2;
  set2.insert(TestEnumClass::kItem0);
  HashSet<std::pair<TestEnum, TestEnumClass>> set3;
  set3.insert(std::make_pair(TestEnum::kItem0, TestEnumClass::kItem0));
}

static_assert(!IsTraceable<HashSet<int>>::value,
              "HashSet<int, int> must not be traceable.");

}  // anonymous namespace

}  // namespace WTF
