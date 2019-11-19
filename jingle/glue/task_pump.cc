// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "jingle/glue/task_pump.h"

namespace jingle_glue {

TaskPump::TaskPump() : posted_wake_(false), stopped_(false) {}

TaskPump::~TaskPump() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TaskPump::WakeTasks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!stopped_ && !posted_wake_) {
    // Do the requested wake up.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&TaskPump::CheckAndRunTasks,
                                  weak_factory_.GetWeakPtr()));
    posted_wake_ = true;
  }
}

void TaskPump::Stop() {
  stopped_ = true;
}

void TaskPump::CheckAndRunTasks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stopped_) {
    return;
  }
  posted_wake_ = false;
  // We shouldn't be using libjingle for timeout tasks, so we should
  // have no timeout tasks at all.

  // TODO(akalin): Add HasTimeoutTask() back in TaskRunner class and
  // uncomment this check.
  // DCHECK(!HasTimeoutTask())
  RunTasks();
}

}  // namespace jingle_glue
