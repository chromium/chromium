/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/wtf/vector.h"

#include <memory>
#include "base/optional.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_test_helper.h"

namespace WTF {

HashSet<void*> g_constructed_wrapped_ints;
unsigned LivenessCounter::live_ = 0;

namespace {

TEST(VectorTest, Basic) {
  Vector<int> int_vector;
  EXPECT_TRUE(int_vector.IsEmpty());
  EXPECT_EQ(0ul, int_vector.size());
  EXPECT_EQ(0ul, int_vector.capacity());
}

TEST(VectorTest, Reverse) {
  Vector<int> int_vector;
  int_vector.push_back(10);
  int_vector.push_back(11);
  int_vector.push_back(12);
  int_vector.push_back(13);
  int_vector.Reverse();

  EXPECT_EQ(13, int_vector[0]);
  EXPECT_EQ(12, int_vector[1]);
  EXPECT_EQ(11, int_vector[2]);
  EXPECT_EQ(10, int_vector[3]);

  int_vector.push_back(9);
  int_vector.Reverse();

  EXPECT_EQ(9, int_vector[0]);
  EXPECT_EQ(10, int_vector[1]);
  EXPECT_EQ(11, int_vector[2]);
  EXPECT_EQ(12, int_vector[3]);
  EXPECT_EQ(13, int_vector[4]);
}

TEST(VectorTest, EraseAtIndex) {
  Vector<int> int_vector;
  int_vector.push_back(0);
  int_vector.push_back(1);
  int_vector.push_back(2);
  int_vector.push_back(3);

  EXPECT_EQ(4u, int_vector.size());
  EXPECT_EQ(0, int_vector[0]);
  EXPECT_EQ(1, int_vector[1]);
  EXPECT_EQ(2, int_vector[2]);
  EXPECT_EQ(3, int_vector[3]);

  int_vector.EraseAt(2, 0);
  EXPECT_EQ(4u, int_vector.size());
  EXPECT_EQ(2, int_vector[2]);

  int_vector.EraseAt(2, 1);
  EXPECT_EQ(3u, int_vector.size());
  EXPECT_EQ(3, int_vector[2]);

  int_vector.EraseAt(0, 0);
  EXPECT_EQ(3u, int_vector.size());
  EXPECT_EQ(0, int_vector[0]);

  int_vector.EraseAt(0);
  EXPECT_EQ(2u, int_vector.size());
  EXPECT_EQ(1, int_vector[0]);
}

TEST(VectorTest, Erase) {
  Vector<int> int_vector({0, 1, 2, 3});

  EXPECT_EQ(4u, int_vector.size());
  EXPECT_EQ(0, int_vector[0]);
  EXPECT_EQ(1, int_vector[1]);
  EXPECT_EQ(2, int_vector[2]);
  EXPECT_EQ(3, int_vector[3]);

  auto* first = int_vector.erase(int_vector.begin());
  EXPECT_EQ(3u, int_vector.size());
  EXPECT_EQ(1, *first);
  EXPECT_EQ(int_vector.begin(), first);

  auto* last = std::lower_bound(int_vector.begin(), int_vector.end(), 3);
  auto* end = int_vector.erase(last);
  EXPECT_EQ(2u, int_vector.size());
  EXPECT_EQ(int_vector.end(), end);
}

TEST(VectorTest, Resize) {
  Vector<int> int_vector;
  int_vector.resize(2);
  EXPECT_EQ(2u, int_vector.size());
  EXPECT_EQ(0, int_vector[0]);
  EXPECT_EQ(0, int_vector[1]);

  Vector<bool> bool_vector;
  bool_vector.resize(3);
  EXPECT_EQ(3u, bool_vector.size());
  EXPECT_EQ(false, bool_vector[0]);
  EXPECT_EQ(false, bool_vector[1]);
  EXPECT_EQ(false, bool_vector[2]);
}

TEST(VectorTest, Iterator) {
  Vector<int> int_vector;
  int_vector.push_back(10);
  int_vector.push_back(11);
  int_vector.push_back(12);
  int_vector.push_back(13);

  Vector<int>::iterator it = int_vector.begin();
  Vector<int>::iterator end = int_vector.end();
  EXPECT_TRUE(end != it);

  EXPECT_EQ(10, *it);
  ++it;
  EXPECT_EQ(11, *it);
  ++it;
  EXPECT_EQ(12, *it);
  ++it;
  EXPECT_EQ(13, *it);
  ++it;

  EXPECT_TRUE(end == it);
}

TEST(VectorTest, ReverseIterator) {
  Vector<int> int_vector;
  int_vector.push_back(10);
  int_vector.push_back(11);
  int_vector.push_back(12);
  int_vector.push_back(13);

  Vector<int>::reverse_iterator it = int_vector.rbegin();
  Vector<int>::reverse_iterator end = int_vector.rend();
  EXPECT_TRUE(end != it);

  EXPECT_EQ(13, *it);
  ++it;
  EXPECT_EQ(12, *it);
  ++it;
  EXPECT_EQ(11, *it);
  ++it;
  EXPECT_EQ(10, *it);
  ++it;

  EXPECT_TRUE(end == it);
}

typedef WTF::Vector<std::unique_ptr<DestructCounter>> OwnPtrVector;

TEST(VectorTest, OwnPtr) {
  int destruct_number = 0;
  OwnPtrVector vector;
  vector.push_back(std::make_unique<DestructCounter>(0, &destruct_number));
  vector.push_back(std::make_unique<DestructCounter>(1, &destruct_number));
  EXPECT_EQ(2u, vector.size());

  std::unique_ptr<DestructCounter>& counter0 = vector.front();
  ASSERT_EQ(0, counter0->Get());
  int counter1 = vector.back()->Get();
  ASSERT_EQ(1, counter1);
  ASSERT_EQ(0, destruct_number);

  wtf_size_t index = 0;
  for (OwnPtrVector::iterator iter = vector.begin(); iter != vector.end();
       ++iter) {
    std::unique_ptr<DestructCounter>* ref_counter = iter;
    EXPECT_EQ(index, static_cast<wtf_size_t>(ref_counter->get()->Get()));
    EXPECT_EQ(index, static_cast<wtf_size_t>((*ref_counter)->Get()));
    index++;
  }
  EXPECT_EQ(0, destruct_number);

  for (index = 0; index < vector.size(); index++) {
    std::unique_ptr<DestructCounter>& ref_counter = vector[index];
    EXPECT_EQ(index, static_cast<wtf_size_t>(ref_counter->Get()));
  }
  EXPECT_EQ(0, destruct_number);

  EXPECT_EQ(0, vector[0]->Get());
  EXPECT_EQ(1, vector[1]->Get());
  vector.EraseAt(0);
  EXPECT_EQ(1, vector[0]->Get());
  EXPECT_EQ(1u, vector.size());
  EXPECT_EQ(1, destruct_number);

  std::unique_ptr<DestructCounter> own_counter1 = std::move(vector[0]);
  vector.EraseAt(0);
  ASSERT_EQ(counter1, own_counter1->Get());
  ASSERT_EQ(0u, vector.size());
  ASSERT_EQ(1, destruct_number);

  own_counter1.reset();
  EXPECT_EQ(2, destruct_number);

  size_t count = 1025;
  destruct_number = 0;
  for (size_t i = 0; i < count; i++)
    vector.push_front(std::make_unique<DestructCounter>(i, &destruct_number));

  // Vector relocation must not destruct std::unique_ptr element.
  EXPECT_EQ(0, destruct_number);
  EXPECT_EQ(count, vector.size());

  OwnPtrVector copy_vector;
  vector.swap(copy_vector);
  EXPECT_EQ(0, destruct_number);
  EXPECT_EQ(count, copy_vector.size());
  EXPECT_EQ(0u, vector.size());

  copy_vector.clear();
  EXPECT_EQ(count, static_cast<size_t>(destruct_number));
}

TEST(VectorTest, MoveOnlyType) {
  WTF::Vector<MoveOnly> vector;
  vector.push_back(MoveOnly(1));
  vector.push_back(MoveOnly(2));
  EXPECT_EQ(2u, vector.size());

  ASSERT_EQ(1, vector.front().Value());
  ASSERT_EQ(2, vector.back().Value());

  vector.EraseAt(0);
  EXPECT_EQ(2, vector[0].Value());
  EXPECT_EQ(1u, vector.size());

  MoveOnly move_only(std::move(vector[0]));
  vector.EraseAt(0);
  ASSERT_EQ(2, move_only.Value());
  ASSERT_EQ(0u, vector.size());

  wtf_size_t count = vector.capacity() + 1;
  for (wtf_size_t i = 0; i < count; i++)
    vector.push_back(
        MoveOnly(i + 1));  // +1 to distinguish from default-constructed.

  // Reallocation did not affect the vector's content.
  EXPECT_EQ(count, vector.size());
  for (wtf_size_t i = 0; i < vector.size(); i++)
    EXPECT_EQ(static_cast<int>(i + 1), vector[i].Value());

  WTF::Vector<MoveOnly> other_vector;
  vector.swap(other_vector);
  EXPECT_EQ(count, other_vector.size());
  EXPECT_EQ(0u, vector.size());

  vector = std::move(other_vector);
  EXPECT_EQ(count, vector.size());
}

TEST(VectorTest, SwapWithInlineCapacity) {
  const size_t kInlineCapacity = 2;
  Vector<WrappedInt, kInlineCapacity> vector_a;
  vector_a.push_back(WrappedInt(1));
  Vector<WrappedInt, kInlineCapacity> vector_b;
  vector_b.push_back(WrappedInt(2));

  EXPECT_EQ(vector_a.size(), vector_b.size());
  vector_a.swap(vector_b);

  EXPECT_EQ(1u, vector_a.size());
  EXPECT_EQ(2, vector_a.at(0).Get());
  EXPECT_EQ(1u, vector_b.size());
  EXPECT_EQ(1, vector_b.at(0).Get());

  vector_a.push_back(WrappedInt(3));

  EXPECT_GT(vector_a.size(), vector_b.size());
  vector_a.swap(vector_b);

  EXPECT_EQ(1u, vector_a.size());
  EXPECT_EQ(1, vector_a.at(0).Get());
  EXPECT_EQ(2u, vector_b.size());
  EXPECT_EQ(2, vector_b.at(0).Get());
  EXPECT_EQ(3, vector_b.at(1).Get());

  EXPECT_LT(vector_a.size(), vector_b.size());
  vector_a.swap(vector_b);

  EXPECT_EQ(2u, vector_a.size());
  EXPECT_EQ(2, vector_a.at(0).Get());
  EXPECT_EQ(3, vector_a.at(1).Get());
  EXPECT_EQ(1u, vector_b.size());
  EXPECT_EQ(1, vector_b.at(0).Get());

  vector_a.push_back(WrappedInt(4));
  EXPECT_GT(vector_a.size(), kInlineCapacity);
  vector_a.swap(vector_b);

  EXPECT_EQ(1u, vector_a.size());
  EXPECT_EQ(1, vector_a.at(0).Get());
  EXPECT_EQ(3u, vector_b.size());
  EXPECT_EQ(2, vector_b.at(0).Get());
  EXPECT_EQ(3, vector_b.at(1).Get());
  EXPECT_EQ(4, vector_b.at(2).Get());

  vector_b.swap(vector_a);
}

#if defined(ANNOTATE_CONTIGUOUS_CONTAINER)
TEST(VectorTest, ContainerAnnotations) {
  Vector<int> vector_a;
  vector_a.push_back(10);
  vector_a.ReserveCapacity(32);

  volatile int* int_pointer_a = vector_a.data();
  EXPECT_DEATH(int_pointer_a[1] = 11, "container-overflow");
  vector_a.push_back(11);
  int_pointer_a[1] = 11;
  EXPECT_DEATH(int_pointer_a[2] = 12, "container-overflow");
  EXPECT_DEATH((void)int_pointer_a[2], "container-overflow");
  vector_a.ShrinkToFit();
  vector_a.ReserveCapacity(16);
  int_pointer_a = vector_a.data();
  EXPECT_DEATH((void)int_pointer_a[2], "container-overflow");

  Vector<int> vector_b(vector_a);
  vector_b.ReserveCapacity(16);
  volatile int* int_pointer_b = vector_b.data();
  EXPECT_DEATH((void)int_pointer_b[2], "container-overflow");

  Vector<int> vector_c((Vector<int>(vector_a)));
  volatile int* int_pointer_c = vector_c.data();
  EXPECT_DEATH((void)int_pointer_c[2], "container-overflow");
  vector_c.push_back(13);
  vector_c.swap(vector_b);

  volatile int* int_pointer_b2 = vector_b.data();
  volatile int* int_pointer_c2 = vector_c.data();
  int_pointer_b2[2] = 13;
  EXPECT_DEATH((void)int_pointer_b2[3], "container-overflow");
  EXPECT_DEATH((void)int_pointer_c2[2], "container-overflow");

  vector_b = vector_c;
  volatile int* int_pointer_b3 = vector_b.data();
  EXPECT_DEATH((void)int_pointer_b3[2], "container-overflow");
}
#endif  // defined(ANNOTATE_CONTIGUOUS_CONTAINER)

class Comparable {};
bool operator==(const Comparable& a, const Comparable& b) {
  return true;
}

template <typename T>
void Compare() {
  EXPECT_TRUE(Vector<T>() == Vector<T>());
  EXPECT_FALSE(Vector<T>(1) == Vector<T>(0));
  EXPECT_FALSE(Vector<T>() == Vector<T>(1));
  EXPECT_TRUE(Vector<T>(1) == Vector<T>(1));

  Vector<T, 1> vector_with_inline_capacity;
  EXPECT_TRUE(vector_with_inline_capacity == Vector<T>());
  EXPECT_FALSE(vector_with_inline_capacity == Vector<T>(1));
}

TEST(VectorTest, Compare) {
  Compare<int>();
  Compare<Comparable>();
  Compare<WTF::String>();
}

TEST(VectorTest, AppendFirst) {
  Vector<WTF::String> vector;
  vector.push_back("string");
  // Test passes if it does not crash (reallocation did not make
  // the input reference stale).
  size_t limit = vector.capacity() + 1;
  for (size_t i = 0; i < limit; i++)
    vector.push_back(vector.front());

  limit = vector.capacity() + 1;
  for (size_t i = 0; i < limit; i++)
    vector.push_back(const_cast<const WTF::String&>(vector.front()));
}

// The test below is for the following issue:
//
// https://bugs.chromium.org/p/chromium/issues/detail?id=592767
//
// where deleted copy assignment operator made canMoveWithMemcpy true because
// of the implementation of std::is_trivially_move_assignable<T>.

class MojoMoveOnlyType final {
 public:
  MojoMoveOnlyType();
  MojoMoveOnlyType(MojoMoveOnlyType&&);
  MojoMoveOnlyType& operator=(MojoMoveOnlyType&&);
  ~MojoMoveOnlyType();

 private:
  MojoMoveOnlyType(const MojoMoveOnlyType&) = delete;
  void operator=(const MojoMoveOnlyType&) = delete;
};

static_assert(!std::is_trivially_move_assignable<MojoMoveOnlyType>::value,
              "MojoMoveOnlyType isn't trivially move assignable.");
static_assert(!std::is_trivially_copy_assignable<MojoMoveOnlyType>::value,
              "MojoMoveOnlyType isn't trivially copy assignable.");

static_assert(!VectorTraits<MojoMoveOnlyType>::kCanMoveWithMemcpy,
              "MojoMoveOnlyType can't be moved with memcpy.");
static_assert(!VectorTraits<MojoMoveOnlyType>::kCanCopyWithMemcpy,
              "MojoMoveOnlyType can't be copied with memcpy.");

class VectorWithDifferingInlineCapacityTest
    : public testing::TestWithParam<size_t> {};

template <size_t inlineCapacity>
void TestVectorDestructorAndConstructorCallsWhenSwappingWithInlineCapacity() {
  LivenessCounter::live_ = 0;
  LivenessCounter counter;
  EXPECT_EQ(0u, LivenessCounter::live_);

  Vector<scoped_refptr<LivenessCounter>, inlineCapacity> vector;
  Vector<scoped_refptr<LivenessCounter>, inlineCapacity> vector2;
  vector.push_back(&counter);
  vector2.push_back(&counter);
  EXPECT_EQ(2u, LivenessCounter::live_);

  for (unsigned i = 0; i < 13; i++) {
    for (unsigned j = 0; j < 13; j++) {
      vector.clear();
      vector2.clear();
      EXPECT_EQ(0u, LivenessCounter::live_);

      for (unsigned k = 0; k < j; k++)
        vector.push_back(&counter);
      EXPECT_EQ(j, LivenessCounter::live_);
      EXPECT_EQ(j, vector.size());

      for (unsigned k = 0; k < i; k++)
        vector2.push_back(&counter);
      EXPECT_EQ(i + j, LivenessCounter::live_);
      EXPECT_EQ(i, vector2.size());

      vector.swap(vector2);
      EXPECT_EQ(i + j, LivenessCounter::live_);
      EXPECT_EQ(i, vector.size());
      EXPECT_EQ(j, vector2.size());

      unsigned size = vector.size();
      unsigned size2 = vector2.size();

      for (unsigned k = 0; k < 5; k++) {
        vector.swap(vector2);
        std::swap(size, size2);
        EXPECT_EQ(i + j, LivenessCounter::live_);
        EXPECT_EQ(size, vector.size());
        EXPECT_EQ(size2, vector2.size());

        vector2.push_back(&counter);
        vector2.EraseAt(0);
      }
    }
  }
}

TEST(VectorTest, SwapWithConstructorsAndDestructors) {
  TestVectorDestructorAndConstructorCallsWhenSwappingWithInlineCapacity<0>();
  TestVectorDestructorAndConstructorCallsWhenSwappingWithInlineCapacity<2>();
  TestVectorDestructorAndConstructorCallsWhenSwappingWithInlineCapacity<10>();
}

template <size_t inlineCapacity>
void TestVectorValuesMovedAndSwappedWithInlineCapacity() {
  Vector<unsigned, inlineCapacity> vector;
  Vector<unsigned, inlineCapacity> vector2;

  for (unsigned size = 0; size < 13; size++) {
    for (unsigned size2 = 0; size2 < 13; size2++) {
      vector.clear();
      vector2.clear();
      for (unsigned i = 0; i < size; i++)
        vector.push_back(i);
      for (unsigned i = 0; i < size2; i++)
        vector2.push_back(i + 42);
      EXPECT_EQ(size, vector.size());
      EXPECT_EQ(size2, vector2.size());
      vector.swap(vector2);
      for (unsigned i = 0; i < size; i++)
        EXPECT_EQ(i, vector2[i]);
      for (unsigned i = 0; i < size2; i++)
        EXPECT_EQ(i + 42, vector[i]);
    }
  }
}

TEST(VectorTest, ValuesMovedAndSwappedWithInlineCapacity) {
  TestVectorValuesMovedAndSwappedWithInlineCapacity<0>();
  TestVectorValuesMovedAndSwappedWithInlineCapacity<2>();
  TestVectorValuesMovedAndSwappedWithInlineCapacity<10>();
}

TEST(VectorTest, UniquePtr) {
  using Pointer = std::unique_ptr<int>;
  Vector<Pointer> vector;
  vector.push_back(Pointer(new int(1)));
  vector.ReserveCapacity(2);
  vector.UncheckedAppend(Pointer(new int(2)));
  vector.insert(2, Pointer(new int(3)));
  vector.push_front(Pointer(new int(0)));

  ASSERT_EQ(4u, vector.size());
  EXPECT_EQ(0, *vector[0]);
  EXPECT_EQ(1, *vector[1]);
  EXPECT_EQ(2, *vector[2]);
  EXPECT_EQ(3, *vector[3]);

  vector.Shrink(3);
  EXPECT_EQ(3u, vector.size());
  vector.Grow(4);
  ASSERT_EQ(4u, vector.size());
  EXPECT_TRUE(!vector[3]);
  vector.EraseAt(3);
  vector[0] = Pointer(new int(-1));
  ASSERT_EQ(3u, vector.size());
  EXPECT_EQ(-1, *vector[0]);
}

bool IsOneTwoThree(const Vector<int>& vector) {
  return vector.size() == 3 && vector[0] == 1 && vector[1] == 2 &&
         vector[2] == 3;
}

Vector<int> ReturnOneTwoThree() {
  return {1, 2, 3};
}

TEST(VectorTest, InitializerList) {
  Vector<int> empty({});
  EXPECT_TRUE(empty.IsEmpty());

  Vector<int> one({1});
  ASSERT_EQ(1u, one.size());
  EXPECT_EQ(1, one[0]);

  Vector<int> one_two_three({1, 2, 3});
  ASSERT_EQ(3u, one_two_three.size());
  EXPECT_EQ(1, one_two_three[0]);
  EXPECT_EQ(2, one_two_three[1]);
  EXPECT_EQ(3, one_two_three[2]);

  // Put some jank so we can check if the assignments later can clear them.
  empty.push_back(9999);
  one.push_back(9999);
  one_two_three.push_back(9999);

  empty = {};
  EXPECT_TRUE(empty.IsEmpty());

  one = {1};
  ASSERT_EQ(1u, one.size());
  EXPECT_EQ(1, one[0]);

  one_two_three = {1, 2, 3};
  ASSERT_EQ(3u, one_two_three.size());
  EXPECT_EQ(1, one_two_three[0]);
  EXPECT_EQ(2, one_two_three[1]);
  EXPECT_EQ(3, one_two_three[2]);

  // Other ways of construction: as a function parameter and in a return
  // statement.
  EXPECT_TRUE(IsOneTwoThree({1, 2, 3}));
  EXPECT_TRUE(IsOneTwoThree(ReturnOneTwoThree()));

  // The tests below correspond to the cases in the "if" branch in
  // operator=(std::initializer_list<T>).

  // Shrinking.
  Vector<int, 1> vector1(3);  // capacity = 3.
  vector1 = {1, 2};
  ASSERT_EQ(2u, vector1.size());
  EXPECT_EQ(1, vector1[0]);
  EXPECT_EQ(2, vector1[1]);

  // Expanding.
  Vector<int, 1> vector2(3);
  vector2 = {1, 2, 3, 4};
  ASSERT_EQ(4u, vector2.size());
  EXPECT_EQ(1, vector2[0]);
  EXPECT_EQ(2, vector2[1]);
  EXPECT_EQ(3, vector2[2]);
  EXPECT_EQ(4, vector2[3]);

  // Exact match.
  Vector<int, 1> vector3(3);
  vector3 = {1, 2, 3};
  ASSERT_EQ(3u, vector3.size());
  EXPECT_EQ(1, vector3[0]);
  EXPECT_EQ(2, vector3[1]);
  EXPECT_EQ(3, vector3[2]);
}

TEST(VectorTest, Optional) {
  base::Optional<Vector<int>> vector;
  EXPECT_FALSE(vector);
  vector.emplace(3);
  EXPECT_TRUE(vector);
  EXPECT_EQ(3u, vector->size());
}

TEST(VectorTest, emplace_back) {
  struct Item {
    Item() = default;
    explicit Item(int value1) : value1(value1), value2() {}
    Item(int value1, int value2) : value1(value1), value2(value2) {}
    int value1;
    int value2;
  };

  Vector<Item> vector;
  vector.emplace_back(1, 2);
  vector.emplace_back(3, 4);
  vector.emplace_back(5);
  vector.emplace_back();

  EXPECT_EQ(4u, vector.size());

  EXPECT_EQ(1, vector[0].value1);
  EXPECT_EQ(2, vector[0].value2);

  EXPECT_EQ(3, vector[1].value1);
  EXPECT_EQ(4, vector[1].value2);

  EXPECT_EQ(5, vector[2].value1);
  EXPECT_EQ(0, vector[2].value2);

  EXPECT_EQ(0, vector[3].value1);
  EXPECT_EQ(0, vector[3].value2);

  // Test returned value.
  Item& item = vector.emplace_back(6, 7);
  EXPECT_EQ(6, item.value1);
  EXPECT_EQ(7, item.value2);
}

TEST(VectorTest, UninitializedFill) {
  Vector<char> v(3, 42);
  EXPECT_EQ(42, v[0]);
  EXPECT_EQ(42, v[1]);
  EXPECT_EQ(42, v[2]);
}

TEST(VectorTest, IteratorSingleInsertion) {
  Vector<int> v;

  v.InsertAt(v.begin(), 1);
  EXPECT_EQ(1, v[0]);

  for (int i : {9, 5, 2, 3, 3, 7, 7, 8, 2, 4, 6})
    v.InsertAt(std::lower_bound(v.begin(), v.end(), i), i);

  EXPECT_TRUE(std::is_sorted(v.begin(), v.end()));
}

TEST(VectorTest, IteratorMultipleInsertion) {
  Vector<int> v = {0, 0, 0, 3, 3, 3};

  Vector<int> q = {1, 1, 1, 1};
  v.InsertAt(std::lower_bound(v.begin(), v.end(), q[0]), &q[0], q.size());

  EXPECT_THAT(v, testing::ElementsAre(0, 0, 0, 1, 1, 1, 1, 3, 3, 3));
  EXPECT_TRUE(std::is_sorted(v.begin(), v.end()));
}

static_assert(VectorTraits<int>::kCanCopyWithMemcpy,
              "int should be copied with memcopy.");
static_assert(VectorTraits<char>::kCanCopyWithMemcpy,
              "char should be copied with memcpy.");
static_assert(VectorTraits<LChar>::kCanCopyWithMemcpy,
              "LChar should be copied with memcpy.");
static_assert(VectorTraits<UChar>::kCanCopyWithMemcpy,
              "UChar should be copied with memcpy.");

class UnknownType;
static_assert(VectorTraits<UnknownType*>::kCanCopyWithMemcpy,
              "Pointers should be copied with memcpy.");

static_assert(!IsTraceable<Vector<int>>::value,
              "Vector<int> must not be traceable.");

}  // anonymous namespace

}  // namespace WTF
