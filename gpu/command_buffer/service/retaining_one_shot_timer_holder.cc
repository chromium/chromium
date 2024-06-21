// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/retaining_one_shot_timer_holder.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"

namespace gpu {

RetainingOneShotTimerHolder::RetainingOneShotTimerHolder(
    base::TimeDelta max_delay,
    base::TimeDelta min_delay,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::RepeatingClosure user_task)
    : min_delay_(min_delay),
      task_runner_(std::move(task_runner)),
      user_task_(std::move(user_task)),
      timer_(std::make_unique<base::RetainingOneShotTimer>(
          FROM_HERE,
          max_delay,
          base::BindRepeating(&RetainingOneShotTimerHolder::OnTimerFired,
                              base::RetainedRef(this)))) {}

void RetainingOneShotTimerHolder::ResetTimerIfNecessary() {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&RetainingOneShotTimerHolder::ResetTimerIfNecessary,
                       base::RetainedRef(this)));
    return;
  }

  if (timer_ &&
      (!timer_->IsRunning() ||
       timer_->desired_run_time() - base::TimeTicks::Now() < min_delay_)) {
    timer_->Reset();
  }
}

void RetainingOneShotTimerHolder::DestroyTimer() {
  {
    base::AutoLock auto_lock(lock_);
    user_task_.Reset();
  }

  if (task_runner_->BelongsToCurrentThread()) {
    DestroyTimerOnTaskRunner();
    return;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RetainingOneShotTimerHolder::DestroyTimerOnTaskRunner,
                     base::RetainedRef(this)));
}

RetainingOneShotTimerHolder::~RetainingOneShotTimerHolder() {
  // DestroyTimer() must have been called to clean up.
  CHECK(user_task_.is_null());
}

void RetainingOneShotTimerHolder::DestroyTimerOnTaskRunner() {
  timer_ = nullptr;
}

void RetainingOneShotTimerHolder::OnTimerFired() {
  base::AutoLock auto_lock(lock_);
  if (user_task_.is_null()) {
    return;
  }

  // Note: `user_task_` is run while holding `lock_` in order to guarantee that
  // once DestroyTimer() returns, `user_task_` definitely won't be called
  // afterwards. Please also see the comment of constructor and DestroyTimer()
  // for more details.
  user_task_.Run();
}

}  // namespace gpu
