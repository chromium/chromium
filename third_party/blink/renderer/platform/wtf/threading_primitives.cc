// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

#include "base/check.h"
#include "base/threading/platform_thread.h"

namespace WTF {

void RecursiveMutex::lock() {
  auto thread_id = base::PlatformThread::CurrentId();
  // Even though the thread checker doesn't complain, we are not guaranteed to
  // hold the lock here. However, reading |owner_| is fine because it is only
  // ever set to |CurrentId()| when the current thread owns the lock. It is
  // reset to another value before releasing the lock.
  //
  // So the observed values can be:
  // 1. Us: we hold the lock
  // 2. Stale kInvalidThreadId, or some other ID: not a problem, cannot be the
  //    current thread ID (as it would be set by the current thread, and thus
  //    not stale, back to case (1))
  // 3. Partial value: not possible, std::atomic<> protects from load shearing.
  if (owner_.load(std::memory_order_relaxed) != thread_id) {
    lock_.Acquire();
    DCHECK_EQ(lock_depth_, 0u);
  }
  lock_.AssertAcquired();
  UpdateStateAfterLockAcquired(thread_id);
}

void RecursiveMutex::unlock() {
  AssertAcquired();
  CHECK_GT(lock_depth_, 0u);  // No underflow.
  lock_depth_--;
  if (lock_depth_ == 0) {
    owner_.store(base::kInvalidThreadId, std::memory_order_relaxed);
    lock_.Release();
  }
}

bool RecursiveMutex::TryLock() {
  auto thread_id = base::PlatformThread::CurrentId();
  // See comment above about reading |owner_|.
  if ((owner_.load(std::memory_order_relaxed) == thread_id) || lock_.Try()) {
    UpdateStateAfterLockAcquired(thread_id);
    return true;
  }

  return false;
}

void RecursiveMutex::UpdateStateAfterLockAcquired(
    base::PlatformThreadId thread_id) {
  lock_depth_++;  // uint64_t, no overflow.
  owner_.store(thread_id, std::memory_order_relaxed);
}

}  // namespace WTF
