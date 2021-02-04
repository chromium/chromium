// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/v8_wrapper/thread_state.h"

#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8.h"
#include "v8/include/v8-cppgc.h"

namespace blink {

// static
base::LazyInstance<WTF::ThreadSpecific<ThreadState*>>::Leaky
    ThreadState::thread_specific_ = LAZY_INSTANCE_INITIALIZER;

// static
alignas(ThreadState) uint8_t
    ThreadState::main_thread_state_storage_[sizeof(ThreadState)];

// static
ThreadState* ThreadState::AttachMainThread() {
  return new (main_thread_state_storage_) ThreadState();
}

// static
ThreadState* ThreadState::AttachCurrentThread() {
  return new ThreadState();
}

// static
void ThreadState::DetachCurrentThread() {
  auto* state = ThreadState::Current();
  DCHECK(state);
  delete state;
}

ThreadState::ThreadState() : thread_id_(CurrentThread()) {}

ThreadState::~ThreadState() {
  DCHECK(!IsMainThread());
  DCHECK(IsCreationThread());
}

void ThreadState::RunTerminationGC() {
  cpp_heap_->Terminate();
}

void ThreadState::SafePoint(BlinkGC::StackState stack_state) {
  DCHECK(IsCreationThread());
  if (stack_state != BlinkGC::kNoHeapPointersOnStack)
    return;

  if (forced_scheduled_gc_for_testing_) {
    CollectAllGarbageForTesting(stack_state);
    forced_scheduled_gc_for_testing_ = false;
  }
}

void ThreadState::NotifyGarbageCollection(v8::GCType type,
                                          v8::GCCallbackFlags flags) {
  if (flags & v8::kGCCallbackFlagForced) {
    // Forces a precise GC at the end of the current event loop. This is
    // required for testing code that cannot use GC internals but rather has
    // to rely on window.gc(). Only schedule additional GCs if the last GC was
    // using conservative stack scanning.
    if (type == v8::kGCTypeScavenge) {
      forced_scheduled_gc_for_testing_ = true;
    } else if (type == v8::kGCTypeMarkSweepCompact) {
      // TODO(1056170): Only need to schedule a forced GC if stack was scanned
      // conservatively in previous GC.
      forced_scheduled_gc_for_testing_ = true;
    }
  }
}

void ThreadState::CollectAllGarbageForTesting(BlinkGC::StackState stack_state) {
  // TODO(1056170): Implement.
}

}  // namespace blink
