// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CALLBACK_TIMEOUT_HELPERS_H_
#define MEDIA_BASE_CALLBACK_TIMEOUT_HELPERS_H_

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

// This file provides callback wrappers to help handle the case where a
// base::OnceCallback has not run before a given timeout. This can help identify
// programming errors (e.g. a callback was saved without ever running), as well
// as performance issues (e.g. slow operation).
//
// The timeout timer starts when the callback is wrapped. If the wrapped
// callback runs before the timeout, the original callback will run exactly the
// same as if without wrapping. Otherwise:
// - `WrapCallbackWithTimeoutHandler`: On timeout, the `timeout_callback` runs.
// WARNING: The original callback is not affected: it will still run if the
// wrapped callback runs after the timeout. Do NOT handle the result in both
// the original callback and the `timeout_callback`.
// - `WrapCallbackWithDefaultInvokeIfTimeout`: On timeout, the original callback
// runs with the default arguments passed when wrapping the callback. It will be
// a no-op if the wrapped callback runs after the timeout.
//
// Example:
//   // If `OnResult()` doesn't run after 10 seconds, it'll be run with "false".
//   foo->DoWorkAndReturnResult(
//       WrapCallbackWithDefaultInvokeIfTimeout(
//           base::BindOnce(&Foo::OnResult, this), base::Seconds(10), false));
//
//   // If `OnResult()` doesn't run after 10 seconds, LogError() will run.
//   // `OnResult()` may still run after that.
//   foo->DoWorkAndReturnResult(
//       WrapCallbackWithTimeoutHandler(
//           base::BindOnce(&Foo::OnResult, this),
//           base::BindOnce(&Foo::LogError, this, TIMEOUT)));

namespace media {

// Enum class for reporting callback timeout status to UMA.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CallbackTimeoutStatus {
  kCreate = 0,
  kTimeout = 1,
  kDestructedBeforeTimeout = 2,
  kMaxValue = kDestructedBeforeTimeout,
};

// Callback time for the timeout handler in `WrapCallbackWithTimeoutHandler`.
// If `called_on_destruction` is true, the timeout callback was called because
// the original callback was destructed without running. Otherwise, it was
// called because the original callback timed out.
using TimeoutCallback = base::OnceCallback<void(bool called_on_destruction)>;

namespace internal {

// First, tell the compiler CallbackWithTimeoutHelper is a class template with
// one type parameter. Then define specializations where the type is a function
// returning void and taking zero or more arguments.
template <typename Signature>
class CallbackWithTimeoutHelper;

// Only support callbacks that return void because otherwise it is odd to call
// the callback in the destructor and drop the return value immediately.
template <typename... Args>
class CallbackWithTimeoutHelper<void(Args...)> {
 public:
  using CallbackType = base::OnceCallback<void(Args...)>;

  // Bound arguments may be different to the callback signature when wrappers
  // are used, e.g. in base::Owned and base::Unretained case, they are
  // OwnedWrapper and UnretainedWrapper. Use BoundArgs to help handle this.
  template <typename... BoundArgs>
  explicit CallbackWithTimeoutHelper(CallbackType callback,
                                     base::TimeDelta timeout_delay,
                                     BoundArgs&&... args)
      : callback_(std::move(callback)) {
    timeout_callback_ = base::BindOnce(
        &CallbackWithTimeoutHelper::RunWithReason, base::Unretained(this),
        std::forward<BoundArgs>(args)...);
    ScheduleTimeoutCallback(timeout_delay);
  }

  // The first int param acts to disambiguate this constructor from the template
  // constructor above. The precedent is C++'s own operator++(int) vs
  // operator++() to distinguish post-increment and pre-increment.
  CallbackWithTimeoutHelper(int ignored,
                            CallbackType callback,
                            base::TimeDelta timeout_delay,
                            TimeoutCallback timeout_callback)
      : callback_(std::move(callback)),
        timeout_callback_(std::move(timeout_callback)) {
    ScheduleTimeoutCallback(timeout_delay);
  }

  CallbackWithTimeoutHelper(const CallbackWithTimeoutHelper&) = delete;
  CallbackWithTimeoutHelper& operator=(const CallbackWithTimeoutHelper&) =
      delete;

  ~CallbackWithTimeoutHelper() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // If the callback is destructed without running, it will time out for sure.
    // Therefore, fire the timeout closure now.
    if (timeout_callback_)
      std::move(timeout_callback_).Run(/*called_on_destruction=*/true);
  }

  void Run(Args... args) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    timeout_callback_.Reset();
    // The callback could already run as part of `OnTimeout()`.
    if (callback_)
      std::move(callback_).Run(std::forward<Args>(args)...);
  }

 private:
  void RunWithReason(Args... args, bool /*called_on_destruction*/) {
    Run(std::forward<Args>(args)...);
  }

  void ScheduleTimeoutCallback(base::TimeDelta timeout_delay) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CallbackWithTimeoutHelper::OnTimeout,
                       weak_factory_.GetWeakPtr()),
        timeout_delay);
  }

  void OnTimeout() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (timeout_callback_)
      std::move(timeout_callback_).Run(/*called_on_destruction=*/false);
  }

  CallbackType callback_;
  TimeoutCallback timeout_callback_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CallbackWithTimeoutHelper> weak_factory_{this};
};

}  // namespace internal

template <typename T, typename... Args>
inline base::OnceCallback<T> WrapCallbackWithTimeoutHandler(
    base::OnceCallback<T> callback,
    base::TimeDelta timeout_delay,
    TimeoutCallback timeout_callback) {
  return base::BindOnce(
      &internal::CallbackWithTimeoutHelper<T>::Run,
      std::make_unique<internal::CallbackWithTimeoutHelper<T>>(
          /*ignored=*/0, std::move(callback), timeout_delay,
          std::move(timeout_callback)));
}

template <typename T, typename... Args>
inline base::OnceCallback<T> WrapCallbackWithDefaultInvokeIfTimeout(
    base::OnceCallback<T> callback,
    base::TimeDelta timeout_delay,
    Args&&... args) {
  return base::BindOnce(
      &internal::CallbackWithTimeoutHelper<T>::Run,
      std::make_unique<internal::CallbackWithTimeoutHelper<T>>(
          std::move(callback), timeout_delay, std::forward<Args>(args)...));
}

}  // namespace media

#endif  // MEDIA_BASE_CALLBACK_TIMEOUT_HELPERS_H_
