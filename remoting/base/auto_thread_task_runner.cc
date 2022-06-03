// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/auto_thread_task_runner.h"

#include <utility>

#include "base/check.h"

namespace remoting {

AutoThreadTaskRunner::AutoThreadTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::OnceClosure stop_task)
    : stop_task_(std::move(stop_task)), task_runner_(task_runner) {
  DCHECK(!stop_task_.is_null());
}

bool AutoThreadTaskRunner::PostDelayedTask(const base::Location& from_here,
                                           base::OnceClosure task,
                                           base::TimeDelta delay) {
  CHECK(task_runner_->PostDelayedTask(from_here, std::move(task), delay));
  return true;
}

bool AutoThreadTaskRunner::PostNonNestableDelayedTask(
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta delay) {
  CHECK(task_runner_->PostNonNestableDelayedTask(from_here, std::move(task),
                                                 delay));
  return true;
}

bool AutoThreadTaskRunner::RunsTasksInCurrentSequence() const {
  return task_runner_->RunsTasksInCurrentSequence();
}

AutoThreadTaskRunner::~AutoThreadTaskRunner() {
  CHECK(task_runner_->PostTask(FROM_HERE, std::move(stop_task_)));
}

}  // namespace remoting
