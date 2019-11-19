// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PERSISTENT_NODE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PERSISTENT_NODE_H_

#include <atomic>
#include <memory>
#include "third_party/blink/renderer/platform/heap/process_heap.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

class CrossThreadPersistentRegion;
class PersistentRegion;

enum WeaknessPersistentConfiguration {
  kNonWeakPersistentConfiguration,
  kWeakPersistentConfiguration
};

enum CrossThreadnessPersistentConfiguration {
  kSingleThreadPersistentConfiguration,
  kCrossThreadPersistentConfiguration
};

template <CrossThreadnessPersistentConfiguration>
struct PersistentMutexTraits {
  struct [[maybe_unused]] Locker{};
  static void AssertAcquired() {}
};

template <>
struct PersistentMutexTraits<kCrossThreadPersistentConfiguration> {
  struct Locker {
    MutexLocker locker{ProcessHeap::CrossThreadPersistentMutex()};
  };
  static void AssertAcquired() {
#if DCHECK_IS_ON()
    ProcessHeap::CrossThreadPersistentMutex().AssertAcquired();
#endif
  }
};

class PersistentNode final {
  DISALLOW_NEW();

 public:
  PersistentNode() { DCHECK(IsUnused()); }

#if DCHECK_IS_ON()
  ~PersistentNode() {
    // If you hit this assert, it means that the thread finished
    // without clearing persistent handles that the thread created.
    // We don't enable the assert for the main thread because the
    // main thread finishes without clearing all persistent handles.
    DCHECK(IsMainThread() || IsUnused());
  }
#endif

  // It is dangerous to copy the PersistentNode because it breaks the
  // free list.
  PersistentNode& operator=(const PersistentNode& otherref) = delete;

  // Ideally the trace method should be virtual and automatically dispatch
  // to the most specific implementation. However having a virtual method
  // on PersistentNode leads to too eager template instantiation with MSVC
  // which leads to include cycles.
  // Instead we call the constructor with a TraceCallback which knows the
  // type of the most specific child and calls trace directly. See
  // TraceMethodDelegate in Visitor.h for how this is done.
  void TracePersistentNode(Visitor* visitor) {
    DCHECK(!IsUnused());
    DCHECK(trace_);
    trace_(visitor, self_);
  }

  void Initialize(void* self, TraceCallback trace) {
    DCHECK(IsUnused());
    self_ = self;
    trace_ = trace;
  }

  void Reinitialize(void* self, TraceCallback trace) {
    self_ = self;
    trace_ = trace;
  }

  void SetFreeListNext(PersistentNode* node) {
    DCHECK(!node || node->IsUnused());
    self_ = node;
    trace_ = nullptr;
    DCHECK(IsUnused());
  }

  PersistentNode* FreeListNext() {
    DCHECK(IsUnused());
    PersistentNode* node = reinterpret_cast<PersistentNode*>(self_);
    DCHECK(!node || node->IsUnused());
    return node;
  }

  bool IsUnused() const { return !trace_; }

  void* Self() const { return self_; }

 private:
  // If this PersistentNode is in use:
  //   - m_self points to the corresponding Persistent handle.
  //   - m_trace points to the trace method.
  // If this PersistentNode is freed:
  //   - m_self points to the next freed PersistentNode.
  //   - m_trace is nullptr.
  void* self_ = nullptr;
  TraceCallback trace_ = nullptr;
};

struct PersistentNodeSlots final {
  USING_FAST_MALLOC(PersistentNodeSlots);

 public:
  static constexpr int kSlotCount = 256;

  PersistentNodeSlots* next;
  PersistentNode slot[kSlotCount];
};

// Used by PersistentBase to manage a pointer to a thread heap persistent node.
// This class mostly passes accesses through, but provides an interface
// compatible with CrossThreadPersistentNodePtr.
template <ThreadAffinity affinity,
          WeaknessPersistentConfiguration weakness_configuration>
class PersistentNodePtr {
  STACK_ALLOCATED();

 public:
  PersistentNode* Get() const { return ptr_; }
  bool IsInitialized() const { return ptr_; }

  void Initialize(void* owner, TraceCallback);
  void Uninitialize();

  PersistentNodePtr& operator=(PersistentNodePtr&& other) {
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
    return *this;
  }

 private:
  PersistentNode* ptr_ = nullptr;
#if DCHECK_IS_ON()
  ThreadState* state_ = nullptr;
#endif
};

// Used by PersistentBase to manage a pointer to a cross-thread persistent node.
// It uses ProcessHeap::CrossThreadPersistentMutex() to protect most accesses,
// but can be polled to see whether it is initialized without the mutex.
template <WeaknessPersistentConfiguration weakness_configuration>
class CrossThreadPersistentNodePtr {
  STACK_ALLOCATED();

 public:
  PersistentNode* Get() const {
    PersistentMutexTraits<
        kCrossThreadPersistentConfiguration>::AssertAcquired();
    return ptr_.load(std::memory_order_relaxed);
  }
  bool IsInitialized() const { return ptr_.load(std::memory_order_acquire); }

  void Initialize(void* owner, TraceCallback);
  void Uninitialize();

  void ClearWithLockHeld();

  CrossThreadPersistentNodePtr& operator=(
      CrossThreadPersistentNodePtr&& other) {
    PersistentMutexTraits<
        kCrossThreadPersistentConfiguration>::AssertAcquired();
    PersistentNode* node = other.ptr_.load(std::memory_order_relaxed);
    ptr_.store(node, std::memory_order_relaxed);
    other.ptr_.store(nullptr, std::memory_order_relaxed);
    return *this;
  }

 private:
  // Access must either be protected by the cross-thread persistent mutex or
  // handle the fact that this may be changed concurrently (with a
  // release-store).
  std::atomic<PersistentNode*> ptr_{nullptr};
};

class PLATFORM_EXPORT PersistentRegionBase {
 public:
  ~PersistentRegionBase();

  inline PersistentNode* AllocateNode(void* self, TraceCallback trace);
  inline void FreeNode(PersistentNode* persistent_node);
  int NodesInUse() const;

 protected:
  using ShouldTraceCallback = bool (*)(Visitor*, PersistentNode*);

  void TraceNodesImpl(Visitor*, ShouldTraceCallback);

  void EnsureNodeSlots();

  PersistentNode* free_list_head_ = nullptr;
  PersistentNodeSlots* slots_ = nullptr;
#if DCHECK_IS_ON()
  size_t used_node_count_ = 0;
#endif
};

inline PersistentNode* PersistentRegionBase::AllocateNode(void* self,
                                                          TraceCallback trace) {
#if DCHECK_IS_ON()
  ++used_node_count_;
#endif
  if (UNLIKELY(!free_list_head_))
    EnsureNodeSlots();
  DCHECK(free_list_head_);
  PersistentNode* node = free_list_head_;
  free_list_head_ = free_list_head_->FreeListNext();
  node->Initialize(self, trace);
  DCHECK(!node->IsUnused());
  return node;
}

void PersistentRegionBase::FreeNode(PersistentNode* persistent_node) {
#if DCHECK_IS_ON()
  DCHECK_GT(used_node_count_, 0u);
#endif
  persistent_node->SetFreeListNext(free_list_head_);
  free_list_head_ = persistent_node;
#if DCHECK_IS_ON()
  --used_node_count_;
#endif
}

class PLATFORM_EXPORT PersistentRegion final : public PersistentRegionBase {
  USING_FAST_MALLOC(PersistentRegion);

 public:
  inline void TraceNodes(Visitor*);

  // Clears the Persistent and then frees the node.
  void ReleaseNode(PersistentNode*);

  void PrepareForThreadStateTermination(ThreadState*);

 private:
  static constexpr bool ShouldTracePersistentNode(Visitor*, PersistentNode*) {
    return true;
  }
};

inline void PersistentRegion::TraceNodes(Visitor* visitor) {
  PersistentRegionBase::TraceNodesImpl(visitor, ShouldTracePersistentNode);
}

class PLATFORM_EXPORT CrossThreadPersistentRegion final
    : public PersistentRegionBase {
  USING_FAST_MALLOC(CrossThreadPersistentRegion);

 public:
  inline PersistentNode* AllocateNode(void* self, TraceCallback trace);
  inline void FreeNode(PersistentNode*);
  inline void TraceNodes(Visitor*);

  void PrepareForThreadStateTermination(ThreadState*);

#if defined(ADDRESS_SANITIZER)
  void UnpoisonCrossThreadPersistents();
#endif

 private:
  NO_SANITIZE_ADDRESS
  static bool ShouldTracePersistentNode(Visitor*, PersistentNode*);
};

inline PersistentNode* CrossThreadPersistentRegion::AllocateNode(
    void* self,
    TraceCallback trace) {
  PersistentMutexTraits<kCrossThreadPersistentConfiguration>::AssertAcquired();
  return PersistentRegionBase::AllocateNode(self, trace);
}

inline void CrossThreadPersistentRegion::FreeNode(PersistentNode* node) {
  PersistentMutexTraits<kCrossThreadPersistentConfiguration>::AssertAcquired();
  // PersistentBase::UninitializeSafe opportunistically checks for uninitialized
  // nodes to allow a fast path destruction of unused nodes. This check is
  // performed without taking the lock that is required for processing a
  // cross-thread node. After taking the lock the condition needs to checked
  // again to avoid double-freeing a node because the node may have been
  // concurrently freed by the garbage collector on another thread.
  if (!node)
    return;
  PersistentRegionBase::FreeNode(node);
}

inline void CrossThreadPersistentRegion::TraceNodes(Visitor* visitor) {
  PersistentRegionBase::TraceNodesImpl(visitor, ShouldTracePersistentNode);
}

template <ThreadAffinity affinity,
          WeaknessPersistentConfiguration weakness_configuration>
void PersistentNodePtr<affinity, weakness_configuration>::Initialize(
    void* owner,
    TraceCallback trace_callback) {
  ThreadState* state = ThreadStateFor<affinity>::GetState();
  DCHECK(state->CheckThread());
  PersistentRegion* region =
      weakness_configuration == kWeakPersistentConfiguration
          ? state->GetWeakPersistentRegion()
          : state->GetPersistentRegion();
  ptr_ = region->AllocateNode(owner, trace_callback);
#if DCHECK_IS_ON()
  state_ = state;
#endif
}

template <ThreadAffinity affinity,
          WeaknessPersistentConfiguration weakness_configuration>
void PersistentNodePtr<affinity, weakness_configuration>::Uninitialize() {
  if (!ptr_)
    return;
  ThreadState* state = ThreadStateFor<affinity>::GetState();
  DCHECK(state->CheckThread());
#if DCHECK_IS_ON()
  DCHECK_EQ(state_, state)
      << "must be initialized and uninitialized on the same thread";
  state_ = nullptr;
#endif
  PersistentRegion* region =
      weakness_configuration == kWeakPersistentConfiguration
          ? state->GetWeakPersistentRegion()
          : state->GetPersistentRegion();
  state->FreePersistentNode(region, ptr_);
  ptr_ = nullptr;
}

template <WeaknessPersistentConfiguration weakness_configuration>
void CrossThreadPersistentNodePtr<weakness_configuration>::Initialize(
    void* owner,
    TraceCallback trace_callback) {
  PersistentMutexTraits<kCrossThreadPersistentConfiguration>::AssertAcquired();
  CrossThreadPersistentRegion& region =
      weakness_configuration == kWeakPersistentConfiguration
          ? ProcessHeap::GetCrossThreadWeakPersistentRegion()
          : ProcessHeap::GetCrossThreadPersistentRegion();
  PersistentNode* node = region.AllocateNode(owner, trace_callback);
  ptr_.store(node, std::memory_order_release);
}

template <WeaknessPersistentConfiguration weakness_configuration>
void CrossThreadPersistentNodePtr<weakness_configuration>::Uninitialize() {
  PersistentMutexTraits<kCrossThreadPersistentConfiguration>::AssertAcquired();
  CrossThreadPersistentRegion& region =
      weakness_configuration == kWeakPersistentConfiguration
          ? ProcessHeap::GetCrossThreadWeakPersistentRegion()
          : ProcessHeap::GetCrossThreadPersistentRegion();
  region.FreeNode(ptr_.load(std::memory_order_relaxed));
  ptr_.store(nullptr, std::memory_order_release);
}

template <WeaknessPersistentConfiguration weakness_configuration>
void CrossThreadPersistentNodePtr<weakness_configuration>::ClearWithLockHeld() {
  PersistentMutexTraits<kCrossThreadPersistentConfiguration>::AssertAcquired();
  CrossThreadPersistentRegion& region =
      weakness_configuration == kWeakPersistentConfiguration
          ? ProcessHeap::GetCrossThreadWeakPersistentRegion()
          : ProcessHeap::GetCrossThreadPersistentRegion();
  region.FreeNode(ptr_.load(std::memory_order_relaxed));
  ptr_.store(nullptr, std::memory_order_release);
}

}  // namespace blink

#endif
