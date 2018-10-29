// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FOREGROUND_TASK_RUNNER_H
#define V8_FOREGROUND_TASK_RUNNER_H

#include "base/memory/ref_counted.h"
#include "gin/v8_foreground_task_runner_base.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gin {

class V8ForegroundTaskRunner : public V8ForegroundTaskRunnerBase {
 public:
  V8ForegroundTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  ~V8ForegroundTaskRunner() override;

  // v8::Platform implementation.
  void PostTask(std::unique_ptr<v8::Task> task) override;

  void PostNonNestableTask(std::unique_ptr<v8::Task> task) override;

  void PostDelayedTask(std::unique_ptr<v8::Task> task,
                       double delay_in_seconds) override;

  void PostIdleTask(std::unique_ptr<v8::IdleTask> task) override;

  bool NonNestableTasksEnabled() const override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace gin

#endif  // V8_FOREGROUND_TASK_RUNNER_H
