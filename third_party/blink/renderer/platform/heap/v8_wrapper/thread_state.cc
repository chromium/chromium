// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/v8_wrapper/thread_state.h"

#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
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

void ThreadState::NotifyGarbageCollection() {
  gc_age_++;
  WTF::Vector<BlinkGCObserver*> observers_to_notify;
  CopyToVector(observers_, observers_to_notify);
  for (auto* const observer : observers_to_notify) {
    observer->OnGarbageCollection();
  }
}

BlinkGCObserver::BlinkGCObserver(ThreadState* thread_state)
    : thread_state_(thread_state) {
  DCHECK(!thread_state_->observers_.Contains(this));
  thread_state_->observers_.insert(this);
}

BlinkGCObserver::~BlinkGCObserver() {
  DCHECK(thread_state_->observers_.Contains(this));
  thread_state_->observers_.erase(this);
}

}  // namespace blink
