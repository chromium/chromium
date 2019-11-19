// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_ATOMIC_ENTRY_FLAG_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_ATOMIC_ENTRY_FLAG_H_

#include <atomic>

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// A flag which provides a fast check whether a scope may be entered on the
// current thread, without needing to access thread-local storage or mutex.
//
// Can have false positives (i.e., spuriously report that it might be entered),
// so it is expected that this will be used in tandem with a precise check that
// the scope is in fact entered on that thread.
//
// Example:
//   g_frobnicating_flag.MightBeEntered() &&
//   ThreadLocalFrobnicator().IsFrobnicating()
//
// Relaxed atomic operations are sufficient, since:
// - all accesses remain atomic
// - each thread must observe its own operations in order
// - no thread ever exits the flag more times than it enters (if used correctly)
// And so if a thread observes zero, it must be because it has observed an equal
// number of exits as entries.
class AtomicEntryFlag {
  DISALLOW_NEW();

 public:
  inline void Enter() { entries_.fetch_add(1, std::memory_order_relaxed); }
  inline void Exit() { entries_.fetch_sub(1, std::memory_order_relaxed); }

  // Returns false only if the current thread is not between a call to Enter and
  // a call to Exit. Returns true if this thread or another thread may currently
  // be in the scope guarded by this flag.
  inline bool MightBeEntered() const {
    return entries_.load(std::memory_order_relaxed) != 0;
  }

 private:
  std::atomic_int entries_{0};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_ATOMIC_ENTRY_FLAG_H_
