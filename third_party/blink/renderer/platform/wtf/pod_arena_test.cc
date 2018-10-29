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

#include "third_party/blink/renderer/platform/wtf/pod_arena.h"

#include <algorithm>
#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/pod_arena_test_helpers.h"

namespace WTF {

using ArenaTestHelpers::TrackedAllocator;

namespace {

// A couple of simple structs to allocate.
struct TestClassXYZW {
  TestClassXYZW() : x(0), y(0), z(0), w(1) {}

  float x, y, z, w;
};

struct TestClassABCD {
  TestClassABCD() : a(1), b(2), c(3), d(4) {}

  float a, b, c, d;
};

}  // anonymous namespace

class PODArenaTest : public testing::Test {};

// Make sure the arena can successfully allocate from more than one
// region.
TEST_F(PODArenaTest, CanAllocateFromMoreThanOneRegion) {
  scoped_refptr<TrackedAllocator> allocator = TrackedAllocator::Create();
  scoped_refptr<PODArena> arena = PODArena::Create(allocator);
  int num_iterations = 10 * PODArena::kDefaultChunkSize / sizeof(TestClassXYZW);
  for (int i = 0; i < num_iterations; ++i)
    arena->AllocateObject<TestClassXYZW>();
  EXPECT_GT(allocator->NumRegions(), 1);
}

// Make sure the arena frees all allocated regions during destruction.
TEST_F(PODArenaTest, FreesAllAllocatedRegions) {
  scoped_refptr<TrackedAllocator> allocator = TrackedAllocator::Create();
  {
    scoped_refptr<PODArena> arena = PODArena::Create(allocator);
    for (int i = 0; i < 3; i++)
      arena->AllocateObject<TestClassXYZW>();
    EXPECT_GT(allocator->NumRegions(), 0);
  }
  EXPECT_TRUE(allocator->IsEmpty());
}

// Make sure the arena runs constructors of the objects allocated within.
TEST_F(PODArenaTest, RunsConstructors) {
  scoped_refptr<PODArena> arena = PODArena::Create();
  for (int i = 0; i < 10000; i++) {
    TestClassXYZW* tc1 = arena->AllocateObject<TestClassXYZW>();
    EXPECT_EQ(0, tc1->x);
    EXPECT_EQ(0, tc1->y);
    EXPECT_EQ(0, tc1->z);
    EXPECT_EQ(1, tc1->w);
    TestClassABCD* tc2 = arena->AllocateObject<TestClassABCD>();
    EXPECT_EQ(1, tc2->a);
    EXPECT_EQ(2, tc2->b);
    EXPECT_EQ(3, tc2->c);
    EXPECT_EQ(4, tc2->d);
  }
}

}  // namespace WTF
