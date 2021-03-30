// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Copied and adopted from V8.
//
// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/impl/worklist.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {
class SomeObject {};
}  // namespace

using TestWorklist = Worklist<SomeObject*, 64 /* entries */, 8 /* tasks */>;

TEST(WorklistTest, SegmentCreate) {
  TestWorklist::Segment segment;
  EXPECT_TRUE(segment.IsEmpty());
  EXPECT_EQ(0u, segment.Size());
  EXPECT_FALSE(segment.IsFull());
}

TEST(WorklistTest, SegmentPush) {
  TestWorklist::Segment segment;
  EXPECT_EQ(0u, segment.Size());
  EXPECT_TRUE(segment.Push(nullptr));
  EXPECT_EQ(1u, segment.Size());
}

TEST(WorklistTest, SegmentPushPop) {
  TestWorklist::Segment segment;
  EXPECT_TRUE(segment.Push(nullptr));
  EXPECT_EQ(1u, segment.Size());
  SomeObject dummy;
  SomeObject* object = &dummy;
  EXPECT_TRUE(segment.Pop(&object));
  EXPECT_EQ(0u, segment.Size());
  EXPECT_EQ(nullptr, object);
}

TEST(WorklistTest, SegmentIsEmpty) {
  TestWorklist::Segment segment;
  EXPECT_TRUE(segment.IsEmpty());
  EXPECT_TRUE(segment.Push(nullptr));
  EXPECT_FALSE(segment.IsEmpty());
}

TEST(WorklistTest, SegmentIsFull) {
  TestWorklist::Segment segment;
  EXPECT_FALSE(segment.IsFull());
  for (size_t i = 0; i < TestWorklist::Segment::kCapacity; i++) {
    EXPECT_TRUE(segment.Push(nullptr));
  }
  EXPECT_TRUE(segment.IsFull());
}

TEST(WorklistTest, SegmentClear) {
  TestWorklist::Segment segment;
  EXPECT_TRUE(segment.Push(nullptr));
  EXPECT_FALSE(segment.IsEmpty());
  segment.Clear();
  EXPECT_TRUE(segment.IsEmpty());
  for (size_t i = 0; i < TestWorklist::Segment::kCapacity; i++) {
    EXPECT_TRUE(segment.Push(nullptr));
  }
}

TEST(WorklistTest, SegmentFullPushFails) {
  TestWorklist::Segment segment;
  EXPECT_FALSE(segment.IsFull());
  for (size_t i = 0; i < TestWorklist::Segment::kCapacity; i++) {
    EXPECT_TRUE(segment.Push(nullptr));
  }
  EXPECT_TRUE(segment.IsFull());
  EXPECT_FALSE(segment.Push(nullptr));
}

TEST(WorklistTest, SegmentEmptyPopFails) {
  TestWorklist::Segment segment;
  EXPECT_TRUE(segment.IsEmpty());
  SomeObject* object;
  EXPECT_FALSE(segment.Pop(&object));
}

TEST(WorklistTest, SegmentUpdateFalse) {
  TestWorklist::Segment segment;
  SomeObject* object;
  object = reinterpret_cast<SomeObject*>(&object);
  EXPECT_TRUE(segment.Push(object));
  segment.Update([](SomeObject* object, SomeObject** out) { return false; });
  EXPECT_TRUE(segment.IsEmpty());
}

TEST(WorklistTest, SegmentUpdate) {
  TestWorklist::Segment segment;
  SomeObject* objectA;
  objectA = reinterpret_cast<SomeObject*>(&objectA);
  SomeObject* objectB;
  objectB = reinterpret_cast<SomeObject*>(&objectB);
  EXPECT_TRUE(segment.Push(objectA));
  segment.Update([objectB](SomeObject* object, SomeObject** out) {
    *out = objectB;
    return true;
  });
  SomeObject* object;
  EXPECT_TRUE(segment.Pop(&object));
  EXPECT_EQ(object, objectB);
}

TEST(WorklistTest, CreateEmpty) {
  TestWorklist worklist;
  TestWorklist::View worklist_view(&worklist, 0);
  EXPECT_TRUE(worklist_view.IsLocalEmpty());
  EXPECT_TRUE(worklist.IsGlobalEmpty());
}

TEST(WorklistTest, LocalPushPop) {
  TestWorklist worklist;
  TestWorklist::View worklist_view(&worklist, 0);
  SomeObject dummy;
  SomeObject* retrieved = nullptr;
  EXPECT_TRUE(worklist_view.Push(&dummy));
  EXPECT_FALSE(worklist_view.IsLocalEmpty());
  EXPECT_TRUE(worklist_view.Pop(&retrieved));
  EXPECT_EQ(&dummy, retrieved);
}

TEST(WorklistTest, LocalIsBasedOnId) {
  TestWorklist worklist;
  // Use the same id.
  TestWorklist::View worklist_view1(&worklist, 0);
  TestWorklist::View worklist_view2(&worklist, 0);
  SomeObject dummy;
  SomeObject* retrieved = nullptr;
  EXPECT_TRUE(worklist_view1.Push(&dummy));
  EXPECT_FALSE(worklist_view1.IsLocalEmpty());
  EXPECT_FALSE(worklist_view2.IsLocalEmpty());
  EXPECT_TRUE(worklist_view2.Pop(&retrieved));
  EXPECT_EQ(&dummy, retrieved);
  EXPECT_TRUE(worklist_view1.IsLocalEmpty());
  EXPECT_TRUE(worklist_view2.IsLocalEmpty());
}

TEST(WorklistTest, LocalPushStaysPrivate) {
  TestWorklist worklist;
  TestWorklist::View worklist_view1(&worklist, 0);
  TestWorklist::View worklist_view2(&worklist, 1);
  SomeObject dummy;
  SomeObject* retrieved = nullptr;
  EXPECT_TRUE(worklist.IsGlobalEmpty());
  EXPECT_EQ(0U, worklist.GlobalPoolSize());
  EXPECT_TRUE(worklist_view1.Push(&dummy));
  EXPECT_FALSE(worklist.IsGlobalEmpty());
  EXPECT_EQ(0U, worklist.GlobalPoolSize());
  EXPECT_FALSE(worklist_view2.Pop(&retrieved));
  EXPECT_EQ(nullptr, retrieved);
  EXPECT_TRUE(worklist_view1.Pop(&retrieved));
  EXPECT_EQ(&dummy, retrieved);
  EXPECT_TRUE(worklist.IsGlobalEmpty());
  EXPECT_EQ(0U, worklist.GlobalPoolSize());
}

TEST(WorklistTest, GlobalUpdateNull) {
  TestWorklist worklist;
  TestWorklist::View worklist_view(&worklist, 0);
  SomeObject* object;
  object = reinterpret_cast<SomeObject*>(&object);
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view.Push(object));
  }
  EXPECT_TRUE(worklist_view.Push(object));
  worklist.Update([](SomeObject* object, SomeObject** out) { return false; });
  EXPECT_TRUE(worklist.IsGlobalEmpty());
  EXPECT_EQ(0U, worklist.GlobalPoolSize());
}

TEST(WorklistTest, GlobalUpdate) {
  TestWorklist worklist;
  TestWorklist::View worklist_view(&worklist, 0);
  SomeObject* objectA = nullptr;
  objectA = reinterpret_cast<SomeObject*>(&objectA);
  SomeObject* objectB = nullptr;
  objectB = reinterpret_cast<SomeObject*>(&objectB);
  SomeObject* objectC = nullptr;
  objectC = reinterpret_cast<SomeObject*>(&objectC);
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view.Push(objectA));
  }
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view.Push(objectB));
  }
  EXPECT_TRUE(worklist_view.Push(objectA));
  worklist.Update([objectA, objectC](SomeObject* object, SomeObject** out) {
    if (object != objectA) {
      *out = objectC;
      return true;
    }
    return false;
  });
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    SomeObject* object;
    EXPECT_TRUE(worklist_view.Pop(&object));
    EXPECT_EQ(object, objectC);
  }
}

TEST(WorklistTest, FlushToGlobalPushSegment) {
  TestWorklist worklist;
  TestWorklist::View worklist_view0(&worklist, 0);
  TestWorklist::View worklist_view1(&worklist, 1);
  SomeObject* object = nullptr;
  SomeObject* objectA = nullptr;
  objectA = reinterpret_cast<SomeObject*>(&objectA);
  EXPECT_TRUE(worklist_view0.Push(objectA));
  worklist.FlushToGlobal(0);
  EXPECT_EQ(1U, worklist.GlobalPoolSize());
  EXPECT_TRUE(worklist_view1.Pop(&object));
}

TEST(WorklistTest, FlushToGlobalPopSegment) {
  TestWorklist worklist;
  TestWorklist::View worklist_view0(&worklist, 0);
  TestWorklist::View worklist_view1(&worklist, 1);
  SomeObject* object = nullptr;
  SomeObject* objectA = nullptr;
  objectA = reinterpret_cast<SomeObject*>(&objectA);
  EXPECT_TRUE(worklist_view0.Push(objectA));
  EXPECT_TRUE(worklist_view0.Push(objectA));
  EXPECT_TRUE(worklist_view0.Pop(&object));
  worklist.FlushToGlobal(0);
  EXPECT_EQ(1U, worklist.GlobalPoolSize());
  EXPECT_TRUE(worklist_view1.Pop(&object));
}

TEST(WorklistTest, Clear) {
  TestWorklist worklist;
  TestWorklist::View worklist_view(&worklist, 0);
  SomeObject* object;
  object = reinterpret_cast<SomeObject*>(&object);
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view.Push(object));
  }
  EXPECT_TRUE(worklist_view.Push(object));
  EXPECT_EQ(1U, worklist.GlobalPoolSize());
  worklist.Clear();
  EXPECT_TRUE(worklist.IsGlobalEmpty());
  EXPECT_EQ(0U, worklist.GlobalPoolSize());
}

TEST(WorklistTest, SingleSegmentSteal) {
  TestWorklist worklist;
  TestWorklist::View worklist_view1(&worklist, 0);
  TestWorklist::View worklist_view2(&worklist, 1);
  SomeObject dummy;
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view1.Push(&dummy));
  }
  SomeObject* retrieved = nullptr;
  // One more push/pop to publish the full segment.
  EXPECT_TRUE(worklist_view1.Push(nullptr));
  EXPECT_TRUE(worklist_view1.Pop(&retrieved));
  EXPECT_EQ(nullptr, retrieved);
  EXPECT_EQ(1U, worklist.GlobalPoolSize());
  // Stealing.
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view2.Pop(&retrieved));
    EXPECT_EQ(&dummy, retrieved);
    EXPECT_FALSE(worklist_view1.Pop(&retrieved));
  }
  EXPECT_TRUE(worklist.IsGlobalEmpty());
  EXPECT_EQ(0U, worklist.GlobalPoolSize());
}

TEST(WorklistTest, MultipleSegmentsStolen) {
  TestWorklist worklist;
  TestWorklist::View worklist_view1(&worklist, 0);
  TestWorklist::View worklist_view2(&worklist, 1);
  TestWorklist::View worklist_view3(&worklist, 2);
  SomeObject dummy1;
  SomeObject dummy2;
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view1.Push(&dummy1));
  }
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view1.Push(&dummy2));
  }
  SomeObject* retrieved = nullptr;
  SomeObject dummy3;
  // One more push/pop to publish the full segment.
  EXPECT_TRUE(worklist_view1.Push(&dummy3));
  EXPECT_TRUE(worklist_view1.Pop(&retrieved));
  EXPECT_EQ(&dummy3, retrieved);
  EXPECT_EQ(2U, worklist.GlobalPoolSize());
  // Stealing.
  EXPECT_TRUE(worklist_view2.Pop(&retrieved));
  SomeObject* const expect_bag2 = retrieved;
  EXPECT_TRUE(worklist_view3.Pop(&retrieved));
  SomeObject* const expect_bag3 = retrieved;
  EXPECT_EQ(0U, worklist.GlobalPoolSize());
  EXPECT_NE(expect_bag2, expect_bag3);
  EXPECT_TRUE(expect_bag2 == &dummy1 || expect_bag2 == &dummy2);
  EXPECT_TRUE(expect_bag3 == &dummy1 || expect_bag3 == &dummy2);
  for (size_t i = 1; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view2.Pop(&retrieved));
    EXPECT_EQ(expect_bag2, retrieved);
    EXPECT_FALSE(worklist_view1.Pop(&retrieved));
  }
  for (size_t i = 1; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view3.Pop(&retrieved));
    EXPECT_EQ(expect_bag3, retrieved);
    EXPECT_FALSE(worklist_view1.Pop(&retrieved));
  }
  EXPECT_TRUE(worklist.IsGlobalEmpty());
}

TEST(WorklistTest, MergeGlobalPool) {
  TestWorklist worklist1;
  TestWorklist::View worklist_view1(&worklist1, 0);
  SomeObject dummy;
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view1.Push(&dummy));
  }
  SomeObject* retrieved = nullptr;
  // One more push/pop to publish the full segment.
  EXPECT_TRUE(worklist_view1.Push(nullptr));
  EXPECT_TRUE(worklist_view1.Pop(&retrieved));
  EXPECT_EQ(nullptr, retrieved);
  EXPECT_EQ(1U, worklist1.GlobalPoolSize());
  // Merging global pool into a new Worklist.
  TestWorklist worklist2;
  TestWorklist::View worklist_view2(&worklist2, 0);
  EXPECT_EQ(0U, worklist2.GlobalPoolSize());
  worklist2.MergeGlobalPool(&worklist1);
  EXPECT_EQ(1U, worklist2.GlobalPoolSize());
  EXPECT_FALSE(worklist2.IsGlobalEmpty());
  for (size_t i = 0; i < TestWorklist::kSegmentCapacity; i++) {
    EXPECT_TRUE(worklist_view2.Pop(&retrieved));
    EXPECT_EQ(&dummy, retrieved);
    EXPECT_FALSE(worklist_view1.Pop(&retrieved));
  }
  EXPECT_TRUE(worklist1.IsGlobalEmpty());
  EXPECT_TRUE(worklist2.IsGlobalEmpty());
}

}  // namespace blink
