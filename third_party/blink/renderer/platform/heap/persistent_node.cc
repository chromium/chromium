// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/persistent_node.h"

#include "base/debug/alias.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/process_heap.h"

namespace blink {

namespace {

class DummyGCBase final : public GarbageCollected<DummyGCBase> {
 public:
  void Trace(blink::Visitor* visitor) {}
};
}

PersistentRegionBase::~PersistentRegionBase() {
  PersistentNodeSlots* slots = slots_;
  while (slots) {
    PersistentNodeSlots* dead_slots = slots;
    slots = slots->next;
    delete dead_slots;
  }
}

int PersistentRegionBase::NodesInUse() const {
  size_t persistent_count = 0;
  for (PersistentNodeSlots* slots = slots_; slots; slots = slots->next) {
    for (int i = 0; i < PersistentNodeSlots::kSlotCount; ++i) {
      if (!slots->slot[i].IsUnused())
        ++persistent_count;
    }
  }
#if DCHECK_IS_ON()
  DCHECK_EQ(persistent_count, used_node_count_);
#endif
  return persistent_count;
}

void PersistentRegionBase::EnsureNodeSlots() {
  DCHECK(!free_list_head_);
  PersistentNodeSlots* slots = new PersistentNodeSlots;
  for (int i = 0; i < PersistentNodeSlots::kSlotCount; ++i) {
    PersistentNode* node = &slots->slot[i];
    node->SetFreeListNext(free_list_head_);
    free_list_head_ = node;
    DCHECK(node->IsUnused());
  }
  slots->next = slots_;
  slots_ = slots;
}

void PersistentRegionBase::TraceNodesImpl(Visitor* visitor,
                                          ShouldTraceCallback should_trace) {
  free_list_head_ = nullptr;
  size_t persistent_count = 0;
  PersistentNodeSlots** prev_next = &slots_;
  PersistentNodeSlots* slots = slots_;
  while (slots) {
    PersistentNode* free_list_next = nullptr;
    PersistentNode* free_list_last = nullptr;
    int free_count = 0;
    for (int i = 0; i < PersistentNodeSlots::kSlotCount; ++i) {
      PersistentNode* node = &slots->slot[i];
      if (node->IsUnused()) {
        if (!free_list_next)
          free_list_last = node;
        node->SetFreeListNext(free_list_next);
        free_list_next = node;
        ++free_count;
      } else {
        ++persistent_count;
        if (!should_trace(visitor, node))
          continue;
        node->TracePersistentNode(visitor);
      }
    }
    if (free_count == PersistentNodeSlots::kSlotCount) {
      PersistentNodeSlots* dead_slots = slots;
      *prev_next = slots->next;
      slots = slots->next;
      delete dead_slots;
    } else {
      if (free_list_last) {
        DCHECK(free_list_next);
        DCHECK(!free_list_last->FreeListNext());
        free_list_last->SetFreeListNext(free_list_head_);
        free_list_head_ = free_list_next;
      }
      prev_next = &slots->next;
      slots = slots->next;
    }
  }
#if DCHECK_IS_ON()
  DCHECK_EQ(persistent_count, used_node_count_);
#endif
}

void PersistentRegion::ReleaseNode(PersistentNode* persistent_node) {
  DCHECK(!persistent_node->IsUnused());
  // 'self' is in use, containing the persistent wrapper object.
  void* self = persistent_node->Self();
  Persistent<DummyGCBase>* persistent =
      reinterpret_cast<Persistent<DummyGCBase>*>(self);
  persistent->Clear();
  DCHECK(persistent_node->IsUnused());
}

void PersistentRegion::PrepareForThreadStateTermination(ThreadState* state) {
  DCHECK_EQ(state, ThreadState::Current());
  DCHECK(!IsMainThread());
  PersistentNodeSlots* slots = slots_;
  while (slots) {
    for (int i = 0; i < PersistentNodeSlots::kSlotCount; ++i) {
      PersistentNode* node = &slots->slot[i];
      if (node->IsUnused())
        continue;
      // It is safe to cast to Persistent<DummyGCBase> because persistent heap
      // collections are banned in non-main threads.
      Persistent<DummyGCBase>* persistent =
          reinterpret_cast<Persistent<DummyGCBase>*>(node->Self());
      DCHECK(persistent);
      persistent->Clear();
      DCHECK(node->IsUnused());
    }
    slots = slots->next;
  }
#if DCHECK_IS_ON()
  DCHECK_EQ(used_node_count_, 0u);
#endif
}

bool CrossThreadPersistentRegion::ShouldTracePersistentNode(
    Visitor* visitor,
    PersistentNode* node) {
  CrossThreadPersistent<DummyGCBase>* persistent =
      reinterpret_cast<CrossThreadPersistent<DummyGCBase>*>(node->Self());
  DCHECK(persistent);
  DCHECK(!persistent->IsHashTableDeletedValue());
  Address raw_object = reinterpret_cast<Address>(persistent->Get());
  if (!raw_object)
    return false;
  return &visitor->Heap() == &ThreadState::FromObject(raw_object)->Heap();
}

void CrossThreadPersistentRegion::PrepareForThreadStateTermination(
    ThreadState* thread_state) {
  // For heaps belonging to a thread that's detaching, any cross-thread
  // persistents pointing into them needs to be disabled. Do that by clearing
  // out the underlying heap reference.
  MutexLocker lock(ProcessHeap::CrossThreadPersistentMutex());

  PersistentNodeSlots* slots = slots_;
  while (slots) {
    for (int i = 0; i < PersistentNodeSlots::kSlotCount; ++i) {
      if (slots->slot[i].IsUnused())
        continue;

      // 'self' is in use, containing the cross-thread persistent wrapper
      // object.
      CrossThreadPersistent<DummyGCBase>* persistent =
          reinterpret_cast<CrossThreadPersistent<DummyGCBase>*>(
              slots->slot[i].Self());
      DCHECK(persistent);
      void* raw_object = persistent->Get();
      if (!raw_object)
        continue;
      BasePage* page = PageFromObject(raw_object);
      DCHECK(page);
      if (page->Arena()->GetThreadState() == thread_state) {
        persistent->ClearWithLockHeld();
        DCHECK(slots->slot[i].IsUnused());
      }
    }
    slots = slots->next;
  }
}

#if defined(ADDRESS_SANITIZER)
void CrossThreadPersistentRegion::UnpoisonCrossThreadPersistents() {
#if DCHECK_IS_ON()
  ProcessHeap::CrossThreadPersistentMutex().AssertAcquired();
#endif
  size_t persistent_count = 0;
  for (PersistentNodeSlots* slots = slots_; slots; slots = slots->next) {
    for (int i = 0; i < PersistentNodeSlots::kSlotCount; ++i) {
      const PersistentNode& node = slots->slot[i];
      if (!node.IsUnused()) {
        ASAN_UNPOISON_MEMORY_REGION(node.Self(),
                                    sizeof(CrossThreadPersistent<void*>));
        ++persistent_count;
      }
    }
  }
#if DCHECK_IS_ON()
  DCHECK_EQ(persistent_count, used_node_count_);
#endif
}
#endif

}  // namespace blink
