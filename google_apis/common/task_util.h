// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_COMMON_TASK_UTIL_H_
#define GOOGLE_APIS_COMMON_TASK_UTIL_H_

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"

namespace google_apis {

// Runs the task with the task runner.
void RunTaskWithTaskRunner(scoped_refptr<base::TaskRunner> task_runner,
                           base::OnceClosure task);

namespace internal {

// Implementation of the composed callback, whose signature is |Sig|.
template <typename Sig>
struct ComposedCallback;

template <typename... Args>
struct ComposedCallback<void(Args...)> {
  static void Run(base::OnceCallback<void(base::OnceClosure)> runner,
                  base::OnceCallback<void(Args...)> callback,
                  Args... args) {
    std::move(runner).Run(
        base::BindOnce(std::move(callback), std::forward<Args>(args)...));
  }
};

}  // namespace internal

// Returns callback that takes arguments (arg1, arg2, ...), create a closure
// by binding them to |callback|, and runs |runner| with the closure.
// I.e. the returned callback works as follows:
//   runner.Run(Bind(callback, arg1, arg2, ...))
template <typename... Args>
base::OnceCallback<void(Args...)> CreateComposedCallback(
    base::OnceCallback<void(base::OnceClosure)> runner,
    base::OnceCallback<void(Args...)> callback) {
  DCHECK(runner);
  DCHECK(callback);
  return base::BindOnce(&internal::ComposedCallback<void(Args...)>::Run,
                        std::move(runner), std::move(callback));
}

template <typename... Args>
base::RepeatingCallback<void(Args...)> CreateComposedCallback(
    base::RepeatingCallback<void(base::OnceClosure)> runner,
    base::RepeatingCallback<void(Args...)> callback) {
  DCHECK(runner);
  DCHECK(callback);
  return base::BindRepeating(&internal::ComposedCallback<void(Args...)>::Run,
                             std::move(runner), std::move(callback));
}

// Returns callback which runs the given |callback| on the current thread.
//
// TODO(tzik): If the resulting callback is destroyed without invocation, its
// |callback| and its bound arguments can be destroyed on the originating
// thread.
//
// TODO(tzik): The parameter of the resulting callback will be forwarded to
// the destination, but it doesn't mirror a wrapper. I.e. In an example below:
//   auto cb1 = base::BindOnce([](int* p) { /* |p| is dangling. */ });
//   auto cb2 = CreateRelayCallback(std::move(cb1));
//   base::BindOnce(std::move(cb2), base::Owned(new int)).Run();
// CreateRelayCallback forwards the callback invocation without base::Owned,
// and forwarded pointer will be dangling in this case.
//
// TODO(tzik): Take FROM_HERE from the caller, and propagate it to the runner.
//
// TODO(crbug.com/40254958): Replace with base::BindPostTaskToCurrentDefault.
template <typename Sig>
base::OnceCallback<Sig> CreateRelayCallback(base::OnceCallback<Sig> callback) {
  return CreateComposedCallback(
      base::BindOnce(&RunTaskWithTaskRunner,
                     base::SingleThreadTaskRunner::GetCurrentDefault()),
      std::move(callback));
}

template <typename Sig>
base::RepeatingCallback<Sig> CreateRelayCallback(
    base::RepeatingCallback<Sig> callback) {
  return CreateComposedCallback(
      base::BindRepeating(&RunTaskWithTaskRunner,
                          base::SingleThreadTaskRunner::GetCurrentDefault()),
      std::move(callback));
}

}  // namespace google_apis

#endif  // GOOGLE_APIS_COMMON_TASK_UTIL_H_
