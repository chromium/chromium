// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/thread_state_storage.h"

#include <new>

#include "base/check_op.h"

namespace blink {

constinit thread_local ThreadStateStorage* g_thread_specific_
    __attribute__((tls_model(BLINK_HEAP_THREAD_LOCAL_MODEL))) = nullptr;

// static
ThreadStateStorage ThreadStateStorage::main_thread_state_storage_;

BLINK_HEAP_DEFINE_THREAD_LOCAL_GETTER(ThreadStateStorage::Current,
                                      ThreadStateStorage*,
                                      g_thread_specific_)

// static
void ThreadStateStorage::AttachMainThread(
    ThreadState& thread_state,
    cppgc::AllocationHandle& allocation_handle,
    cppgc::HeapHandle& heap_handle) {
  g_thread_specific_ = new (&main_thread_state_storage_)
      ThreadStateStorage(thread_state, allocation_handle, heap_handle);
}

// static
void ThreadStateStorage::AttachNonMainThread(
    ThreadState& thread_state,
    cppgc::AllocationHandle& allocation_handle,
    cppgc::HeapHandle& heap_handle) {
  g_thread_specific_ =
      new ThreadStateStorage(thread_state, allocation_handle, heap_handle);
}

// static
void ThreadStateStorage::DetachNonMainThread(
    ThreadStateStorage& thread_state_storage) {
  CHECK_NE(MainThreadStateStorage(), &thread_state_storage);
  CHECK_EQ(g_thread_specific_, &thread_state_storage);
  delete &thread_state_storage;
  g_thread_specific_ = nullptr;
}

ThreadStateStorage::ThreadStateStorage(
    ThreadState& thread_state,
    cppgc::AllocationHandle& allocation_handle,
    cppgc::HeapHandle& heap_handle)
    : allocation_handle_(&allocation_handle),
      heap_handle_(&heap_handle),
      thread_state_(&thread_state) {}

}  // namespace blink
