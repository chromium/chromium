/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Tests for the interval tree class.

#include "third_party/blink/renderer/platform/wtf/pod_interval_tree.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/pod_tree_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

using tree_test_helpers::InitRandom;
using tree_test_helpers::NextRandom;

#ifndef NDEBUG
template <>
struct ValueToString<void*> {
  static String ToString(void* const& value) {
    return String::Format("0x%p", value);
  }
};
#endif

TEST(PodIntevalTreeTest, TestInsertion) {
  PodIntervalTree<float> tree;
  tree.Add(PodInterval<float>(2, 4));
  ASSERT_TRUE(tree.CheckInvariants());
}

TEST(PodIntevalTreeTest, TestInsertionAndQuery) {
  PodIntervalTree<float> tree;
  tree.Add(PodInterval<float>(2, 4));
  ASSERT_TRUE(tree.CheckInvariants());
  Vector<PodInterval<float>> overlap =
      tree.AllOverlaps(PodInterval<float>(1, 3));
  EXPECT_EQ(1U, overlap.size());
  EXPECT_EQ(2, overlap[0].Low());
  EXPECT_EQ(4, overlap[0].High());

  auto next_point = tree.NextIntervalPoint(1);
  EXPECT_TRUE(next_point.has_value());
  EXPECT_EQ(2, next_point.value());

  next_point = tree.NextIntervalPoint(2);
  EXPECT_TRUE(next_point.has_value());
  EXPECT_EQ(4, next_point.value());

  next_point = tree.NextIntervalPoint(3);
  EXPECT_TRUE(next_point.has_value());
  EXPECT_EQ(4, next_point.value());

  next_point = tree.NextIntervalPoint(4);
  EXPECT_FALSE(next_point.has_value());
}

TEST(PodIntevalTreeTest, TestQueryAgainstZeroSizeInterval) {
  PodIntervalTree<float> tree;
  tree.Add(PodInterval<float>(1, 2.5));
  tree.Add(PodInterval<float>(3.5, 5));
  tree.Add(PodInterval<float>(2, 4));
  ASSERT_TRUE(tree.CheckInvariants());
  Vector<PodInterval<float>> result =
      tree.AllOverlaps(PodInterval<float>(3, 3));
  EXPECT_EQ(1U, result.size());
  EXPECT_EQ(2, result[0].Low());
  EXPECT_EQ(4, result[0].High());
}

#ifndef NDEBUG
template <>
struct ValueToString<int*> {
  static String ToString(int* const& value) {
    return String::Format("0x%p", value);
  }
};
#endif

TEST(PodIntevalTreeTest, TestDuplicateElementInsertion) {
  PodIntervalTree<float, int*> tree;
  int tmp1 = 1;
  int tmp2 = 2;
  using IntervalType = PodIntervalTree<float, int*>::IntervalType;
  IntervalType interval1(1, 3, &tmp1);
  IntervalType interval2(1, 3, &tmp2);
  tree.Add(interval1);
  tree.Add(interval2);
  ASSERT_TRUE(tree.CheckInvariants());
  EXPECT_TRUE(tree.Contains(interval1));
  EXPECT_TRUE(tree.Contains(interval2));
  EXPECT_TRUE(tree.Remove(interval1));
  EXPECT_TRUE(tree.Contains(interval2));
  EXPECT_FALSE(tree.Contains(interval1));
  EXPECT_TRUE(tree.Remove(interval2));
  EXPECT_EQ(0, tree.size());
}

namespace {

struct UserData1 {
 public:
  UserData1() : a(0), b(1) {}

  float a;
  int b;
};

}  // anonymous namespace

#ifndef NDEBUG
template <>
struct ValueToString<UserData1> {
  static String ToString(const UserData1& value) {
    return String("[UserData1 a=") + String::Number(value.a) +
           " b=" + String::Number(value.b) + "]";
  }
};
#endif

TEST(PodIntevalTreeTest, TestInsertionOfComplexUserData) {
  PodIntervalTree<float, UserData1> tree;
  UserData1 data1;
  data1.a = 5;
  data1.b = 6;
  tree.Add(tree.CreateInterval(2, 4, data1));
  ASSERT_TRUE(tree.CheckInvariants());
}

TEST(PodIntevalTreeTest, TestQueryingOfComplexUserData) {
  PodIntervalTree<float, UserData1> tree;
  UserData1 data1;
  data1.a = 5;
  data1.b = 6;
  tree.Add(tree.CreateInterval(2, 4, data1));
  ASSERT_TRUE(tree.CheckInvariants());
  Vector<PodInterval<float, UserData1>> overlaps =
      tree.AllOverlaps(tree.CreateInterval(3, 5, data1));
  EXPECT_EQ(1U, overlaps.size());
  EXPECT_EQ(5, overlaps[0].Data().a);
  EXPECT_EQ(6, overlaps[0].Data().b);
}

namespace {

class EndpointType1 {
  STACK_ALLOCATED();

 public:
  explicit EndpointType1(int value) : value_(value) {}

  int Value() const { return value_; }

  bool operator<(const EndpointType1& other) const {
    return value_ < other.value_;
  }
  bool operator==(const EndpointType1& other) const {
    return value_ == other.value_;
  }

 private:
  int value_;
  // These operators should not be called by the interval tree.
  bool operator>(const EndpointType1& other);
  bool operator<=(const EndpointType1& other);
  bool operator>=(const EndpointType1& other);
  bool operator!=(const EndpointType1& other);
};

}  // anonymous namespace

#ifndef NDEBUG
template <>
struct ValueToString<EndpointType1> {
  static String ToString(const EndpointType1& value) {
    return String("[EndpointType1 value=") + String::Number(value.Value()) +
           "]";
  }
};
#endif

TEST(PodIntevalTreeTest, TestTreeDoesNotRequireMostOperators) {
  PodIntervalTree<EndpointType1> tree;
  tree.Add(tree.CreateInterval(EndpointType1(1), EndpointType1(2)));
  ASSERT_TRUE(tree.CheckInvariants());
}

// Uncomment to debug a failure of the insertion and deletion test. Won't work
// in release builds.
// #define DEBUG_INSERTION_AND_DELETION_TEST

namespace {

void TreeInsertionAndDeletionTest(int32_t seed, int tree_size) {
  InitRandom(seed);
  int maximum_value = tree_size;
  // Build the tree
  PodIntervalTree<int> tree;
  Vector<PodInterval<int>> added_elements;
  Vector<PodInterval<int>> removed_elements;
  for (int i = 0; i < tree_size; i++) {
    int left = NextRandom(maximum_value);
    int length = NextRandom(maximum_value);
    PodInterval<int> interval(left, left + length);
    tree.Add(interval);
#ifdef DEBUG_INSERTION_AND_DELETION_TEST
    DLOG(ERROR) << "*** Adding element "
                << ValueToString<PodInterval<int>>::ToString(interval);
#endif
    added_elements.push_back(interval);
  }
  // Churn the tree's contents.
  // First remove half of the elements in random order.
  for (int i = 0; i < tree_size / 2; i++) {
    int index = NextRandom(added_elements.size());
#ifdef DEBUG_INSERTION_AND_DELETION_TEST
    DLOG(ERROR) << "*** Removing element "
                << ValueToString<PodInterval<int>>::ToString(
                       added_elements[index]);
#endif
    ASSERT_TRUE(tree.Contains(added_elements[index]))
        << "Test failed for seed " << seed;
    tree.Remove(added_elements[index]);
    removed_elements.push_back(added_elements[index]);
    added_elements.EraseAt(index);
    ASSERT_TRUE(tree.CheckInvariants()) << "Test failed for seed " << seed;
  }
  // Now randomly add or remove elements.
  for (int i = 0; i < 2 * tree_size; i++) {
    bool add = false;
    if (!added_elements.size())
      add = true;
    else if (!removed_elements.size())
      add = false;
    else
      add = (NextRandom(2) == 1);
    if (add) {
      int index = NextRandom(removed_elements.size());
#ifdef DEBUG_INSERTION_AND_DELETION_TEST
      DLOG(ERROR) << "*** Adding element "
                  << ValueToString<PodInterval<int>>::ToString(
                         removed_elements[index]);
#endif
      tree.Add(removed_elements[index]);
      added_elements.push_back(removed_elements[index]);
      removed_elements.EraseAt(index);
    } else {
      int index = NextRandom(added_elements.size());
#ifdef DEBUG_INSERTION_AND_DELETION_TEST
      DLOG(ERROR) << "*** Removing element "
                  << ValueToString<PodInterval<int>>::ToString(
                         added_elements[index]);
#endif
      ASSERT_TRUE(tree.Contains(added_elements[index]))
          << "Test failed for seed " << seed;
      ASSERT_TRUE(tree.Remove(added_elements[index]))
          << "Test failed for seed " << seed;
      removed_elements.push_back(added_elements[index]);
      added_elements.EraseAt(index);
    }
    ASSERT_TRUE(tree.CheckInvariants()) << "Test failed for seed " << seed;
  }
}

}  // anonymous namespace

TEST(PodIntevalTreeTest, RandomDeletionAndInsertionRegressionTest1) {
  TreeInsertionAndDeletionTest(13972, 100);
}

TEST(PodIntevalTreeTest, RandomDeletionAndInsertionRegressionTest2) {
  TreeInsertionAndDeletionTest(1283382113, 10);
}

TEST(PodIntevalTreeTest, RandomDeletionAndInsertionRegressionTest3) {
  // This is the sequence of insertions and deletions that triggered
  // the failure in RandomDeletionAndInsertionRegressionTest2.
  PodIntervalTree<int> tree;
  tree.Add(tree.CreateInterval(0, 5));
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Add(tree.CreateInterval(4, 5));
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Add(tree.CreateInterval(8, 9));
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Add(tree.CreateInterval(1, 4));
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Add(tree.CreateInterval(3, 5));
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Add(tree.CreateInterval(4, 12));
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Add(tree.CreateInterval(0, 2));
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Add(tree.CreateInterval(0, 2));
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Add(tree.CreateInterval(9, 13));
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Add(tree.CreateInterval(0, 1));
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Remove(tree.CreateInterval(0, 2));
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Remove(tree.CreateInterval(9, 13));
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Remove(tree.CreateInterval(0, 2));
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Remove(tree.CreateInterval(0, 1));
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Remove(tree.CreateInterval(4, 5));
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Remove(tree.CreateInterval(4, 12));
  ASSERT_TRUE(tree.CheckInvariants());
}

TEST(PodIntevalTreeTest, RandomDeletionAndInsertionRegressionTest4) {
  // Even further reduced test case for
  // RandomDeletionAndInsertionRegressionTest3.
  PodIntervalTree<int> tree;
  tree.Add(tree.CreateInterval(0, 5));
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Add(tree.CreateInterval(8, 9));
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Add(tree.CreateInterval(1, 4));
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Add(tree.CreateInterval(3, 5));
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Add(tree.CreateInterval(4, 12));
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Remove(tree.CreateInterval(4, 12));
  ASSERT_TRUE(tree.CheckInvariants());
}

}  // namespace blink
