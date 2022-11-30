// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_UTILITY_COMPLETION_CALLBACK_FACTORY_THREAD_TRAITS_H_
#define PPAPI_UTILITY_COMPLETION_CALLBACK_FACTORY_THREAD_TRAITS_H_

#include <stdint.h>

#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"
#include "ppapi/utility/threading/lock.h"

/// @file
/// Defines the traits structures for thread-safety of a completion callback
/// factory. We provide thread-safe and non-thread-safe version. The thread-safe
/// version is always correct (if you follow the thread usage rules of the
/// callback factory), but if you know your object will only be used on one
/// thread, you can uses the non-thread-safe version.
///
/// The traits defines three nested classes to perform reference counting,
/// locks, and scoped locking.

namespace pp {

/// The thread-safe version of thread traits. Using this class as the "traits"
/// template argument to a completion callback factory will make it "somewhat
/// thread-friendly." It will allow you to create completion callbacks from
/// background threads and post them to another thread to run.
///
/// Care still must be taken to ensure that the completion callbacks are
/// executed on the same thread that the factory is destroyed on to avoid a
/// race on destruction.
///
/// Implementation note: this uses a lock instead of atomic add instructions.
/// The number of platforms we need to support right now makes atomic
/// operations unwieldy for this case that we don't actually use that often.
/// As a further optimization, we can add support for this later.
class ThreadSafeThreadTraits {
 public:
  class RefCount {
   public:
    /// Default constructor. In debug mode, this checks that the object is being
    /// created on the main thread.
    RefCount() : ref_(0) {
    }

    /// AddRef() increments the reference counter.
    ///
    /// @return An int32_t with the incremented reference counter.
    int32_t AddRef() {
      AutoLock lock(lock_);
      return ++ref_;
    }

    /// Release() decrements the reference counter.
    ///
    /// @return An int32_t with the decremeneted reference counter.
    int32_t Release() {
      AutoLock lock(lock_);
      PP_DCHECK(ref_ > 0);
      return --ref_;
    }

   private:
    Lock lock_;
    int32_t ref_;
  };

  typedef pp::Lock Lock;
  typedef pp::AutoLock AutoLock;
};

/// The non-thread-safe version of thread traits. Using this class as the
/// "traits" template argument to a completion callback factory will make it
/// not thread-safe but with potential extra performance.
class NonThreadSafeThreadTraits {
 public:
  /// A simple reference counter that is not thread-safe.
  ///
  /// <strong>Note:</strong> in Debug mode, it checks that it is either called
  /// on the main thread, or always called on another thread.
  class RefCount {
   public:
    /// Default constructor. In debug mode, this checks that the object is being
    /// created on the main thread.
    RefCount() : ref_(0) {
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
      is_main_thread_ = Module::Get()->core()->IsMainThread();
#endif
    }

    /// Destructor.
    ~RefCount() {
      PP_DCHECK(is_main_thread_ == Module::Get()->core()->IsMainThread());
    }

    /// AddRef() increments the reference counter.
    ///
    /// @return An int32_t with the incremented reference counter.
    int32_t AddRef() {
      PP_DCHECK(is_main_thread_ == Module::Get()->core()->IsMainThread());
      return ++ref_;
    }

    /// Release() decrements the reference counter.
    ///
    /// @return An int32_t with the decremeneted reference counter.
    int32_t Release() {
      PP_DCHECK(is_main_thread_ == Module::Get()->core()->IsMainThread());
      return --ref_;
    }

   private:
    int32_t ref_;
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
    bool is_main_thread_;
#endif
  };

  /// A simple object that acts like a lock but does nothing.
  ///
  /// <strong>Note:</strong> in Debug mode, it checks that it is either
  /// called on the main thread, or always called on another thread. It also
  /// asserts that the caller does not recursively lock.
  class Lock {
   public:
    Lock() {
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
      is_main_thread_ = Module::Get()->core()->IsMainThread();
      lock_held_ = false;
#endif
    }

    ~Lock() {
      PP_DCHECK(is_main_thread_ == Module::Get()->core()->IsMainThread());
    }

    /// Acquires the fake "lock". This does nothing except perform checks in
    /// debug mode.
    void Acquire() {
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
      PP_DCHECK(!lock_held_);
      lock_held_ = true;
#endif
    }

    /// Releases the fake "lock". This does nothing except perform checks in
    /// debug mode.
    void Release() {
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
      PP_DCHECK(lock_held_);
      lock_held_ = false;
#endif
    }

   private:
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
    bool is_main_thread_;
    bool lock_held_;
#endif
  };

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
  };
};

}  // namespace pp

#endif  // PPAPI_UTILITY_COMPLETION_CALLBACK_FACTORY_THREAD_TRAITS_H_
