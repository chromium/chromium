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

// Tests for the red-black tree class.

#include "third_party/blink/renderer/platform/wtf/pod_red_black_tree.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/pod_arena_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/pod_tree_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

using arena_test_helpers::TrackedAllocator;
using tree_test_helpers::InitRandom;
using tree_test_helpers::NextRandom;

TEST(PodRedBlackTreeTest, TestTreeAllocatesFromArena) {
  scoped_refptr<TrackedAllocator> allocator = TrackedAllocator::Create();
  {
    using PodIntegerArena = PodFreeListArena<PodRedBlackTree<int>::Node>;
    scoped_refptr<PodIntegerArena> arena = PodIntegerArena::Create(allocator);
    PodRedBlackTree<int> tree(arena);
    int num_additions = 2 * PodArena::kDefaultChunkSize / sizeof(int);
    for (int i = 0; i < num_additions; ++i)
      tree.Add(i);
    EXPECT_GT(allocator->NumRegions(), 1);
  }
  EXPECT_EQ(allocator->NumRegions(), 0);
}

TEST(PodRedBlackTreeTest, TestSingleElementInsertion) {
  PodRedBlackTree<int> tree;
  tree.Add(5);
  ASSERT_TRUE(tree.CheckInvariants());
  EXPECT_TRUE(tree.Contains(5));
}

TEST(PodRedBlackTreeTest, TestMultipleElementInsertion) {
  PodRedBlackTree<int> tree;
  tree.Add(4);
  ASSERT_TRUE(tree.CheckInvariants());
  EXPECT_TRUE(tree.Contains(4));
  tree.Add(3);
  ASSERT_TRUE(tree.CheckInvariants());
  EXPECT_TRUE(tree.Contains(3));
  tree.Add(5);
  ASSERT_TRUE(tree.CheckInvariants());
  EXPECT_TRUE(tree.Contains(5));
  EXPECT_TRUE(tree.Contains(4));
  EXPECT_TRUE(tree.Contains(3));
}

TEST(PodRedBlackTreeTest, TestDuplicateElementInsertion) {
  PodRedBlackTree<int> tree;
  tree.Add(3);
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Add(3);
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Add(3);
  ASSERT_TRUE(tree.CheckInvariants());
  EXPECT_EQ(3, tree.size());
  EXPECT_TRUE(tree.Contains(3));
}

TEST(PodRedBlackTreeTest, TestSingleElementInsertionAndDeletion) {
  PodRedBlackTree<int> tree;
  tree.Add(5);
  ASSERT_TRUE(tree.CheckInvariants());
  EXPECT_TRUE(tree.Contains(5));
  tree.Remove(5);
  ASSERT_TRUE(tree.CheckInvariants());
  EXPECT_FALSE(tree.Contains(5));
}

TEST(PodRedBlackTreeTest, TestMultipleElementInsertionAndDeletion) {
  PodRedBlackTree<int> tree;
  tree.Add(4);
  ASSERT_TRUE(tree.CheckInvariants());
  EXPECT_TRUE(tree.Contains(4));
  tree.Add(3);
  ASSERT_TRUE(tree.CheckInvariants());
  EXPECT_TRUE(tree.Contains(3));
  tree.Add(5);
  ASSERT_TRUE(tree.CheckInvariants());
  EXPECT_TRUE(tree.Contains(5));
  EXPECT_TRUE(tree.Contains(4));
  EXPECT_TRUE(tree.Contains(3));
  tree.Remove(4);
  ASSERT_TRUE(tree.CheckInvariants());
  EXPECT_TRUE(tree.Contains(3));
  EXPECT_FALSE(tree.Contains(4));
  EXPECT_TRUE(tree.Contains(5));
  tree.Remove(5);
  ASSERT_TRUE(tree.CheckInvariants());
  EXPECT_TRUE(tree.Contains(3));
  EXPECT_FALSE(tree.Contains(4));
  EXPECT_FALSE(tree.Contains(5));
  EXPECT_EQ(1, tree.size());
}

TEST(PodRedBlackTreeTest, TestDuplicateElementInsertionAndDeletion) {
  PodRedBlackTree<int> tree;
  tree.Add(3);
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Add(3);
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Add(3);
  ASSERT_TRUE(tree.CheckInvariants());
  EXPECT_EQ(3, tree.size());
  EXPECT_TRUE(tree.Contains(3));
  tree.Remove(3);
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Remove(3);
  ASSERT_TRUE(tree.CheckInvariants());
  EXPECT_EQ(1, tree.size());
  EXPECT_TRUE(tree.Contains(3));
  tree.Remove(3);
  ASSERT_TRUE(tree.CheckInvariants());
  EXPECT_EQ(0, tree.size());
  EXPECT_FALSE(tree.Contains(3));
}

TEST(PodRedBlackTreeTest, FailingInsertionRegressionTest1) {
  // These numbers came from a previously-failing randomized test run.
  PodRedBlackTree<int> tree;
  tree.Add(5113);
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Add(4517);
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Add(3373);
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Add(9307);
  ASSERT_TRUE(tree.CheckInvariants());
  tree.Add(7077);
  ASSERT_TRUE(tree.CheckInvariants());
}

namespace {
void InsertionAndDeletionTest(const int32_t seed, const int tree_size) {
  InitRandom(seed);
  const int maximum_value = tree_size;
  // Build the tree.
  PodRedBlackTree<int> tree;
  Vector<int> values;
  for (int i = 0; i < tree_size; i++) {
    int value = NextRandom(maximum_value);
    tree.Add(value);
    ASSERT_TRUE(tree.CheckInvariants()) << "Test failed for seed " << seed;
    values.push_back(value);
  }
  // Churn the tree's contents.
  for (int i = 0; i < tree_size; i++) {
    // Pick a random value to remove.
    int index = NextRandom(tree_size);
    int value = values[index];
    // Remove this value.
    tree.Remove(value);
    ASSERT_TRUE(tree.CheckInvariants()) << "Test failed for seed " << seed;
    // Replace it with a new one.
    value = NextRandom(maximum_value);
    values[index] = value;
    tree.Add(value);
    ASSERT_TRUE(tree.CheckInvariants()) << "Test failed for seed " << seed;
  }
}
}  // anonymous namespace

TEST(PodRedBlackTreeTest, RandomDeletionAndInsertionRegressionTest1) {
  InsertionAndDeletionTest(12311, 100);
}

}  // namespace blink
