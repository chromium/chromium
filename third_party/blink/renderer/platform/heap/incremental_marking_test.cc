// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <initializer_list>

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/heap_buildflags.h"
#include "third_party/blink/renderer/platform/heap/heap_compact.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/heap/trace_traits.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class IncrementalMarkingTest : public TestSupportingGC {};

namespace incremental_marking_test {

// Visitor that expects every directly reachable object from a given backing
// store to be in the set of provided objects.
class BackingVisitor : public Visitor {
 public:
  BackingVisitor(ThreadState* state, Vector<void*>* objects)
      : Visitor(state), objects_(objects) {}
  ~BackingVisitor() final {}

  void ProcessBackingStore(HeapObjectHeader* header) {
    EXPECT_TRUE(header->IsMarked());
    header->Unmark();
    GCInfoTable::Get()
        .GCInfoFromIndex(header->GcInfoIndex())
        ->trace(this, header->Payload());
  }

  void Visit(void* obj, TraceDescriptor desc) final {
    EXPECT_TRUE(obj);
    auto** pos = std::find(objects_->begin(), objects_->end(), obj);
    if (objects_->end() != pos)
      objects_->erase(pos);
    // The garbage collector will find those objects so we can mark them.
    HeapObjectHeader* const header =
        HeapObjectHeader::FromPayload(desc.base_object_payload);
    if (!header->IsMarked())
      header->Mark();
  }

  bool VisitEphemeronKeyValuePair(
      void* key,
      void* value,
      EphemeronTracingCallback key_trace_callback,
      EphemeronTracingCallback value_trace_callback) final {
    const bool key_is_dead = key_trace_callback(this, key);
    if (key_is_dead)
      return true;
    const bool value_is_dead = value_trace_callback(this, value);
    DCHECK(!value_is_dead);
    return false;
  }

  // Unused overrides.
  void VisitWeak(void* object,
                 void* object_weak_ref,
                 TraceDescriptor desc,
                 WeakCallback callback) final {}
  void VisitBackingStoreStrongly(void* object,
                                 void** object_slot,
                                 TraceDescriptor desc) final {}
  void VisitBackingStoreWeakly(void*,
                               void**,
                               TraceDescriptor,
                               TraceDescriptor,
                               WeakCallback,
                               void*) final {}
  void VisitBackingStoreOnly(void*, void**) final {}
  void RegisterBackingStoreCallback(void* slot, MovingObjectCallback) final {}
  void RegisterWeakCallback(WeakCallback, void*) final {}
  void Visit(const TraceWrapperV8Reference<v8::Value>&) final {}

 private:
  Vector<void*>* objects_;
};

// Base class for initializing worklists.
class IncrementalMarkingScopeBase {
  DISALLOW_NEW();

 public:
  explicit IncrementalMarkingScopeBase(ThreadState* thread_state)
      : thread_state_(thread_state), heap_(thread_state_->Heap()) {
    if (thread_state_->IsMarkingInProgress() ||
        thread_state_->IsSweepingInProgress()) {
      TestSupportingGC::PreciselyCollectGarbage();
    }
    heap_.SetupWorklists();
  }

  ~IncrementalMarkingScopeBase() {
    heap_.DestroyMarkingWorklists(BlinkGC::StackState::kNoHeapPointersOnStack);
    heap_.DestroyCompactionWorklists();
  }

  ThreadHeap& heap() const { return heap_; }

 protected:
  ThreadState* const thread_state_;
  ThreadHeap& heap_;
};

class IncrementalMarkingScope : public IncrementalMarkingScopeBase {
 public:
  explicit IncrementalMarkingScope(ThreadState* thread_state)
      : IncrementalMarkingScopeBase(thread_state),
        gc_forbidden_scope_(thread_state),
        marking_worklist_(heap_.GetMarkingWorklist()),
        write_barrier_worklist_(heap_.GetWriteBarrierWorklist()),
        not_fully_constructed_worklist_(
            heap_.GetNotFullyConstructedWorklist()) {
    thread_state_->SetGCPhase(ThreadState::GCPhase::kMarking);
    ThreadState::AtomicPauseScope atomic_pause_scope_(thread_state_);
    ScriptForbiddenScope script_forbidden_scope;
    EXPECT_TRUE(marking_worklist_->IsGlobalEmpty());
    EXPECT_TRUE(write_barrier_worklist_->IsGlobalEmpty());
    EXPECT_TRUE(not_fully_constructed_worklist_->IsGlobalEmpty());
    thread_state->EnableIncrementalMarkingBarrier();
    thread_state->current_gc_data_.visitor = std::make_unique<MarkingVisitor>(
        thread_state, MarkingVisitor::kGlobalMarking);
  }

  ~IncrementalMarkingScope() {
    EXPECT_TRUE(marking_worklist_->IsGlobalEmpty());
    EXPECT_TRUE(write_barrier_worklist_->IsGlobalEmpty());
    EXPECT_TRUE(not_fully_constructed_worklist_->IsGlobalEmpty());
    thread_state_->DisableIncrementalMarkingBarrier();
    // Need to clear out unused worklists that might have been polluted during
    // test.
    heap_.GetWeakCallbackWorklist()->Clear();
    thread_state_->SetGCPhase(ThreadState::GCPhase::kSweeping);
    thread_state_->SetGCPhase(ThreadState::GCPhase::kNone);
  }

  MarkingWorklist* marking_worklist() const { return marking_worklist_; }
  WriteBarrierWorklist* write_barrier_worklist() const {
    return write_barrier_worklist_;
  }
  NotFullyConstructedWorklist* not_fully_constructed_worklist() const {
    return not_fully_constructed_worklist_;
  }

 protected:
  ThreadState::GCForbiddenScope gc_forbidden_scope_;
  MarkingWorklist* const marking_worklist_;
  WriteBarrierWorklist* const write_barrier_worklist_;
  NotFullyConstructedWorklist* const not_fully_constructed_worklist_;
};

// Expects that the write barrier fires for the objects passed to the
// constructor. This requires that the objects are added to the marking stack
// as well as headers being marked.
class ExpectWriteBarrierFires : public IncrementalMarkingScope {
 public:
  ExpectWriteBarrierFires(ThreadState* thread_state,
                          std::initializer_list<void*> objects)
      : IncrementalMarkingScope(thread_state),
        objects_(objects),
        backing_visitor_(thread_state_, &objects_) {
    EXPECT_TRUE(marking_worklist_->IsGlobalEmpty());
    EXPECT_TRUE(write_barrier_worklist_->IsGlobalEmpty());
    for (void* object : objects_) {
      // Ensure that the object is in the normal arena so we can ignore backing
      // objects on the marking stack.
      CHECK(ThreadHeap::IsNormalArenaIndex(
          PageFromObject(object)->Arena()->ArenaIndex()));
      headers_.push_back(HeapObjectHeader::FromPayload(object));
      EXPECT_FALSE(headers_.back()->IsMarked());
    }
    EXPECT_FALSE(objects_.IsEmpty());
  }

  ~ExpectWriteBarrierFires() {
    // All objects watched should be on the marking or write barrier worklist.
    MarkingItem item;
    while (marking_worklist_->Pop(WorklistTaskId::MutatorThread, &item)) {
      // Inspect backing stores to allow specifying objects that are only
      // reachable through a backing store.
      if (!ThreadHeap::IsNormalArenaIndex(
              PageFromObject(item.base_object_payload)
                  ->Arena()
                  ->ArenaIndex())) {
        backing_visitor_.ProcessBackingStore(
            HeapObjectHeader::FromPayload(item.base_object_payload));
        continue;
      }
      auto** pos =
          std::find(objects_.begin(), objects_.end(), item.base_object_payload);
      if (objects_.end() != pos)
        objects_.erase(pos);
    }
    HeapObjectHeader* header;
    while (
        write_barrier_worklist_->Pop(WorklistTaskId::MutatorThread, &header)) {
      // Inspect backing stores to allow specifying objects that are only
      // reachable through a backing store.
      if (!ThreadHeap::IsNormalArenaIndex(
              PageFromObject(header->Payload())->Arena()->ArenaIndex())) {
        backing_visitor_.ProcessBackingStore(header);
        continue;
      }
      auto** pos =
          std::find(objects_.begin(), objects_.end(), header->Payload());
      if (objects_.end() != pos)
        objects_.erase(pos);
    }
    EXPECT_TRUE(objects_.IsEmpty());
    // All headers of objects watched should be marked at this point.
    for (HeapObjectHeader* header : headers_) {
      EXPECT_TRUE(header->IsMarked());
      header->Unmark();
    }
    EXPECT_TRUE(marking_worklist_->IsGlobalEmpty());
    EXPECT_TRUE(write_barrier_worklist_->IsGlobalEmpty());
  }

 private:
  Vector<void*> objects_;
  Vector<HeapObjectHeader*> headers_;
  BackingVisitor backing_visitor_;
};

// Expects that no write barrier fires for the objects passed to the
// constructor. This requires that the marking stack stays empty and the marking
// state of the object stays the same across the lifetime of the scope.
class ExpectNoWriteBarrierFires : public IncrementalMarkingScope {
 public:
  ExpectNoWriteBarrierFires(ThreadState* thread_state,
                            std::initializer_list<void*> objects)
      : IncrementalMarkingScope(thread_state) {
    EXPECT_TRUE(marking_worklist_->IsGlobalEmpty());
    EXPECT_TRUE(write_barrier_worklist_->IsGlobalEmpty());
    for (void* object : objects_) {
      HeapObjectHeader* header = HeapObjectHeader::FromPayload(object);
      headers_.push_back(std::make_pair(header, header->IsMarked()));
    }
  }

  ~ExpectNoWriteBarrierFires() {
    EXPECT_TRUE(marking_worklist_->IsGlobalEmpty());
    EXPECT_TRUE(write_barrier_worklist_->IsGlobalEmpty());
    for (const auto& pair : headers_) {
      EXPECT_EQ(pair.second, pair.first->IsMarked());
      pair.first->Unmark();
    }
  }

 private:
  Vector<void*> objects_;
  Vector<std::pair<HeapObjectHeader*, bool /* was marked */>> headers_;
};

class Object : public LinkedObject {
 public:
  Object() = default;
  explicit Object(Object* next) : LinkedObject(next) {}

  bool IsMarked() const {
    return HeapObjectHeader::FromPayload(this)->IsMarked();
  }

  void Trace(Visitor* visitor) { LinkedObject::Trace(visitor); }
};

// =============================================================================
// Basic infrastructure support. ===============================================
// =============================================================================

TEST_F(IncrementalMarkingTest, EnableDisableBarrier) {
  EXPECT_FALSE(ThreadState::Current()->IsIncrementalMarking());
  ThreadState::Current()->EnableIncrementalMarkingBarrier();
  EXPECT_TRUE(ThreadState::Current()->IsIncrementalMarking());
  EXPECT_TRUE(ThreadState::IsAnyIncrementalMarking());
  ThreadState::Current()->DisableIncrementalMarkingBarrier();
  EXPECT_FALSE(ThreadState::Current()->IsIncrementalMarking());
}

TEST_F(IncrementalMarkingTest, ManualWriteBarrierTriggersWhenMarkingIsOn) {
  auto* object = MakeGarbageCollected<Object>();
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {object});
    EXPECT_FALSE(object->IsMarked());
    MarkingVisitor::WriteBarrier(object);
    EXPECT_TRUE(object->IsMarked());
  }
}

TEST_F(IncrementalMarkingTest, ManualWriteBarrierBailoutWhenMarkingIsOff) {
  auto* object = MakeGarbageCollected<Object>();
  EXPECT_FALSE(object->IsMarked());
  MarkingVisitor::WriteBarrier(object);
  EXPECT_FALSE(object->IsMarked());
}

// =============================================================================
// Member<T> support. ==========================================================
// =============================================================================

TEST_F(IncrementalMarkingTest, MemberSetUnmarkedObject) {
  auto* parent = MakeGarbageCollected<Object>();
  auto* child = MakeGarbageCollected<Object>();
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {child});
    EXPECT_FALSE(child->IsMarked());
    parent->set_next(child);
    EXPECT_TRUE(child->IsMarked());
  }
}

TEST_F(IncrementalMarkingTest, MemberSetMarkedObjectNoBarrier) {
  auto* parent = MakeGarbageCollected<Object>();
  auto* child = MakeGarbageCollected<Object>();
  HeapObjectHeader::FromPayload(child)->Mark();
  {
    ExpectNoWriteBarrierFires scope(ThreadState::Current(), {child});
    parent->set_next(child);
  }
}

TEST_F(IncrementalMarkingTest, MemberInitializingStoreNoBarrier) {
  auto* object1 = MakeGarbageCollected<Object>();
  HeapObjectHeader* object1_header = HeapObjectHeader::FromPayload(object1);
  {
    IncrementalMarkingScope scope(ThreadState::Current());
    EXPECT_FALSE(object1_header->IsMarked());
    auto* object2 = MakeGarbageCollected<Object>(object1);
    HeapObjectHeader* object2_header = HeapObjectHeader::FromPayload(object2);
    EXPECT_FALSE(object1_header->IsMarked());
    EXPECT_FALSE(object2_header->IsMarked());
  }
}

TEST_F(IncrementalMarkingTest, MemberReferenceAssignMember) {
  auto* obj = MakeGarbageCollected<Object>();
  Member<Object> m1;
  Member<Object>& m2 = m1;
  Member<Object> m3(obj);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj});
    m2 = m3;
  }
}

TEST_F(IncrementalMarkingTest, MemberSetDeletedValueNoBarrier) {
  Member<Object> m;
  {
    ExpectNoWriteBarrierFires scope(ThreadState::Current(), {});
    m = WTF::kHashTableDeletedValue;
  }
}

TEST_F(IncrementalMarkingTest, MemberCopyDeletedValueNoBarrier) {
  Member<Object> m1(WTF::kHashTableDeletedValue);
  {
    ExpectNoWriteBarrierFires scope(ThreadState::Current(), {});
    Member<Object> m2(m1);
  }
}

TEST_F(IncrementalMarkingTest, MemberHashTraitConstructDeletedValueNoBarrier) {
  Member<Object> m1;
  {
    ExpectNoWriteBarrierFires scope(ThreadState::Current(), {});
    HashTraits<Member<Object>>::ConstructDeletedValue(m1, false);
  }
}

TEST_F(IncrementalMarkingTest, MemberHashTraitIsDeletedValueNoBarrier) {
  Member<Object> m1(MakeGarbageCollected<Object>());
  {
    ExpectNoWriteBarrierFires scope(ThreadState::Current(), {});
    EXPECT_FALSE(HashTraits<Member<Object>>::IsDeletedValue(m1));
  }
}

// =============================================================================
// Mixin support. ==============================================================
// =============================================================================

namespace {

class Mixin : public GarbageCollectedMixin {
 public:
  Mixin() : next_(nullptr) {}
  virtual ~Mixin() {}

  void Trace(blink::Visitor* visitor) override { visitor->Trace(next_); }

  virtual void Bar() {}

 protected:
  Member<Object> next_;
};

class ClassWithVirtual {
 protected:
  virtual void Foo() {}
};

class Child : public GarbageCollected<Child>,
              public ClassWithVirtual,
              public Mixin {
  USING_GARBAGE_COLLECTED_MIXIN(Child);

 public:
  Child() : ClassWithVirtual(), Mixin() {}
  ~Child() override {}

  void Trace(blink::Visitor* visitor) override { Mixin::Trace(visitor); }

  void Foo() override {}
  void Bar() override {}
};

class ParentWithMixinPointer : public GarbageCollected<ParentWithMixinPointer> {
 public:
  ParentWithMixinPointer() : mixin_(nullptr) {}

  void set_mixin(Mixin* mixin) { mixin_ = mixin; }

  virtual void Trace(blink::Visitor* visitor) { visitor->Trace(mixin_); }

 protected:
  Member<Mixin> mixin_;
};

}  // namespace

TEST_F(IncrementalMarkingTest, WriteBarrierOnUnmarkedMixinApplication) {
  ParentWithMixinPointer* parent =
      MakeGarbageCollected<ParentWithMixinPointer>();
  auto* child = MakeGarbageCollected<Child>();
  Mixin* mixin = static_cast<Mixin*>(child);
  EXPECT_NE(static_cast<void*>(child), static_cast<void*>(mixin));
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {child});
    parent->set_mixin(mixin);
  }
}

TEST_F(IncrementalMarkingTest, NoWriteBarrierOnMarkedMixinApplication) {
  ParentWithMixinPointer* parent =
      MakeGarbageCollected<ParentWithMixinPointer>();
  auto* child = MakeGarbageCollected<Child>();
  HeapObjectHeader::FromPayload(child)->Mark();
  Mixin* mixin = static_cast<Mixin*>(child);
  EXPECT_NE(static_cast<void*>(child), static_cast<void*>(mixin));
  {
    ExpectNoWriteBarrierFires scope(ThreadState::Current(), {child});
    parent->set_mixin(mixin);
  }
}

// =============================================================================
// HeapVector support. =========================================================
// =============================================================================

namespace {

// HeapVector allows for insertion of container objects that can be traced but
// are themselves non-garbage collected.
class NonGarbageCollectedContainer {
  DISALLOW_NEW();

 public:
  NonGarbageCollectedContainer(Object* obj, int y) : obj_(obj), y_(y) {}

  virtual ~NonGarbageCollectedContainer() {}
  virtual void Trace(blink::Visitor* visitor) { visitor->Trace(obj_); }

 private:
  Member<Object> obj_;
  int y_;
};

class NonGarbageCollectedContainerRoot {
  DISALLOW_NEW();

 public:
  NonGarbageCollectedContainerRoot(Object* obj1, Object* obj2, int y)
      : next_(obj1, y), obj_(obj2) {}
  virtual ~NonGarbageCollectedContainerRoot() {}

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(next_);
    visitor->Trace(obj_);
  }

 private:
  NonGarbageCollectedContainer next_;
  Member<Object> obj_;
};

}  // namespace

TEST_F(IncrementalMarkingTest, HeapVectorPushBackMember) {
  auto* obj = MakeGarbageCollected<Object>();
  HeapVector<Member<Object>> vec;
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj});
    vec.push_back(obj);
  }
}

TEST_F(IncrementalMarkingTest, HeapVectorPushBackNonGCedContainer) {
  auto* obj = MakeGarbageCollected<Object>();
  HeapVector<NonGarbageCollectedContainer> vec;
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj});
    vec.push_back(NonGarbageCollectedContainer(obj, 1));
  }
}

TEST_F(IncrementalMarkingTest, HeapVectorPushBackStdPair) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapVector<std::pair<Member<Object>, Member<Object>>> vec;
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    vec.push_back(std::make_pair(Member<Object>(obj1), Member<Object>(obj2)));
  }
}

TEST_F(IncrementalMarkingTest, HeapVectorEmplaceBackMember) {
  auto* obj = MakeGarbageCollected<Object>();
  HeapVector<Member<Object>> vec;
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj});
    vec.emplace_back(obj);
  }
}

TEST_F(IncrementalMarkingTest, HeapVectorEmplaceBackNonGCedContainer) {
  auto* obj = MakeGarbageCollected<Object>();
  HeapVector<NonGarbageCollectedContainer> vec;
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj});
    vec.emplace_back(obj, 1);
  }
}

TEST_F(IncrementalMarkingTest, HeapVectorEmplaceBackStdPair) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapVector<std::pair<Member<Object>, Member<Object>>> vec;
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    vec.emplace_back(obj1, obj2);
  }
}

TEST_F(IncrementalMarkingTest, HeapVectorCopyMember) {
  auto* object = MakeGarbageCollected<Object>();
  HeapVector<Member<Object>> vec1;
  vec1.push_back(object);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {object});
    HeapVector<Member<Object>> vec2(vec1);
  }
}

TEST_F(IncrementalMarkingTest, HeapVectorCopyNonGCedContainer) {
  auto* obj = MakeGarbageCollected<Object>();
  HeapVector<NonGarbageCollectedContainer> vec1;
  vec1.emplace_back(obj, 1);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj});
    HeapVector<NonGarbageCollectedContainer> vec2(vec1);
  }
}

TEST_F(IncrementalMarkingTest, HeapVectorCopyStdPair) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapVector<std::pair<Member<Object>, Member<Object>>> vec1;
  vec1.emplace_back(obj1, obj2);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    HeapVector<std::pair<Member<Object>, Member<Object>>> vec2(vec1);
  }
}

TEST_F(IncrementalMarkingTest, HeapVectorMoveMember) {
  auto* obj = MakeGarbageCollected<Object>();
  HeapVector<Member<Object>> vec1;
  vec1.push_back(obj);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj});
    HeapVector<Member<Object>> vec2(std::move(vec1));
  }
}

TEST_F(IncrementalMarkingTest, HeapVectorMoveNonGCedContainer) {
  auto* obj = MakeGarbageCollected<Object>();
  HeapVector<NonGarbageCollectedContainer> vec1;
  vec1.emplace_back(obj, 1);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj});
    HeapVector<NonGarbageCollectedContainer> vec2(std::move(vec1));
  }
}

TEST_F(IncrementalMarkingTest, HeapVectorMoveStdPair) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapVector<std::pair<Member<Object>, Member<Object>>> vec1;
  vec1.emplace_back(obj1, obj2);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    HeapVector<std::pair<Member<Object>, Member<Object>>> vec2(std::move(vec1));
  }
}

TEST_F(IncrementalMarkingTest, HeapVectorSwapMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapVector<Member<Object>> vec1;
  vec1.push_back(obj1);
  HeapVector<Member<Object>> vec2;
  vec2.push_back(obj2);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    std::swap(vec1, vec2);
  }
}

TEST_F(IncrementalMarkingTest, HeapVectorSwapNonGCedContainer) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapVector<NonGarbageCollectedContainer> vec1;
  vec1.emplace_back(obj1, 1);
  HeapVector<NonGarbageCollectedContainer> vec2;
  vec2.emplace_back(obj2, 2);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    std::swap(vec1, vec2);
  }
}

TEST_F(IncrementalMarkingTest, HeapVectorSwapStdPair) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapVector<std::pair<Member<Object>, Member<Object>>> vec1;
  vec1.emplace_back(obj1, nullptr);
  HeapVector<std::pair<Member<Object>, Member<Object>>> vec2;
  vec2.emplace_back(nullptr, obj2);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    std::swap(vec1, vec2);
  }
}

TEST_F(IncrementalMarkingTest, HeapVectorSubscriptOperator) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapVector<Member<Object>> vec;
  vec.push_back(obj1);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj2});
    EXPECT_EQ(1u, vec.size());
    EXPECT_EQ(obj1, vec[0]);
    vec[0] = obj2;
    EXPECT_EQ(obj2, vec[0]);
    EXPECT_FALSE(obj1->IsMarked());
  }
}

TEST_F(IncrementalMarkingTest, HeapVectorEagerTracingStopsAtMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  auto* obj3 = MakeGarbageCollected<Object>();
  obj1->set_next(obj3);
  HeapVector<NonGarbageCollectedContainerRoot> vec;
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    vec.emplace_back(obj1, obj2, 3);
    // |obj3| is only reachable from |obj1| which is not eagerly traced. Only
    // objects without object headers are eagerly traced.
    EXPECT_FALSE(obj3->IsMarked());
  }
}

// =============================================================================
// HeapDeque support. ==========================================================
// =============================================================================

TEST_F(IncrementalMarkingTest, HeapDequePushBackMember) {
  auto* obj = MakeGarbageCollected<Object>();
  HeapDeque<Member<Object>> deq;
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj});
    deq.push_back(obj);
  }
}

TEST_F(IncrementalMarkingTest, HeapDequePushFrontMember) {
  auto* obj = MakeGarbageCollected<Object>();
  HeapDeque<Member<Object>> deq;
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj});
    deq.push_front(obj);
  }
}

TEST_F(IncrementalMarkingTest, HeapDequeEmplaceBackMember) {
  auto* obj = MakeGarbageCollected<Object>();
  HeapDeque<Member<Object>> deq;
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj});
    deq.emplace_back(obj);
  }
}

TEST_F(IncrementalMarkingTest, HeapDequeEmplaceFrontMember) {
  auto* obj = MakeGarbageCollected<Object>();
  HeapDeque<Member<Object>> deq;
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj});
    deq.emplace_front(obj);
  }
}

TEST_F(IncrementalMarkingTest, HeapDequeCopyMember) {
  auto* object = MakeGarbageCollected<Object>();
  HeapDeque<Member<Object>> deq1;
  deq1.push_back(object);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {object});
    HeapDeque<Member<Object>> deq2(deq1);
  }
}

TEST_F(IncrementalMarkingTest, HeapDequeMoveMember) {
  auto* object = MakeGarbageCollected<Object>();
  HeapDeque<Member<Object>> deq1;
  deq1.push_back(object);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {object});
    HeapDeque<Member<Object>> deq2(std::move(deq1));
  }
}

TEST_F(IncrementalMarkingTest, HeapDequeSwapMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapDeque<Member<Object>> deq1;
  deq1.push_back(obj1);
  HeapDeque<Member<Object>> deq2;
  deq2.push_back(obj2);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    std::swap(deq1, deq2);
  }
}

// =============================================================================
// HeapHashSet support. ========================================================
// =============================================================================

namespace {

template <typename Container>
void Insert() {
  auto* obj = MakeGarbageCollected<Object>();
  Container container;
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj});
    container.insert(obj);
  }
}

template <typename Container>
void InsertNoBarrier() {
  auto* obj = MakeGarbageCollected<Object>();
  Container container;
  {
    ExpectNoWriteBarrierFires scope(ThreadState::Current(), {obj});
    container.insert(obj);
  }
}

template <typename Container>
void Copy() {
  auto* obj = MakeGarbageCollected<Object>();
  Container container1;
  container1.insert(obj);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj});
    Container container2(container1);
    EXPECT_TRUE(container1.Contains(obj));
    EXPECT_TRUE(container2.Contains(obj));
  }
}

template <typename Container>
void CopyNoBarrier() {
  auto* obj = MakeGarbageCollected<Object>();
  Container container1;
  container1.insert(obj);
  {
    ExpectNoWriteBarrierFires scope(ThreadState::Current(), {obj});
    Container container2(container1);
    EXPECT_TRUE(container1.Contains(obj));
    EXPECT_TRUE(container2.Contains(obj));
  }
}

template <typename Container>
void Move() {
  auto* obj = MakeGarbageCollected<Object>();
  Container container1;
  Container container2;
  container1.insert(obj);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj});
    container2 = std::move(container1);
  }
}

template <typename Container>
void MoveNoBarrier() {
  auto* obj = MakeGarbageCollected<Object>();
  Container container1;
  container1.insert(obj);
  {
    ExpectNoWriteBarrierFires scope(ThreadState::Current(), {obj});
    Container container2(std::move(container1));
  }
}

template <typename Container>
void Swap() {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  Container container1;
  container1.insert(obj1);
  Container container2;
  container2.insert(obj2);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    std::swap(container1, container2);
  }
}

template <typename Container>
void SwapNoBarrier() {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  Container container1;
  container1.insert(obj1);
  Container container2;
  container2.insert(obj2);
  {
    ExpectNoWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    std::swap(container1, container2);
  }
}

}  // namespace

TEST_F(IncrementalMarkingTest, HeapHashSetInsert) {
  Insert<HeapHashSet<Member<Object>>>();
  // Weak references are strongified for the current cycle.
  Insert<HeapHashSet<WeakMember<Object>>>();
}

TEST_F(IncrementalMarkingTest, HeapHashSetCopy) {
  Copy<HeapHashSet<Member<Object>>>();
  // Weak references are strongified for the current cycle.
  Copy<HeapHashSet<WeakMember<Object>>>();
}

TEST_F(IncrementalMarkingTest, HeapHashSetMove) {
  Move<HeapHashSet<Member<Object>>>();
  // Weak references are strongified for the current cycle.
  Move<HeapHashSet<WeakMember<Object>>>();
}

TEST_F(IncrementalMarkingTest, HeapHashSetSwap) {
  Swap<HeapHashSet<Member<Object>>>();
  // Weak references are strongified for the current cycle.
  Swap<HeapHashSet<WeakMember<Object>>>();
}

// =============================================================================
// HeapLinkedHashSet support. ==================================================
// =============================================================================

TEST_F(IncrementalMarkingTest, HeapLinkedHashSetInsert) {
  Insert<HeapLinkedHashSet<Member<Object>>>();
  // Weak references are strongified for the current cycle.
  Insert<HeapLinkedHashSet<WeakMember<Object>>>();
}

TEST_F(IncrementalMarkingTest, HeapLinkedHashSetCopy) {
  Copy<HeapLinkedHashSet<Member<Object>>>();
  // Weak references are strongified for the current cycle.
  Copy<HeapLinkedHashSet<WeakMember<Object>>>();
}

TEST_F(IncrementalMarkingTest, HeapLinkedHashSetMove) {
  Move<HeapLinkedHashSet<Member<Object>>>();
  // Weak references are strongified for the current cycle.
  Move<HeapLinkedHashSet<WeakMember<Object>>>();
}

TEST_F(IncrementalMarkingTest, HeapLinkedHashSetSwap) {
  Swap<HeapLinkedHashSet<Member<Object>>>();
  // Weak references are strongified for the current cycle.
  Swap<HeapLinkedHashSet<WeakMember<Object>>>();
}

// =============================================================================
// HeapHashCountedSet support. =================================================
// =============================================================================

// HeapHashCountedSet does not support copy or move.

TEST_F(IncrementalMarkingTest, HeapHashCountedSetInsert) {
  Insert<HeapHashCountedSet<Member<Object>>>();
  // Weak references are strongified for the current cycle.
  Insert<HeapHashCountedSet<WeakMember<Object>>>();
}

TEST_F(IncrementalMarkingTest, HeapHashCountedSetSwap) {
  // HeapHashCountedSet is not move constructible so we cannot use std::swap.
  {
    auto* obj1 = MakeGarbageCollected<Object>();
    auto* obj2 = MakeGarbageCollected<Object>();
    HeapHashCountedSet<Member<Object>> container1;
    container1.insert(obj1);
    HeapHashCountedSet<Member<Object>> container2;
    container2.insert(obj2);
    {
      ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
      container1.swap(container2);
    }
  }
  {
    auto* obj1 = MakeGarbageCollected<Object>();
    auto* obj2 = MakeGarbageCollected<Object>();
    HeapHashCountedSet<WeakMember<Object>> container1;
    container1.insert(obj1);
    HeapHashCountedSet<WeakMember<Object>> container2;
    container2.insert(obj2);
    {
      // Weak references are strongified for the current cycle.
      ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
      container1.swap(container2);
    }
  }
}

// =============================================================================
// HeapHashMap support. ========================================================
// =============================================================================

TEST_F(IncrementalMarkingTest, HeapHashMapInsertMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapHashMap<Member<Object>, Member<Object>> map;
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    map.insert(obj1, obj2);
  }
}

TEST_F(IncrementalMarkingTest, HeapHashMapInsertWeakMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapHashMap<WeakMember<Object>, WeakMember<Object>> map;
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    map.insert(obj1, obj2);
  }
}

TEST_F(IncrementalMarkingTest, HeapHashMapInsertMemberWeakMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapHashMap<Member<Object>, WeakMember<Object>> map;
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    map.insert(obj1, obj2);
  }
}

TEST_F(IncrementalMarkingTest, HeapHashMapInsertWeakMemberMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapHashMap<WeakMember<Object>, Member<Object>> map;
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    map.insert(obj1, obj2);
  }
}

TEST_F(IncrementalMarkingTest, HeapHashMapSetMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapHashMap<Member<Object>, Member<Object>> map;
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    map.Set(obj1, obj2);
  }
}

TEST_F(IncrementalMarkingTest, HeapHashMapSetMemberUpdateValue) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  auto* obj3 = MakeGarbageCollected<Object>();
  HeapHashMap<Member<Object>, Member<Object>> map;
  map.insert(obj1, obj2);
  {
    // Only |obj3| is newly added to |map|, so we only expect the barrier to
    // fire on this one.
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj3});
    map.Set(obj1, obj3);
    EXPECT_FALSE(HeapObjectHeader::FromPayload(obj1)->IsMarked());
    EXPECT_FALSE(HeapObjectHeader::FromPayload(obj2)->IsMarked());
  }
}

TEST_F(IncrementalMarkingTest, HeapHashMapIteratorChangeKey) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  auto* obj3 = MakeGarbageCollected<Object>();
  HeapHashMap<Member<Object>, Member<Object>> map;
  map.insert(obj1, obj2);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj3});
    auto it = map.find(obj1);
    EXPECT_NE(map.end(), it);
    it->key = obj3;
  }
}

TEST_F(IncrementalMarkingTest, HeapHashMapIteratorChangeValue) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  auto* obj3 = MakeGarbageCollected<Object>();
  HeapHashMap<Member<Object>, Member<Object>> map;
  map.insert(obj1, obj2);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj3});
    auto it = map.find(obj1);
    EXPECT_NE(map.end(), it);
    it->value = obj3;
  }
}

TEST_F(IncrementalMarkingTest, HeapHashMapCopyMemberMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapHashMap<Member<Object>, Member<Object>> map1;
  map1.insert(obj1, obj2);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    EXPECT_TRUE(map1.Contains(obj1));
    HeapHashMap<Member<Object>, Member<Object>> map2(map1);
    EXPECT_TRUE(map1.Contains(obj1));
    EXPECT_TRUE(map2.Contains(obj1));
  }
}

TEST_F(IncrementalMarkingTest, HeapHashMapCopyWeakMemberWeakMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapHashMap<WeakMember<Object>, WeakMember<Object>> map1;
  map1.insert(obj1, obj2);
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    EXPECT_TRUE(map1.Contains(obj1));
    HeapHashMap<WeakMember<Object>, WeakMember<Object>> map2(map1);
    EXPECT_TRUE(map1.Contains(obj1));
    EXPECT_TRUE(map2.Contains(obj1));
  }
}

TEST_F(IncrementalMarkingTest, HeapHashMapCopyMemberWeakMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapHashMap<Member<Object>, WeakMember<Object>> map1;
  map1.insert(obj1, obj2);
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    EXPECT_TRUE(map1.Contains(obj1));
    HeapHashMap<Member<Object>, WeakMember<Object>> map2(map1);
    EXPECT_TRUE(map1.Contains(obj1));
    EXPECT_TRUE(map2.Contains(obj1));
  }
}

TEST_F(IncrementalMarkingTest, HeapHashMapCopyWeakMemberMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapHashMap<WeakMember<Object>, Member<Object>> map1;
  map1.insert(obj1, obj2);
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    EXPECT_TRUE(map1.Contains(obj1));
    HeapHashMap<WeakMember<Object>, Member<Object>> map2(map1);
    EXPECT_TRUE(map1.Contains(obj1));
    EXPECT_TRUE(map2.Contains(obj1));
  }
}

TEST_F(IncrementalMarkingTest, HeapHashMapMoveMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapHashMap<Member<Object>, Member<Object>> map1;
  map1.insert(obj1, obj2);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    HeapHashMap<Member<Object>, Member<Object>> map2(std::move(map1));
  }
}

TEST_F(IncrementalMarkingTest, HeapHashMapMoveWeakMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapHashMap<WeakMember<Object>, WeakMember<Object>> map1;
  map1.insert(obj1, obj2);
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    HeapHashMap<WeakMember<Object>, WeakMember<Object>> map2(std::move(map1));
  }
}

TEST_F(IncrementalMarkingTest, HeapHashMapMoveMemberWeakMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapHashMap<Member<Object>, WeakMember<Object>> map1;
  map1.insert(obj1, obj2);
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    HeapHashMap<Member<Object>, WeakMember<Object>> map2(std::move(map1));
  }
}

TEST_F(IncrementalMarkingTest, HeapHashMapMoveWeakMemberMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapHashMap<WeakMember<Object>, Member<Object>> map1;
  map1.insert(obj1, obj2);
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1, obj2});
    HeapHashMap<WeakMember<Object>, Member<Object>> map2(std::move(map1));
  }
}

TEST_F(IncrementalMarkingTest, HeapHashMapSwapMemberMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  auto* obj3 = MakeGarbageCollected<Object>();
  auto* obj4 = MakeGarbageCollected<Object>();
  HeapHashMap<Member<Object>, Member<Object>> map1;
  map1.insert(obj1, obj2);
  HeapHashMap<Member<Object>, Member<Object>> map2;
  map2.insert(obj3, obj4);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(),
                                  {obj1, obj2, obj3, obj4});
    std::swap(map1, map2);
  }
}

TEST_F(IncrementalMarkingTest, HeapHashMapSwapWeakMemberWeakMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  auto* obj3 = MakeGarbageCollected<Object>();
  auto* obj4 = MakeGarbageCollected<Object>();
  HeapHashMap<WeakMember<Object>, WeakMember<Object>> map1;
  map1.insert(obj1, obj2);
  HeapHashMap<WeakMember<Object>, WeakMember<Object>> map2;
  map2.insert(obj3, obj4);
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires scope(ThreadState::Current(),
                                  {obj1, obj2, obj3, obj4});
    std::swap(map1, map2);
  }
}

TEST_F(IncrementalMarkingTest, HeapHashMapSwapMemberWeakMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  auto* obj3 = MakeGarbageCollected<Object>();
  auto* obj4 = MakeGarbageCollected<Object>();
  HeapHashMap<Member<Object>, WeakMember<Object>> map1;
  map1.insert(obj1, obj2);
  HeapHashMap<Member<Object>, WeakMember<Object>> map2;
  map2.insert(obj3, obj4);
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires scope(ThreadState::Current(),
                                  {obj1, obj2, obj3, obj4});
    std::swap(map1, map2);
  }
}

TEST_F(IncrementalMarkingTest, HeapHashMapSwapWeakMemberMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  auto* obj3 = MakeGarbageCollected<Object>();
  auto* obj4 = MakeGarbageCollected<Object>();
  HeapHashMap<WeakMember<Object>, Member<Object>> map1;
  map1.insert(obj1, obj2);
  HeapHashMap<WeakMember<Object>, Member<Object>> map2;
  map2.insert(obj3, obj4);
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires scope(ThreadState::Current(),
                                  {obj1, obj2, obj3, obj4});
    std::swap(map1, map2);
  }
}

TEST_F(IncrementalMarkingTest, HeapHashMapCopyKeysToVectorMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapHashMap<Member<Object>, Member<Object>> map;
  map.insert(obj1, obj2);
  HeapVector<Member<Object>> vec;
  {
    // Only key should have its write barrier fired. A write barrier call for
    // value hints to an inefficient implementation.
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj1});
    CopyKeysToVector(map, vec);
  }
}

TEST_F(IncrementalMarkingTest, HeapHashMapCopyValuesToVectorMember) {
  auto* obj1 = MakeGarbageCollected<Object>();
  auto* obj2 = MakeGarbageCollected<Object>();
  HeapHashMap<Member<Object>, Member<Object>> map;
  map.insert(obj1, obj2);
  HeapVector<Member<Object>> vec;
  {
    // Only value should have its write barrier fired. A write barrier call for
    // key hints to an inefficient implementation.
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj2});
    CopyValuesToVector(map, vec);
  }
}

// TODO(keishi) Non-weak hash table backings should be promptly freed but they
// are currently not because we emit write barriers for the backings, and we
// don't free marked backings.
TEST_F(IncrementalMarkingTest, DISABLED_WeakHashMapPromptlyFreeDisabled) {
  ThreadState* state = ThreadState::Current();
  state->SetGCState(ThreadState::kIncrementalMarkingStepScheduled);
  Persistent<Object> obj1 = MakeGarbageCollected<Object>();
  NormalPageArena* arena = static_cast<NormalPageArena*>(
      ThreadState::Current()->Heap().Arena(BlinkGC::kHashTableArenaIndex));
  CHECK(arena);
  {
    size_t before = arena->promptly_freed_size();
    // Create two maps so we don't promptly free at the allocation point.
    HeapHashMap<WeakMember<Object>, Member<Object>> weak_map1;
    HeapHashMap<WeakMember<Object>, Member<Object>> weak_map2;
    weak_map1.insert(obj1, obj1);
    weak_map2.insert(obj1, obj1);
    weak_map1.clear();
    size_t after = arena->promptly_freed_size();
    // Weak hash table backings should not be promptly freed.
    EXPECT_EQ(after, before);
  }
  {
    size_t before = arena->promptly_freed_size();
    // Create two maps so we don't promptly free at the allocation point.
    HeapHashMap<Member<Object>, Member<Object>> map1;
    HeapHashMap<Member<Object>, Member<Object>> map2;
    map1.insert(obj1, obj1);
    map2.insert(obj1, obj1);
    map1.clear();
    size_t after = arena->promptly_freed_size();
    // Non-weak hash table backings should be promptly freed.
    EXPECT_GT(after, before);
  }
  state->SetGCState(ThreadState::kIncrementalMarkingFinalizeScheduled);
  state->SetGCState(ThreadState::kNoGCScheduled);
}

namespace {

class RegisteringMixin;
using ObjectRegistry = HeapHashMap<void*, Member<RegisteringMixin>>;

class RegisteringMixin : public GarbageCollectedMixin {
 public:
  explicit RegisteringMixin(ObjectRegistry* registry) {
    HeapObjectHeader* header = GetHeapObjectHeader();
    const void* uninitialized_value = BlinkGC::kNotFullyConstructedObject;
    EXPECT_EQ(uninitialized_value, header);
    registry->insert(reinterpret_cast<void*>(this), this);
  }
};

class RegisteringObject : public GarbageCollected<RegisteringObject>,
                          public RegisteringMixin {
  USING_GARBAGE_COLLECTED_MIXIN(RegisteringObject);

 public:
  explicit RegisteringObject(ObjectRegistry* registry)
      : RegisteringMixin(registry) {}
};

}  // namespace

TEST_F(IncrementalMarkingTest, WriteBarrierDuringMixinConstruction) {
  IncrementalMarkingScope scope(ThreadState::Current());
  ObjectRegistry registry;
  RegisteringObject* object =
      MakeGarbageCollected<RegisteringObject>(&registry);

  // Clear any objects that have been added to the regular marking worklist in
  // the process of calling the constructor.
  MarkingItem marking_item;
  while (scope.marking_worklist()->Pop(WorklistTaskId::MutatorThread,
                                       &marking_item)) {
    HeapObjectHeader* header =
        HeapObjectHeader::FromPayload(marking_item.base_object_payload);
    if (header->IsMarked())
      header->Unmark();
  }
  EXPECT_TRUE(scope.marking_worklist()->IsGlobalEmpty());
  // Clear any write barriers so far.
  HeapObjectHeader* header;
  while (scope.write_barrier_worklist()->Pop(WorklistTaskId::MutatorThread,
                                             &header)) {
    if (header->IsMarked())
      header->Unmark();
  }
  EXPECT_TRUE(scope.write_barrier_worklist()->IsGlobalEmpty());

  EXPECT_FALSE(scope.not_fully_constructed_worklist()->IsGlobalEmpty());
  NotFullyConstructedItem partial_item;
  bool found_mixin_object = false;
  // The same object may be on the marking work list because of expanding
  // and rehashing of the backing store in the registry.
  while (scope.not_fully_constructed_worklist()->Pop(
      WorklistTaskId::MutatorThread, &partial_item)) {
    if (object == partial_item)
      found_mixin_object = true;
    HeapObjectHeader* header = HeapObjectHeader::FromPayload(partial_item);
    if (header->IsMarked())
      header->Unmark();
  }
  EXPECT_TRUE(found_mixin_object);
  EXPECT_TRUE(scope.not_fully_constructed_worklist()->IsGlobalEmpty());
}

TEST_F(IncrementalMarkingTest, OverrideAfterMixinConstruction) {
  ObjectRegistry registry;
  RegisteringMixin* mixin = MakeGarbageCollected<RegisteringObject>(&registry);
  HeapObjectHeader* header = mixin->GetHeapObjectHeader();
  const void* uninitialized_value = BlinkGC::kNotFullyConstructedObject;
  EXPECT_NE(uninitialized_value, header);
}

// =============================================================================
// Tests that execute complete incremental garbage collections. ================
// =============================================================================

TEST_F(IncrementalMarkingTest, TestDriver) {
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.Start();
  EXPECT_TRUE(ThreadState::Current()->IsIncrementalMarking());
  driver.SingleStep();
  EXPECT_TRUE(ThreadState::Current()->IsIncrementalMarking());
  driver.FinishGC();
  EXPECT_FALSE(ThreadState::Current()->IsIncrementalMarking());
}

TEST_F(IncrementalMarkingTest, DropBackingStore) {
  // Regression test: https://crbug.com/828537
  using WeakStore = HeapHashCountedSet<WeakMember<Object>>;

  Persistent<WeakStore> persistent(MakeGarbageCollected<WeakStore>());
  persistent->insert(MakeGarbageCollected<Object>());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.Start();
  driver.FinishSteps();
  persistent->clear();
  // Marking verifier should not crash on a black backing store with all
  // black->white edges.
  driver.FinishGC();
}

TEST_F(IncrementalMarkingTest, WeakCallbackDoesNotReviveDeletedValue) {
  // Regression test: https://crbug.com/870196

  // std::pair avoids treating the hashset backing as weak backing.
  using WeakStore = HeapHashCountedSet<std::pair<WeakMember<Object>, size_t>>;

  Persistent<WeakStore> persistent(MakeGarbageCollected<WeakStore>());
  // Create at least two entries to avoid completely emptying out the data
  // structure. The values for .second are chosen to be non-null as they
  // would otherwise count as empty and be skipped during iteration after the
  // first part died.
  persistent->insert({MakeGarbageCollected<Object>(), 1});
  persistent->insert({MakeGarbageCollected<Object>(), 2});
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.Start();
  // The backing is not treated as weak backing and thus eagerly processed,
  // effectively registering the slots of WeakMembers.
  driver.FinishSteps();
  // The following deletes the first found entry. The second entry is left
  // untouched.
  for (auto& entries : *persistent) {
    persistent->erase(entries.key);
    break;
  }
  driver.FinishGC();

  size_t count = 0;
  for (const auto& entry : *persistent) {
    count++;
    // Use the entry to keep compilers happy.
    if (entry.key.second > 0) {
    }
  }
  CHECK_EQ(1u, count);
}

TEST_F(IncrementalMarkingTest, NoBackingFreeDuringIncrementalMarking) {
  // Regression test: https://crbug.com/870306
  // Only reproduces in ASAN configurations.
  using WeakStore = HeapHashCountedSet<std::pair<WeakMember<Object>, size_t>>;

  Persistent<WeakStore> persistent(MakeGarbageCollected<WeakStore>());
  // Prefill the collection to grow backing store. A new backing store allocaton
  // would trigger the write barrier, mitigating the bug where a backing store
  // is promptly freed.
  for (size_t i = 0; i < 8; i++) {
    persistent->insert({MakeGarbageCollected<Object>(), i});
  }
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.Start();
  persistent->insert({MakeGarbageCollected<Object>(), 8});
  // Is not allowed to free the backing store as the previous insert may have
  // registered a slot.
  persistent->clear();
  driver.FinishSteps();
  driver.FinishGC();
}

TEST_F(IncrementalMarkingTest, DropReferenceWithHeapCompaction) {
  using Store = HeapHashCountedSet<Member<Object>>;

  Persistent<Store> persistent(MakeGarbageCollected<Store>());
  persistent->insert(MakeGarbageCollected<Object>());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  ThreadState::Current()->EnableCompactionForNextGCForTesting();
  driver.Start();
  driver.FinishSteps();
  persistent->clear();
  // Registration of movable and updatable references should not crash because
  // if a slot have nullptr reference, it doesn't call registeration method.
  driver.FinishGC();
}

TEST_F(IncrementalMarkingTest, HasInlineCapacityCollectionWithHeapCompaction) {
  using Store = HeapVector<Member<Object>, 2>;

  Persistent<Store> persistent(MakeGarbageCollected<Store>());
  Persistent<Store> persistent2(MakeGarbageCollected<Store>());

  IncrementalMarkingTestDriver driver(ThreadState::Current());
  ThreadState::Current()->EnableCompactionForNextGCForTesting();
  persistent->push_back(MakeGarbageCollected<Object>());
  driver.Start();
  driver.FinishGC();

  // Should collect also slots that has only inline buffer and nullptr
  // references.
#if defined(ANNOTATE_CONTIGUOUS_CONTAINER)
  // When ANNOTATE_CONTIGUOUS_CONTAINER is defined, inline capacity is ignored.
  EXPECT_EQ(driver.GetHeapCompactLastFixupCount(), 1u);
#else
  EXPECT_EQ(driver.GetHeapCompactLastFixupCount(), 2u);
#endif
}

TEST_F(IncrementalMarkingTest, WeakHashMapHeapCompaction) {
  using Store = HeapHashCountedSet<WeakMember<Object>>;

  Persistent<Store> persistent(MakeGarbageCollected<Store>());

  IncrementalMarkingTestDriver driver(ThreadState::Current());
  ThreadState::Current()->EnableCompactionForNextGCForTesting();
  driver.Start();
  driver.FinishSteps();
  persistent->insert(MakeGarbageCollected<Object>());
  driver.FinishGC();

  // Weak callback should register the slot.
  EXPECT_EQ(driver.GetHeapCompactLastFixupCount(), 2u);
}

TEST_F(IncrementalMarkingTest, ConservativeGCWhileCompactionScheduled) {
  using Store = HeapVector<Member<Object>>;
  Persistent<Store> persistent(MakeGarbageCollected<Store>());
  persistent->push_back(MakeGarbageCollected<Object>());

  IncrementalMarkingTestDriver driver(ThreadState::Current());
  ThreadState::Current()->EnableCompactionForNextGCForTesting();
  driver.Start();
  driver.FinishSteps();
  ThreadState::Current()->CollectGarbage(
      BlinkGC::kHeapPointersOnStack, BlinkGC::kAtomicMarking,
      BlinkGC::kConcurrentAndLazySweeping, BlinkGC::GCReason::kConservativeGC);

  // Heap compaction should be canceled if incremental marking finishes with a
  // conservative GC.
  EXPECT_EQ(driver.GetHeapCompactLastFixupCount(), 0u);
}

namespace {

class ObjectWithWeakMember : public GarbageCollected<ObjectWithWeakMember> {
 public:
  ObjectWithWeakMember() = default;

  void set_object(Object* object) { object_ = object; }

  void Trace(Visitor* visitor) { visitor->Trace(object_); }

 private:
  WeakMember<Object> object_ = nullptr;
};

}  // namespace

TEST_F(IncrementalMarkingTest, WeakMember) {
  // Regression test: https://crbug.com/913431

  Persistent<ObjectWithWeakMember> persistent(
      MakeGarbageCollected<ObjectWithWeakMember>());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.Start();
  driver.FinishSteps();
  persistent->set_object(MakeGarbageCollected<Object>());
  driver.FinishGC();
  ConservativelyCollectGarbage();
}

TEST_F(IncrementalMarkingTest, MemberSwap) {
  // Regression test: https://crbug.com/913431
  //
  // MemberBase::Swap may be used to swap in a not-yet-processed member into an
  // already-processed member. This leads to a stale pointer that is not marked.

  Persistent<Object> object1(MakeGarbageCollected<Object>());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.Start();
  // The repro leverages the fact that initializing stores do not emit a barrier
  // (because they are still reachable from stack) to simulate the problematic
  // interleaving.
  driver.FinishSteps();
  Object* object2 =
      MakeGarbageCollected<Object>(MakeGarbageCollected<Object>());
  object2->next_ref().Swap(object1->next_ref());
  driver.FinishGC();
  ConservativelyCollectGarbage();
}

namespace {

template <typename T>
class ObjectHolder : public GarbageCollected<ObjectHolder<T>> {
 public:
  ObjectHolder() = default;

  virtual void Trace(Visitor* visitor) { visitor->Trace(holder_); }

  void set_value(T* value) { holder_ = value; }
  T* value() const { return holder_.Get(); }

 private:
  Member<T> holder_;
};

}  // namespace

TEST_F(IncrementalMarkingTest, StepDuringObjectConstruction) {
  // Test ensures that objects in construction are delayed for processing to
  // allow omitting write barriers on initializing stores.

  using O = ObjectWithCallbackBeforeInitializer<Object>;
  using Holder = ObjectHolder<O>;
  Persistent<Holder> holder(MakeGarbageCollected<Holder>());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.Start();
  MakeGarbageCollected<O>(
      base::BindOnce(
          [](IncrementalMarkingTestDriver* driver, Holder* holder, O* thiz) {
            // Publish not-fully-constructed object |thiz| by triggering write
            // barrier for the object.
            holder->set_value(thiz);
            // Finish call incremental steps.
            driver->FinishSteps(BlinkGC::StackState::kHeapPointersOnStack);
          },
          &driver, holder.Get()),
      MakeGarbageCollected<Object>());
  driver.FinishGC();
  PreciselyCollectGarbage();
}

TEST_F(IncrementalMarkingTest, StepDuringMixinObjectConstruction) {
  // Test ensures that mixin objects in construction are delayed for processing
  // to allow omitting write barriers on initializing stores.

  using Parent = ObjectWithMixinWithCallbackBeforeInitializer<Object>;
  using Mixin = MixinWithCallbackBeforeInitializer<Object>;
  using Holder = ObjectHolder<Mixin>;
  Persistent<Holder> holder(MakeGarbageCollected<Holder>());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.Start();
  MakeGarbageCollected<Parent>(
      base::BindOnce(
          [](IncrementalMarkingTestDriver* driver, Holder* holder,
             Mixin* thiz) {
            // Publish not-fully-constructed object
            // |thiz| by triggering write barrier for
            // the object.
            holder->set_value(thiz);
            // Finish call incremental steps.
            driver->FinishSteps(BlinkGC::StackState::kHeapPointersOnStack);
          },
          &driver, holder.Get()),
      MakeGarbageCollected<Object>());
  driver.FinishGC();
  PreciselyCollectGarbage();
}

TEST_F(IncrementalMarkingTest, IncrementalMarkingShrinkingBackingCompaction) {
  // Regression test: https://crbug.com/918064

  using Nested = HeapVector<HeapVector<Member<Object>>>;
  // The following setup will ensure that the outer HeapVector's backing store
  // contains slots to other to-be-compacted backings.
  Persistent<Nested> holder(MakeGarbageCollected<Nested>());
  for (int i = 0; i < 32; i++) {
    holder->emplace_back();
    holder->at(i).emplace_back(MakeGarbageCollected<Object>());
  }
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  ThreadState::Current()->EnableCompactionForNextGCForTesting();
  driver.Start();
  driver.FinishSteps();
  // Reduce size of the outer backing store.
  for (int i = 0; i < 16; i++) {
    holder->pop_back();
  }
  // Ensure that shrinking the backing does not crash in compaction as there may
  // be registered slots left in the area that is already freed.
  holder->ShrinkToFit();
  driver.FinishGC();
}

TEST_F(IncrementalMarkingTest,
       InPayloadWriteBarrierRegistersInvalidSlotForCompaction) {
  // Regression test: https://crbug.com/918064

  using Nested = HeapVector<HeapVector<Member<Object>>>;
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  ThreadState::Current()->EnableCompactionForNextGCForTesting();
  // Allocate a vector and reserve a buffer to avoid triggering the write
  // barrier during incremental marking.
  Nested* nested = MakeGarbageCollected<Nested>();
  nested->ReserveCapacity(32);
  driver.Start();
  // Initialize the inner vector, triggering tracing and slots registration.
  // This could be an object using DISALLOW_NEW() but HeapVector is easier to
  // test.
  nested->emplace_back(1);
  // Use the inner vector as otherwise the slot would not be registered due to
  // not having a backing store itself.
  nested->at(0).emplace_back(MakeGarbageCollected<Object>());
  driver.FinishSteps();
  // GCs here are without stack. This is just to show that we don't want this
  // object marked.
  CHECK(!HeapObjectHeader::FromPayload(nested)
             ->IsMarked<HeapObjectHeader::AccessMode::kAtomic>());
  nested = nullptr;
  driver.FinishGC();
}

TEST_F(IncrementalMarkingTest, AdjustMarkedBytesOnMarkedBackingStore) {
  // Regression test: https://crbug.com/966456
  //
  // Test ensures that backing expansion does not crash in trying to adjust
  // marked bytes when the page is actually about to be swept and marking is not
  // in progress.

  // Disable concurrent sweeping to check that sweeping is not in progress after
  // the FinishGC call.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      blink::features::kBlinkHeapConcurrentSweeping);
  using Container = HeapVector<Member<Object>>;
  Persistent<Container> holder(MakeGarbageCollected<Container>());
  holder->push_back(MakeGarbageCollected<Object>());
  holder->Grow(16);
  ThreadState::Current()->Heap().ResetAllocationPointForTesting();
  // Slowly shrink down the backing, only adjusting capacity without performing
  // free as the resulting memory block is too small for a free list entry.
  for (int i = 15; i > 0; i--) {
    holder->Shrink(i);
    holder->ShrinkToFit();
  }
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.Start();
  driver.FinishSteps();
  // The object is marked at this point.
  CHECK(HeapObjectHeader::FromPayload(holder.Get())
            ->IsMarked<HeapObjectHeader::AccessMode::kAtomic>());
  driver.FinishGC(false);
  // The object is still marked as sweeping did not make any progress.
  CHECK(HeapObjectHeader::FromPayload(holder.Get())->IsMarked());
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

  using Nested = HeapVector<HeapVector<Member<Object>>>;

  // Allocate dummy storage so that other vector backings are actually moved.
  MakeGarbageCollected<HeapVector<Member<Object>>>()->push_back(
      MakeGarbageCollected<Object>());

  IncrementalMarkingTestDriver driver(ThreadState::Current());
  ThreadState::Current()->EnableCompactionForNextGCForTesting();
  driver.Start();
  Nested* outer = MakeGarbageCollected<Nested>();
  outer->push_back(HeapVector<Member<Object>>());
  outer->at(0).push_back(MakeGarbageCollected<Object>());
  // The outer HeapVector object is not marked, which leaves the backing store
  // as marked with a valid slot inside. Now, if the outer backing store moves
  // first and its page is freed, then referring to the slot when the inner
  // backing store is moved may crash.
  outer = nullptr;
  driver.FinishSteps();
  driver.FinishGC();
}

class Destructed final : public GarbageCollected<Destructed> {
 public:
  ~Destructed() { n_destructed++; }

  void Trace(Visitor*) {}

  static size_t n_destructed;
};

size_t Destructed::n_destructed = 0;

class Wrapper final : public GarbageCollected<Wrapper> {
 public:
  using HashType = HeapLinkedHashSet<Member<Destructed>>;

  Wrapper() {
    for (size_t i = 0; i < 10; ++i) {
      hash_set_.insert(MakeGarbageCollected<Destructed>());
    }
  }

  void Trace(Visitor* v) { v->Trace(hash_set_); }

  void Swap() {
    HashType hash_set;
    hash_set_.Swap(hash_set);
  }

  HashType hash_set_;
};

TEST_F(IncrementalMarkingTest, MovingCallback) {
  ClearOutOldGarbage();

  Destructed::n_destructed = 0;
  {
    HeapHashSet<Member<Destructed>> to_be_destroyed;
    to_be_destroyed.ReserveCapacityForSize(100);
  }
  Persistent<Wrapper> wrapper = MakeGarbageCollected<Wrapper>();

  IncrementalMarkingTestDriver driver(ThreadState::Current());
  ThreadState::Current()->EnableCompactionForNextGCForTesting();
  driver.Start();
  driver.FinishSteps();

  // Destroy the link between original HeapLinkedHashSet object and its backign
  // store.
  wrapper->Swap();
  DCHECK(wrapper->hash_set_.IsEmpty());

  PreciselyCollectGarbage();

  EXPECT_EQ(10u, Destructed::n_destructed);
}

}  // namespace incremental_marking_test
}  // namespace blink
