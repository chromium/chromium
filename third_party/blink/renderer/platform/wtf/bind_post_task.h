// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_BIND_POST_TASK_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_BIND_POST_TASK_H_

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

// See base::BindPostTask() for docs, this is the WTF cross-thread version.

namespace blink {

namespace internal {

// Based on base::internal::BindPostTaskTrampoline with modifications for
// CrossThreadFunction/CrossThreadOnceFunction. CrossThreadOnceFunction works
// identically to the base version, but CrossThreadFunction is not copyable or
// convertible to a CrossThreadOnceFunction, so a task must be posted to run it
// instead of directly passing it to `task_runner`. Safety is guaranteed by
// ensuring CrossThreadBindPostTaskTrampoline is destructed on `task_runner`.
template <typename CallbackType>
class CrossThreadBindPostTaskTrampoline {
 public:
  CrossThreadBindPostTaskTrampoline(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::Location& location,
      CallbackType callback)
      : task_runner_(std::move(task_runner)),
        location_(location),
        callback_(std::move(callback)) {
    DCHECK(task_runner_);
    CHECK(callback_);
  }
  ~CrossThreadBindPostTaskTrampoline() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
  }

  CrossThreadBindPostTaskTrampoline(
      const CrossThreadBindPostTaskTrampoline& other) = delete;
  CrossThreadBindPostTaskTrampoline& operator=(
      const CrossThreadBindPostTaskTrampoline& other) = delete;

  template <typename... Args>
  void RunOnce(Args... args) {
    task_runner_->PostTask(
        location_, ConvertToBaseOnceCallback(CrossThreadBindOnce(
                       std::move(callback_), std::forward<Args>(args)...)));
  }

  template <typename... Args>
  void RunRepeating(Args... args) {
    // Safe since the destruction of `this` is posted to `task_runner_`.
    task_runner_->PostTask(
        location_,
        ConvertToBaseOnceCallback(CrossThreadBindOnce(
            &RunOnTaskRunner<Args...>, CrossThreadUnretained(&callback_),
            std::forward<Args>(args)...)));
  }

 private:
  template <typename... Args>
  static void RunOnTaskRunner(CallbackType* callback, Args... args) {
    callback->Run(std::forward<Args>(args)...);
  }

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const base::Location location_;
  CallbackType callback_;
};

}  // namespace internal

template <typename ReturnType, typename... Args>
  requires std::is_void_v<ReturnType>
CrossThreadOnceFunction<void(Args...)> BindPostTask(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    CrossThreadOnceFunction<ReturnType(Args...)> callback,
    const base::Location& location = FROM_HERE) {
  using Helper = internal::CrossThreadBindPostTaskTrampoline<
      CrossThreadOnceFunction<void(Args...)>>;

  std::unique_ptr<Helper, base::OnTaskRunnerDeleter> helper(
      new Helper(task_runner, location, std::move(callback)),
      base::OnTaskRunnerDeleter(task_runner));
  return CrossThreadBindOnce(&Helper::template RunOnce<Args...>,
                             std::move(helper));
}

template <typename ReturnType, typename... Args>
  requires std::is_void_v<ReturnType>
CrossThreadFunction<void(Args...)> BindPostTask(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    CrossThreadFunction<ReturnType(Args...)> callback,
    const base::Location& location = FROM_HERE) {
  using Helper = internal::CrossThreadBindPostTaskTrampoline<
      CrossThreadFunction<void(Args...)>>;

  std::unique_ptr<Helper, base::OnTaskRunnerDeleter> helper(
      new Helper(task_runner, location, std::move(callback)),
      base::OnTaskRunnerDeleter(task_runner));
  return CrossThreadBindRepeating(&Helper::template RunRepeating<Args...>,
                                  std::move(helper));
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_BIND_POST_TASK_H_
