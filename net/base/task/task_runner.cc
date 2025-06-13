// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/task/task_runner.h"

#include "base/no_destructor.h"

namespace net {

const scoped_refptr<base::SingleThreadTaskRunner>& GetTaskRunner(
    RequestPriority priority) {
  if (priority == RequestPriority::HIGHEST &&
      internal::GetTaskRunnerGlobals().high_priority_task_runner) {
    return internal::GetTaskRunnerGlobals().high_priority_task_runner;
  }
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

namespace internal {

TaskRunnerGlobals::TaskRunnerGlobals() = default;
TaskRunnerGlobals::~TaskRunnerGlobals() = default;

TaskRunnerGlobals& GetTaskRunnerGlobals() {
  static base::NoDestructor<TaskRunnerGlobals> globals;
  return *globals;
}

}  // namespace internal

}  // namespace net
