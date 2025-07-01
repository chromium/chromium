// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_THREAD_AWARE_CALLBACK_H_
#define PPAPI_SHARED_IMPL_THREAD_AWARE_CALLBACK_H_

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"
#include "ppapi/shared_impl/proxy_lock.h"

namespace ppapi {

class MessageLoopShared;

namespace internal {

class PPAPI_SHARED_EXPORT ThreadAwareCallbackBase {
 public:
  ThreadAwareCallbackBase(const ThreadAwareCallbackBase&) = delete;
  ThreadAwareCallbackBase& operator=(const ThreadAwareCallbackBase&) = delete;

 protected:
  ThreadAwareCallbackBase();
  ~ThreadAwareCallbackBase();

  static bool HasTargetLoop();

  void InternalRunOnTargetThread(base::OnceClosure closure);

 private:
  class Core;

  scoped_refptr<MessageLoopShared> target_loop_;
  scoped_refptr<Core> core_;
};

}  // namespace internal

// Some PPB interfaces have methods that set a custom callback. Usually, the
// callback has to be called on the same thread as the one it was set on.
// ThreadAwareCallback keeps track of the target thread, and posts a task to run
// on it if requested from a different thread.
//
// Please note that:
// - Unlike TrackedCallback, there is no restriction on how many times the
//   callback will be called.
// - When a ThreadAwareCallback object is destroyed, all pending tasks to run
//   the callback will be ignored. It is designed this way so that when the
//   resource is destroyed or the callback is cancelled by the plugin, we can
//   simply delete the ThreadAwareCallback object to prevent touching the
//   callback later.
// - When RunOnTargetThread() is called on the target thread, the callback runs
//   immediately.
template <class FuncType>
class ThreadAwareCallback : public internal::ThreadAwareCallbackBase {
 public:
  // The caller takes ownership of the returned object.
  // NULL is returned if the current thread doesn't have an associated Pepper
  // message loop, or |func| is NULL.
  static ThreadAwareCallback* Create(FuncType func) {
    if (!func || !HasTargetLoop())
      return NULL;
    return new ThreadAwareCallback(func);
  }

  ~ThreadAwareCallback() {}

  void RunOnTargetThread() { InternalRunOnTargetThread(base::BindOnce(func_)); }

  template <class P1>
  void RunOnTargetThread(const P1& p1) {
    InternalRunOnTargetThread(base::BindOnce(func_, p1));
  }

  template <class P1, class P2>
  void RunOnTargetThread(const P1& p1, const P2& p2) {
    InternalRunOnTargetThread(base::BindOnce(func_, p1, p2));
  }

  template <class P1, class P2, class P3>
  void RunOnTargetThread(const P1& p1, const P2& p2, const P3& p3) {
    InternalRunOnTargetThread(base::BindOnce(func_, p1, p2, p3));
  }

  template <class P1, class P2, class P3, class P4>
  void RunOnTargetThread(const P1& p1,
                         const P2& p2,
                         const P3& p3,
                         const P4& p4) {
    InternalRunOnTargetThread(base::BindOnce(func_, p1, p2, p3, p4));
  }

  template <class P1, class P2, class P3, class P4, class P5>
  void RunOnTargetThread(const P1& p1,
                         const P2& p2,
                         const P3& p3,
                         const P4& p4,
                         const P5& p5) {
    InternalRunOnTargetThread(base::BindOnce(func_, p1, p2, p3, p4, p5));
  }

 private:
  explicit ThreadAwareCallback(FuncType func) : func_(func) {}

  FuncType func_;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_THREAD_AWARE_CALLBACK_H_
