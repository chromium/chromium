/*
 * Copyright (C) 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood (jhaygood@reaktix.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_THREADING_PRIMITIVES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_THREADING_PRIMITIVES_H_

#include <atomic>

#include "base/dcheck_is_on.h"
#include "base/gtest_prod_util.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace blink {
class DeferredTaskHandler;
}

namespace WTF {

class ThreadCondition;

// Note: Prefer base::Lock to WTF::Mutex. The implementation is the same, this
// will be removed. crbug.com/1290281.
class LOCKABLE WTF_EXPORT Mutex {
 public:
  Mutex() = default;
  bool TryLock() EXCLUSIVE_TRYLOCK_FUNCTION(true) { return lock_.Try(); }

  // Overridden solely for the purpose of annotating them.
  // The compiler is expected to optimize the calls away.
  void lock() EXCLUSIVE_LOCK_FUNCTION() { lock_.Acquire(); }
  void unlock() UNLOCK_FUNCTION() { lock_.Release(); }
  void AssertAcquired() const ASSERT_EXCLUSIVE_LOCK() {
    lock_.AssertAcquired();
  }

 private:
  base::Lock lock_;

  friend class ThreadCondition;
};

// RecursiveMutex is deprecated AND WILL BE REMOVED.
// https://crbug.com/856641
class LOCKABLE WTF_EXPORT RecursiveMutex {
 public:
  // Overridden solely for the purpose of annotating them.
  // The compiler is expected to optimize the calls away.
  void lock() EXCLUSIVE_LOCK_FUNCTION();
  void unlock() UNLOCK_FUNCTION();
  void AssertAcquired() const ASSERT_EXCLUSIVE_LOCK() {
    // TS_UNCHECKED_READ: Either we are the owner and then the value can be
    // read, or we aren't, and we are guaranteed to not see our own thread ID.
    DCHECK_EQ(TS_UNCHECKED_READ(owner_), base::PlatformThread::CurrentId());
  }
  bool TryLock() EXCLUSIVE_TRYLOCK_FUNCTION(true);

 private:
  // Private constructor to ensure that no new users appear. This class will be
  // removed.
  RecursiveMutex() = default;
  void UpdateStateAfterLockAcquired(base::PlatformThreadId thread_id)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  base::Lock lock_;
  // Atomic only used to avoid load shearing.
  std::atomic<base::PlatformThreadId> owner_ GUARDED_BY(lock_) =
      base::kInvalidThreadId;
  uint64_t lock_depth_ GUARDED_BY(lock_) = 0;

  // DO NOT ADD any new caller.
  friend class ::blink::DeferredTaskHandler;

  FRIEND_TEST_ALL_PREFIXES(RecursiveMutexTest, LockUnlock);
  FRIEND_TEST_ALL_PREFIXES(RecursiveMutexTest, LockUnlockRecursive);
  FRIEND_TEST_ALL_PREFIXES(RecursiveMutexTest, LockUnlockThreads);
};

class SCOPED_LOCKABLE MutexLocker final {
  STACK_ALLOCATED();

 public:
  MutexLocker(Mutex& mutex) EXCLUSIVE_LOCK_FUNCTION(mutex) : mutex_(mutex) {
    mutex_.lock();
  }
  MutexLocker(const MutexLocker&) = delete;
  MutexLocker& operator=(const MutexLocker&) = delete;
  ~MutexLocker() UNLOCK_FUNCTION() { mutex_.unlock(); }

 private:
  Mutex& mutex_;
};

class MutexTryLocker final {
  STACK_ALLOCATED();

 public:
  MutexTryLocker(Mutex& mutex) : mutex_(mutex), locked_(mutex.TryLock()) {}
  MutexTryLocker(const MutexTryLocker&) = delete;
  MutexTryLocker& operator=(const MutexTryLocker&) = delete;
  ~MutexTryLocker() {
    if (locked_)
      mutex_.unlock();
  }

  bool Locked() const { return locked_; }

 private:
  Mutex& mutex_;
  bool locked_;
};

class WTF_EXPORT ThreadCondition final {
  USING_FAST_MALLOC(ThreadCondition);  // Only HeapTest.cpp requires.

 public:
  explicit ThreadCondition(Mutex& mutex) : cv_(&mutex.lock_) {}
  ThreadCondition(const ThreadCondition&) = delete;
  ThreadCondition& operator=(const ThreadCondition&) = delete;
  ~ThreadCondition() = default;

  void Wait() { cv_.Wait(); }
  void Signal() { cv_.Signal(); }
  void Broadcast() { cv_.Broadcast(); }

 private:
  base::ConditionVariable cv_;
};

}  // namespace WTF

using WTF::Mutex;
using WTF::MutexLocker;
using WTF::MutexTryLocker;
using WTF::RecursiveMutex;
using WTF::ThreadCondition;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_THREADING_PRIMITIVES_H_
