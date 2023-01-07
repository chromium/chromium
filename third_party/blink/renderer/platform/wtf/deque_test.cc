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

#include "third_party/blink/renderer/platform/wtf/deque.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/wtf_test_helper.h"

namespace WTF {

namespace {

TEST(DequeTest, Basic) {
  Deque<int> int_deque;
  EXPECT_TRUE(int_deque.empty());
  EXPECT_EQ(0ul, int_deque.size());
}

TEST(DequeTest, Iterators) {
  Deque<int, 2> deque;
  deque.push_back(0);
  deque.push_back(1);

  {
    auto it = deque.begin();
    EXPECT_EQ(*it, 0);
    EXPECT_EQ(*++it, 1);
    EXPECT_EQ(++it, deque.end());
  }

  {
    auto it = deque.begin();
    EXPECT_EQ(*it++, 0);
    EXPECT_EQ(*it++, 1);
    EXPECT_EQ(it, deque.end());
  }

  {
    const Deque<int, 2>& c_deque = deque;
    auto c_it = c_deque.begin();
    EXPECT_EQ(*c_it, 0);
    EXPECT_EQ(*++c_it, 1);
    EXPECT_EQ(++c_it, c_deque.end());
  }

  {
    const Deque<int, 2>& c_deque = deque;
    auto c_it = c_deque.begin();
    EXPECT_EQ(*c_it++, 0);
    EXPECT_EQ(*c_it++, 1);
    EXPECT_EQ(c_it, c_deque.end());
  }
}

template <wtf_size_t inlineCapacity>
void CheckNumberSequence(Deque<int, inlineCapacity>& deque,
                         int from,
                         int to,
                         bool increment) {
  auto it = increment ? deque.begin() : deque.end();
  wtf_size_t index = increment ? 0 : deque.size();
  int step = from < to ? 1 : -1;
  for (int i = from; i != to + step; i += step) {
    if (!increment) {
      --it;
      --index;
    }

    EXPECT_EQ(i, *it);
    EXPECT_EQ(i, deque[index]);

    if (increment) {
      ++it;
      ++index;
    }
  }
  EXPECT_EQ(increment ? deque.end() : deque.begin(), it);
  EXPECT_EQ(increment ? deque.size() : 0, index);
}

template <wtf_size_t inlineCapacity>
void CheckNumberSequenceReverse(Deque<int, inlineCapacity>& deque,
                                int from,
                                int to,
                                bool increment) {
  auto it = increment ? deque.rbegin() : deque.rend();
  wtf_size_t index = increment ? 0 : deque.size();
  int step = from < to ? 1 : -1;
  for (int i = from; i != to + step; i += step) {
    if (!increment) {
      --it;
      --index;
    }

    EXPECT_EQ(i, *it);
    EXPECT_EQ(i, deque.at(deque.size() - 1 - index));

    if (increment) {
      ++it;
      ++index;
    }
  }
  EXPECT_EQ(increment ? deque.rend() : deque.rbegin(), it);
  EXPECT_EQ(increment ? deque.size() : 0, index);
}

template <wtf_size_t inlineCapacity>
void ReverseTest() {
  Deque<int, inlineCapacity> int_deque;
  int_deque.push_back(10);
  int_deque.push_back(11);
  int_deque.push_back(12);
  int_deque.push_back(13);

  CheckNumberSequence(int_deque, 10, 13, true);
  CheckNumberSequence(int_deque, 13, 10, false);
  CheckNumberSequenceReverse(int_deque, 13, 10, true);
  CheckNumberSequenceReverse(int_deque, 10, 13, false);

  int_deque.push_back(14);
  int_deque.push_back(15);
  EXPECT_EQ(10, int_deque.TakeFirst());
  EXPECT_EQ(15, int_deque.TakeLast());
  CheckNumberSequence(int_deque, 11, 14, true);
  CheckNumberSequence(int_deque, 14, 11, false);
  CheckNumberSequenceReverse(int_deque, 14, 11, true);
  CheckNumberSequenceReverse(int_deque, 11, 14, false);

  for (int i = 15; i < 200; ++i)
    int_deque.push_back(i);
  CheckNumberSequence(int_deque, 11, 199, true);
  CheckNumberSequence(int_deque, 199, 11, false);
  CheckNumberSequenceReverse(int_deque, 199, 11, true);
  CheckNumberSequenceReverse(int_deque, 11, 199, false);

  for (int i = 0; i < 180; ++i) {
    EXPECT_EQ(i + 11, int_deque[0]);
    EXPECT_EQ(i + 11, int_deque.TakeFirst());
  }
  CheckNumberSequence(int_deque, 191, 199, true);
  CheckNumberSequence(int_deque, 199, 191, false);
  CheckNumberSequenceReverse(int_deque, 199, 191, true);
  CheckNumberSequenceReverse(int_deque, 191, 199, false);

  Deque<int, inlineCapacity> int_deque2;
  swap(int_deque, int_deque2);

  CheckNumberSequence(int_deque2, 191, 199, true);
  CheckNumberSequence(int_deque2, 199, 191, false);
  CheckNumberSequenceReverse(int_deque2, 199, 191, true);
  CheckNumberSequenceReverse(int_deque2, 191, 199, false);

  int_deque.Swap(int_deque2);

  CheckNumberSequence(int_deque, 191, 199, true);
  CheckNumberSequence(int_deque, 199, 191, false);
  CheckNumberSequenceReverse(int_deque, 199, 191, true);
  CheckNumberSequenceReverse(int_deque, 191, 199, false);

  int_deque.Swap(int_deque2);

  CheckNumberSequence(int_deque2, 191, 199, true);
  CheckNumberSequence(int_deque2, 199, 191, false);
  CheckNumberSequenceReverse(int_deque2, 199, 191, true);
  CheckNumberSequenceReverse(int_deque2, 191, 199, false);
}

TEST(DequeTest, Reverse) {
  ReverseTest<0>();
  ReverseTest<2>();
}

template <typename OwnPtrDeque>
void OwnPtrTest() {
  int destruct_number = 0;
  OwnPtrDeque deque;
  deque.push_back(std::make_unique<DestructCounter>(0, &destruct_number));
  deque.push_back(std::make_unique<DestructCounter>(1, &destruct_number));
  EXPECT_EQ(2u, deque.size());

  std::unique_ptr<DestructCounter>& counter0 = deque.front();
  EXPECT_EQ(0, counter0->Get());
  int counter1 = deque.back()->Get();
  EXPECT_EQ(1, counter1);
  EXPECT_EQ(0, destruct_number);

  size_t index = 0;
  for (auto iter = deque.begin(); iter != deque.end(); ++iter) {
    std::unique_ptr<DestructCounter>& ref_counter = *iter;
    EXPECT_EQ(index, static_cast<size_t>(ref_counter->Get()));
    EXPECT_EQ(index, static_cast<size_t>((*ref_counter).Get()));
    index++;
  }
  EXPECT_EQ(0, destruct_number);

  auto it = deque.begin();
  for (index = 0; index < deque.size(); ++index) {
    std::unique_ptr<DestructCounter>& ref_counter = *it;
    EXPECT_EQ(index, static_cast<size_t>(ref_counter->Get()));
    index++;
    ++it;
  }
  EXPECT_EQ(0, destruct_number);

  EXPECT_EQ(0, deque.front()->Get());
  deque.pop_front();
  EXPECT_EQ(1, deque.front()->Get());
  EXPECT_EQ(1u, deque.size());
  EXPECT_EQ(1, destruct_number);

  std::unique_ptr<DestructCounter> own_counter1 = std::move(deque.front());
  deque.pop_front();
  EXPECT_EQ(counter1, own_counter1->Get());
  EXPECT_EQ(0u, deque.size());
  EXPECT_EQ(1, destruct_number);

  own_counter1.reset();
  EXPECT_EQ(2, destruct_number);

  size_t count = 1025;
  destruct_number = 0;
  for (size_t i = 0; i < count; ++i)
    deque.push_front(std::make_unique<DestructCounter>(i, &destruct_number));

  // Deque relocation must not destruct std::unique_ptr element.
  EXPECT_EQ(0, destruct_number);
  EXPECT_EQ(count, deque.size());

  OwnPtrDeque copy_deque;
  deque.Swap(copy_deque);
  EXPECT_EQ(0, destruct_number);
  EXPECT_EQ(count, copy_deque.size());
  EXPECT_EQ(0u, deque.size());

  copy_deque.clear();
  EXPECT_EQ(count, static_cast<size_t>(destruct_number));
}

TEST(DequeTest, OwnPtr) {
  OwnPtrTest<Deque<std::unique_ptr<DestructCounter>>>();
  OwnPtrTest<Deque<std::unique_ptr<DestructCounter>, 2>>();
}

TEST(DequeTest, MoveOnlyType) {
  Deque<MoveOnly> deque;
  deque.push_back(MoveOnly(1));
  deque.push_back(MoveOnly(2));
  EXPECT_EQ(2u, deque.size());

  ASSERT_EQ(1, deque.front().Value());
  ASSERT_EQ(2, deque.back().Value());

  MoveOnly old_first = deque.TakeFirst();
  ASSERT_EQ(1, old_first.Value());
  EXPECT_EQ(1u, deque.size());

  Deque<MoveOnly> other_deque;
  deque.Swap(other_deque);
  EXPECT_EQ(1u, other_deque.size());
  EXPECT_EQ(0u, deque.size());
}

HashSet<void*> g_constructed_wrapped_ints;

template <wtf_size_t inlineCapacity>
void SwapWithOrWithoutInlineCapacity() {
  Deque<WrappedInt, inlineCapacity> deque_a;
  deque_a.push_back(WrappedInt(1));
  Deque<WrappedInt, inlineCapacity> deque_b;
  deque_b.push_back(WrappedInt(2));

  ASSERT_EQ(deque_a.size(), deque_b.size());
  deque_a.Swap(deque_b);

  ASSERT_EQ(1u, deque_a.size());
  EXPECT_EQ(2, deque_a.front().Get());
  ASSERT_EQ(1u, deque_b.size());
  EXPECT_EQ(1, deque_b.front().Get());

  deque_a.push_back(WrappedInt(3));

  ASSERT_GT(deque_a.size(), deque_b.size());
  deque_a.Swap(deque_b);

  ASSERT_EQ(1u, deque_a.size());
  EXPECT_EQ(1, deque_a.front().Get());
  ASSERT_EQ(2u, deque_b.size());
  EXPECT_EQ(2, deque_b.front().Get());

  ASSERT_LT(deque_a.size(), deque_b.size());
  deque_a.Swap(deque_b);

  ASSERT_EQ(2u, deque_a.size());
  EXPECT_EQ(2, deque_a.front().Get());
  ASSERT_EQ(1u, deque_b.size());
  EXPECT_EQ(1, deque_b.front().Get());

  deque_a.push_back(WrappedInt(4));
  deque_a.Swap(deque_b);

  ASSERT_EQ(1u, deque_a.size());
  EXPECT_EQ(1, deque_a.front().Get());
  ASSERT_EQ(3u, deque_b.size());
  EXPECT_EQ(2, deque_b.front().Get());

  deque_b.Swap(deque_a);
}

TEST(DequeTest, SwapWithOrWithoutInlineCapacity) {
  SwapWithOrWithoutInlineCapacity<0>();
  SwapWithOrWithoutInlineCapacity<2>();
}

// Filter a few numbers out to improve the running speed of the tests. The
// test has nested loops, and removing even numbers from 4 and up from the
// loops makes it run 10 times faster.
bool InterestingNumber(int i) {
  return i < 4 || (i & 1);
}

template <wtf_size_t inlineCapacity>
void TestDequeDestructorAndConstructorCallsWhenSwappingWithInlineCapacity() {
  LivenessCounter::live_ = 0;
  LivenessCounter counter;
  EXPECT_EQ(0u, LivenessCounter::live_);

  Deque<scoped_refptr<LivenessCounter>, inlineCapacity> deque;
  Deque<scoped_refptr<LivenessCounter>, inlineCapacity> deque2;
  deque.push_back(&counter);
  deque2.push_back(&counter);
  EXPECT_EQ(2u, LivenessCounter::live_);

  // Add various numbers of elements to deques, then remove various numbers
  // of elements from the head. This creates in-use ranges in the backing
  // that sometimes wrap around the end of the buffer, testing various ways
  // in which the in-use ranges of the inline buffers can overlap when we
  // call swap().
  for (unsigned i = 0; i < 12; i++) {
    if (!InterestingNumber(i))
      continue;
    for (unsigned j = i; j < 12; j++) {
      if (!InterestingNumber(j))
        continue;
      deque.clear();
      deque2.clear();
      EXPECT_EQ(0u, LivenessCounter::live_);
      for (unsigned k = 0; k < j; k++)
        deque.push_back(&counter);
      EXPECT_EQ(j, LivenessCounter::live_);
      EXPECT_EQ(j, deque.size());
      for (unsigned k = 0; k < i; k++)
        deque.pop_front();

      EXPECT_EQ(j - i, LivenessCounter::live_);
      EXPECT_EQ(j - i, deque.size());
      deque.Swap(deque2);
      EXPECT_EQ(j - i, LivenessCounter::live_);
      EXPECT_EQ(0u, deque.size());
      EXPECT_EQ(j - i, deque2.size());
      deque.Swap(deque2);
      EXPECT_EQ(j - i, LivenessCounter::live_);

      deque2.push_back(&counter);
      deque2.push_back(&counter);
      deque2.push_back(&counter);

      for (unsigned k = 0; k < 12; k++) {
        EXPECT_EQ(3 + j - i, LivenessCounter::live_);
        EXPECT_EQ(j - i, deque.size());
        EXPECT_EQ(3u, deque2.size());
        deque.Swap(deque2);
        EXPECT_EQ(3 + j - i, LivenessCounter::live_);
        EXPECT_EQ(j - i, deque2.size());
        EXPECT_EQ(3u, deque.size());
        deque.Swap(deque2);
        EXPECT_EQ(3 + j - i, LivenessCounter::live_);
        EXPECT_EQ(j - i, deque.size());
        EXPECT_EQ(3u, deque2.size());

        deque2.pop_front();
        deque2.push_back(&counter);
      }
    }
  }
}

TEST(DequeTest, SwapWithConstructorsAndDestructors) {
  TestDequeDestructorAndConstructorCallsWhenSwappingWithInlineCapacity<0>();
  TestDequeDestructorAndConstructorCallsWhenSwappingWithInlineCapacity<4>();
  TestDequeDestructorAndConstructorCallsWhenSwappingWithInlineCapacity<9>();
}

template <wtf_size_t inlineCapacity>
void TestDequeValuesMovedAndSwappedWithInlineCapacity() {
  Deque<unsigned, inlineCapacity> deque;
  Deque<unsigned, inlineCapacity> deque2;

  // Add various numbers of elements to deques, then remove various numbers
  // of elements from the head. This creates in-use ranges in the backing
  // that sometimes wrap around the end of the buffer, testing various ways
  // in which the in-use ranges of the inline buffers can overlap when we
  // call swap().
  for (unsigned pad = 0; pad < 12; pad++) {
    if (!InterestingNumber(pad))
      continue;
    for (unsigned pad2 = 0; pad2 < 12; pad2++) {
      if (!InterestingNumber(pad2))
        continue;
      for (unsigned size = 0; size < 12; size++) {
        if (!InterestingNumber(size))
          continue;
        for (unsigned size2 = 0; size2 < 12; size2++) {
          if (!InterestingNumber(size2))
            continue;
          deque.clear();
          deque2.clear();
          for (unsigned i = 0; i < pad; i++)
            deque.push_back(103);
          for (unsigned i = 0; i < pad2; i++)
            deque2.push_back(888);
          for (unsigned i = 0; i < size; i++)
            deque.push_back(i);
          for (unsigned i = 0; i < size2; i++)
            deque2.push_back(i + 42);
          for (unsigned i = 0; i < pad; i++)
            EXPECT_EQ(103u, deque.TakeFirst());
          for (unsigned i = 0; i < pad2; i++)
            EXPECT_EQ(888u, deque2.TakeFirst());
          EXPECT_EQ(size, deque.size());
          EXPECT_EQ(size2, deque2.size());
          deque.Swap(deque2);
          for (unsigned i = 0; i < size; i++)
            EXPECT_EQ(i, deque2.TakeFirst());
          for (unsigned i = 0; i < size2; i++)
            EXPECT_EQ(i + 42, deque.TakeFirst());
        }
      }
    }
  }
}

TEST(DequeTest, ValuesMovedAndSwappedWithInlineCapacity) {
  TestDequeValuesMovedAndSwappedWithInlineCapacity<0>();
  TestDequeValuesMovedAndSwappedWithInlineCapacity<4>();
  TestDequeValuesMovedAndSwappedWithInlineCapacity<9>();
}

TEST(DequeTest, UniquePtr) {
  using Pointer = std::unique_ptr<int>;
  Deque<Pointer> deque;
  deque.push_back(std::make_unique<int>(1));
  deque.push_back(std::make_unique<int>(2));
  deque.push_front(std::make_unique<int>(-1));
  deque.push_front(std::make_unique<int>(-2));
  ASSERT_EQ(4u, deque.size());
  EXPECT_EQ(-2, *deque[0]);
  EXPECT_EQ(-1, *deque[1]);
  EXPECT_EQ(1, *deque[2]);
  EXPECT_EQ(2, *deque[3]);

  Pointer first(deque.TakeFirst());
  EXPECT_EQ(-2, *first);
  Pointer last(deque.TakeLast());
  EXPECT_EQ(2, *last);

  EXPECT_EQ(2u, deque.size());
  deque.pop_front();
  deque.pop_back();
  EXPECT_EQ(0u, deque.size());

  deque.push_back(std::make_unique<int>(42));
  deque[0] = std::make_unique<int>(24);
  ASSERT_EQ(1u, deque.size());
  EXPECT_EQ(24, *deque[0]);

  deque.clear();
}

TEST(DequeTest, MoveShouldNotMakeCopy) {
  // Because data in inline buffer may be swapped or moved individually, we
  // force the creation of out-of-line buffer so we can make sure there's no
  // element-wise copy/move.
  Deque<CountCopy, 1> deque;
  int counter = 0;
  deque.push_back(CountCopy(&counter));
  deque.push_back(CountCopy(&counter));

  Deque<CountCopy, 1> other(deque);
  counter = 0;
  deque = std::move(other);  // Move assignment.
  EXPECT_EQ(0, counter);

  counter = 0;
  Deque<CountCopy, 1> yet_another(std::move(deque));  // Move construction.
  EXPECT_EQ(0, counter);
}

TEST(DequeTest, RemoveWhileIterating) {
  Deque<int> deque;
  for (int i = 0; i < 10; ++i)
    deque.push_back(i);

  // All numbers present.
  {
    int i = 0;
    for (int v : deque)
      EXPECT_EQ(i++, v);
  }

  // Remove the even numbers while iterating.
  for (auto it = deque.begin(); it != deque.end(); ++it) {
    if (*it % 2 == 0) {
      deque.erase(it);
      --it;
    }
  }

  // Only odd numbers left.
  {
    int i = 1;
    for (int v : deque)
      EXPECT_EQ(i + 2, v);
  }
}

struct Item {
  Item(int value1, int value2) : value1(value1), value2(value2) {}
  int value1;
  int value2;
};

TEST(DequeTest, emplace_back) {
  Deque<Item> deque;
  deque.emplace_back(1, 2);
  deque.emplace_back(3, 4);

  EXPECT_EQ(2u, deque.size());
  EXPECT_EQ(1, deque[0].value1);
  EXPECT_EQ(2, deque[0].value2);
  EXPECT_EQ(3, deque[1].value1);
  EXPECT_EQ(4, deque[1].value2);
}

TEST(DequeTest, emplace_front) {
  Deque<Item> deque;
  deque.emplace_front(1, 2);
  deque.emplace_front(3, 4);

  EXPECT_EQ(2u, deque.size());
  EXPECT_EQ(3, deque[0].value1);
  EXPECT_EQ(4, deque[0].value2);
  EXPECT_EQ(1, deque[1].value1);
  EXPECT_EQ(2, deque[1].value2);
}

static_assert(!IsTraceable<Deque<int>>::value,
              "Deque<int> must not be traceable.");

}  // anonymous namespace
}  // namespace WTF
