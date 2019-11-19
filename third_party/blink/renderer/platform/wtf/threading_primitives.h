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

#include "base/macros.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
#include <pthread.h>
#endif

namespace WTF {

#if defined(OS_WIN)
struct PlatformMutex {
  CRITICAL_SECTION internal_mutex_;
  size_t recursion_count_;
};
typedef CONDITION_VARIABLE PlatformCondition;
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
struct PlatformMutex {
  pthread_mutex_t internal_mutex_;
#if DCHECK_IS_ON()
  size_t recursion_count_;
#endif
};
typedef pthread_cond_t PlatformCondition;
#endif

class WTF_EXPORT MutexBase {
  USING_FAST_MALLOC(MutexBase);

 public:
  ~MutexBase();

  void lock();
  void unlock();
  void AssertAcquired() const {
#if DCHECK_IS_ON()
    DCHECK(mutex_.recursion_count_);
#endif
  }

 public:
  PlatformMutex& Impl() { return mutex_; }

 protected:
  MutexBase(bool recursive);

  PlatformMutex mutex_;

  DISALLOW_COPY_AND_ASSIGN(MutexBase);
};

class LOCKABLE WTF_EXPORT Mutex : public MutexBase {
 public:
  Mutex() : MutexBase(false) {}
  bool TryLock() EXCLUSIVE_TRYLOCK_FUNCTION(true);

  // Overridden solely for the purpose of annotating them.
  // The compiler is expected to optimize the calls away.
  void lock() EXCLUSIVE_LOCK_FUNCTION() { MutexBase::lock(); }
  void unlock() UNLOCK_FUNCTION() { MutexBase::unlock(); }
  void AssertAcquired() const ASSERT_EXCLUSIVE_LOCK() {
    MutexBase::AssertAcquired();
  }
};

// RecursiveMutex is deprecated AND WILL BE REMOVED.
// https://crbug.com/856641
class WTF_EXPORT RecursiveMutex : public MutexBase {
 public:
  RecursiveMutex() : MutexBase(true) {}
  bool TryLock();
};

class SCOPED_LOCKABLE MutexLocker final {
  STACK_ALLOCATED();

 public:
  MutexLocker(Mutex& mutex) EXCLUSIVE_LOCK_FUNCTION(mutex) : mutex_(mutex) {
    mutex_.lock();
  }
  ~MutexLocker() UNLOCK_FUNCTION() { mutex_.unlock(); }

 private:
  Mutex& mutex_;

  DISALLOW_COPY_AND_ASSIGN(MutexLocker);
};

class MutexTryLocker final {
  STACK_ALLOCATED();

 public:
  MutexTryLocker(Mutex& mutex) : mutex_(mutex), locked_(mutex.TryLock()) {}
  ~MutexTryLocker() {
    if (locked_)
      mutex_.unlock();
  }

  bool Locked() const { return locked_; }

 private:
  Mutex& mutex_;
  bool locked_;

  DISALLOW_COPY_AND_ASSIGN(MutexTryLocker);
};

class WTF_EXPORT ThreadCondition final {
  USING_FAST_MALLOC(ThreadCondition);  // Only HeapTest.cpp requires.

 public:
  explicit ThreadCondition(Mutex&);
  ~ThreadCondition();

  void Wait();
  void Signal();
  void Broadcast();

 private:
  PlatformCondition condition_;
  PlatformMutex& mutex_;

  DISALLOW_COPY_AND_ASSIGN(ThreadCondition);
};

}  // namespace WTF

using WTF::MutexBase;
using WTF::Mutex;
using WTF::RecursiveMutex;
using WTF::MutexLocker;
using WTF::MutexTryLocker;
using WTF::ThreadCondition;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_THREADING_PRIMITIVES_H_
