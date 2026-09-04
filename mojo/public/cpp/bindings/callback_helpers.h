// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_CALLBACK_HELPERS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_CALLBACK_HELPERS_H_

#include <memory>
#include <type_traits>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"

// This is a helper utility to wrap a base::OnceCallback such that if the
// callback is destructed before it has a chance to run (e.g. the callback is
// bound into a task and the task is dropped), it will be run with the
// default arguments passed into WrapCallbackWithDefaultInvokeIfNotRun.
// Alternatively, it will invoke the default invoke callback passed to
// WrapCallbackWithDefaultInvokeCallbackIfNotRun, or run the delete closure
// passed to WrapCallbackWithDropHandler.
//
// These helpers are intended for use on the client side of a mojo interface,
// where users want to know if their individual callback was dropped (e.g.
// due to connection error). This can save the burden of tracking pending
// mojo callbacks in a map so they can be cleaned up in the interface's
// connection error callback.
//
// Caveats:
// 1) The default form of the callback (the one called when the original is
//    dropped before running) will run on the thread where the callback's
//    destructor runs - it may not run on the thread you expected. If this is a
//    problem for your code, DO NOT USE these helpers. This is *not* a problem
//    for callbacks for mojo asynchronous methods, because in this case the
//    callback is run and destroyed on the same thread - the thread that
//    mojo::Remote is bound to.
// 2) There is no type information that indicates the wrapped object has special
//    destructor behavior.  It is therefore not recommended to pass these
//    wrapped callbacks into deep call graphs where code readers could be
//    confused whether or not the Run() method should be invoked.
//
// Example:
//   foo->DoWorkAndReturnResult(
//       WrapCallbackWithDefaultInvokeIfNotRun(
//           base::BindOnce(&Foo::OnResult, this), false));
//
// If the callback is destructed without running, it'll be run with "false".
//
//  foo->DoWorkAndReturnResult(
//      WrapCallbackWithDropHandler(base::BindOnce(&Foo::OnResult, this),
//                         base::BindOnce(&Foo::LogError, this, WAS_DROPPED)));
//
// If the callback is destructed without running, it'll run the delete closure.
//
//  foo->DoWorkAndReturnResult(
//      WrapCallbackWithDefaultInvokeCallbackIfNotRun(
//          base::BindOnce(&Foo::OnResultWithImmobile, this),
//          base::BindOnce([](base::OnceCallback<void(const Immobile&)> cb) {
//            Immobile default_val;
//            std::move(cb).Run(default_val);
//          })));
//
// If the callback is destructed without running, it'll invoke the default
// invoke callback with the original callback.

namespace mojo {
namespace internal {

// First, tell the compiler CallbackWithDeleteHelper is a class template with
// one type parameter. Then define specializations where the type is a function
// returning void and taking zero or more arguments.
template <typename Signature>
class CallbackWithDeleteHelper;

// Only support callbacks that return void because otherwise it is odd to call
// the callback in the destructor and drop the return value immediately.
template <typename... Args>
class CallbackWithDeleteHelper<void(Args...)> {
 public:
  using CallbackType = base::OnceCallback<void(Args...)>;

  // Bound arguments may be different to the callback signature when wrappers
  // are used, e.g. in base::Owned and base::Unretained case, they are
  // OwnedWrapper and UnretainedWrapper. Use BoundArgs to help handle this.
  template <typename... BoundArgs>
  explicit CallbackWithDeleteHelper(CallbackType callback, BoundArgs&&... args)
      : callback_(std::move(callback)) {
    delete_callback_ =
        base::BindOnce(&CallbackWithDeleteHelper::Run, base::Unretained(this),
                       std::forward<BoundArgs>(args)...);
  }

  // The first int param acts to disambiguate this constructor from the template
  // constructor above. The precendent is C++'s own operator++(int) vs
  // operator++() to distinguish post-increment and pre-increment.
  CallbackWithDeleteHelper(int ignored,
                           CallbackType callback,
                           base::OnceClosure delete_callback)
      : callback_(std::move(callback)),
        delete_callback_(std::move(delete_callback)) {}

  CallbackWithDeleteHelper(const CallbackWithDeleteHelper&) = delete;
  CallbackWithDeleteHelper& operator=(const CallbackWithDeleteHelper&) = delete;

  ~CallbackWithDeleteHelper() {
    if (delete_callback_) {
      std::move(delete_callback_).Run();
    }
  }

  void Run(Args... args) {
    delete_callback_.Reset();
    std::move(callback_).Run(std::forward<Args>(args)...);
  }

 private:
  CallbackType callback_;
  base::OnceClosure delete_callback_;
};

template <typename Signature>
class CallbackWithDefaultInvokeCallbackHelper;

// Only support callbacks that return void because otherwise it is odd to call
// the callback in the destructor and drop the return value immediately.
template <typename... Args>
class CallbackWithDefaultInvokeCallbackHelper<void(Args...)> {
 public:
  using CallbackType = base::OnceCallback<void(Args...)>;
  using DefaultInvokeCallback = base::OnceCallback<void(CallbackType)>;

  explicit CallbackWithDefaultInvokeCallbackHelper(
      CallbackType callback,
      DefaultInvokeCallback default_invoke_callback)
      : callback_(std::move(callback)),
        default_invoke_callback_(std::move(default_invoke_callback)) {}

  CallbackWithDefaultInvokeCallbackHelper(
      const CallbackWithDefaultInvokeCallbackHelper&) = delete;
  CallbackWithDefaultInvokeCallbackHelper& operator=(
      const CallbackWithDefaultInvokeCallbackHelper&) = delete;

  ~CallbackWithDefaultInvokeCallbackHelper() {
    if (default_invoke_callback_) {
      std::move(default_invoke_callback_).Run(std::move(callback_));
    }
  }

  void Run(Args... args) {
    default_invoke_callback_.Reset();
    std::move(callback_).Run(std::forward<Args>(args)...);
  }

 private:
  CallbackType callback_;
  DefaultInvokeCallback default_invoke_callback_;
};

}  // namespace internal

template <typename T, typename... Args>
inline base::OnceCallback<T> WrapCallbackWithDropHandler(
    base::OnceCallback<T> cb,
    base::OnceClosure delete_cb) {
  return base::BindOnce(&internal::CallbackWithDeleteHelper<T>::Run,
                        std::make_unique<internal::CallbackWithDeleteHelper<T>>(
                            0, std::move(cb), std::move(delete_cb)));
}

template <typename T, typename... Args>
inline base::OnceCallback<T> WrapCallbackWithDefaultInvokeIfNotRun(
    base::OnceCallback<T> cb,
    Args&&... args) {
  return base::BindOnce(&internal::CallbackWithDeleteHelper<T>::Run,
                        std::make_unique<internal::CallbackWithDeleteHelper<T>>(
                            std::move(cb), std::forward<Args>(args)...));
}

// Wraps `cb` such that if it is destructed before being run,
// `default_invoke_cb` is invoked with `cb`. This is useful when the fallback
// arguments are immobile (e.g. non-copyable and non-movable) and cannot be
// bound eagerly by value into `WrapCallbackWithDefaultInvokeIfNotRun`.
template <typename T>
[[nodiscard]] inline base::OnceCallback<T>
WrapCallbackWithDefaultInvokeCallbackIfNotRun(
    base::OnceCallback<T> cb,
    std::type_identity_t<base::OnceCallback<void(base::OnceCallback<T>)>>
        default_invoke_cb) {
  return base::BindOnce(
      &internal::CallbackWithDefaultInvokeCallbackHelper<T>::Run,
      std::make_unique<internal::CallbackWithDefaultInvokeCallbackHelper<T>>(
          std::move(cb), std::move(default_invoke_cb)));
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_CALLBACK_HELPERS_H_
