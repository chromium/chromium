// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/v8_foreground_task_runner_with_locker.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "v8/include/v8.h"

namespace gin {

V8ForegroundTaskRunnerWithLocker::V8ForegroundTaskRunnerWithLocker(
    v8::Isolate* isolate,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : isolate_(isolate), task_runner_(std::move(task_runner)) {
  DCHECK(task_runner_);
}

V8ForegroundTaskRunnerWithLocker::~V8ForegroundTaskRunnerWithLocker() = default;

namespace {

void RunWithLocker(v8::Isolate* isolate, std::unique_ptr<v8::Task> task) {
  v8::Locker lock(isolate);
  task->Run();
}

class IdleTaskWithLocker : public v8::IdleTask {
 public:
  IdleTaskWithLocker(v8::Isolate* isolate, std::unique_ptr<v8::IdleTask> task)
      : isolate_(isolate), task_(std::move(task)) {}

  ~IdleTaskWithLocker() override = default;

  // v8::IdleTask implementation.
  void Run(double deadline_in_seconds) override {
    v8::Locker lock(isolate_);
    task_->Run(deadline_in_seconds);
  }

 private:
  v8::Isolate* isolate_;
  std::unique_ptr<v8::IdleTask> task_;

  DISALLOW_COPY_AND_ASSIGN(IdleTaskWithLocker);
};

}  // namespace

void V8ForegroundTaskRunnerWithLocker::PostTask(
    std::unique_ptr<v8::Task> task) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(RunWithLocker, base::Unretained(isolate_),
                                std::move(task)));
}

void V8ForegroundTaskRunnerWithLocker::PostNonNestableTask(
    std::unique_ptr<v8::Task> task) {
  task_runner_->PostNonNestableTask(
      FROM_HERE, base::BindOnce(RunWithLocker, base::Unretained(isolate_),
                                std::move(task)));
}

void V8ForegroundTaskRunnerWithLocker::PostDelayedTask(
    std::unique_ptr<v8::Task> task,
    double delay_in_seconds) {
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(RunWithLocker, base::Unretained(isolate_),
                     std::move(task)),
      base::TimeDelta::FromSecondsD(delay_in_seconds));
}

void V8ForegroundTaskRunnerWithLocker::PostIdleTask(
    std::unique_ptr<v8::IdleTask> task) {
  DCHECK(IdleTasksEnabled());
  idle_task_runner()->PostIdleTask(
      std::make_unique<IdleTaskWithLocker>(isolate_, std::move(task)));
}

bool V8ForegroundTaskRunnerWithLocker::NonNestableTasksEnabled() const {
  return true;
}

}  // namespace gin
