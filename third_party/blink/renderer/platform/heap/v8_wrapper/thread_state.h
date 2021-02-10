// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_THREAD_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_THREAD_STATE_H_

#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "third_party/blink/renderer/platform/heap/blink_gc.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
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

// TODO(mlippautz): Provide specializations.
template <typename T>
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

class PLATFORM_EXPORT ThreadState final {
 public:
  class NoAllocationScope;

  static ALWAYS_INLINE ThreadState* Current() {
    return *(thread_specific_.Get());
  }

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
    return *allocation_handle_;
  }
  ALWAYS_INLINE v8::CppHeap& cpp_heap() const { return *cpp_heap_; }
  ALWAYS_INLINE v8::Isolate* GetIsolate() const { return isolate_; }

  // Forced garbage collection for testing:
  //
  // Collects garbage as long as live memory decreases (capped at 5).
  void CollectAllGarbageForTesting(
      BlinkGC::StackState stack_state =
          BlinkGC::StackState::kNoHeapPointersOnStack);

  void RunTerminationGC();

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

 private:
  // Main-thread ThreadState avoids TLS completely by using a regular global.
  // The object is manually managed and should not rely on global ctor/dtor.
  static uint8_t main_thread_state_storage_[];
  // Storage for all ThreadState objects. This includes the main-thread
  // ThreadState as well.
  static base::LazyInstance<WTF::ThreadSpecific<ThreadState*>>::Leaky
      thread_specific_;

  explicit ThreadState();
  ~ThreadState();

  // Handle is the most frequently accessed field as it is required for
  // MakeGarbageCollected().
  cppgc::AllocationHandle* allocation_handle_ = nullptr;
  std::unique_ptr<v8::CppHeap> cpp_heap_;
  v8::Isolate* isolate_ = nullptr;
  base::PlatformThreadId thread_id_;
  bool forced_scheduled_gc_for_testing_ = false;
};

template <>
class ThreadStateFor<kMainThreadOnly> {
  STATIC_ONLY(ThreadStateFor);

 public:
  static ThreadState* GetState() { return ThreadState::MainThreadState(); }
};

template <>
class ThreadStateFor<kAnyThread> {
  STATIC_ONLY(ThreadStateFor);

 public:
  static ThreadState* GetState() { return ThreadState::Current(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_THREAD_STATE_H_
