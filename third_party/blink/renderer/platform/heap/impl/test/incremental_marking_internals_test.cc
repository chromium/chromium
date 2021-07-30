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
#include "third_party/blink/renderer/platform/heap/heap_test_objects.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/impl/heap_compact.h"
#include "third_party/blink/renderer/platform/heap/impl/trace_traits.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class IncrementalMarkingInternalsTest : public TestSupportingGC {};

namespace incremental_marking_test {

// Visitor that expects every directly reachable object from a given backing
// store to be in the set of provided objects.
class BackingVisitor final : public Visitor {
 public:
  BackingVisitor(ThreadState* state, Vector<void*>* objects)
      : Visitor(state), objects_(objects) {}
  ~BackingVisitor() override = default;

  void ProcessBackingStore(HeapObjectHeader* header) {
    EXPECT_TRUE(header->IsMarked());
    header->Unmark();

    GCInfo::From(header->GcInfoIndex()).trace(this, header->Payload());
  }

  void Visit(const void* obj, TraceDescriptor desc) override {
    EXPECT_TRUE(obj);
    auto** pos = std::find(objects_->begin(), objects_->end(), obj);
    if (objects_->end() != pos)
      objects_->erase(pos);
    // The garbage collector will find those objects so we can mark them.
    HeapObjectHeader* const header =
        HeapObjectHeader::FromPayload(desc.base_object_payload);
    if (!header->IsMarked())
      EXPECT_TRUE(header->TryMark());
  }

  void VisitEphemeron(const void* key, TraceDescriptor value_desc) override {
    if (!HeapObjectHeader::FromPayload(key)->IsMarked())
      return;
    value_desc.callback(this, value_desc.base_object_payload);
  }

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
    heap_.SetupWorklists(false);
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
  STACK_ALLOCATED();

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
    for (void* object : objects) {
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
  Vector<std::pair<HeapObjectHeader*, bool /* was marked */>> headers_;
};

class Object : public LinkedObject {
 public:
  Object() = default;
  explicit Object(Object* next) : LinkedObject(next) {}

  bool IsMarked() const {
    return HeapObjectHeader::FromPayload(this)->IsMarked();
  }

  void Trace(Visitor* visitor) const override { LinkedObject::Trace(visitor); }
};

class ObjectWithWriteBarrier : public GarbageCollected<ObjectWithWriteBarrier> {
 public:
  void Trace(Visitor* v) const { v->Trace(object_); }

  void Set(Object* object) { object_ = object; }

 private:
  Member<Object> object_;
};

// =============================================================================
// Member<T> support. ==========================================================
// =============================================================================

TEST_F(IncrementalMarkingInternalsTest, MemberSetUnmarkedObject) {
  auto* parent = MakeGarbageCollected<Object>();
  auto* child = MakeGarbageCollected<Object>();
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {child});
    EXPECT_FALSE(child->IsMarked());
    parent->set_next(child);
    EXPECT_TRUE(child->IsMarked());
  }
}

TEST_F(IncrementalMarkingInternalsTest, MemberSetMarkedObjectNoBarrier) {
  auto* parent = MakeGarbageCollected<Object>();
  auto* child = MakeGarbageCollected<Object>();
  EXPECT_TRUE(HeapObjectHeader::FromPayload(child)->TryMark());
  {
    ExpectNoWriteBarrierFires scope(ThreadState::Current(), {child});
    parent->set_next(child);
  }
}

TEST_F(IncrementalMarkingInternalsTest, MemberInitializingStoreNoBarrier) {
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

TEST_F(IncrementalMarkingInternalsTest, MemberReferenceAssignMember) {
  auto* obj = MakeGarbageCollected<LinkedObject>();
  auto* ref_obj = MakeGarbageCollected<LinkedObject>();
  Member<LinkedObject>& m2 = ref_obj->next_ref();
  Member<LinkedObject> m3(obj);
  {
    ExpectWriteBarrierFires scope(ThreadState::Current(), {obj});
    m2 = m3;
  }
}

TEST_F(IncrementalMarkingInternalsTest, MemberSetDeletedValueNoBarrier) {
  auto* obj = MakeGarbageCollected<LinkedObject>();
  Member<LinkedObject>& m = obj->next_ref();
  {
    ExpectNoWriteBarrierFires scope(ThreadState::Current(), {});
    m = WTF::kHashTableDeletedValue;
  }
}

TEST_F(IncrementalMarkingInternalsTest, MemberCopyDeletedValueNoBarrier) {
  auto* obj1 = MakeGarbageCollected<LinkedObject>();
  Member<LinkedObject>& m1 = obj1->next_ref();
  m1 = WTF::kHashTableDeletedValue;
  {
    ExpectNoWriteBarrierFires scope(ThreadState::Current(), {});
    auto* obj2 = MakeGarbageCollected<LinkedObject>();
    obj2->next_ref() = m1;
  }
}

TEST_F(IncrementalMarkingInternalsTest,
       MemberHashTraitConstructDeletedValueNoBarrier) {
  auto* obj = MakeGarbageCollected<LinkedObject>();
  Member<LinkedObject>& m = obj->next_ref();
  {
    ExpectNoWriteBarrierFires scope(ThreadState::Current(), {});
    HashTraits<Member<LinkedObject>>::ConstructDeletedValue(m, false);
  }
}

TEST_F(IncrementalMarkingInternalsTest,
       MemberHashTraitIsDeletedValueNoBarrier) {
  auto* obj =
      MakeGarbageCollected<LinkedObject>(MakeGarbageCollected<LinkedObject>());
  Member<LinkedObject>& m = obj->next_ref();
  {
    ExpectNoWriteBarrierFires scope(ThreadState::Current(), {});
    EXPECT_FALSE(HashTraits<Member<LinkedObject>>::IsDeletedValue(m));
  }
}

// =============================================================================
// Mixin support. ==============================================================
// =============================================================================

namespace {

class Mixin : public GarbageCollectedMixin {
 public:
  Mixin() : next_(nullptr) {}
  virtual ~Mixin() = default;

  void Trace(Visitor* visitor) const override { visitor->Trace(next_); }

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
 public:
  Child() : ClassWithVirtual() {}
  ~Child() override = default;

  void Trace(Visitor* visitor) const override { Mixin::Trace(visitor); }

  void Foo() override {}
  void Bar() override {}
};

class ParentWithMixinPointer : public GarbageCollected<ParentWithMixinPointer> {
 public:
  ParentWithMixinPointer() : mixin_(nullptr) {}

  void set_mixin(Mixin* mixin) { mixin_ = mixin; }

  virtual void Trace(Visitor* visitor) const { visitor->Trace(mixin_); }

 protected:
  Member<Mixin> mixin_;
};

}  // namespace

TEST_F(IncrementalMarkingInternalsTest,
       WriteBarrierOnUnmarkedMixinApplication) {
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

TEST_F(IncrementalMarkingInternalsTest,
       NoWriteBarrierOnMarkedMixinApplication) {
  ParentWithMixinPointer* parent =
      MakeGarbageCollected<ParentWithMixinPointer>();
  auto* child = MakeGarbageCollected<Child>();
  EXPECT_TRUE(HeapObjectHeader::FromPayload(child)->TryMark());
  Mixin* mixin = static_cast<Mixin*>(child);
  EXPECT_NE(static_cast<void*>(child), static_cast<void*>(mixin));
  {
    ExpectNoWriteBarrierFires scope(ThreadState::Current(), {child});
    parent->set_mixin(mixin);
  }
}

// TODO(keishi) Non-weak hash table backings should be promptly freed but they
// are currently not because we emit write barriers for the backings, and we
// don't free marked backings.
TEST_F(IncrementalMarkingInternalsTest,
       DISABLED_WeakHashMapPromptlyFreeDisabled) {
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
    HeapObjectHeader* header = HeapObjectHeader::FromPayload(
        TraceTrait<RegisteringMixin>::GetTraceDescriptor(this)
            .base_object_payload);
    EXPECT_TRUE(header->IsInConstruction());
    registry->insert(reinterpret_cast<void*>(this), this);
  }
};

class RegisteringObject : public GarbageCollected<RegisteringObject>,
                          public RegisteringMixin {
 public:
  explicit RegisteringObject(ObjectRegistry* registry)
      : RegisteringMixin(registry) {}
};

}  // namespace

TEST_F(IncrementalMarkingInternalsTest, WriteBarrierDuringMixinConstruction) {
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

TEST_F(IncrementalMarkingInternalsTest, OverrideAfterMixinConstruction) {
  ObjectRegistry registry;
  RegisteringMixin* mixin = MakeGarbageCollected<RegisteringObject>(&registry);
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(
      TraceTrait<RegisteringMixin>::GetTraceDescriptor(mixin)
          .base_object_payload);

  EXPECT_FALSE(header->IsInConstruction());
}

// =============================================================================
// Tests that execute complete incremental garbage collections. ================
// =============================================================================

TEST_F(IncrementalMarkingInternalsTest,
       HasInlineCapacityCollectionWithHeapCompaction) {
  using Store = HeapVector<Member<Object>, 2>;

  Persistent<Store> persistent(MakeGarbageCollected<Store>());
  Persistent<Store> persistent2(MakeGarbageCollected<Store>());

  IncrementalMarkingTestDriver driver(ThreadState::Current());
  ThreadState::Current()->EnableCompactionForNextGCForTesting();
  persistent->push_back(MakeGarbageCollected<Object>());
  driver.StartGC();
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

TEST_F(IncrementalMarkingInternalsTest, WeakHashMapHeapCompaction) {
  using Store = HeapHashCountedSet<WeakMember<Object>>;

  Persistent<Store> persistent(MakeGarbageCollected<Store>());

  IncrementalMarkingTestDriver driver(ThreadState::Current());
  ThreadState::Current()->EnableCompactionForNextGCForTesting();
  driver.StartGC();
  driver.TriggerMarkingSteps();
  persistent->insert(MakeGarbageCollected<Object>());
  driver.FinishGC();

  // Weak callback should register the slot.
  EXPECT_EQ(1u, driver.GetHeapCompactLastFixupCount());
}

TEST_F(IncrementalMarkingInternalsTest,
       ConservativeGCWhileCompactionScheduled) {
  using Store = HeapVector<Member<Object>>;
  Persistent<Store> persistent(MakeGarbageCollected<Store>());
  persistent->push_back(MakeGarbageCollected<Object>());

  IncrementalMarkingTestDriver driver(ThreadState::Current());
  ThreadState::Current()->EnableCompactionForNextGCForTesting();
  driver.StartGC();
  driver.TriggerMarkingSteps();
  ThreadState::Current()->CollectGarbageForTesting(
      BlinkGC::CollectionType::kMajor, BlinkGC::kHeapPointersOnStack,
      BlinkGC::kAtomicMarking, BlinkGC::kConcurrentAndLazySweeping,
      BlinkGC::GCReason::kForcedGCForTesting);

  // Heap compaction should be canceled if incremental marking finishes with a
  // conservative GC.
  EXPECT_EQ(driver.GetHeapCompactLastFixupCount(), 0u);
}

}  // namespace incremental_marking_test
}  // namespace blink
