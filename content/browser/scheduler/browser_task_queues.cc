// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_task_queues.h"

#include <iterator>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"

namespace content {
namespace {

using QueuePriority = ::base::sequence_manager::TaskQueue::QueuePriority;
using InsertFencePosition =
    ::base::sequence_manager::TaskQueue::InsertFencePosition;

const char* GetControlTaskQueueName(BrowserThread::ID thread_id) {
  switch (thread_id) {
    case BrowserThread::UI:
      return "ui_control_tq";
    case BrowserThread::IO:
      return "io_control_tq";
    case BrowserThread::ID_COUNT:
      break;
  }
  NOTREACHED();
  return "";
}

const char* GetRunAllPendingTaskQueueName(BrowserThread::ID thread_id) {
  switch (thread_id) {
    case BrowserThread::UI:
      return "ui_run_all_pending_tq";
    case BrowserThread::IO:
      return "io_run_all_pending_tq";
    case BrowserThread::ID_COUNT:
      break;
  }
  NOTREACHED();
  return "";
}

const char* GetUITaskQueueName(BrowserTaskQueues::QueueType queue_type) {
  switch (queue_type) {
    case BrowserTaskQueues::QueueType::kBestEffort:
      return "ui_best_effort_tq";
    case BrowserTaskQueues::QueueType::kBootstrap:
      return "ui_bootstrap_tq";
    case BrowserTaskQueues::QueueType::kNavigationAndPreconnection:
      return "ui_navigation_and_preconnection_tq";
    case BrowserTaskQueues::QueueType::kDefault:
      return "ui_default_tq";
    case BrowserTaskQueues::QueueType::kUserBlocking:
      return "ui_user_blocking_tq";
    case BrowserTaskQueues::QueueType::kUserVisible:
      return "ui_user_visible_tq";
  }
}

const char* GetIOTaskQueueName(BrowserTaskQueues::QueueType queue_type) {
  switch (queue_type) {
    case BrowserTaskQueues::QueueType::kBestEffort:
      return "io_best_effort_tq";
    case BrowserTaskQueues::QueueType::kBootstrap:
      return "io_bootstrap_tq";
    case BrowserTaskQueues::QueueType::kNavigationAndPreconnection:
      return "io_navigation_and_preconnection_tq";
    case BrowserTaskQueues::QueueType::kDefault:
      return "io_default_tq";
    case BrowserTaskQueues::QueueType::kUserBlocking:
      return "io_user_blocking_tq";
    case BrowserTaskQueues::QueueType::kUserVisible:
      return "io_user_visible_tq";
  }
}

const char* GetTaskQueueName(BrowserThread::ID thread_id,
                             BrowserTaskQueues::QueueType queue_type) {
  switch (thread_id) {
    case BrowserThread::UI:
      return GetUITaskQueueName(queue_type);
    case BrowserThread::IO:
      return GetIOTaskQueueName(queue_type);
    case BrowserThread::ID_COUNT:
      break;
  }
  NOTREACHED();
  return "";
}

const char* GetDefaultQueueName(BrowserThread::ID thread_id) {
  switch (thread_id) {
    case BrowserThread::UI:
      return "ui_thread_tq";
    case BrowserThread::IO:
      return "io_thread_tq";
    case BrowserThread::ID_COUNT:
      break;
  }
  NOTREACHED();
  return "";
}

}  // namespace

BrowserTaskQueues::Handle::~Handle() = default;

BrowserTaskQueues::Handle::Handle(BrowserTaskQueues* outer)
    : outer_(outer),
      control_task_runner_(outer_->control_queue_->task_runner()),
      default_task_runner_(outer_->default_task_queue_->task_runner()),
      browser_task_runners_(outer_->CreateBrowserTaskRunners()) {}

void BrowserTaskQueues::Handle::PostFeatureListInitializationSetup() {
  control_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserTaskQueues::PostFeatureListInitializationSetup,
                     base::Unretained(outer_)));
}

void BrowserTaskQueues::Handle::EnableAllQueues() {
  control_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BrowserTaskQueues::EnableAllQueues,
                                base::Unretained(outer_)));
}

void BrowserTaskQueues::Handle::EnableAllExceptBestEffortQueues() {
  control_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserTaskQueues::EnableAllExceptBestEffortQueues,
                     base::Unretained(outer_)));
}

void BrowserTaskQueues::Handle::ScheduleRunAllPendingTasksForTesting(
    base::OnceClosure on_pending_task_ran) {
  control_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BrowserTaskQueues::StartRunAllPendingTasksForTesting,
          base::Unretained(outer_),
          base::ScopedClosureRunner(std::move(on_pending_task_ran))));
}

#if DCHECK_IS_ON()

void BrowserTaskQueues::Handle::AddValidator(QueueType queue_type,
                                             Validator* validator) {
  validator_sets_[static_cast<size_t>(queue_type)].AddValidator(validator);
}

void BrowserTaskQueues::Handle::RemoveValidator(QueueType queue_type,
                                                Validator* validator) {
  validator_sets_[static_cast<size_t>(queue_type)].RemoveValidator(validator);
}

BrowserTaskQueues::ValidatorSet::ValidatorSet() = default;

BrowserTaskQueues::ValidatorSet::~ValidatorSet() {
  // Note the queue has already been shut down by the time we're deleted so we
  // don't need to unregister.
  DCHECK(validators_.empty());
}

void BrowserTaskQueues::ValidatorSet::AddValidator(Validator* validator) {
  base::AutoLock lock(lock_);
  DCHECK_EQ(validators_.count(validator), 0u)
      << "Validator added more than once";
  validators_.insert(validator);
}

void BrowserTaskQueues::ValidatorSet::RemoveValidator(Validator* validator) {
  base::AutoLock lock(lock_);
  size_t num_erased = validators_.erase(validator);
  DCHECK_EQ(num_erased, 1u) << "Validator not in set";
}

void BrowserTaskQueues::ValidatorSet::OnPostTask(base::Location from_here,
                                                 base::TimeDelta delay) {
  base::AutoLock lock(lock_);
  for (Validator* validator : validators_) {
    validator->ValidatePostTask(from_here);
  }
}

void BrowserTaskQueues::ValidatorSet::OnQueueNextWakeUpChanged(
    base::TimeTicks next_wake_up) {}

#endif  // DCHECK_IS_ON()

BrowserTaskQueues::QueueData::QueueData() = default;
BrowserTaskQueues::QueueData::~QueueData() = default;

BrowserTaskQueues::BrowserTaskQueues(
    BrowserThread::ID thread_id,
    base::sequence_manager::SequenceManager* sequence_manager,
    base::sequence_manager::TimeDomain* time_domain) {
  for (size_t i = 0; i < queue_data_.size(); ++i) {
    queue_data_[i].task_queue = sequence_manager->CreateTaskQueue(
        base::sequence_manager::TaskQueue::Spec(
            GetTaskQueueName(thread_id, static_cast<QueueType>(i)))
            .SetTimeDomain(time_domain));
    queue_data_[i].voter = queue_data_[i].task_queue->CreateQueueEnabledVoter();
    queue_data_[i].voter->SetVoteToEnable(false);
  }

  // Default task queue
  default_task_queue_ = sequence_manager->CreateTaskQueue(
      base::sequence_manager::TaskQueue::Spec(GetDefaultQueueName(thread_id))
          .SetTimeDomain(time_domain));

  GetBrowserTaskQueue(QueueType::kUserVisible)
      ->SetQueuePriority(QueuePriority::kLowPriority);

  // Best effort queue
  GetBrowserTaskQueue(QueueType::kBestEffort)
      ->SetQueuePriority(QueuePriority::kBestEffortPriority);

  // Control queue
  control_queue_ =
      sequence_manager->CreateTaskQueue(base::sequence_manager::TaskQueue::Spec(
                                            GetControlTaskQueueName(thread_id))
                                            .SetTimeDomain(time_domain));
  control_queue_->SetQueuePriority(QueuePriority::kControlPriority);

  // Run all pending queue
  run_all_pending_tasks_queue_ = sequence_manager->CreateTaskQueue(
      base::sequence_manager::TaskQueue::Spec(
          GetRunAllPendingTaskQueueName(thread_id))
          .SetTimeDomain(time_domain));
  run_all_pending_tasks_queue_->SetQueuePriority(
      QueuePriority::kBestEffortPriority);

  handle_ = base::AdoptRef(new Handle(this));

#if DCHECK_IS_ON()
  for (size_t i = 0; i < queue_data_.size(); ++i) {
    queue_data_[i].task_queue->SetObserver(&handle_->validator_sets_[i]);
  }

  // Treat the |default_task_queue_| the same as the USER_BLOCKING task queue
  // from a validation point of view.
  default_task_queue_->SetObserver(
      &handle_->validator_sets_[static_cast<int>(QueueType::kUserBlocking)]);
#endif
}

BrowserTaskQueues::~BrowserTaskQueues() {
  for (auto& queue : queue_data_) {
    queue.task_queue->ShutdownTaskQueue();
  }
  control_queue_->ShutdownTaskQueue();
  default_task_queue_->ShutdownTaskQueue();
  run_all_pending_tasks_queue_->ShutdownTaskQueue();
}

std::array<scoped_refptr<base::SingleThreadTaskRunner>,
           BrowserTaskQueues::kNumQueueTypes>
BrowserTaskQueues::CreateBrowserTaskRunners() const {
  std::array<scoped_refptr<base::SingleThreadTaskRunner>, kNumQueueTypes>
      task_runners;
  for (size_t i = 0; i < queue_data_.size(); ++i) {
    task_runners[i] = queue_data_[i].task_queue->task_runner();
  }
  return task_runners;
}

void BrowserTaskQueues::PostFeatureListInitializationSetup() {
  if (base::FeatureList::IsEnabled(features::kPrioritizeBootstrapTasks)) {
    GetBrowserTaskQueue(QueueType::kBootstrap)
        ->SetQueuePriority(QueuePriority::kHighestPriority);

    // Navigation and preconnection tasks are also important during startup so
    // prioritize them too.
    GetBrowserTaskQueue(QueueType::kNavigationAndPreconnection)
        ->SetQueuePriority(QueuePriority::kHighPriority);
  }
}

void BrowserTaskQueues::EnableAllQueues() {
  for (size_t i = 0; i < queue_data_.size(); ++i) {
    queue_data_[i].voter->SetVoteToEnable(true);
  }
}

void BrowserTaskQueues::EnableAllExceptBestEffortQueues() {
  for (size_t i = 0; i < queue_data_.size(); ++i) {
    if (i != static_cast<size_t>(QueueType::kBestEffort))
      queue_data_[i].voter->SetVoteToEnable(true);
  }
}

// To run all pending tasks we do the following. We insert a fence in all queues
// and post a task to the |run_all_pending_queue_| which has the lowest priority
// possible. That makes sure that all tasks up to the fences will have run
// before this posted task runs. Note that among tasks with the same priority
// ties are broken by using the enqueue order, so all prior best effort tasks
// will have run before this one does. This task will then remove all the fences
// and call the user provided callback to signal that all pending tasks have
// run. This method is "reentrant" as in we can call it multiple times as the
// fences will just be moved back, but we need to make sure that only the last
// call removes the fences, for that we keep track of "nesting" with
// |run_all_pending_nesting_level_|
void BrowserTaskQueues::StartRunAllPendingTasksForTesting(
    base::ScopedClosureRunner on_pending_task_ran) {
  ++run_all_pending_nesting_level_;
  for (const auto& queue : queue_data_) {
    queue.task_queue->InsertFence(InsertFencePosition::kNow);
  }
  default_task_queue_->InsertFence(InsertFencePosition::kNow);
  run_all_pending_tasks_queue_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserTaskQueues::EndRunAllPendingTasksForTesting,
                     base::Unretained(this), std::move(on_pending_task_ran)));
}

void BrowserTaskQueues::EndRunAllPendingTasksForTesting(
    base::ScopedClosureRunner on_pending_task_ran) {
  --run_all_pending_nesting_level_;
  if (run_all_pending_nesting_level_ == 0) {
    for (const auto& queue : queue_data_) {
      queue.task_queue->RemoveFence();
    }
    default_task_queue_->RemoveFence();
  }
}

}  // namespace content
