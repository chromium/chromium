// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_THREAD_STATE_SCOPES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_THREAD_STATE_SCOPES_H_

#include "third_party/blink/renderer/platform/heap/thread_state.h"

#if defined(LEAK_SANITIZER)
#include "third_party/blink/renderer/platform/wtf/leak_annotations.h"
#endif

namespace blink {

// The NoAllocationScope class is used in debug mode to catch unwanted
// allocations. E.g. allocations during GC.
class ThreadState::NoAllocationScope final {
  STACK_ALLOCATED();

 public:
  explicit NoAllocationScope(ThreadState* state) : state_(state) {
    state_->EnterNoAllocationScope();
  }
  NoAllocationScope(const NoAllocationScope&) = delete;
  NoAllocationScope& operator=(const NoAllocationScope&) = delete;
  ~NoAllocationScope() { state_->LeaveNoAllocationScope(); }

 private:
  ThreadState* const state_;
};

class ThreadState::SweepForbiddenScope final {
  STACK_ALLOCATED();

 public:
  explicit SweepForbiddenScope(ThreadState* state) : state_(state) {
    DCHECK(!state_->sweep_forbidden_);
    state_->sweep_forbidden_ = true;
  }
  SweepForbiddenScope(const SweepForbiddenScope&) = delete;
  SweepForbiddenScope& operator=(const SweepForbiddenScope&) = delete;
  ~SweepForbiddenScope() {
    DCHECK(state_->sweep_forbidden_);
    state_->sweep_forbidden_ = false;
  }

 private:
  ThreadState* const state_;
};

class ThreadState::GCForbiddenScope final {
  STACK_ALLOCATED();

 public:
  explicit GCForbiddenScope(ThreadState* thread_state)
      : thread_state_(thread_state) {
    thread_state_->EnterGCForbiddenScope();
  }
  ~GCForbiddenScope() { thread_state_->LeaveGCForbiddenScope(); }

 private:
  ThreadState* const thread_state_;
};

// Used to mark when we are in an atomic pause for GC.
class ThreadState::AtomicPauseScope final {
  STACK_ALLOCATED();

 public:
  explicit AtomicPauseScope(ThreadState* thread_state)
      : thread_state_(thread_state), gc_forbidden_scope(thread_state) {
    thread_state_->EnterAtomicPause();
  }
  ~AtomicPauseScope() { thread_state_->LeaveAtomicPause(); }

 private:
  ThreadState* const thread_state_;
  GCForbiddenScope gc_forbidden_scope;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_THREAD_STATE_SCOPES_H_
