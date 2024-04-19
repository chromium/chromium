// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/task_service.h"

#include <limits>

#include "base/functional/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"

namespace midi {

constexpr TaskService::RunnerId TaskService::kDefaultRunnerId;
constexpr TaskService::InstanceId TaskService::kInvalidInstanceId;

TaskService::TaskService() : no_tasks_in_flight_cv_(&tasks_in_flight_lock_) {
  DETACH_FROM_SEQUENCE(instance_binding_sequence_checker_);
}

TaskService::~TaskService() {
  std::vector<std::unique_ptr<base::Thread>> threads;
  {
    base::AutoLock lock(lock_);
    threads = std::move(threads_);
    DCHECK_EQ(kInvalidInstanceId, bound_instance_id_);
  }
  // Should not have any lock to perform thread joins on thread destruction.
  // All posted tasks should run before quitting the thread message loop.
  threads.clear();
}

bool TaskService::BindInstance() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(instance_binding_sequence_checker_);
  base::AutoLock lock(lock_);
  if (bound_instance_id_ != kInvalidInstanceId)
    return false;

  // If the InstanceId reaches to the limit, just fail rather than doing
  // something nicer for such impractical case.
  if (std::numeric_limits<InstanceId>::max() == next_instance_id_)
    return false;

  bound_instance_id_ = ++next_instance_id_;

  DCHECK(!default_task_runner_);
  default_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();

  return true;
}

bool TaskService::UnbindInstance() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(instance_binding_sequence_checker_);
  {
    base::AutoLock lock(lock_);
    if (bound_instance_id_ == kInvalidInstanceId)
      return false;

    DCHECK_EQ(next_instance_id_, bound_instance_id_);
    bound_instance_id_ = kInvalidInstanceId;

    DCHECK(default_task_runner_);
    default_task_runner_ = nullptr;
  }
  // From now on RunTask will never run any task bound to the instance id.
  // But invoked tasks might be still running here. To ensure no task runs on
  // quitting this method, wait for all tasks to complete.
  base::AutoLock tasks_in_flight_lock(tasks_in_flight_lock_);
  // TODO(crbug.com/40555725): Remove sync operations on the I/O thread.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  while (tasks_in_flight_ > 0)
    no_tasks_in_flight_cv_.Wait();

  return true;
}

bool TaskService::IsOnTaskRunner(RunnerId runner_id) {
  base::AutoLock lock(lock_);
  if (bound_instance_id_ == kInvalidInstanceId)
    return false;

  if (runner_id == kDefaultRunnerId)
    return default_task_runner_->BelongsToCurrentThread();

  size_t thread = runner_id - 1;
  if (threads_.size() <= thread || !threads_[thread])
    return false;

  return threads_[thread]->task_runner()->BelongsToCurrentThread();
}

void TaskService::PostStaticTask(RunnerId runner_id, base::OnceClosure task) {
  DCHECK_NE(kDefaultRunnerId, runner_id);
  GetTaskRunner(runner_id)->PostTask(FROM_HERE, std::move(task));
}

void TaskService::PostBoundTask(RunnerId runner_id, base::OnceClosure task) {
  InstanceId instance_id;
  {
    base::AutoLock lock(lock_);
    if (bound_instance_id_ == kInvalidInstanceId)
      return;
    instance_id = bound_instance_id_;
  }
  GetTaskRunner(runner_id)->PostTask(
      FROM_HERE, base::BindOnce(&TaskService::RunTask, base::Unretained(this),
                                instance_id, runner_id, std::move(task)));
}

void TaskService::PostBoundDelayedTask(RunnerId runner_id,
                                       base::OnceClosure task,
                                       base::TimeDelta delay) {
  InstanceId instance_id;
  {
    base::AutoLock lock(lock_);
    if (bound_instance_id_ == kInvalidInstanceId)
      return;
    instance_id = bound_instance_id_;
  }
  GetTaskRunner(runner_id)->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TaskService::RunTask, base::Unretained(this), instance_id,
                     runner_id, std::move(task)),
      delay);
}

void TaskService::OverflowInstanceIdForTesting() {
  next_instance_id_ = std::numeric_limits<InstanceId>::max();
}

scoped_refptr<base::SingleThreadTaskRunner> TaskService::GetTaskRunner(
    RunnerId runner_id) {
  base::AutoLock lock(lock_);
  if (runner_id == kDefaultRunnerId)
    return default_task_runner_;

  if (threads_.size() < runner_id)
    threads_.resize(runner_id);

  size_t thread = runner_id - 1;
  if (!threads_[thread]) {
    threads_[thread] = std::make_unique<base::Thread>(
        base::StringPrintf("MidiService_TaskService_Thread(%zu)", runner_id));
    base::Thread::Options options;
#if BUILDFLAG(IS_WIN)
    threads_[thread]->init_com_with_mta(true);
#elif BUILDFLAG(IS_MAC)
    options.message_pump_type = base::MessagePumpType::UI;
#endif
    threads_[thread]->StartWithOptions(std::move(options));
  }
  return threads_[thread]->task_runner();
}

void TaskService::RunTask(InstanceId instance_id,
                          RunnerId runner_id,
                          base::OnceClosure task) {
  {
    base::AutoLock tasks_in_flight_lock(tasks_in_flight_lock_);
    ++tasks_in_flight_;
  }

  if (IsInstanceIdStillBound(instance_id))
    std::move(task).Run();

  {
    base::AutoLock tasks_in_flight_lock(tasks_in_flight_lock_);
    --tasks_in_flight_;
    DCHECK_GE(tasks_in_flight_, 0);
    if (tasks_in_flight_ == 0)
      no_tasks_in_flight_cv_.Signal();
  }
}

bool TaskService::IsInstanceIdStillBound(InstanceId instance_id) {
  base::AutoLock lock(lock_);
  return instance_id == bound_instance_id_;
}

}  // namespace midi
