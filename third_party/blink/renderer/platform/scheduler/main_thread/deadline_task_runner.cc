// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/deadline_task_runner.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"

namespace blink {
namespace scheduler {

DeadlineTaskRunner::DeadlineTaskRunner(
    const base::RepeatingClosure& callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : callback_(callback), task_runner_(task_runner) {
  cancelable_run_internal_.Reset(base::BindRepeating(
      &DeadlineTaskRunner::RunInternal, base::Unretained(this)));
}

DeadlineTaskRunner::~DeadlineTaskRunner() = default;

void DeadlineTaskRunner::SetDeadline(const base::Location& from_here,
                                     base::TimeDelta delay,
                                     base::TimeTicks now) {
  DCHECK(delay.is_positive());
  base::TimeTicks deadline = now + delay;
  if (deadline_.is_null() || deadline < deadline_) {
    deadline_ = deadline;
    cancelable_run_internal_.Cancel();
    task_runner_->PostDelayedTask(
        from_here, cancelable_run_internal_.GetCallback(), delay);
  }
}

void DeadlineTaskRunner::RunInternal() {
  deadline_ = base::TimeTicks();
  callback_.Run();
}

}  // namespace scheduler
}  // namespace blink
