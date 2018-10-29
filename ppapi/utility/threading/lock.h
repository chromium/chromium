// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_UTILITY_THREADING_LOCK_H_
#define PPAPI_UTILITY_THREADING_LOCK_H_

#ifdef WIN32
#include <windows.h>
// MemoryBarrier is a Win32 macro that clashes with MemoryBarrier in
// base/atomicops.h.
#undef MemoryBarrier
#else
#include <pthread.h>
#endif

namespace pp {

/// A simple wrapper around a platform-specific lock. See also AutoLock.
class Lock {
 public:
  /// Creates a lock in the "not held" state.
  Lock();

  /// Destroys the lock.
  ~Lock();

  /// Acquires the lock, blocking if it's already held by a different thread.
  /// The lock must not already be held on the current thread (i.e. recursive
  /// locks are not supported).
  ///
  /// Most callers should consider using an AutoLock instead to automatically
  /// acquire and release the lock.
  void Acquire();

  /// Releases the lock. This must be paired with a call to Acquire().
  void Release();

 private:
#if defined(WIN32)
  typedef CRITICAL_SECTION OSLockType;
#else
  typedef pthread_mutex_t OSLockType;
#endif

  OSLockType os_lock_;

  // Copy and assign not supported.
  Lock(const Lock&);
  Lock& operator=(const Lock&);
};

/// A helper class that scopes holding a lock.
///
/// @code
///   class MyClass {
///    public:
///     void DoSomething() {
///       pp::AutoLock lock(lock_);
///       ...do something with the lock held...
///     }
///
///    private:
///     pp::Lock lock_;
///   };
/// @endcode
class AutoLock {
 public:
  explicit AutoLock(Lock& lock) : lock_(lock) {
    lock_.Acquire();
  }

  ~AutoLock() {
    lock_.Release();
  }

 private:
  Lock& lock_;

  // Copy and assign not supported.
  AutoLock(const AutoLock&);
  AutoLock& operator=(const AutoLock&);
};

}  // namespace pp

#endif  // PPAPI_UTILITY_THREADING_LOCK_H_
