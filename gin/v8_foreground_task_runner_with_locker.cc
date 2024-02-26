// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/v8_foreground_task_runner_with_locker.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "gin/converter.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-locker.h"

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
  v8::Isolate::Scope isolate_scope(isolate);
  task->Run();
}

class IdleTaskWithLocker : public v8::IdleTask {
 public:
  IdleTaskWithLocker(v8::Isolate* isolate, std::unique_ptr<v8::IdleTask> task)
      : isolate_(isolate), task_(std::move(task)) {}
  IdleTaskWithLocker(const IdleTaskWithLocker&) = delete;
  IdleTaskWithLocker& operator=(const IdleTaskWithLocker&) = delete;
  ~IdleTaskWithLocker() override = default;

  // v8::IdleTask implementation.
  void Run(double deadline_in_seconds) override {
    v8::Locker lock(isolate_);
    v8::Isolate::Scope isolate_scope(isolate_);
    task_->Run(deadline_in_seconds);
  }

 private:
  raw_ptr<v8::Isolate> isolate_;
  std::unique_ptr<v8::IdleTask> task_;
};

}  // namespace

void V8ForegroundTaskRunnerWithLocker::PostTaskImpl(
    std::unique_ptr<v8::Task> task,
    const v8::SourceLocation& location) {
  task_runner_->PostTask(
      V8ToBaseLocation(location),
      base::BindOnce(RunWithLocker, base::Unretained(isolate_),
                     std::move(task)));
}

void V8ForegroundTaskRunnerWithLocker::PostNonNestableTaskImpl(
    std::unique_ptr<v8::Task> task,
    const v8::SourceLocation& location) {
  task_runner_->PostNonNestableTask(
      V8ToBaseLocation(location),
      base::BindOnce(RunWithLocker, base::Unretained(isolate_),
                     std::move(task)));
}

void V8ForegroundTaskRunnerWithLocker::PostDelayedTaskImpl(
    std::unique_ptr<v8::Task> task,
    double delay_in_seconds,
    const v8::SourceLocation& location) {
  task_runner_->PostDelayedTask(
      V8ToBaseLocation(location),
      base::BindOnce(RunWithLocker, base::Unretained(isolate_),
                     std::move(task)),
      base::Seconds(delay_in_seconds));
}

void V8ForegroundTaskRunnerWithLocker::PostIdleTaskImpl(
    std::unique_ptr<v8::IdleTask> task,
    const v8::SourceLocation& location) {
  DCHECK(IdleTasksEnabled());
  idle_task_runner()->PostIdleTask(
      std::make_unique<IdleTaskWithLocker>(isolate_, std::move(task)));
}

bool V8ForegroundTaskRunnerWithLocker::NonNestableTasksEnabled() const {
  return true;
}

}  // namespace gin
