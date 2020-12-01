// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/v8_wrapper/thread_state.h"

#include "v8/include/v8-cppgc.h"

namespace blink {

// static
base::LazyInstance<WTF::ThreadSpecific<ThreadState*>>::Leaky
    ThreadState::thread_specific_ = LAZY_INSTANCE_INITIALIZER;

// static
alignas(ThreadState) uint8_t
    ThreadState::main_thread_state_storage_[sizeof(ThreadState)];

// static
ThreadState* ThreadState::AttachMainThread(v8::CppHeap& cpp_heap) {
  return new (main_thread_state_storage_) ThreadState(cpp_heap);
}

// static
ThreadState* ThreadState::AttachCurrentThread(v8::CppHeap& cpp_heap) {
  return new ThreadState(cpp_heap);
}

// static
void ThreadState::DetachCurrentThread() {
  auto* state = ThreadState::Current();
  DCHECK(state);
  delete state;
}

ThreadState::ThreadState(v8::CppHeap& cpp_heap)
    : cpp_heap_(cpp_heap),
      allocation_handle_(cpp_heap.GetAllocationHandle())
          thread_id_(CurrentThread()) {}

ThreadState::~ThreadState() {
  DCHECK(!IsMainThread());
  DCHECK(IsCreationThread());
}

}  // namespace blink
