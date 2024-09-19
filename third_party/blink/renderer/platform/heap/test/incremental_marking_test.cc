// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <initializer_list>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_counted_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_buildflags.h"
#include "third_party/blink/renderer/platform/heap/heap_test_objects.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/heap/trace_traits.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class IncrementalMarkingTest : public TestSupportingGC {};

namespace incremental_marking_test {

// =============================================================================
// HeapVector support. =========================================================
// =============================================================================

namespace {

// HeapVector allows for insertion of container objects that can be traced but
// are themselves non-garbage collected.
class NonGarbageCollectedContainer {
  DISALLOW_NEW();

 public:
  NonGarbageCollectedContainer(LinkedObject* obj, int y) : obj_(obj), y_(y) {}

  virtual ~NonGarbageCollectedContainer() {}
  virtual void Trace(Visitor* visitor) const { visitor->Trace(obj_); }

 private:
  Member<LinkedObject> obj_;
  int y_;
};

class NonGarbageCollectedContainerRoot {
  DISALLOW_NEW();

 public:
  NonGarbageCollectedContainerRoot(LinkedObject* obj1,
                                   LinkedObject* obj2,
                                   int y)
      : next_(obj1, y), obj_(obj2) {}
  virtual ~NonGarbageCollectedContainerRoot() {}

  virtual void Trace(Visitor* visitor) const {
    visitor->Trace(next_);
    visitor->Trace(obj_);
  }

 private:
  NonGarbageCollectedContainer next_;
  Member<LinkedObject> obj_;
};

}  // namespace

TEST_F(IncrementalMarkingTest, HeapVectorPushBackMember) {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  HeapVector<Member<LinkedObject>>* vec =
      MakeGarbageCollected<HeapVector<Member<LinkedObject>>>();
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  vec->push_back(obj);
  driver.FinishGC();
  EXPECT_TRUE(obj);
}

TEST_F(IncrementalMarkingTest, HeapVectorPushBackNonGCedContainer) {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  HeapVector<NonGarbageCollectedContainer>* vec =
      MakeGarbageCollected<HeapVector<NonGarbageCollectedContainer>>();
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  vec->push_back(NonGarbageCollectedContainer(obj, 1));
  driver.FinishGC();
  EXPECT_TRUE(obj);
}

TEST_F(IncrementalMarkingTest, HeapVectorPushBackStdPair) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapVector<std::pair<Member<LinkedObject>, Member<LinkedObject>>>* vec =
      MakeGarbageCollected<
          HeapVector<std::pair<Member<LinkedObject>, Member<LinkedObject>>>>();
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  vec->push_back(
      std::make_pair(Member<LinkedObject>(obj1), Member<LinkedObject>(obj2)));
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapVectorEmplaceBackMember) {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  HeapVector<Member<LinkedObject>>* vec =
      MakeGarbageCollected<HeapVector<Member<LinkedObject>>>();
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  vec->emplace_back(obj);
  driver.FinishGC();
  EXPECT_TRUE(obj);
}

TEST_F(IncrementalMarkingTest, HeapVectorEmplaceBackNonGCedContainer) {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  HeapVector<NonGarbageCollectedContainer>* vec =
      MakeGarbageCollected<HeapVector<NonGarbageCollectedContainer>>();
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  vec->emplace_back(obj, 1);
  driver.FinishGC();
  EXPECT_TRUE(obj);
}

TEST_F(IncrementalMarkingTest, HeapVectorEmplaceBackStdPair) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapVector<std::pair<Member<LinkedObject>, Member<LinkedObject>>>* vec =
      MakeGarbageCollected<
          HeapVector<std::pair<Member<LinkedObject>, Member<LinkedObject>>>>();
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  vec->emplace_back(obj1, obj2);
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapVectorCopyMember) {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  HeapVector<Member<LinkedObject>>* vec =
      MakeGarbageCollected<HeapVector<Member<LinkedObject>>>();
  vec->push_back(obj);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  *MakeGarbageCollected<HeapVector<Member<LinkedObject>>>() = *vec;
  driver.FinishGC();
  EXPECT_TRUE(obj);
}

TEST_F(IncrementalMarkingTest, HeapVectorCopyMemberInCtor) {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  HeapVector<Member<LinkedObject>>* vec =
      MakeGarbageCollected<HeapVector<Member<LinkedObject>>>();
  vec->push_back(obj);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  MakeGarbageCollected<HeapVector<Member<LinkedObject>>>(*vec);
  driver.FinishGC();
  // Copy during object construction does not emit write barriers as
  // in-construction/on-stack objects would be found during conservative GC.
  EXPECT_FALSE(obj);
}

TEST_F(IncrementalMarkingTest, HeapVectorCopyNonGCedContainer) {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  HeapVector<NonGarbageCollectedContainer>* vec =
      MakeGarbageCollected<HeapVector<NonGarbageCollectedContainer>>();
  vec->emplace_back(obj, 1);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  *MakeGarbageCollected<HeapVector<NonGarbageCollectedContainer>>() = *vec;
  driver.FinishGC();
  EXPECT_TRUE(obj);
}

TEST_F(IncrementalMarkingTest, HeapVectorCopyNonGCedContainerInCtor) {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  HeapVector<NonGarbageCollectedContainer>* vec =
      MakeGarbageCollected<HeapVector<NonGarbageCollectedContainer>>();
  vec->emplace_back(obj, 1);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  MakeGarbageCollected<HeapVector<NonGarbageCollectedContainer>>(*vec);
  driver.FinishGC();
  // Copy during object construction does not emit write barriers as
  // in-construction/on-stack objects would be found during conservative GC.
  EXPECT_FALSE(obj);
}

TEST_F(IncrementalMarkingTest, HeapVectorCopyStdPair) {
  using ValueType = std::pair<Member<LinkedObject>, Member<LinkedObject>>;
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapVector<ValueType>* vec = MakeGarbageCollected<HeapVector<ValueType>>();
  vec->emplace_back(obj1, obj2);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  *MakeGarbageCollected<HeapVector<ValueType>>() = *vec;
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapVectorCopyStdPairInCtor) {
  using ValueType = std::pair<Member<LinkedObject>, Member<LinkedObject>>;
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapVector<ValueType>* vec = MakeGarbageCollected<HeapVector<ValueType>>();
  vec->emplace_back(obj1, obj2);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  MakeGarbageCollected<HeapVector<ValueType>>(*vec);
  driver.FinishGC();
  // Copy during object construction does not emit write barriers as
  // in-construction/on-stack objects would be found during conservative GC.
  EXPECT_FALSE(obj1);
  EXPECT_FALSE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapVectorMoveMember) {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  HeapVector<Member<LinkedObject>>* vec =
      MakeGarbageCollected<HeapVector<Member<LinkedObject>>>();
  vec->push_back(obj);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  *MakeGarbageCollected<HeapVector<Member<LinkedObject>>>() = std::move(*vec);
  driver.FinishGC();
  EXPECT_TRUE(obj);
}

TEST_F(IncrementalMarkingTest, HeapVectorMoveMemberInCtor) {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  HeapVector<Member<LinkedObject>>* vec =
      MakeGarbageCollected<HeapVector<Member<LinkedObject>>>();
  vec->push_back(obj);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  MakeGarbageCollected<HeapVector<Member<LinkedObject>>>(std::move(*vec));
  driver.FinishGC();
  // Move during object construction does not emit write barriers as
  // in-construction/on-stack objects would be found during conservative GC.
  EXPECT_FALSE(obj);
}

TEST_F(IncrementalMarkingTest, HeapVectorMoveNonGCedContainer) {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  HeapVector<NonGarbageCollectedContainer>* vec =
      MakeGarbageCollected<HeapVector<NonGarbageCollectedContainer>>();
  vec->emplace_back(obj, 1);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  *MakeGarbageCollected<HeapVector<NonGarbageCollectedContainer>>() =
      std::move(*vec);
  driver.FinishGC();
  EXPECT_TRUE(obj);
}

TEST_F(IncrementalMarkingTest, HeapVectorMoveNonGCedContainerInCtor) {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  HeapVector<NonGarbageCollectedContainer>* vec =
      MakeGarbageCollected<HeapVector<NonGarbageCollectedContainer>>();
  vec->emplace_back(obj, 1);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  MakeGarbageCollected<HeapVector<NonGarbageCollectedContainer>>(
      std::move(*vec));
  driver.FinishGC();
  // Move during object construction does not emit write barriers as
  // in-construction/on-stack objects would be found during conservative GC.
  EXPECT_FALSE(obj);
}

TEST_F(IncrementalMarkingTest, HeapVectorMoveStdPair) {
  using ValueType = std::pair<Member<LinkedObject>, Member<LinkedObject>>;
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapVector<ValueType>* vec = MakeGarbageCollected<HeapVector<ValueType>>();
  vec->emplace_back(obj1, obj2);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  *MakeGarbageCollected<HeapVector<ValueType>>() = std::move(*vec);
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapVectorMoveStdPairInCtor) {
  using ValueType = std::pair<Member<LinkedObject>, Member<LinkedObject>>;
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapVector<ValueType>* vec = MakeGarbageCollected<HeapVector<ValueType>>();
  vec->emplace_back(obj1, obj2);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  MakeGarbageCollected<HeapVector<ValueType>>(std::move(*vec));
  driver.FinishGC();
  // Move during object construction does not emit write barriers as
  // in-construction/on-stack objects would be found during conservative GC.
  EXPECT_FALSE(obj1);
  EXPECT_FALSE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapVectorSwapMember) {
  using VectorType = HeapVector<Member<LinkedObject>>;
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  VectorType* vec1 = MakeGarbageCollected<VectorType>();
  vec1->push_back(obj1);
  VectorType* vec2 = MakeGarbageCollected<VectorType>();
  vec2->push_back(obj2);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  std::swap(*vec1, *vec2);
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapVectorSwapNonGCedContainer) {
  using VectorType = HeapVector<NonGarbageCollectedContainer>;
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  VectorType* vec1 = MakeGarbageCollected<VectorType>();
  vec1->emplace_back(obj1, 1);
  VectorType* vec2 = MakeGarbageCollected<VectorType>();
  vec2->emplace_back(obj2, 2);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  std::swap(*vec1, *vec2);
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapVectorSwapStdPair) {
  using ValueType = std::pair<Member<LinkedObject>, Member<LinkedObject>>;
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapVector<ValueType>* vec1 = MakeGarbageCollected<HeapVector<ValueType>>();
  vec1->emplace_back(obj1, nullptr);
  HeapVector<ValueType>* vec2 = MakeGarbageCollected<HeapVector<ValueType>>();
  vec2->emplace_back(nullptr, obj2);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  std::swap(*vec1, *vec2);
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapVectorSubscriptOperator) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapVector<Member<LinkedObject>>* vec =
      MakeGarbageCollected<HeapVector<Member<LinkedObject>>>();
  vec->push_back(obj1);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  EXPECT_EQ(1u, vec->size());
  EXPECT_EQ(obj1, (*vec)[0]);
  (*vec)[0] = obj2.Get();
  EXPECT_EQ(obj2, (*vec)[0]);
  driver.FinishGC();
  EXPECT_FALSE(obj1);
  EXPECT_TRUE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapVectorEagerTracingStopsAtMember) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj3 = MakeGarbageCollected<LinkedObject>();
  obj1->set_next(obj3);
  HeapVector<NonGarbageCollectedContainerRoot>* vec =
      MakeGarbageCollected<HeapVector<NonGarbageCollectedContainerRoot>>();
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  vec->emplace_back(obj1, obj2, 3);
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
  EXPECT_TRUE(obj3);
}

// =============================================================================
// HeapDeque support. ==========================================================
// =============================================================================

TEST_F(IncrementalMarkingTest, HeapDequePushBackMember) {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  HeapDeque<Member<LinkedObject>>* deq =
      MakeGarbageCollected<HeapDeque<Member<LinkedObject>>>();
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  deq->push_back(obj);
  driver.FinishGC();
  EXPECT_TRUE(obj);
}

TEST_F(IncrementalMarkingTest, HeapDequePushFrontMember) {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  HeapDeque<Member<LinkedObject>>* deq =
      MakeGarbageCollected<HeapDeque<Member<LinkedObject>>>();
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  deq->push_front(obj);
  driver.FinishGC();
  EXPECT_TRUE(obj);
}

TEST_F(IncrementalMarkingTest, HeapDequeEmplaceBackMember) {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  HeapDeque<Member<LinkedObject>>* deq =
      MakeGarbageCollected<HeapDeque<Member<LinkedObject>>>();
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  deq->emplace_back(obj);
  driver.FinishGC();
  EXPECT_TRUE(obj);
}

TEST_F(IncrementalMarkingTest, HeapDequeEmplaceFrontMember) {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  HeapDeque<Member<LinkedObject>>* deq =
      MakeGarbageCollected<HeapDeque<Member<LinkedObject>>>();
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  deq->emplace_front(obj);
  driver.FinishGC();
  EXPECT_TRUE(obj);
}

TEST_F(IncrementalMarkingTest, HeapDequeCopyMember) {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  HeapDeque<Member<LinkedObject>>* deq =
      MakeGarbageCollected<HeapDeque<Member<LinkedObject>>>();
  deq->push_back(obj);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  *MakeGarbageCollected<HeapDeque<Member<LinkedObject>>>() = *deq;
  driver.FinishGC();
  EXPECT_TRUE(obj);
}

TEST_F(IncrementalMarkingTest, HeapDequeCopyMemberInCtor) {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  HeapDeque<Member<LinkedObject>>* deq =
      MakeGarbageCollected<HeapDeque<Member<LinkedObject>>>();
  deq->push_back(obj);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  MakeGarbageCollected<HeapDeque<Member<LinkedObject>>>(*deq);
  driver.FinishGC();
  // Copy during object construction does not emit write barriers as
  // in-construction/on-stack objects would be found during conservative GC.
  EXPECT_FALSE(obj);
}

TEST_F(IncrementalMarkingTest, HeapDequeMoveMember) {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  HeapDeque<Member<LinkedObject>>* deq =
      MakeGarbageCollected<HeapDeque<Member<LinkedObject>>>();
  deq->push_back(obj);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  *MakeGarbageCollected<HeapDeque<Member<LinkedObject>>>() = std::move(*deq);
  driver.FinishGC();
  EXPECT_TRUE(obj);
}

TEST_F(IncrementalMarkingTest, HeapDequeMoveMemberInCtor) {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  HeapDeque<Member<LinkedObject>>* deq =
      MakeGarbageCollected<HeapDeque<Member<LinkedObject>>>();
  deq->push_back(obj);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  MakeGarbageCollected<HeapDeque<Member<LinkedObject>>>(std::move(*deq));
  driver.FinishGC();
  // Move construction does not emit a write barrier.
  EXPECT_FALSE(obj);
}

TEST_F(IncrementalMarkingTest, HeapDequeSwapMember) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapDeque<Member<LinkedObject>>* deq1 =
      MakeGarbageCollected<HeapDeque<Member<LinkedObject>>>();
  deq1->push_back(obj1);
  HeapDeque<Member<LinkedObject>>* deq2 =
      MakeGarbageCollected<HeapDeque<Member<LinkedObject>>>();
  deq2->push_back(obj2);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  std::swap(*deq1, *deq2);
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

// =============================================================================
// HeapHashSet support. ========================================================
// =============================================================================

namespace {

template <typename Container>
void Insert() {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  Container* container = MakeGarbageCollected<Container>();
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  container->insert(obj.Get());
  driver.FinishGC();
  EXPECT_TRUE(obj);
}

template <typename Container>
void Copy() {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  Container* container1 = MakeGarbageCollected<Container>();
  container1->insert(obj.Get());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  Container* container2 = MakeGarbageCollected<Container>(*container1);
  EXPECT_TRUE(container1->Contains(obj));
  EXPECT_TRUE(container2->Contains(obj));
  driver.FinishGC();
  EXPECT_TRUE(obj);
}

template <typename Container>
void Move() {
  WeakPersistent<LinkedObject> obj = MakeGarbageCollected<LinkedObject>();
  Container* container1 = MakeGarbageCollected<Container>();
  Container* container2 = MakeGarbageCollected<Container>();
  container1->insert(obj.Get());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  *container2 = std::move(*container1);
  driver.FinishGC();
  EXPECT_TRUE(obj);
}

template <typename Container>
void Swap() {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  Container* container1 = MakeGarbageCollected<Container>();
  container1->insert(obj1.Get());
  Container* container2 = MakeGarbageCollected<Container>();
  container2->insert(obj2.Get());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  std::swap(*container1, *container2);
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

}  // namespace

TEST_F(IncrementalMarkingTest, HeapHashSetInsert) {
  Insert<HeapHashSet<Member<LinkedObject>>>();
  // Weak references are strongified for the current cycle.
  Insert<HeapHashSet<WeakMember<LinkedObject>>>();
}

TEST_F(IncrementalMarkingTest, HeapHashSetCopy) {
  Copy<HeapHashSet<Member<LinkedObject>>>();
  // Weak references are strongified for the current cycle.
  Copy<HeapHashSet<WeakMember<LinkedObject>>>();
}

TEST_F(IncrementalMarkingTest, HeapHashSetMove) {
  Move<HeapHashSet<Member<LinkedObject>>>();
  // Weak references are strongified for the current cycle.
  Move<HeapHashSet<WeakMember<LinkedObject>>>();
}

TEST_F(IncrementalMarkingTest, HeapHashSetSwap) {
  Swap<HeapHashSet<Member<LinkedObject>>>();
  // Weak references are strongified for the current cycle.
  Swap<HeapHashSet<WeakMember<LinkedObject>>>();
}

// =============================================================================
// HeapLinkedHashSet support. ==================================================
// =============================================================================

TEST_F(IncrementalMarkingTest, HeapLinkedHashSetInsert) {
  Insert<HeapLinkedHashSet<Member<LinkedObject>>>();
  // Weak references are strongified for the current cycle.
  Insert<HeapLinkedHashSet<WeakMember<LinkedObject>>>();
}

TEST_F(IncrementalMarkingTest, HeapLinkedHashSetCopy) {
  Copy<HeapLinkedHashSet<Member<LinkedObject>>>();
  // Weak references are strongified for the current cycle.
  Copy<HeapLinkedHashSet<WeakMember<LinkedObject>>>();
}

TEST_F(IncrementalMarkingTest, HeapLinkedHashSetMove) {
  Move<HeapLinkedHashSet<Member<LinkedObject>>>();
  // Weak references are strongified for the current cycle.
  Move<HeapLinkedHashSet<WeakMember<LinkedObject>>>();
}

TEST_F(IncrementalMarkingTest, HeapLinkedHashSetSwap) {
  Swap<HeapLinkedHashSet<Member<LinkedObject>>>();
  // Weak references are strongified for the current cycle.
  Swap<HeapLinkedHashSet<WeakMember<LinkedObject>>>();
}

// =============================================================================
// HeapHashCountedSet support. =================================================
// =============================================================================

// HeapHashCountedSet does not support copy or move.

TEST_F(IncrementalMarkingTest, HeapHashCountedSetInsert) {
  Insert<HeapHashCountedSet<Member<LinkedObject>>>();
  // Weak references are strongified for the current cycle.
  Insert<HeapHashCountedSet<WeakMember<LinkedObject>>>();
}

TEST_F(IncrementalMarkingTest, HeapHashCountedSetSwap) {
  // HeapHashCountedSet is not move constructible so we cannot use std::swap.
  {
    WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
    WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
    HeapHashCountedSet<Member<LinkedObject>>* container1 =
        MakeGarbageCollected<HeapHashCountedSet<Member<LinkedObject>>>();
    container1->insert(obj1.Get());
    HeapHashCountedSet<Member<LinkedObject>>* container2 =
        MakeGarbageCollected<HeapHashCountedSet<Member<LinkedObject>>>();
    container2->insert(obj2.Get());
    IncrementalMarkingTestDriver driver(ThreadState::Current());
    driver.StartGC();
    container1->swap(*container2);
    driver.FinishGC();
    EXPECT_TRUE(obj1);
    EXPECT_TRUE(obj2);
  }
  {
    WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
    WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
    HeapHashCountedSet<WeakMember<LinkedObject>>* container1 =
        MakeGarbageCollected<HeapHashCountedSet<WeakMember<LinkedObject>>>();
    container1->insert(obj1.Get());
    HeapHashCountedSet<WeakMember<LinkedObject>>* container2 =
        MakeGarbageCollected<HeapHashCountedSet<WeakMember<LinkedObject>>>();
    container2->insert(obj2.Get());
    IncrementalMarkingTestDriver driver(ThreadState::Current());
    driver.StartGC();
    container1->swap(*container2);
    driver.FinishGC();
    EXPECT_TRUE(obj1);
    EXPECT_TRUE(obj2);
  }
}

// =============================================================================
// HeapHashMap support. ========================================================
// =============================================================================

TEST_F(IncrementalMarkingTest, HeapHashMapInsertMember) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>* map =
      MakeGarbageCollected<
          HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>>();
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  map->insert(obj1.Get(), obj2.Get());
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapHashMapInsertWeakMember) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<WeakMember<LinkedObject>, WeakMember<LinkedObject>>* map =
      MakeGarbageCollected<
          HeapHashMap<WeakMember<LinkedObject>, WeakMember<LinkedObject>>>();
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  map->insert(obj1.Get(), obj2.Get());
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapHashMapInsertMemberWeakMember) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<Member<LinkedObject>, WeakMember<LinkedObject>>* map =
      MakeGarbageCollected<
          HeapHashMap<Member<LinkedObject>, WeakMember<LinkedObject>>>();
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  map->insert(obj1.Get(), obj2.Get());
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapHashMapInsertWeakMemberMember) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<WeakMember<LinkedObject>, Member<LinkedObject>>* map =
      MakeGarbageCollected<
          HeapHashMap<WeakMember<LinkedObject>, Member<LinkedObject>>>();
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  map->insert(obj1.Get(), obj2.Get());
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapHashMapSetMember) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>* map =
      MakeGarbageCollected<
          HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>>();
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  map->Set(obj1.Get(), obj2.Get());
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapHashMapSetMemberUpdateValue) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj3 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>* map =
      MakeGarbageCollected<
          HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>>();
  map->insert(obj1.Get(), obj2.Get());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  // Only |obj3| is newly added to |map|, so we only expect the barrier to
  // fire on this one.
  map->Set(obj1.Get(), obj3.Get());
  driver.FinishGC();
  EXPECT_FALSE(obj1);
  EXPECT_FALSE(obj2);
  EXPECT_TRUE(obj3);
}

TEST_F(IncrementalMarkingTest, HeapHashMapIteratorChangeKey) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj3 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>* map =
      MakeGarbageCollected<
          HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>>();
  map->insert(obj1.Get(), obj2.Get());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  auto it = map->find(obj1.Get());
  EXPECT_NE(map->end(), it);
  it->key = obj3.Get();
  driver.FinishGC();
  EXPECT_FALSE(obj1);
  EXPECT_FALSE(obj2);
  EXPECT_TRUE(obj3);
}

TEST_F(IncrementalMarkingTest, HeapHashMapIteratorChangeValue) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj3 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>* map =
      MakeGarbageCollected<
          HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>>();
  map->insert(obj1.Get(), obj2.Get());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  auto it = map->find(obj1.Get());
  EXPECT_NE(map->end(), it);
  it->value = obj3.Get();
  driver.FinishGC();
  EXPECT_FALSE(obj1);
  EXPECT_FALSE(obj2);
  EXPECT_TRUE(obj3);
}

TEST_F(IncrementalMarkingTest, HeapHashMapCopyMemberMember) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>* map1 =
      MakeGarbageCollected<
          HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>>();
  map1->insert(obj1.Get(), obj2.Get());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  EXPECT_TRUE(map1->Contains(obj1));
  HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>* map2 =
      MakeGarbageCollected<
          HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>>(*map1);
  EXPECT_TRUE(map1->Contains(obj1));
  EXPECT_TRUE(map2->Contains(obj1));
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapHashMapCopyWeakMemberWeakMember) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<WeakMember<LinkedObject>, WeakMember<LinkedObject>>* map1 =
      MakeGarbageCollected<
          HeapHashMap<WeakMember<LinkedObject>, WeakMember<LinkedObject>>>();
  map1->insert(obj1.Get(), obj2.Get());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  EXPECT_TRUE(map1->Contains(obj1));
  // Weak references are strongified for the current cycle.
  HeapHashMap<WeakMember<LinkedObject>, WeakMember<LinkedObject>>* map2 =
      MakeGarbageCollected<
          HeapHashMap<WeakMember<LinkedObject>, WeakMember<LinkedObject>>>(
          *map1);
  EXPECT_TRUE(map1->Contains(obj1));
  EXPECT_TRUE(map2->Contains(obj1));
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapHashMapCopyMemberWeakMember) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<Member<LinkedObject>, WeakMember<LinkedObject>>* map1 =
      MakeGarbageCollected<
          HeapHashMap<Member<LinkedObject>, WeakMember<LinkedObject>>>();
  map1->insert(obj1.Get(), obj2.Get());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  EXPECT_TRUE(map1->Contains(obj1));
  // Weak references are strongified for the current cycle.
  HeapHashMap<Member<LinkedObject>, WeakMember<LinkedObject>>* map2 =
      MakeGarbageCollected<
          HeapHashMap<Member<LinkedObject>, WeakMember<LinkedObject>>>(*map1);
  EXPECT_TRUE(map1->Contains(obj1));
  EXPECT_TRUE(map2->Contains(obj1));
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapHashMapCopyWeakMemberMember) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<WeakMember<LinkedObject>, Member<LinkedObject>>* map1 =
      MakeGarbageCollected<
          HeapHashMap<WeakMember<LinkedObject>, Member<LinkedObject>>>();
  map1->insert(obj1.Get(), obj2.Get());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  EXPECT_TRUE(map1->Contains(obj1));
  // Weak references are strongified for the current cycle.
  HeapHashMap<WeakMember<LinkedObject>, Member<LinkedObject>>* map2 =
      MakeGarbageCollected<
          HeapHashMap<WeakMember<LinkedObject>, Member<LinkedObject>>>(*map1);
  EXPECT_TRUE(map1->Contains(obj1));
  EXPECT_TRUE(map2->Contains(obj1));
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapHashMapMoveMember) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>* map =
      MakeGarbageCollected<
          HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>>();
  map->insert(obj1.Get(), obj2.Get());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  MakeGarbageCollected<HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>>(
      std::move(*map));
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapHashMapMoveWeakMember) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<WeakMember<LinkedObject>, WeakMember<LinkedObject>>* map =
      MakeGarbageCollected<
          HeapHashMap<WeakMember<LinkedObject>, WeakMember<LinkedObject>>>();
  map->insert(obj1.Get(), obj2.Get());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  MakeGarbageCollected<
      HeapHashMap<WeakMember<LinkedObject>, WeakMember<LinkedObject>>>(
      std::move(*map));
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapHashMapMoveMemberWeakMember) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<Member<LinkedObject>, WeakMember<LinkedObject>>* map =
      MakeGarbageCollected<
          HeapHashMap<Member<LinkedObject>, WeakMember<LinkedObject>>>();
  map->insert(obj1.Get(), obj2.Get());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  MakeGarbageCollected<
      HeapHashMap<Member<LinkedObject>, WeakMember<LinkedObject>>>(
      std::move(*map));
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapHashMapMoveWeakMemberMember) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<WeakMember<LinkedObject>, Member<LinkedObject>>* map =
      MakeGarbageCollected<
          HeapHashMap<WeakMember<LinkedObject>, Member<LinkedObject>>>();
  map->insert(obj1.Get(), obj2.Get());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  MakeGarbageCollected<
      HeapHashMap<WeakMember<LinkedObject>, Member<LinkedObject>>>(
      std::move(*map));
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapHashMapSwapMemberMember) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj3 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj4 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>* map1 =
      MakeGarbageCollected<
          HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>>();
  map1->insert(obj1.Get(), obj2.Get());
  HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>* map2 =
      MakeGarbageCollected<
          HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>>();
  map2->insert(obj3.Get(), obj4.Get());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  std::swap(*map1, *map2);
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
  EXPECT_TRUE(obj3);
  EXPECT_TRUE(obj4);
}

TEST_F(IncrementalMarkingTest, HeapHashMapSwapWeakMemberWeakMember) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj3 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj4 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<WeakMember<LinkedObject>, WeakMember<LinkedObject>>* map1 =
      MakeGarbageCollected<
          HeapHashMap<WeakMember<LinkedObject>, WeakMember<LinkedObject>>>();
  map1->insert(obj1.Get(), obj2.Get());
  HeapHashMap<WeakMember<LinkedObject>, WeakMember<LinkedObject>>* map2 =
      MakeGarbageCollected<
          HeapHashMap<WeakMember<LinkedObject>, WeakMember<LinkedObject>>>();
  map2->insert(obj3.Get(), obj4.Get());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  std::swap(*map1, *map2);
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
  EXPECT_TRUE(obj3);
  EXPECT_TRUE(obj4);
}

TEST_F(IncrementalMarkingTest, HeapHashMapSwapMemberWeakMember) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj3 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj4 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<Member<LinkedObject>, WeakMember<LinkedObject>>* map1 =
      MakeGarbageCollected<
          HeapHashMap<Member<LinkedObject>, WeakMember<LinkedObject>>>();
  map1->insert(obj1.Get(), obj2.Get());
  HeapHashMap<Member<LinkedObject>, WeakMember<LinkedObject>>* map2 =
      MakeGarbageCollected<
          HeapHashMap<Member<LinkedObject>, WeakMember<LinkedObject>>>();
  map2->insert(obj3.Get(), obj4.Get());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  std::swap(*map1, *map2);
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
  EXPECT_TRUE(obj3);
  EXPECT_TRUE(obj4);
}

TEST_F(IncrementalMarkingTest, HeapHashMapSwapWeakMemberMember) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj3 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj4 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<WeakMember<LinkedObject>, Member<LinkedObject>>* map1 =
      MakeGarbageCollected<
          HeapHashMap<WeakMember<LinkedObject>, Member<LinkedObject>>>();
  map1->insert(obj1.Get(), obj2.Get());
  HeapHashMap<WeakMember<LinkedObject>, Member<LinkedObject>>* map2 =
      MakeGarbageCollected<
          HeapHashMap<WeakMember<LinkedObject>, Member<LinkedObject>>>();
  map2->insert(obj3.Get(), obj4.Get());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  std::swap(*map1, *map2);
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_TRUE(obj2);
  EXPECT_TRUE(obj3);
  EXPECT_TRUE(obj4);
}

TEST_F(IncrementalMarkingTest, HeapHashMapCopyKeysToVectorMember) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>* map =
      MakeGarbageCollected<
          HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>>();
  map->insert(obj1.Get(), obj2.Get());
  HeapVector<Member<LinkedObject>>* vec =
      MakeGarbageCollected<HeapVector<Member<LinkedObject>>>();
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  // Only key should have its write barrier fired. A write barrier call for
  // value hints to an inefficient implementation.
  CopyKeysToVector(*map, *vec);
  driver.FinishGC();
  EXPECT_TRUE(obj1);
  EXPECT_FALSE(obj2);
}

TEST_F(IncrementalMarkingTest, HeapHashMapCopyValuesToVectorMember) {
  WeakPersistent<LinkedObject> obj1 = MakeGarbageCollected<LinkedObject>();
  WeakPersistent<LinkedObject> obj2 = MakeGarbageCollected<LinkedObject>();
  HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>* map =
      MakeGarbageCollected<
          HeapHashMap<Member<LinkedObject>, Member<LinkedObject>>>();
  map->insert(obj1.Get(), obj2.Get());
  HeapVector<Member<LinkedObject>>* vec =
      MakeGarbageCollected<HeapVector<Member<LinkedObject>>>();
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  // Only value should have its write barrier fired. A write barrier call for
  // key hints to an inefficient implementation.
  CopyValuesToVector(*map, *vec);
  driver.FinishGC();
  EXPECT_FALSE(obj1);
  EXPECT_TRUE(obj2);
}

// =============================================================================
// Tests that execute complete incremental garbage collections. ================
// =============================================================================

TEST_F(IncrementalMarkingTest, TestDriver) {
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  EXPECT_TRUE(ThreadState::Current()->IsIncrementalMarking());
  driver.TriggerMarkingSteps();
  EXPECT_TRUE(ThreadState::Current()->IsIncrementalMarking());
  driver.FinishGC();
  EXPECT_FALSE(ThreadState::Current()->IsIncrementalMarking());
}

TEST_F(IncrementalMarkingTest, DropBackingStore) {
  // Regression test: https://crbug.com/828537
  using WeakStore = HeapHashCountedSet<WeakMember<LinkedObject>>;

  Persistent<WeakStore> persistent(MakeGarbageCollected<WeakStore>());
  persistent->insert(MakeGarbageCollected<LinkedObject>());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  driver.TriggerMarkingSteps();
  persistent->clear();
  // Marking verifier should not crash on a black backing store with all
  // black->white edges.
  driver.FinishGC();
}

TEST_F(IncrementalMarkingTest, NoBackingFreeDuringIncrementalMarking) {
  // Regression test: https://crbug.com/870306
  // Only reproduces in ASAN configurations.
  using WeakStore = HeapHashCountedSet<WeakMember<LinkedObject>>;

  Persistent<WeakStore> persistent(MakeGarbageCollected<WeakStore>());
  // Prefill the collection to grow backing store. A new backing store
  // allocationwould trigger the write barrier, mitigating the bug where
  // a backing store is promptly freed.
  for (size_t i = 0; i < 8; i++) {
    persistent->insert(MakeGarbageCollected<LinkedObject>());
  }
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  persistent->insert(MakeGarbageCollected<LinkedObject>());
  // Is not allowed to free the backing store as the previous insert may have
  // registered a slot.
  persistent->clear();
  driver.TriggerMarkingSteps();
  driver.FinishGC();
}

TEST_F(IncrementalMarkingTest, DropReferenceWithHeapCompaction) {
  using Store = HeapHashCountedSet<Member<LinkedObject>>;

  Persistent<Store> persistent(MakeGarbageCollected<Store>());
  persistent->insert(MakeGarbageCollected<LinkedObject>());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  CompactionTestDriver(ThreadState::Current()).ForceCompactionForNextGC();
  driver.StartGC();
  driver.TriggerMarkingSteps();
  persistent->clear();
  driver.FinishGC();
}

namespace {

class ObjectWithWeakMember : public GarbageCollected<ObjectWithWeakMember> {
 public:
  ObjectWithWeakMember() = default;

  void set_object(LinkedObject* object) { object_ = object; }

  void Trace(Visitor* visitor) const { visitor->Trace(object_); }

 private:
  WeakMember<LinkedObject> object_ = nullptr;
};

}  // namespace

TEST_F(IncrementalMarkingTest, WeakMember) {
  // Regression test: https://crbug.com/913431

  Persistent<ObjectWithWeakMember> persistent(
      MakeGarbageCollected<ObjectWithWeakMember>());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  driver.TriggerMarkingSteps();
  persistent->set_object(MakeGarbageCollected<LinkedObject>());
  driver.FinishGC();
  ConservativelyCollectGarbage();
}

TEST_F(IncrementalMarkingTest, MemberSwap) {
  // Regression test: https://crbug.com/913431
  //
  // MemberBase::Swap may be used to swap in a not-yet-processed member into an
  // already-processed member. This leads to a stale pointer that is not marked.

  Persistent<LinkedObject> object1(MakeGarbageCollected<LinkedObject>());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  // The repro leverages the fact that initializing stores do not emit a barrier
  // (because they are still reachable from stack) to simulate the problematic
  // interleaving.
  driver.TriggerMarkingSteps();
  LinkedObject* object2 =
      MakeGarbageCollected<LinkedObject>(MakeGarbageCollected<LinkedObject>());
  object2->next_ref().Swap(object1->next_ref());
  driver.FinishGC();
  ConservativelyCollectGarbage();
}

namespace {

template <typename T>
class ObjectHolder : public GarbageCollected<ObjectHolder<T>> {
 public:
  ObjectHolder() = default;

  virtual void Trace(Visitor* visitor) const { visitor->Trace(holder_); }

  void set_value(T* value) { holder_ = value; }
  T* value() const { return holder_.Get(); }

 private:
  Member<T> holder_;
};

}  // namespace

TEST_F(IncrementalMarkingTest, StepDuringObjectConstruction) {
  // Test ensures that objects in construction are delayed for processing to
  // allow omitting write barriers on initializing stores.

  using O = ObjectWithCallbackBeforeInitializer<LinkedObject>;
  using Holder = ObjectHolder<O>;
  Persistent<Holder> holder(MakeGarbageCollected<Holder>());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  MakeGarbageCollected<O>(
      WTF::BindOnce(
          [](IncrementalMarkingTestDriver* driver, Holder* holder, O* thiz) {
            // Publish not-fully-constructed object |thiz| by triggering write
            // barrier for the object.
            holder->set_value(thiz);
            // Finish call incremental steps.
            driver->TriggerMarkingStepsWithStack();
          },
          WTF::Unretained(&driver), WrapWeakPersistent(holder.Get())),
      MakeGarbageCollected<LinkedObject>());
  driver.FinishGC();
  PreciselyCollectGarbage();
}

TEST_F(IncrementalMarkingTest, StepDuringMixinObjectConstruction) {
  // Test ensures that mixin objects in construction are delayed for processing
  // to allow omitting write barriers on initializing stores.

  using Parent = ObjectWithMixinWithCallbackBeforeInitializer<LinkedObject>;
  using Mixin = MixinWithCallbackBeforeInitializer<LinkedObject>;
  using Holder = ObjectHolder<Mixin>;
  Persistent<Holder> holder(MakeGarbageCollected<Holder>());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  MakeGarbageCollected<Parent>(
      WTF::BindOnce(
          [](IncrementalMarkingTestDriver* driver, Holder* holder,
             Mixin* thiz) {
            // Publish not-fully-constructed object
            // |thiz| by triggering write barrier for
            // the object.
            holder->set_value(thiz);
            // Finish call incremental steps.
            driver->TriggerMarkingStepsWithStack();
          },
          WTF::Unretained(&driver), WrapWeakPersistent(holder.Get())),
      MakeGarbageCollected<LinkedObject>());
  driver.FinishGC();
  PreciselyCollectGarbage();
}

TEST_F(IncrementalMarkingTest, IncrementalMarkingShrinkingBackingCompaction) {
  // Regression test: https://crbug.com/918064

  using Nested = HeapVector<HeapVector<Member<LinkedObject>>>;
  // The following setup will ensure that the outer HeapVector's backing store
  // contains slots to other to-be-compacted backings.
  Persistent<Nested> holder(MakeGarbageCollected<Nested>());
  for (int i = 0; i < 32; i++) {
    holder->emplace_back();
    holder->at(i).emplace_back(MakeGarbageCollected<LinkedObject>());
  }
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  CompactionTestDriver(ThreadState::Current()).ForceCompactionForNextGC();
  driver.StartGC();
  driver.TriggerMarkingSteps();
  // Reduce size of the outer backing store.
  for (int i = 0; i < 16; i++) {
    holder->pop_back();
  }
  // Ensure that shrinking the backing does not crash in compaction as there may
  // be registered slots left in the area that is already freed.
  holder->shrink_to_fit();
  driver.FinishGC();
}

TEST_F(IncrementalMarkingTest,
       InPayloadWriteBarrierRegistersInvalidSlotForCompaction) {
  // Regression test: https://crbug.com/918064

  using Nested = HeapVector<HeapVector<Member<LinkedObject>>>;
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  CompactionTestDriver(ThreadState::Current()).ForceCompactionForNextGC();
  // Allocate a vector and reserve a buffer to avoid triggering the write
  // barrier during incremental marking.
  WeakPersistent<Nested> nested = MakeGarbageCollected<Nested>();
  nested->reserve(32);
  driver.StartGC();
  // Initialize the inner vector, triggering tracing and slots registration.
  // This could be an object using DISALLOW_NEW() but HeapVector is easier to
  // test.
  nested->emplace_back(1);
  // Use the inner vector as otherwise the slot would not be registered due to
  // not having a backing store itself.
  nested->at(0).emplace_back(MakeGarbageCollected<LinkedObject>());
  driver.TriggerMarkingSteps();
  // GCs here are without stack. This is just to show that we don't want this
  // object marked.
  driver.FinishGC();
  EXPECT_FALSE(nested);
}

TEST_F(IncrementalMarkingTest, AdjustMarkedBytesOnMarkedBackingStore) {
  // Regression test: https://crbug.com/966456
  //
  // Test ensures that backing expansion does not crash in trying to adjust
  // marked bytes when the page is actually about to be swept and marking is not
  // in progress.

  // Disable concurrent sweeping to check that sweeping is not in progress after
  // the FinishGC call.
  using Container = HeapVector<Member<LinkedObject>>;
  Persistent<Container> holder(MakeGarbageCollected<Container>());
  WeakPersistent<Container> canary(holder.Get());
  holder->push_back(MakeGarbageCollected<LinkedObject>());
  holder->Grow(16);
  PreciselyCollectGarbage();
  // Slowly shrink down the backing, only adjusting capacity without performing
  // free as the resulting memory block is too small for a free list entry.
  for (int i = 15; i > 0; i--) {
    holder->Shrink(i);
    holder->shrink_to_fit();
  }
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  driver.TriggerMarkingSteps();
  driver.FinishGC();
  CHECK(canary);
  // Re-grow to some size within the initial payload size (capacity=16).
  holder->Grow(8);
}

TEST_F(IncrementalMarkingTest, HeapCompactWithStaleSlotInNestedContainer) {
  // Regression test: https://crbug.com/980962
  //
  // Test ensures that interior pointers are updated even if the backing store
  // itself is not referenced anymore. Consider the case where a |B| is
  // references a value |V| through slot |B.x|. Even if |B| is not referred to
  // from an actual object any more, the slot |B.x| needs to be in valid state
  // when |V| is moved.

  using Nested = HeapVector<HeapVector<Member<LinkedObject>>>;

  // Allocate dummy storage so that other vector backings are actually moved.
  HeapVector<Member<LinkedObject>> unused{MakeGarbageCollected<LinkedObject>()};

  IncrementalMarkingTestDriver driver(ThreadState::Current());
  CompactionTestDriver(ThreadState::Current()).ForceCompactionForNextGC();
  driver.StartGC();
  Nested* outer = MakeGarbageCollected<Nested>();
  outer->push_back(HeapVector<Member<LinkedObject>>());
  outer->at(0).push_back(MakeGarbageCollected<LinkedObject>());
  // The outer HeapVector object is not marked, which leaves the backing store
  // as marked with a valid slot inside. Now, if the outer backing store moves
  // first and its page is freed, then referring to the slot when the inner
  // backing store is moved may crash.
  outer = nullptr;
  driver.TriggerMarkingSteps();
  driver.FinishGC();
}

class Destructed final : public GarbageCollected<Destructed> {
 public:
  ~Destructed() { n_destructed++; }

  void Trace(Visitor*) const {}

  static size_t n_destructed;
};

size_t Destructed::n_destructed = 0;

class LinkedHashSetWrapper final
    : public GarbageCollected<LinkedHashSetWrapper> {
 public:
  using HashType = HeapLinkedHashSet<Member<Destructed>>;

  LinkedHashSetWrapper() {
    for (size_t i = 0; i < 10; ++i) {
      hash_set_.insert(MakeGarbageCollected<Destructed>());
    }
  }

  void Trace(Visitor* v) const { v->Trace(hash_set_); }

  void Swap() {
    HashType hash_set;
    hash_set_.Swap(hash_set);
  }

  HashType hash_set_;
};

TEST_F(IncrementalMarkingTest, LinkedHashSetMovingCallback) {
  ClearOutOldGarbage();

  Destructed::n_destructed = 0;
  {
    HeapHashSet<Member<Destructed>> to_be_destroyed;
    to_be_destroyed.ReserveCapacityForSize(100);
  }
  Persistent<LinkedHashSetWrapper> wrapper =
      MakeGarbageCollected<LinkedHashSetWrapper>();

  IncrementalMarkingTestDriver driver(ThreadState::Current());
  CompactionTestDriver(ThreadState::Current()).ForceCompactionForNextGC();
  driver.StartGC();
  driver.TriggerMarkingSteps();

  // Destroy the link between original HeapLinkedHashSet object and its backing
  // store.
  wrapper->Swap();
  DCHECK(wrapper->hash_set_.empty());

  PreciselyCollectGarbage();

  EXPECT_EQ(10u, Destructed::n_destructed);
}

class DestructedAndTraced final : public GarbageCollected<DestructedAndTraced> {
 public:
  ~DestructedAndTraced() { n_destructed++; }

  void Trace(Visitor*) const { n_traced++; }

  static size_t n_destructed;
  static size_t n_traced;
};

size_t DestructedAndTraced::n_destructed = 0;
size_t DestructedAndTraced::n_traced = 0;

// Flaky <https://crbug.com/1351511>.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_ConservativeGCOfWeakContainer \
  DISABLED_ConservativeGCOfWeakContainer
#else
#define MAYBE_ConservativeGCOfWeakContainer ConservativeGCOfWeakContainer
#endif
TEST_F(IncrementalMarkingTest, MAYBE_ConservativeGCOfWeakContainer) {
  // Regression test: https://crbug.com/1108676
  //
  // Test ensures that on-stack references to weak containers (e.g. iterators)
  // force re-tracing of the entire container. Otherwise, if the container was
  // previously traced and is not re-traced, some bucket might be deleted which
  // will make existing iterators invalid.

  using WeakContainer = HeapHashMap<WeakMember<DestructedAndTraced>, size_t>;
  Persistent<WeakContainer> map = MakeGarbageCollected<WeakContainer>();
  static constexpr size_t kNumObjects = 10u;
  for (size_t i = 0; i < kNumObjects; ++i) {
    map->insert(MakeGarbageCollected<DestructedAndTraced>(), i);
  }
  DestructedAndTraced::n_destructed = 0;

  for (auto it = map->begin(); it != map->end(); ++it) {
    size_t value = it->value;
    DestructedAndTraced::n_traced = 0;
    IncrementalMarkingTestDriver driver(ThreadState::Current());
    driver.StartGC();
    driver.TriggerMarkingSteps();
    // map should now be marked, but has not been traced since it's weak.
    EXPECT_EQ(0u, DestructedAndTraced::n_traced);
    ConservativelyCollectGarbage();
    // map buckets were traced (at least once).
    EXPECT_NE(kNumObjects, DestructedAndTraced::n_traced);
    // Check that iterator is still valid.
    EXPECT_EQ(value, it->value);
  }

  // All buckets were kept alive.
  EXPECT_EQ(0u, DestructedAndTraced::n_destructed);
}

TEST_F(IncrementalMarkingTest,
       WriteBarrierOfWeakContainersStrongifiesBackingStore) {
  // Regression test: https://crbug.com/1244057
  //
  // Test ensures that weak backing stores are strongified as part of their
  // write barrier.
  using WeakMap = HeapHashMap<WeakMember<DestructedAndTraced>, size_t>;
  Persistent<WeakMap> map = MakeGarbageCollected<WeakMap>();
  map->insert(MakeGarbageCollected<DestructedAndTraced>(), 0);
  DestructedAndTraced::n_destructed = 0;
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  driver.TriggerMarkingSteps();
  {
    WeakMap tmp_map;
    map->swap(tmp_map);
  }
  driver.FinishGC();
  // All buckets were kept alive.
  EXPECT_EQ(0u, DestructedAndTraced::n_destructed);
}

TEST_F(IncrementalMarkingTest, NestedVectorsWithInlineCapacityOnStack) {
  // Regression test: https://crbug.com/339967265
  //
  // Regression test ensures that on-stack nested vectors do not have their
  // backing slot registered for compaction. Registering the slot would result
  // in a nullptr crash.
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  CompactionTestDriver(ThreadState::Current()).ForceCompactionForNextGC();
  // Pre-filled vector to trigger write barrier for backing below.
  HeapVector<int> inner_vector({1});
  driver.StartGC();
  // Vector with inline capacity on stack.
  HeapVector<HeapVector<int>, 1> vector;
  vector.push_back(inner_vector);
  driver.FinishGC();
}

}  // namespace incremental_marking_test
}  // namespace blink
