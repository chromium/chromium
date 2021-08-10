// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_THREAD_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_THREAD_STATE_H_

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/heap/blink_gc.h"
#include "third_party/blink/renderer/platform/heap/v8_wrapper/thread_local.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "v8-profiler.h"
#include "v8/include/cppgc/heap-consistency.h"
#include "v8/include/cppgc/prefinalizer.h"
#include "v8/include/v8-cppgc.h"
#include "v8/include/v8.h"

namespace v8 {
class CppHeap;
}  // namespace v8

namespace cppgc {
class AllocationHandle;
}  // namespace cppgc

namespace v8 {
class EmbedderGraph;
}  // namespace v8

namespace blink {

#define USING_PRE_FINALIZER(Class, PreFinalizer) \
  CPPGC_USING_PRE_FINALIZER(Class, PreFinalizer)

// ThreadAffinity indicates which threads objects can be used on. We
// distinguish between objects that can be used on the main thread
// only and objects that can be used on any thread.
//
// For objects that can only be used on the main thread, we avoid going
// through thread-local storage to get to the thread state. This is
// important for performance.
enum ThreadAffinity {
  kAnyThread,
  kMainThreadOnly,
};

template <typename T, typename = void>
struct ThreadingTrait {
  STATIC_ONLY(ThreadingTrait);
  static constexpr ThreadAffinity kAffinity = kAnyThread;
};

template <ThreadAffinity>
class ThreadStateFor;
class ThreadState;

using V8BuildEmbedderGraphCallback = void (*)(v8::Isolate*,
                                              v8::EmbedderGraph*,
                                              void*);

// Storage for all ThreadState objects. This includes the main-thread
// ThreadState as well. Keep it outside the class so that PLATFORM_EXPORT
// doesn't apply to it (otherwise, clang-cl complains).
extern thread_local ThreadState* g_thread_specific_ CONSTINIT
    __attribute__((tls_model(BLINK_HEAP_THREAD_LOCAL_MODEL)));

class PLATFORM_EXPORT ThreadState final {
 public:
  class NoAllocationScope;
  class GCForbiddenScope;

  BLINK_HEAP_DECLARE_THREAD_LOCAL_GETTER(Current,
                                         ThreadState*,
                                         g_thread_specific_)

  static ALWAYS_INLINE ThreadState* MainThreadState() {
    return reinterpret_cast<ThreadState*>(main_thread_state_storage_);
  }

  // Attaches a ThreadState to the main-thread.
  static ThreadState* AttachMainThread();
  // Attaches a ThreadState to the currently running thread. Must not be the
  // main thread and must be called after AttachMainThread().
  static ThreadState* AttachCurrentThread();
  static void DetachCurrentThread();

  void AttachToIsolate(v8::Isolate* isolate, V8BuildEmbedderGraphCallback);
  void DetachFromIsolate();

  ALWAYS_INLINE cppgc::AllocationHandle& allocation_handle() const {
    return allocation_handle_;
  }
  ALWAYS_INLINE cppgc::HeapHandle& heap_handle() const { return heap_handle_; }
  ALWAYS_INLINE v8::CppHeap& cpp_heap() const { return *cpp_heap_; }
  ALWAYS_INLINE v8::Isolate* GetIsolate() const { return isolate_; }

  void SafePoint(BlinkGC::StackState);

  bool IsMainThread() const { return this == MainThreadState(); }
  bool IsCreationThread() const { return thread_id_ == CurrentThread(); }

  void NotifyGarbageCollection(v8::GCType, v8::GCCallbackFlags);

  bool InAtomicSweepingPause() const {
    auto& heap_handle = cpp_heap().GetHeapHandle();
    return cppgc::subtle::HeapState::IsInAtomicPause(heap_handle) &&
           cppgc::subtle::HeapState::IsSweeping(heap_handle);
  }

  bool IsAllocationAllowed() const {
    return cppgc::subtle::DisallowGarbageCollectionScope::
        IsGarbageCollectionAllowed(cpp_heap().GetHeapHandle());
  }

  // Waits until sweeping is done and invokes the given callback with
  // the total sizes of live objects in Node and CSS arenas.
  void CollectNodeAndCssStatistics(
      base::OnceCallback<void(size_t allocated_node_bytes,
                              size_t allocated_css_bytes)>);

  bool IsIncrementalMarking();

  // Forced garbage collection for testing:
  //
  // Collects garbage as long as live memory decreases (capped at 5).
  void CollectAllGarbageForTesting(
      BlinkGC::StackState stack_state =
          BlinkGC::StackState::kNoHeapPointersOnStack);

  void EnableDetachedGarbageCollectionsForTesting();

  static ThreadState* AttachMainThreadForTesting(v8::Platform*);
  static ThreadState* AttachCurrentThreadForTesting(v8::Platform*);

 private:
  // Main-thread ThreadState avoids TLS completely by using a regular global.
  // The object is manually managed and should not rely on global ctor/dtor.
  static uint8_t main_thread_state_storage_[];

  explicit ThreadState(v8::Platform*);
  ~ThreadState();

  std::unique_ptr<v8::CppHeap> cpp_heap_;
  std::unique_ptr<v8::EmbedderRootsHandler> embedder_roots_handler_;
  cppgc::AllocationHandle& allocation_handle_;
  cppgc::HeapHandle& heap_handle_;
  v8::Isolate* isolate_ = nullptr;
  base::PlatformThreadId thread_id_;
  bool forced_scheduled_gc_for_testing_ = false;
};

template <>
class ThreadStateFor<kMainThreadOnly> {
  STATIC_ONLY(ThreadStateFor);

 public:
  static ALWAYS_INLINE ThreadState* GetState() {
    return ThreadState::MainThreadState();
  }
};

template <>
class ThreadStateFor<kAnyThread> {
  STATIC_ONLY(ThreadStateFor);

 public:
  static ALWAYS_INLINE ThreadState* GetState() {
    return ThreadState::Current();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_THREAD_STATE_H_
