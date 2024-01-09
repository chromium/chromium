// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_V8_FOREGROUND_TASK_RUNNER_WITH_LOCKER_H_
#define GIN_V8_FOREGROUND_TASK_RUNNER_WITH_LOCKER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "gin/v8_foreground_task_runner_base.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gin {

class V8ForegroundTaskRunnerWithLocker : public V8ForegroundTaskRunnerBase {
 public:
  V8ForegroundTaskRunnerWithLocker(
      v8::Isolate* isolate,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  ~V8ForegroundTaskRunnerWithLocker() override;

  // v8::Platform implementation.
  void PostTask(std::unique_ptr<v8::Task> task) override;

  void PostNonNestableTask(std::unique_ptr<v8::Task> task) override;

  void PostDelayedTask(std::unique_ptr<v8::Task> task,
                       double delay_in_seconds) override;

  void PostIdleTask(std::unique_ptr<v8::IdleTask> task) override;

  bool NonNestableTasksEnabled() const override;

 private:
  // This dangles because the isolate must be disposed before the task runner
  // can safely be destroyed. V8-managed tasks in other threads might try to
  // post more tasks whilst the isolate is being disposed (before V8 cancels
  // them as part of disposal).
  //
  // Once the isolate is disposed, V8 has made sure that no more tasks should be
  // running or get posted, and this task runner will quickly get destroyed
  // afterwards.
  raw_ptr<v8::Isolate, DanglingUntriaged> isolate_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace gin

#endif  // GIN_V8_FOREGROUND_TASK_RUNNER_WITH_LOCKER_H_
