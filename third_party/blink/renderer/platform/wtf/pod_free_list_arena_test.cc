/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/wtf/pod_free_list_arena.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/pod_arena_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace WTF {

using arena_test_helpers::TrackedAllocator;

namespace {

// A couple of simple structs to allocate.
struct TestClass1 {
  TestClass1() : x(0), y(0), z(0), w(1) {}

  float x, y, z, w;
};

struct TestClass2 {
  TestClass2() : padding(0) {
    static int test_ids = 0;
    id = test_ids++;
  }
  int id;
  int padding;
};

}  // anonymous namespace

class PODFreeListArenaTest : public testing::Test {
 protected:
  int GetFreeListSize(scoped_refptr<PODFreeListArena<TestClass1>> arena) const {
    return arena->GetFreeListSizeForTesting();
  }
};

// Make sure the arena can successfully allocate from more than one
// region.
TEST_F(PODFreeListArenaTest, CanAllocateFromMoreThanOneRegion) {
  scoped_refptr<TrackedAllocator> allocator = TrackedAllocator::Create();
  scoped_refptr<PODFreeListArena<TestClass1>> arena =
      PODFreeListArena<TestClass1>::Create(allocator);
  int num_iterations = 10 * PODArena::kDefaultChunkSize / sizeof(TestClass1);
  for (int i = 0; i < num_iterations; ++i)
    arena->AllocateObject();
  EXPECT_GT(allocator->NumRegions(), 1);
}

// Make sure the arena frees all allocated regions during destruction.
TEST_F(PODFreeListArenaTest, FreesAllAllocatedRegions) {
  scoped_refptr<TrackedAllocator> allocator = TrackedAllocator::Create();
  {
    scoped_refptr<PODFreeListArena<TestClass1>> arena =
        PODFreeListArena<TestClass1>::Create(allocator);
    for (int i = 0; i < 3; i++)
      arena->AllocateObject();
    EXPECT_GT(allocator->NumRegions(), 0);
  }
  EXPECT_TRUE(allocator->IsEmpty());
}

// Make sure the arena runs constructors of the objects allocated within.
TEST_F(PODFreeListArenaTest, RunsConstructorsOnNewObjects) {
  scoped_refptr<PODFreeListArena<TestClass1>> arena =
      PODFreeListArena<TestClass1>::Create();
  for (int i = 0; i < 10000; i++) {
    TestClass1* tc1 = arena->AllocateObject();
    EXPECT_EQ(0, tc1->x);
    EXPECT_EQ(0, tc1->y);
    EXPECT_EQ(0, tc1->z);
    EXPECT_EQ(1, tc1->w);
  }
}

// Make sure the arena runs constructors of the objects allocated within.
TEST_F(PODFreeListArenaTest, RunsConstructorsOnReusedObjects) {
  HashSet<TestClass1*> objects;
  scoped_refptr<PODFreeListArena<TestClass1>> arena =
      PODFreeListArena<TestClass1>::Create();
  for (int i = 0; i < 100; i++) {
    TestClass1* tc1 = arena->AllocateObject();
    tc1->x = 100;
    tc1->y = 101;
    tc1->z = 102;
    tc1->w = 103;

    objects.insert(tc1);
  }
  for (HashSet<TestClass1*>::iterator it = objects.begin(); it != objects.end();
       ++it) {
    arena->FreeObject(*it);
  }
  for (int i = 0; i < 100; i++) {
    TestClass1* cur = arena->AllocateObject();
    EXPECT_TRUE(objects.find(cur) != objects.end());
    EXPECT_EQ(0, cur->x);
    EXPECT_EQ(0, cur->y);
    EXPECT_EQ(0, cur->z);
    EXPECT_EQ(1, cur->w);

    objects.erase(cur);
  }
}

// Make sure freeObject puts the object in the free list.
TEST_F(PODFreeListArenaTest, AddsFreedObjectsToFreedList) {
  Vector<TestClass1*, 100> objects;
  scoped_refptr<PODFreeListArena<TestClass1>> arena =
      PODFreeListArena<TestClass1>::Create();
  for (int i = 0; i < 100; i++) {
    objects.push_back(arena->AllocateObject());
  }
  for (auto* object : objects) {
    arena->FreeObject(object);
  }
  EXPECT_EQ(100, GetFreeListSize(arena));
}

// Make sure allocations use previously freed memory.
TEST_F(PODFreeListArenaTest, ReusesPreviouslyFreedObjects) {
  HashSet<TestClass2*> objects;
  scoped_refptr<PODFreeListArena<TestClass2>> arena =
      PODFreeListArena<TestClass2>::Create();
  for (int i = 0; i < 100; i++) {
    objects.insert(arena->AllocateObject());
  }
  for (HashSet<TestClass2*>::iterator it = objects.begin(); it != objects.end();
       ++it) {
    arena->FreeObject(*it);
  }
  for (int i = 0; i < 100; i++) {
    TestClass2* cur = arena->AllocateObject();
    EXPECT_TRUE(objects.find(cur) != objects.end());
    EXPECT_TRUE(cur->id >= 100 && cur->id < 200);
    objects.erase(cur);
  }
}

}  // namespace WTF
