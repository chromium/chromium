// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TASK_RUNNER_DEFERRING_THROTTLE_H_
#define CONTENT_TEST_TASK_RUNNER_DEFERRING_THROTTLE_H_

#include "base/test/test_simple_task_runner.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

// This class defers a navigation via a no-op async task on the provided task
// runner.
class TaskRunnerDeferringThrottle : public NavigationThrottle {
 public:
  TaskRunnerDeferringThrottle(scoped_refptr<base::TaskRunner> task_runner,
                              bool defer_start,
                              bool defer_redirect,
                              bool defer_response,
                              NavigationHandle* handle);

  TaskRunnerDeferringThrottle(const TaskRunnerDeferringThrottle&) = delete;
  TaskRunnerDeferringThrottle& operator=(const TaskRunnerDeferringThrottle&) =
      delete;

  ~TaskRunnerDeferringThrottle() override;

  static std::unique_ptr<NavigationThrottle> Create(
      scoped_refptr<base::TaskRunner> task_runner,
      bool defer_start,
      bool defer_redirect,
      bool defer_response,
      NavigationHandle* handle);

  // NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 protected:
  // Tests may override this to change how/when Resume() is called.
  virtual void AsyncResume();

 private:
  ThrottleCheckResult DeferToPostTask();

  bool defer_start_ = true;
  bool defer_redirect_ = true;
  bool defer_response_ = true;
  scoped_refptr<base::TaskRunner> task_runner_;
  base::WeakPtrFactory<TaskRunnerDeferringThrottle> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_TEST_TASK_RUNNER_DEFERRING_THROTTLE_H_
