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

#include "third_party/blink/renderer/platform/wtf/list_hash_set.h"

#include <memory>
#include <type_traits>
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/wtf_test_helper.h"

namespace WTF {

namespace {

template <typename Set>
class ListOrLinkedHashSetTest : public testing::Test {};

using SetTypes =
    testing::Types<ListHashSet<int>, ListHashSet<int, 1>, LinkedHashSet<int>>;
TYPED_TEST_SUITE(ListOrLinkedHashSetTest, SetTypes);

TYPED_TEST(ListOrLinkedHashSetTest, RemoveFirst) {
  using Set = TypeParam;
  Set list;
  list.insert(-1);
  list.insert(0);
  list.insert(1);
  list.insert(2);
  list.insert(3);

  EXPECT_EQ(-1, list.front());
  EXPECT_EQ(3, list.back());

  list.RemoveFirst();
  EXPECT_EQ(0, list.front());

  list.pop_back();
  EXPECT_EQ(2, list.back());

  list.RemoveFirst();
  EXPECT_EQ(1, list.front());

  list.RemoveFirst();
  EXPECT_EQ(2, list.front());

  list.RemoveFirst();
  EXPECT_TRUE(list.IsEmpty());
}

TYPED_TEST(ListOrLinkedHashSetTest, AppendOrMoveToLastNewItems) {
  using Set = TypeParam;
  Set list;
  typename Set::AddResult result = list.AppendOrMoveToLast(1);
  EXPECT_TRUE(result.is_new_entry);
  result = list.insert(2);
  EXPECT_TRUE(result.is_new_entry);
  result = list.AppendOrMoveToLast(3);
  EXPECT_TRUE(result.is_new_entry);

  EXPECT_EQ(list.size(), 3UL);

  // The list should be in order 1, 2, 3.
  typename Set::iterator iterator = list.begin();
  EXPECT_EQ(1, *iterator);
  ++iterator;
  EXPECT_EQ(2, *iterator);
  ++iterator;
  EXPECT_EQ(3, *iterator);
  ++iterator;
}

TYPED_TEST(ListOrLinkedHashSetTest, AppendOrMoveToLastWithDuplicates) {
  using Set = TypeParam;
  Set list;

  // Add a single element twice.
  typename Set::AddResult result = list.insert(1);
  EXPECT_TRUE(result.is_new_entry);
  result = list.AppendOrMoveToLast(1);
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(1UL, list.size());

  list.insert(2);
  list.insert(3);
  EXPECT_EQ(3UL, list.size());

  // Appending 2 move it to the end.
  EXPECT_EQ(3, list.back());
  result = list.AppendOrMoveToLast(2);
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(2, list.back());

  // Inverse the list by moving each element to end end.
  result = list.AppendOrMoveToLast(3);
  EXPECT_FALSE(result.is_new_entry);
  result = list.AppendOrMoveToLast(2);
  EXPECT_FALSE(result.is_new_entry);
  result = list.AppendOrMoveToLast(1);
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(3UL, list.size());

  typename Set::iterator iterator = list.begin();
  EXPECT_EQ(3, *iterator);
  ++iterator;
  EXPECT_EQ(2, *iterator);
  ++iterator;
  EXPECT_EQ(1, *iterator);
  ++iterator;
}

TYPED_TEST(ListOrLinkedHashSetTest, PrependOrMoveToFirstNewItems) {
  using Set = TypeParam;
  Set list;
  typename Set::AddResult result = list.PrependOrMoveToFirst(1);
  EXPECT_TRUE(result.is_new_entry);
  result = list.insert(2);
  EXPECT_TRUE(result.is_new_entry);
  result = list.PrependOrMoveToFirst(3);
  EXPECT_TRUE(result.is_new_entry);

  EXPECT_EQ(list.size(), 3UL);

  // The list should be in order 3, 1, 2.
  typename Set::iterator iterator = list.begin();
  EXPECT_EQ(3, *iterator);
  ++iterator;
  EXPECT_EQ(1, *iterator);
  ++iterator;
  EXPECT_EQ(2, *iterator);
  ++iterator;
}

TYPED_TEST(ListOrLinkedHashSetTest, PrependOrMoveToLastWithDuplicates) {
  using Set = TypeParam;
  Set list;

  // Add a single element twice.
  typename Set::AddResult result = list.insert(1);
  EXPECT_TRUE(result.is_new_entry);
  result = list.PrependOrMoveToFirst(1);
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(1UL, list.size());

  list.insert(2);
  list.insert(3);
  EXPECT_EQ(3UL, list.size());

  // Prepending 2 move it to the beginning.
  EXPECT_EQ(1, list.front());
  result = list.PrependOrMoveToFirst(2);
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(2, list.front());

  // Inverse the list by moving each element to the first position.
  result = list.PrependOrMoveToFirst(1);
  EXPECT_FALSE(result.is_new_entry);
  result = list.PrependOrMoveToFirst(2);
  EXPECT_FALSE(result.is_new_entry);
  result = list.PrependOrMoveToFirst(3);
  EXPECT_FALSE(result.is_new_entry);
  EXPECT_EQ(3UL, list.size());

  typename Set::iterator iterator = list.begin();
  EXPECT_EQ(3, *iterator);
  ++iterator;
  EXPECT_EQ(2, *iterator);
  ++iterator;
  EXPECT_EQ(1, *iterator);
  ++iterator;
}

TYPED_TEST(ListOrLinkedHashSetTest, Find) {
  using Set = TypeParam;
  Set set;
  set.insert(-1);
  set.insert(0);
  set.insert(1);
  set.insert(2);
  set.insert(3);

  {
    const Set& ref = set;
    typename Set::const_iterator it = ref.find(2);
    EXPECT_EQ(2, *it);
    ++it;
    EXPECT_EQ(3, *it);
    --it;
    --it;
    EXPECT_EQ(1, *it);
  }
  {
    Set& ref = set;
    typename Set::iterator it = ref.find(2);
    EXPECT_EQ(2, *it);
    ++it;
    EXPECT_EQ(3, *it);
    --it;
    --it;
    EXPECT_EQ(1, *it);
  }
}

TYPED_TEST(ListOrLinkedHashSetTest, InsertBefore) {
  using Set = TypeParam;
  bool can_modify_while_iterating =
      !std::is_same<Set, LinkedHashSet<int>>::value;
  Set set;
  set.insert(-1);
  set.insert(0);
  set.insert(2);
  set.insert(3);

  typename Set::iterator it = set.find(2);
  EXPECT_EQ(2, *it);
  set.InsertBefore(it, 1);
  if (!can_modify_while_iterating)
    it = set.find(2);
  ++it;
  EXPECT_EQ(3, *it);
  EXPECT_EQ(5u, set.size());
  --it;
  --it;
  EXPECT_EQ(1, *it);
  if (can_modify_while_iterating) {
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
  }
  set.InsertBefore(2, 42);
  set.InsertBefore(-1, 103);
  EXPECT_EQ(103, set.front());
  if (!can_modify_while_iterating)
    it = set.find(1);
  ++it;
  EXPECT_EQ(42, *it);
  EXPECT_EQ(7u, set.size());
}

TYPED_TEST(ListOrLinkedHashSetTest, AddReturnIterator) {
  using Set = TypeParam;
  bool can_modify_while_iterating =
      !std::is_same<Set, LinkedHashSet<int>>::value;
  Set set;
  set.insert(-1);
  set.insert(0);
  set.insert(1);
  set.insert(2);

  typename Set::iterator it = set.AddReturnIterator(3);
  EXPECT_EQ(3, *it);
  --it;
  EXPECT_EQ(2, *it);
  EXPECT_EQ(5u, set.size());
  --it;
  EXPECT_EQ(1, *it);
  --it;
  EXPECT_EQ(0, *it);
  it = set.AddReturnIterator(4);
  if (can_modify_while_iterating) {
    set.erase(3);
    set.erase(2);
    set.erase(1);
    set.erase(0);
    set.erase(-1);
    EXPECT_EQ(1u, set.size());
  }
  EXPECT_EQ(4, *it);
  ++it;
  EXPECT_EQ(it, set.end());
  --it;
  EXPECT_EQ(4, *it);
  if (can_modify_while_iterating) {
    set.InsertBefore(it, -1);
    set.InsertBefore(it, 0);
    set.InsertBefore(it, 1);
    set.InsertBefore(it, 2);
    set.InsertBefore(it, 3);
  }
  EXPECT_EQ(6u, set.size());
  it = set.AddReturnIterator(5);
  EXPECT_EQ(7u, set.size());
  set.erase(it);
  EXPECT_EQ(6u, set.size());
  EXPECT_EQ(4, set.back());
}

TYPED_TEST(ListOrLinkedHashSetTest, Swap) {
  using Set = TypeParam;
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

TYPED_TEST(ListOrLinkedHashSetTest, IteratorsConvertToConstVersions) {
  using Set = TypeParam;
  Set set;
  set.insert(42);
  typename Set::iterator it = set.begin();
  typename Set::const_iterator cit = it;
  typename Set::reverse_iterator rit = set.rbegin();
  typename Set::const_reverse_iterator crit = rit;
  // Use the variables to make the compiler happy.
  ASSERT_EQ(*cit, *crit);
}

class DummyRefCounted : public RefCounted<DummyRefCounted> {
 public:
  DummyRefCounted(bool& is_deleted) : is_deleted_(is_deleted) {
    is_deleted_ = false;
  }
  ~DummyRefCounted() { is_deleted_ = true; }
  void AddRef() {
    WTF::RefCounted<DummyRefCounted>::AddRef();
    ++ref_invokes_count_;
  }

  static int ref_invokes_count_;

 private:
  bool& is_deleted_;
};

int DummyRefCounted::ref_invokes_count_ = 0;

template <typename Set>
class ListOrLinkedHashSetRefPtrTest : public testing::Test {};

using RefPtrSetTypes =
    testing::Types<ListHashSet<scoped_refptr<DummyRefCounted>>,
                   ListHashSet<scoped_refptr<DummyRefCounted>, 1>,
                   LinkedHashSet<scoped_refptr<DummyRefCounted>>>;
TYPED_TEST_SUITE(ListOrLinkedHashSetRefPtrTest, RefPtrSetTypes);

TYPED_TEST(ListOrLinkedHashSetRefPtrTest, WithRefPtr) {
  using Set = TypeParam;
  bool is_deleted = false;
  DummyRefCounted::ref_invokes_count_ = 0;
  scoped_refptr<DummyRefCounted> ptr =
      base::AdoptRef(new DummyRefCounted(is_deleted));
  EXPECT_EQ(0, DummyRefCounted::ref_invokes_count_);

  Set set;
  set.insert(ptr);
  // Referenced only once (to store a copy in the container).
  EXPECT_EQ(1, DummyRefCounted::ref_invokes_count_);
  EXPECT_EQ(ptr, set.front());
  EXPECT_EQ(1, DummyRefCounted::ref_invokes_count_);

  DummyRefCounted* raw_ptr = ptr.get();

  EXPECT_TRUE(set.Contains(ptr));
  EXPECT_TRUE(set.Contains(raw_ptr));
  EXPECT_EQ(1, DummyRefCounted::ref_invokes_count_);

  ptr = nullptr;
  EXPECT_FALSE(is_deleted);
  EXPECT_EQ(1, DummyRefCounted::ref_invokes_count_);

  set.erase(raw_ptr);
  EXPECT_TRUE(is_deleted);

  EXPECT_EQ(1, DummyRefCounted::ref_invokes_count_);
}

TYPED_TEST(ListOrLinkedHashSetRefPtrTest, ExerciseValuePeekInType) {
  using Set = TypeParam;
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
  typename Set::iterator it = set.AddReturnIterator(ptr);
  set.AppendOrMoveToLast(ptr);
  set.PrependOrMoveToFirst(ptr);
  set.InsertBefore(ptr, ptr);
  set.InsertBefore(it, ptr);
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
  Simple(int value) : value_(value) {}
  int value_;
};

struct Complicated {
  Complicated() : Complicated(0) {}
  Complicated(int value) : simple_(value) { objects_constructed_++; }

  Complicated(const Complicated& other) : simple_(other.simple_) {
    objects_constructed_++;
  }

  Simple simple_;
  static int objects_constructed_;
};

int Complicated::objects_constructed_ = 0;

struct ComplicatedHashFunctions {
  static unsigned GetHash(const Complicated& key) { return key.simple_.value_; }
  static bool Equal(const Complicated& a, const Complicated& b) {
    return a.simple_.value_ == b.simple_.value_;
  }
};
struct ComplexityTranslator {
  static unsigned GetHash(const Simple& key) { return key.value_; }
  static bool Equal(const Complicated& a, const Simple& b) {
    return a.simple_.value_ == b.value_;
  }
};

template <typename Set>
class ListOrLinkedHashSetTranslatorTest : public testing::Test {};

using TranslatorSetTypes =
    testing::Types<ListHashSet<Complicated, 256, ComplicatedHashFunctions>,
                   ListHashSet<Complicated, 1, ComplicatedHashFunctions>,
                   LinkedHashSet<Complicated, ComplicatedHashFunctions>>;
TYPED_TEST_SUITE(ListOrLinkedHashSetTranslatorTest, TranslatorSetTypes);

TYPED_TEST(ListOrLinkedHashSetTranslatorTest, ComplexityTranslator) {
  using Set = TypeParam;
  Set set;
  set.insert(Complicated(42));
  int base_line = Complicated::objects_constructed_;

  typename Set::iterator it =
      set.template Find<ComplexityTranslator>(Simple(42));
  EXPECT_NE(it, set.end());
  EXPECT_EQ(base_line, Complicated::objects_constructed_);

  it = set.template Find<ComplexityTranslator>(Simple(103));
  EXPECT_EQ(it, set.end());
  EXPECT_EQ(base_line, Complicated::objects_constructed_);

  const Set& const_set(set);

  typename Set::const_iterator const_iterator =
      const_set.template Find<ComplexityTranslator>(Simple(42));
  EXPECT_NE(const_iterator, const_set.end());
  EXPECT_EQ(base_line, Complicated::objects_constructed_);

  const_iterator = const_set.template Find<ComplexityTranslator>(Simple(103));
  EXPECT_EQ(const_iterator, const_set.end());
  EXPECT_EQ(base_line, Complicated::objects_constructed_);
}

TEST(ListHashSetTest, WithOwnPtr) {
  bool deleted1 = false, deleted2 = false;

  typedef ListHashSet<std::unique_ptr<Dummy>> OwnPtrSet;
  OwnPtrSet set;

  Dummy* ptr1 = new Dummy(deleted1);
  {
    // AddResult in a separate scope to avoid assertion hit,
    // since we modify the container further.
    OwnPtrSet::AddResult res1 = set.insert(base::WrapUnique(ptr1));
    EXPECT_EQ(res1.stored_value->get(), ptr1);
  }

  EXPECT_FALSE(deleted1);
  EXPECT_EQ(1UL, set.size());
  OwnPtrSet::iterator it1 = set.find(ptr1);
  EXPECT_NE(set.end(), it1);
  EXPECT_EQ(ptr1, (*it1).get());

  Dummy* ptr2 = new Dummy(deleted2);
  {
    OwnPtrSet::AddResult res2 = set.insert(base::WrapUnique(ptr2));
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
    own_ptr1 = set.TakeFirst();
    EXPECT_EQ(1UL, set.size());
    own_ptr2 = set.Take(ptr2);
    EXPECT_TRUE(set.IsEmpty());
  }
  EXPECT_FALSE(deleted1);
  EXPECT_FALSE(deleted2);

  EXPECT_EQ(ptr1, own_ptr1.get());
  EXPECT_EQ(ptr2, own_ptr2.get());
}

template <typename Set>
class ListOrLinkedHashSetCountCopyTest : public testing::Test {};

using CountCopySetTypes = testing::Types<ListHashSet<CountCopy>,
                                         ListHashSet<CountCopy, 1>,
                                         LinkedHashSet<CountCopy>>;
TYPED_TEST_SUITE(ListOrLinkedHashSetCountCopyTest, CountCopySetTypes);

TYPED_TEST(ListOrLinkedHashSetCountCopyTest,
           MoveConstructionShouldNotMakeCopy) {
  using Set = TypeParam;
  Set set;
  int counter = 0;
  set.insert(CountCopy(&counter));

  counter = 0;
  Set other(std::move(set));
  EXPECT_EQ(0, counter);
}

TYPED_TEST(ListOrLinkedHashSetCountCopyTest, MoveAssignmentShouldNotMakeACopy) {
  using Set = TypeParam;
  Set set;
  int counter = 0;
  set.insert(CountCopy(&counter));

  Set other(set);
  counter = 0;
  set = std::move(other);
  EXPECT_EQ(0, counter);
}

template <typename Set>
class ListOrLinkedHashSetMoveOnlyTest : public testing::Test {};

using MoveOnlySetTypes = testing::Types<ListHashSet<MoveOnlyHashValue>,
                                        ListHashSet<MoveOnlyHashValue, 1>,
                                        LinkedHashSet<MoveOnlyHashValue>>;
TYPED_TEST_SUITE(ListOrLinkedHashSetMoveOnlyTest, MoveOnlySetTypes);

TYPED_TEST(ListOrLinkedHashSetMoveOnlyTest, MoveOnlyValue) {
  using Set = TypeParam;
  using AddResult = typename Set::AddResult;
  Set set;
  {
    AddResult add_result = set.insert(MoveOnlyHashValue(1, 1));
    EXPECT_TRUE(add_result.is_new_entry);
    EXPECT_EQ(1, add_result.stored_value->Value());
    EXPECT_EQ(1, add_result.stored_value->Id());
  }
  {
    AddResult add_result = set.insert(MoveOnlyHashValue(1, 111));
    EXPECT_FALSE(add_result.is_new_entry);
    EXPECT_EQ(1, add_result.stored_value->Value());
    EXPECT_EQ(1, add_result.stored_value->Id());
  }
  auto iter = set.find(MoveOnlyHashValue(1));
  ASSERT_TRUE(iter != set.end());
  EXPECT_EQ(1, iter->Value());
  EXPECT_EQ(1, iter->Id());

  iter = set.find(MoveOnlyHashValue(2));
  EXPECT_TRUE(iter == set.end());

  // ListHashSet and LinkedHashSet have several flavors of add().
  iter = set.AddReturnIterator(MoveOnlyHashValue(2, 2));
  EXPECT_EQ(2, iter->Value());
  EXPECT_EQ(2, iter->Id());

  iter = set.AddReturnIterator(MoveOnlyHashValue(2, 222));
  EXPECT_EQ(2, iter->Value());
  EXPECT_EQ(2, iter->Id());

  {
    AddResult add_result = set.AppendOrMoveToLast(MoveOnlyHashValue(3, 3));
    EXPECT_TRUE(add_result.is_new_entry);
    EXPECT_EQ(3, add_result.stored_value->Value());
    EXPECT_EQ(3, add_result.stored_value->Id());
  }
  {
    AddResult add_result = set.PrependOrMoveToFirst(MoveOnlyHashValue(4, 4));
    EXPECT_TRUE(add_result.is_new_entry);
    EXPECT_EQ(4, add_result.stored_value->Value());
    EXPECT_EQ(4, add_result.stored_value->Id());
  }
  {
    AddResult add_result =
        set.InsertBefore(MoveOnlyHashValue(4), MoveOnlyHashValue(5, 5));
    EXPECT_TRUE(add_result.is_new_entry);
    EXPECT_EQ(5, add_result.stored_value->Value());
    EXPECT_EQ(5, add_result.stored_value->Id());
  }
  {
    iter = set.find(MoveOnlyHashValue(5));
    ASSERT_TRUE(iter != set.end());
    AddResult add_result = set.InsertBefore(iter, MoveOnlyHashValue(6, 6));
    EXPECT_TRUE(add_result.is_new_entry);
    EXPECT_EQ(6, add_result.stored_value->Value());
    EXPECT_EQ(6, add_result.stored_value->Id());
  }

  // ... but they don't have any pass-out (like take()) methods.

  set.erase(MoveOnlyHashValue(3));
  set.clear();
}

}  // anonymous namespace

// A unit type which objects to its state being initialized wrong.
struct InvalidZeroValue {
  InvalidZeroValue() = default;
  InvalidZeroValue(WTF::HashTableDeletedValueType) : deleted_(true) {}
  ~InvalidZeroValue() { CHECK(ok_); }
  bool IsHashTableDeletedValue() const { return deleted_; }

  bool ok_ = true;
  bool deleted_ = false;
};

template <>
struct HashTraits<InvalidZeroValue> : SimpleClassHashTraits<InvalidZeroValue> {
  static const bool kEmptyValueIsZero = false;
};

template <>
struct DefaultHash<InvalidZeroValue> {
  struct Hash {
    static unsigned GetHash(const InvalidZeroValue&) { return 0; }
    static bool Equal(const InvalidZeroValue&, const InvalidZeroValue&) {
      return true;
    }
  };
};

template <typename Set>
class ListOrLinkedHashSetInvalidZeroTest : public testing::Test {};

using InvalidZeroValueSetTypes =
    testing::Types<ListHashSet<InvalidZeroValue>,
                   ListHashSet<InvalidZeroValue, 1>,
                   LinkedHashSet<InvalidZeroValue>>;
TYPED_TEST_SUITE(ListOrLinkedHashSetInvalidZeroTest, InvalidZeroValueSetTypes);

TYPED_TEST(ListOrLinkedHashSetInvalidZeroTest, InvalidZeroValue) {
  using Set = TypeParam;
  Set set;
  set.insert(InvalidZeroValue());
}

}  // namespace WTF
