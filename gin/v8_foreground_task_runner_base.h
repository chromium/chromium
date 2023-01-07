// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_V8_FOREGROUND_TASK_RUNNER_BASE_H_
#define GIN_V8_FOREGROUND_TASK_RUNNER_BASE_H_

#include "v8/include/v8-platform.h"

#include "gin/gin_export.h"
#include "gin/public/v8_idle_task_runner.h"

namespace gin {

// Base class for the V8ForegroundTaskRunners to share the capability of
// enabling IdleTasks.
class V8ForegroundTaskRunnerBase : public v8::TaskRunner {
 public:
  V8ForegroundTaskRunnerBase();

  ~V8ForegroundTaskRunnerBase() override;

  void EnableIdleTasks(std::unique_ptr<V8IdleTaskRunner> idle_task_runner);

  bool IdleTasksEnabled() override;

 protected:
  V8IdleTaskRunner* idle_task_runner() { return idle_task_runner_.get(); }

 private:
  std::unique_ptr<V8IdleTaskRunner> idle_task_runner_;
};

}  // namespace gin

#endif  // GIN_V8_FOREGROUND_TASK_RUNNER_BASE_H_
