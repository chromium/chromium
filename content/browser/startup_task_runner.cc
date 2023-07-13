// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/startup_task_runner.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"

namespace content {

StartupTaskRunner::StartupTaskRunner(
    base::OnceCallback<void(int)> startup_complete_callback,
    scoped_refptr<base::SingleThreadTaskRunner> proxy)
    : startup_complete_callback_(std::move(startup_complete_callback)),
      proxy_(proxy) {}

StartupTaskRunner::~StartupTaskRunner() {}

void StartupTaskRunner::AddTask(StartupTask callback) {
  task_list_.push_back(std::move(callback));
}

void StartupTaskRunner::StartRunningTasksAsync() {
  DCHECK(proxy_.get());
  int result = 0;
  if (task_list_.empty()) {
    if (!startup_complete_callback_.is_null()) {
      std::move(startup_complete_callback_).Run(result);
    }
  } else {
    base::OnceClosure next_task =
        base::BindOnce(&StartupTaskRunner::WrappedTask, base::Unretained(this));
    proxy_->PostNonNestableTask(FROM_HERE, std::move(next_task));
  }
}

void StartupTaskRunner::RunAllTasksNow() {
  int result = 0;
  for (auto it = task_list_.begin(); it != task_list_.end(); it++) {
    result = std::move(*it).Run();
    if (result > 0) break;
  }
  task_list_.clear();
  if (!startup_complete_callback_.is_null()) {
    std::move(startup_complete_callback_).Run(result);
  }
}

void StartupTaskRunner::WrappedTask() {
  if (task_list_.empty()) {
    // This will happen if the remaining tasks have been run synchronously since
    // the WrappedTask was created. Any callback will already have been called,
    // so there is nothing to do
    return;
  }

  int result = std::move(task_list_.front()).Run();
  task_list_.pop_front();
  if (result > 0) {
    // Stop now and throw away the remaining tasks
    task_list_.clear();
  }
  if (task_list_.empty()) {
    if (!startup_complete_callback_.is_null()) {
      std::move(startup_complete_callback_).Run(result);
    }
  } else {
    base::OnceClosure next_task =
        base::BindOnce(&StartupTaskRunner::WrappedTask, base::Unretained(this));
    proxy_->PostNonNestableTask(FROM_HERE, std::move(next_task));
  }
}

}  // namespace content
