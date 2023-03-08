// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PROXY_LOCK_H_
#define PPAPI_SHARED_IMPL_PROXY_LOCK_H_

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/threading/thread_checker.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace base {
class Lock;
}

namespace content {
class HostGlobals;
}

namespace ppapi {

// This is the one lock to rule them all for the ppapi proxy. All PPB interface
// functions that need to be synchronized should lock this lock on entry. This
// is normally accomplished by using an appropriate Enter RAII object at the
// beginning of each thunk function.
//
// TODO(dmichael): If this turns out to be too slow and contentious, we'll want
// to use multiple locks. E.g., one for the var tracker, one for the resource
// tracker, etc.
class PPAPI_SHARED_EXPORT ProxyLock {
 public:
  ProxyLock() = delete;
  ProxyLock(const ProxyLock&) = delete;
  ProxyLock& operator=(const ProxyLock&) = delete;

  // Return the global ProxyLock. Normally, you should not access this
  // directly but instead use ProxyAutoLock or ProxyAutoUnlock. But sometimes
  // you need access to the ProxyLock, for example to create a condition
  // variable.
  static base::Lock* Get();

  // Acquire the proxy lock. If it is currently held by another thread, block
  // until it is available. If the lock has not been set using the 'Set' method,
  // this operation does nothing. That is the normal case for the host side;
  // see PluginResourceTracker for where the lock gets set for the out-of-
  // process plugin case.
  static void Acquire();
  // Relinquish the proxy lock. If the lock has not been set, this does nothing.
  static void Release();

  // Assert that the lock is owned by the current thread (in the plugin
  // process). Does nothing when running in-process (or in the host process).
  static void AssertAcquired();
  static void AssertAcquiredDebugOnly() {
#ifndef NDEBUG
    AssertAcquired();
#endif
  }

  // We have some unit tests where one thread pretends to be the host and one
  // pretends to be the plugin. This allows the lock to do nothing on only one
  // thread to support these tests. See TwoWayTest for more information.
  class PPAPI_SHARED_EXPORT LockingDisablerForTest {
   public:
    LockingDisablerForTest();
    ~LockingDisablerForTest() = default;

   private:
    const base::AutoReset<bool> resetter_;
  };

 private:
  friend class content::HostGlobals;
  // On the host side, we do not lock. This must be called at most once at
  // startup, before other threads that may access the ProxyLock have had a
  // chance to run.
  static void DisableLocking();
};

// A simple RAII class for locking the PPAPI proxy lock on entry and releasing
// on exit. This is for simple interfaces that don't use the 'thunk' system,
// such as PPB_Var and PPB_Core.
class ProxyAutoLock {
 public:
  ProxyAutoLock() { ProxyLock::Acquire(); }

  ProxyAutoLock(const ProxyAutoLock&) = delete;
  ProxyAutoLock& operator=(const ProxyAutoLock&) = delete;

  ~ProxyAutoLock() { ProxyLock::Release(); }
};

// The inverse of the above; unlock on construction, lock on destruction. This
// is useful for calling out to the plugin, when we need to unlock but ensure
// that we re-acquire the lock when the plugin is returns or raises an
// exception.
class ProxyAutoUnlock {
 public:
  ProxyAutoUnlock() { ProxyLock::Release(); }

  ProxyAutoUnlock(const ProxyAutoUnlock&) = delete;
  ProxyAutoUnlock& operator=(const ProxyAutoUnlock&) = delete;

  ~ProxyAutoUnlock() { ProxyLock::Acquire(); }
};

// A set of function template overloads for invoking a function pointer while
// the ProxyLock is unlocked. This assumes that the luck is held.
// CallWhileUnlocked unlocks the ProxyLock just before invoking the given
// function. The lock is immediately re-acquired when the invoked function
// function returns. CallWhileUnlocked returns whatever the given function
// returned.
//
// Example usage:
//   *result = CallWhileUnlocked(ppp_input_event_impl_->HandleInputEvent,
//                               instance,
//                               resource->pp_resource());
template <class ReturnType>
ReturnType CallWhileUnlocked(ReturnType (*function)()) {
  ProxyAutoUnlock unlock;
  return function();
}
// Note we use 2 types for the params, even though for the most part we expect
// A1 to match P1. We let the compiler determine if P1 can convert safely to
// A1. This allows callers to avoid having to do things like
// const_cast to add const.
template <class ReturnType, class A1, class P1>
ReturnType CallWhileUnlocked(ReturnType (*function)(A1), const P1& p1) {
  ProxyAutoUnlock unlock;
  return function(p1);
}
template <class ReturnType, class A1, class A2, class P1, class P2>
ReturnType CallWhileUnlocked(ReturnType (*function)(A1, A2),
                             const P1& p1,
                             const P2& p2) {
  ProxyAutoUnlock unlock;
  return function(p1, p2);
}
template <class ReturnType, class A1, class A2, class A3, class P1, class P2,
          class P3>
ReturnType CallWhileUnlocked(ReturnType (*function)(A1, A2, A3),
                             const P1& p1,
                             const P2& p2,
                             const P3& p3) {
  ProxyAutoUnlock unlock;
  return function(p1, p2, p3);
}
template <class ReturnType, class A1, class A2, class A3, class A4, class P1,
          class P2, class P3, class P4>
ReturnType CallWhileUnlocked(ReturnType (*function)(A1, A2, A3, A4),
                             const P1& p1,
                             const P2& p2,
                             const P3& p3,
                             const P4& p4) {
  ProxyAutoUnlock unlock;
  return function(p1, p2, p3, p4);
}
template <class ReturnType, class A1, class A2, class A3, class A4, class A5,
          class P1, class P2, class P3, class P4, class P5>
ReturnType CallWhileUnlocked(ReturnType (*function)(A1, A2, A3, A4, A5),
                             const P1& p1,
                             const P2& p2,
                             const P3& p3,
                             const P4& p4,
                             const P5& p5) {
  ProxyAutoUnlock unlock;
  return function(p1, p2, p3, p4, p5);
}
void PPAPI_SHARED_EXPORT CallWhileUnlocked(base::OnceClosure closure);

namespace internal {

template <typename RunType>
class RunWhileLockedHelper;

// A helper class to ensure that a callback is always run and destroyed while
// the ProxyLock is held. A callback that is bound with ref-counted Var or
// Resource parameters may invoke methods on the VarTracker or the
// ResourceTracker in its destructor, and these require the ProxyLock.
template <>
class RunWhileLockedHelper<void()> {
 public:
  typedef base::OnceCallback<void()> CallbackType;
  explicit RunWhileLockedHelper(CallbackType callback)
      : callback_(std::move(callback)) {
    // CallWhileLocked and destruction might happen on a different thread from
    // creation.
    thread_checker_.DetachFromThread();
  }
  static void CallWhileLocked(std::unique_ptr<RunWhileLockedHelper> ptr) {
    // Bind thread_checker_ to this thread so we can check in the destructor.
    // *If* the callback gets invoked, it's important that RunWhileLockedHelper
    // is destroyed on the same thread (see the comments in the destructor).
    DCHECK(ptr->thread_checker_.CalledOnValidThread());
    ProxyAutoLock lock;

    // Relax the cross-thread access restriction to non-thread-safe RefCount.
    // |lock| above protects the access to Resource instances.
    base::ScopedAllowCrossThreadRefCountAccess
        allow_cross_thread_ref_count_access;

    {
      // Use a scope and local Callback to ensure that the callback is cleared
      // before the lock is released, even in the unlikely event that Run()
      // throws an exception.
      CallbackType temp_callback = std::move(ptr->callback_);
      std::move(temp_callback).Run();
    }
  }

  RunWhileLockedHelper(const RunWhileLockedHelper&) = delete;
  RunWhileLockedHelper& operator=(const RunWhileLockedHelper&) = delete;

  ~RunWhileLockedHelper() {
    // Check that the Callback is destroyed on the same thread as where
    // CallWhileLocked happened if CallWhileLocked happened. If we weren't
    // invoked, thread_checked_ isn't bound to a thread.
    DCHECK(thread_checker_.CalledOnValidThread());
    // Here we read callback_ without the lock. This is why the callback must be
    // destroyed on the same thread where it runs. Note that callback_ will be
    // NULL if it has already been run via CallWhileLocked. In this case,
    // there's no need to acquire the lock, because we don't touch any shared
    // data.
    if (callback_) {
      // If the callback was *not* run, we're in a case where the task queue
      // we got pushed to has been destroyed (e.g., the thread is shut down and
      // its MessageLoop destroyed before all tasks have run.)
      //
      // We still need to have the lock when we destroy the callback:
      // - Because Resource and Var inherit RefCounted (not
      //   ThreadSafeRefCounted).
      // - Because if the callback owns the last ref to a Resource, it will
      //   call the ResourceTracker and also the Resource's destructor, which
      //   both require the ProxyLock.
      ProxyAutoLock lock;

      // Relax the cross-thread access restriction to non-thread-safe RefCount.
      // |lock| above protects the access to Resource instances.
      base::ScopedAllowCrossThreadRefCountAccess
          allow_cross_thread_ref_count_access;

      callback_.Reset();
    }
  }

 private:
  CallbackType callback_;

  // Used to ensure that the Callback is run and deleted on the same thread.
  base::ThreadChecker thread_checker_;
};

template <typename P1>
class RunWhileLockedHelper<void(P1)> {
 public:
  typedef base::OnceCallback<void(P1)> CallbackType;
  explicit RunWhileLockedHelper(CallbackType callback)
      : callback_(std::move(callback)) {
    thread_checker_.DetachFromThread();
  }
  static void CallWhileLocked(std::unique_ptr<RunWhileLockedHelper> ptr,
                              P1 p1) {
    DCHECK(ptr->thread_checker_.CalledOnValidThread());
    ProxyAutoLock lock;
    // Relax the cross-thread access restriction to non-thread-safe RefCount.
    // |lock| above protects the access to Resource instances.
    base::ScopedAllowCrossThreadRefCountAccess
        allow_cross_thread_ref_count_access;
    {
      CallbackType temp_callback = std::move(ptr->callback_);
      std::move(temp_callback).Run(p1);
    }
  }

  RunWhileLockedHelper(const RunWhileLockedHelper&) = delete;
  RunWhileLockedHelper& operator=(const RunWhileLockedHelper&) = delete;

  ~RunWhileLockedHelper() {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (callback_) {
      ProxyAutoLock lock;
      // Relax the cross-thread access restriction to non-thread-safe RefCount.
      // |lock| above protects the access to Resource instances.
      base::ScopedAllowCrossThreadRefCountAccess
          allow_cross_thread_ref_count_access;
      callback_.Reset();
    }
  }

 private:
  CallbackType callback_;
  base::ThreadChecker thread_checker_;
};

template <typename P1, typename P2>
class RunWhileLockedHelper<void(P1, P2)> {
 public:
  typedef base::OnceCallback<void(P1, P2)> CallbackType;
  explicit RunWhileLockedHelper(CallbackType callback)
      : callback_(std::move(callback)) {
    thread_checker_.DetachFromThread();
  }
  static void CallWhileLocked(std::unique_ptr<RunWhileLockedHelper> ptr,
                              P1 p1,
                              P2 p2) {
    DCHECK(ptr->thread_checker_.CalledOnValidThread());
    ProxyAutoLock lock;
    // Relax the cross-thread access restriction to non-thread-safe RefCount.
    // |lock| above protects the access to Resource instances.
    base::ScopedAllowCrossThreadRefCountAccess
        allow_cross_thread_ref_count_access;
    {
      CallbackType temp_callback = std::move(ptr->callback_);
      std::move(temp_callback).Run(p1, p2);
    }
  }

  RunWhileLockedHelper(const RunWhileLockedHelper&) = delete;
  RunWhileLockedHelper& operator=(const RunWhileLockedHelper&) = delete;

  ~RunWhileLockedHelper() {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (callback_) {
      ProxyAutoLock lock;
      // Relax the cross-thread access restriction to non-thread-safe RefCount.
      // |lock| above protects the access to Resource instances.
      base::ScopedAllowCrossThreadRefCountAccess
          allow_cross_thread_ref_count_access;
      callback_.Reset();
    }
  }

 private:
  CallbackType callback_;
  base::ThreadChecker thread_checker_;
};

template <typename P1, typename P2, typename P3>
class RunWhileLockedHelper<void(P1, P2, P3)> {
 public:
  typedef base::OnceCallback<void(P1, P2, P3)> CallbackType;
  explicit RunWhileLockedHelper(CallbackType callback)
      : callback_(std::move(callback)) {
    thread_checker_.DetachFromThread();
  }
  static void CallWhileLocked(std::unique_ptr<RunWhileLockedHelper> ptr,
                              P1 p1,
                              P2 p2,
                              P3 p3) {
    DCHECK(ptr->thread_checker_.CalledOnValidThread());
    ProxyAutoLock lock;
    // Relax the cross-thread access restriction to non-thread-safe RefCount.
    // |lock| above protects the access to Resource instances.
    base::ScopedAllowCrossThreadRefCountAccess
        allow_cross_thread_ref_count_access;
    {
      CallbackType temp_callback = std::move(ptr->callback_);
      std::move(temp_callback).Run(p1, p2, p3);
    }
  }

  RunWhileLockedHelper(const RunWhileLockedHelper&) = delete;
  RunWhileLockedHelper& operator=(const RunWhileLockedHelper&) = delete;

  ~RunWhileLockedHelper() {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (callback_) {
      ProxyAutoLock lock;
      // Relax the cross-thread access restriction to non-thread-safe RefCount.
      // |lock| above protects the access to Resource instances.
      base::ScopedAllowCrossThreadRefCountAccess
          allow_cross_thread_ref_count_access;
      callback_.Reset();
    }
  }

 private:
  CallbackType callback_;
  base::ThreadChecker thread_checker_;
};

}  // namespace internal

// RunWhileLocked wraps the given Callback in a new Callback that, when invoked:
//  1) Locks the ProxyLock.
//  2) Runs the original Callback (forwarding arguments, if any).
//  3) Clears the original Callback (while the lock is held).
//  4) Unlocks the ProxyLock.
// Note that it's important that the callback is cleared in step (3), in case
// clearing the Callback causes a destructor (e.g., for a Resource) to run,
// which should hold the ProxyLock to avoid data races.
//
// This is for cases where you want to run a task or store a Callback, but you
// want to ensure that the ProxyLock is acquired for the duration of the task
// that the Callback runs.
// EXAMPLE USAGE:
// GetMainThreadMessageLoop()->PostDelayedTask(
//     FROM_HERE,
//     RunWhileLocked(
//         base::BindOnce(&CallbackWrapper, std::move(callback), result)),
//     delay_in_ms);
//
// In normal usage like the above, this all should "just work". However, if you
// do something unusual, you may get a runtime crash due to deadlock. Here are
// the ways that the returned Callback must be used to avoid a deadlock:
// (1) copied to another Callback. After that, the original callback can be
// destroyed with or without the proxy lock acquired, while the newly assigned
// callback has to conform to these same restrictions. Or
// (2) run without proxy lock acquired (e.g., being posted to a MessageLoop
// and run there). The callback must be destroyed on the same thread where it
// was run (but can be destroyed with or without the proxy lock acquired). Or
// (3) destroyed without the proxy lock acquired.
template <class FunctionType>
inline base::OnceCallback<FunctionType> RunWhileLocked(
    base::OnceCallback<FunctionType> callback) {
  // NOTE: the reason we use "scoped_ptr" here instead of letting the callback
  // own it via base::Owned is kind of subtle. Imagine for the moment that we
  // call RunWhileLocked without the ProxyLock:
  // {
  //   base::OnceCallback<void ()> local_callback = base::BinOnced(&Foo);
  //   some_task_runner.PostTask(FROM_HERE,
  //                             RunWhileLocked(std::move(local_callback)));
  // }
  // In this case, since we don't have a lock synchronizing us, it's possible
  // for the callback to run on the other thread before we return and destroy
  // |local_callback|. The important thing here is that even though the other
  // thread gets a copy of the callback, the internal "BindState" of the
  // callback is refcounted and shared between all copies of the callback. So
  // in that case, if we used base::Owned, we might delete RunWhileLockedHelper
  // on this thread, which will violate the RunWhileLockedHelper's assumption
  // that it is destroyed on the same thread where it is run.
  std::unique_ptr<internal::RunWhileLockedHelper<FunctionType>> helper(
      new internal::RunWhileLockedHelper<FunctionType>(std::move(callback)));
  return base::BindOnce(
      &internal::RunWhileLockedHelper<FunctionType>::CallWhileLocked,
      std::move(helper));
}

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PROXY_LOCK_H_
