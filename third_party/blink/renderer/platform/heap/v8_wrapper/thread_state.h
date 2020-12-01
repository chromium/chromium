// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_THREAD_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_THREAD_STATE_H_

#include "base/lazy_instance.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "v8/include/cppgc/prefinalizer.h"
#include "v8/include/v8.h"

namespace v8 {
class CppHeap;
}  // namespace v8

namespace cppgc {
class AllocationHandle;
}  // namespace cppgc

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

class ThreadState final {
 public:
  static ALWAYS_INLINE ThreadState* Current() {
    return *(thread_specific_.Get());
  }

  static ALWAYS_INLINE ThreadState* MainThreadState() {
    DCHECK(Current()->IsMainThread());
    return reinterpret_cast<ThreadState*>(main_thread_state_storage_);
  }

  // Attaches a ThreadState to the main-thread.
  static ThreadState* AttachMainThread(v8::CppHeap&);
  // Attaches a ThreadState to the currently running thread. Must not be the
  // main thread and must be called after AttachMainThread().
  static ThreadState* AttachCurrentThread(v8::CppHeap&);
  static void DetachCurrentThread();

  ALWAYS_INLINE cppgc::AllocationHandle& allocation_handle() const {
    return allocation_handle_;
  }

 private:
  // Main-thread ThreadState avoids TLS completely by using a regular global.
  // The object is manually managed and should not rely on global ctor/dtor.
  static uint8_t main_thread_state_storage_[] alignas(ThreadState);
  // Storage for all ThreadState objects. This includes the main-thread
  // ThreadState as well.
  static base::LazyInstance<WTF::ThreadSpecific<ThreadState*>>::Leaky
      thread_specific_;

  explicit ThreadState(v8::CppHeap&);
  ~ThreadState();

  bool IsMainThread() const { return this == MainThreadState(); }
  bool IsCreationThread() const { return thread_id_ == CurrentThread(); }

  // Handle is the most frequently accessed field as it is required for
  // MakeGarbageCollected().
  cppgc::AllocationHandle& allocation_handle_;
  v8::CppHeap& cpp_heap_;
  base::PlatformThreadId thread_id_;
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
