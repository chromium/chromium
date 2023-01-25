// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/pollable_thread_safe_flag.h"

#include <atomic>

PollableThreadSafeFlag::PollableThreadSafeFlag(base::Lock* write_lock_)
    : flag_(false), write_lock_(write_lock_) {}

void PollableThreadSafeFlag::SetWhileLocked(bool value) {
  write_lock_->AssertAcquired();
  flag_.store(value, std::memory_order_release);
}

bool PollableThreadSafeFlag::IsSet() const {
  return flag_.load(std::memory_order_acquire);
}
