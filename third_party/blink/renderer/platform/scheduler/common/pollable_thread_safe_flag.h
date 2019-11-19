// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_POLLABLE_THREAD_SAFE_FLAG_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_POLLABLE_THREAD_SAFE_FLAG_H_

#include "base/atomicops.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

// A PollableThreadSafeFlag can be polled without requiring a lock, but can only
// be updated if a lock is held. This enables lock-free checking as to whether a
// condition has changed, while protecting operations which update the condition
// with a lock. You must ensure that the flag is only updated within the same
// lock-protected critical section as any other variables on which the condition
// depends.
class PollableThreadSafeFlag {
  DISALLOW_NEW();

 public:
  explicit PollableThreadSafeFlag(base::Lock* write_lock);
  ~PollableThreadSafeFlag();

  // Set the flag. May only be called if |write_lock| is held.
  void SetWhileLocked(bool value);

  // Returns true iff the flag is set to true.
  bool IsSet() const;

 private:
  base::subtle::Atomic32 flag_;
  base::Lock* write_lock_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(PollableThreadSafeFlag);
};

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_POLLABLE_THREAD_SAFE_FLAG_H_
