// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/pollable_thread_safe_flag.h"

PollableThreadSafeFlag::PollableThreadSafeFlag(base::Lock* write_lock_)
    : flag_(false), write_lock_(write_lock_) {}

PollableThreadSafeFlag::~PollableThreadSafeFlag() = default;

void PollableThreadSafeFlag::SetWhileLocked(bool value) {
  write_lock_->AssertAcquired();
  base::subtle::Release_Store(&flag_, value);
}

bool PollableThreadSafeFlag::IsSet() const {
  return base::subtle::Acquire_Load(&flag_);
}
