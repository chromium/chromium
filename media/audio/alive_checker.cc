// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/alive_checker.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"

namespace media {

AliveChecker::AliveChecker(base::RepeatingClosure dead_callback,
                           base::TimeDelta check_interval,
                           base::TimeDelta timeout,
                           bool stop_at_first_alive_notification,
                           bool pause_check_during_suspend)
    : AliveChecker(std::move(dead_callback),
                   check_interval,
                   timeout,
                   stop_at_first_alive_notification,
                   pause_check_during_suspend,
                   PowerObserverHelperFactoryCallback()) {}

AliveChecker::AliveChecker(
    base::RepeatingClosure dead_callback,
    base::TimeDelta check_interval,
    base::TimeDelta timeout,
    bool stop_at_first_alive_notification,
    PowerObserverHelperFactoryCallback power_observer_helper_factory_callback)
    : AliveChecker(std::move(dead_callback),
                   check_interval,
                   timeout,
                   stop_at_first_alive_notification,
                   true,
                   std::move(power_observer_helper_factory_callback)) {}

// The private constructor called by the above public constructors.
AliveChecker::AliveChecker(
    base::RepeatingClosure dead_callback,
    base::TimeDelta check_interval,
    base::TimeDelta timeout,
    bool stop_at_first_alive_notification,
    bool pause_check_during_suspend,
    PowerObserverHelperFactoryCallback power_observer_helper_factory_callback)
    : check_interval_(check_interval),
      timeout_(timeout),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      dead_callback_(std::move(dead_callback)),
      stop_at_first_alive_notification_(stop_at_first_alive_notification) {
  DCHECK(!dead_callback_.is_null());
  DCHECK_GT(check_interval_, base::TimeDelta());
  DCHECK_GT(timeout_, check_interval_);

  if (pause_check_during_suspend) {
    // When suspending, we don't need to take any action. When resuming, we
    // reset |last_alive_notification_time_| to avoid false alarms.
    // Unretained is safe since the PowerObserverHelper runs the callback on
    // the task runner the AliveChecker (and consequently the
    // PowerObserverHelper) is destroyed on.
    if (power_observer_helper_factory_callback.is_null()) {
      power_observer_ = std::make_unique<PowerObserverHelper>(
          task_runner_, base::DoNothing(),
          base::BindRepeating(
              &AliveChecker::SetLastAliveNotificationTimeToNowOnTaskRunner,
              base::Unretained(this)));
    } else {
      power_observer_ =
          std::move(power_observer_helper_factory_callback)
              .Run(task_runner_, base::DoNothing(),
                   base::BindRepeating(
                       &AliveChecker::
                           SetLastAliveNotificationTimeToNowOnTaskRunner,
                       base::Unretained(this)));
    }
  } else {
    // If |pause_check_during_suspend| is false, we expect an empty factory
    // callback.
    DCHECK(power_observer_helper_factory_callback.is_null());
  }
}

AliveChecker::~AliveChecker() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

void AliveChecker::Start() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  SetLastAliveNotificationTimeToNowOnTaskRunner();
  detected_dead_ = false;

  DCHECK(!check_alive_timer_);
  check_alive_timer_ = std::make_unique<base::RepeatingTimer>();
  check_alive_timer_->Start(FROM_HERE, check_interval_, this,
                            &AliveChecker::CheckIfAlive);
  DCHECK(check_alive_timer_->IsRunning());
}

void AliveChecker::Stop() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  check_alive_timer_.reset();
}

bool AliveChecker::DetectedDead() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return detected_dead_;
}

void AliveChecker::NotifyAlive() {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // We don't need high precision for setting |last_alive_notification_time_|
    // so we don't have to care about the delay added with posting the task.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AliveChecker::NotifyAlive, weak_factory_.GetWeakPtr()));
    return;
  }

  SetLastAliveNotificationTimeToNowOnTaskRunner();
  if (stop_at_first_alive_notification_)
    Stop();
}

void AliveChecker::CheckIfAlive() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // The reason we check a flag instead of stopping the timer that runs this
  // function at suspend is that it would require knowing what state we're in
  // when resuming and maybe start the timer. Also, we would still need this
  // flag anyway to maybe start the timer at stream creation.
  // TODO(grunell): Suspend/resume notifications are not supported on Linux. We
  // could possibly use wall clock time as a complement to be able to detect
  // time jumps that probably are caused by suspend/resume.
  if (power_observer_ && power_observer_->IsSuspending())
    return;

  if (base::TimeTicks::Now() - last_alive_notification_time_ > timeout_) {
    Stop();
    detected_dead_ = true;
    dead_callback_.Run();
  }
}

void AliveChecker::SetLastAliveNotificationTimeToNowOnTaskRunner() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  last_alive_notification_time_ = base::TimeTicks::Now();
}

}  // namespace media
