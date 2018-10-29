// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/v8_foreground_task_runner.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace gin {

V8ForegroundTaskRunner::V8ForegroundTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {
  DCHECK(task_runner_);
}

V8ForegroundTaskRunner::~V8ForegroundTaskRunner() = default;

void V8ForegroundTaskRunner::PostTask(std::unique_ptr<v8::Task> task) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&v8::Task::Run, std::move(task)));
}

void V8ForegroundTaskRunner::PostNonNestableTask(
    std::unique_ptr<v8::Task> task) {
  task_runner_->PostNonNestableTask(
      FROM_HERE, base::BindOnce(&v8::Task::Run, std::move(task)));
}

void V8ForegroundTaskRunner::PostDelayedTask(std::unique_ptr<v8::Task> task,
                                             double delay_in_seconds) {
  task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&v8::Task::Run, std::move(task)),
      base::TimeDelta::FromSecondsD(delay_in_seconds));
}

void V8ForegroundTaskRunner::PostIdleTask(std::unique_ptr<v8::IdleTask> task) {
  DCHECK(IdleTasksEnabled());
  idle_task_runner()->PostIdleTask(std::move(task));
}

bool V8ForegroundTaskRunner::NonNestableTasksEnabled() const {
  return true;
}

}  // namespace gin
