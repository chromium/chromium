// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/v8_foreground_task_runner.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "gin/converter.h"

namespace gin {

V8ForegroundTaskRunner::V8ForegroundTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {
  DCHECK(task_runner_);
}

V8ForegroundTaskRunner::~V8ForegroundTaskRunner() = default;

void V8ForegroundTaskRunner::PostTaskImpl(std::unique_ptr<v8::Task> task,
                                          const v8::SourceLocation& location) {
  task_runner_->PostTask(V8ToBaseLocation(location),
                         base::BindOnce(&v8::Task::Run, std::move(task)));
}

void V8ForegroundTaskRunner::PostNonNestableTaskImpl(
    std::unique_ptr<v8::Task> task,
    const v8::SourceLocation& location) {
  task_runner_->PostNonNestableTask(
      V8ToBaseLocation(location),
      base::BindOnce(&v8::Task::Run, std::move(task)));
}

void V8ForegroundTaskRunner::PostDelayedTaskImpl(
    std::unique_ptr<v8::Task> task,
    double delay_in_seconds,
    const v8::SourceLocation& location) {
  task_runner_->PostDelayedTask(V8ToBaseLocation(location),
                                base::BindOnce(&v8::Task::Run, std::move(task)),
                                base::Seconds(delay_in_seconds));
}

void V8ForegroundTaskRunner::PostNonNestableDelayedTaskImpl(
    std::unique_ptr<v8::Task> task,
    double delay_in_seconds,
    const v8::SourceLocation& location) {
  task_runner_->PostNonNestableDelayedTask(
      V8ToBaseLocation(location),
      base::BindOnce(&v8::Task::Run, std::move(task)),
      base::Seconds(delay_in_seconds));
}

void V8ForegroundTaskRunner::PostIdleTaskImpl(
    std::unique_ptr<v8::IdleTask> task,
    const v8::SourceLocation& location) {
  DCHECK(IdleTasksEnabled());
  idle_task_runner()->PostIdleTask(std::move(task));
}

bool V8ForegroundTaskRunner::NonNestableTasksEnabled() const {
  return true;
}

bool V8ForegroundTaskRunner::NonNestableDelayedTasksEnabled() const {
  return true;
}

}  // namespace gin
