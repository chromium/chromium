// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/thread_state_storage.h"

#include <new>

namespace blink {

thread_local ThreadStateStorage* g_thread_specific_ CONSTINIT
    __attribute__((tls_model(BLINK_HEAP_THREAD_LOCAL_MODEL))) = nullptr;

// static
alignas(ThreadStateStorage) uint8_t
    ThreadStateStorage::main_thread_state_storage_[sizeof(ThreadStateStorage)];

BLINK_HEAP_DEFINE_THREAD_LOCAL_GETTER(ThreadStateStorage::Current,
                                      ThreadStateStorage*,
                                      g_thread_specific_)

// static
void ThreadStateStorage::CreateMain(ThreadState& thread_state,
                                    cppgc::AllocationHandle& allocation_handle,
                                    cppgc::HeapHandle& heap_handle) {
  new (main_thread_state_storage_)
      ThreadStateStorage(thread_state, allocation_handle, heap_handle);
}

// static
void ThreadStateStorage::Create(ThreadState& thread_state,
                                cppgc::AllocationHandle& allocation_handle,
                                cppgc::HeapHandle& heap_handle) {
  new ThreadStateStorage(thread_state, allocation_handle, heap_handle);
}

ThreadStateStorage::ThreadStateStorage(
    ThreadState& thread_state,
    cppgc::AllocationHandle& allocation_handle,
    cppgc::HeapHandle& heap_handle)
    : allocation_handle_(allocation_handle),
      heap_handle_(heap_handle),
      thread_state_(thread_state) {
  g_thread_specific_ = this;
}

ThreadStateStorage::~ThreadStateStorage() {
  DCHECK(!IsMainThread());
  g_thread_specific_ = nullptr;
}

}  // namespace blink
