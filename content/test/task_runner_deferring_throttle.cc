// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/task_runner_deferring_throttle.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"

namespace content {

TaskRunnerDeferringThrottle::TaskRunnerDeferringThrottle(
    scoped_refptr<base::TaskRunner> task_runner,
    bool defer_start,
    bool defer_redirect,
    bool defer_response,
    NavigationHandle* handle)
    : NavigationThrottle(handle),
      defer_start_(defer_start),
      defer_redirect_(defer_redirect),
      defer_response_(defer_response),
      task_runner_(std::move(task_runner)) {}

TaskRunnerDeferringThrottle::~TaskRunnerDeferringThrottle() = default;

// static
std::unique_ptr<NavigationThrottle> TaskRunnerDeferringThrottle::Create(
    scoped_refptr<base::TaskRunner> task_runner,
    bool defer_start,
    bool defer_redirect,
    bool defer_response,
    NavigationHandle* handle) {
  return base::WrapUnique(
      new TaskRunnerDeferringThrottle(std::move(task_runner), defer_start,
                                      defer_redirect, defer_response, handle));
}

NavigationThrottle::ThrottleCheckResult
TaskRunnerDeferringThrottle::WillStartRequest() {
  return defer_start_ ? DeferToPostTask()
                      : content::NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
TaskRunnerDeferringThrottle::WillRedirectRequest() {
  return defer_redirect_ ? DeferToPostTask()
                         : content::NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
TaskRunnerDeferringThrottle::WillProcessResponse() {
  return defer_response_ ? DeferToPostTask()
                         : content::NavigationThrottle::PROCEED;
}

const char* TaskRunnerDeferringThrottle::GetNameForLogging() {
  return "TaskRunnerDeferringThrottle";
}

void TaskRunnerDeferringThrottle::AsyncResume() {
  Resume();
}

NavigationThrottle::ThrottleCheckResult
TaskRunnerDeferringThrottle::DeferToPostTask() {
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::DoNothing(),
      base::BindOnce(&TaskRunnerDeferringThrottle::AsyncResume,
                     weak_factory_.GetWeakPtr()));

  return NavigationThrottle::DEFER;
}

}  // namespace content
