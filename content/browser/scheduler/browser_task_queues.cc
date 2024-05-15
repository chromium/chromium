// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/browser_task_queues.h"

#include <array>
#include <cstdint>
#include <iterator>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/scheduler/browser_task_priority.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"

namespace content {
namespace {

// (crbug/1375174): Make kServiceWorkerStorageControlResponse queue use high
// priority.
BASE_FEATURE(kServiceWorkerStorageControlResponseUseHighPriority,
             "ServiceWorkerStorageControlResponseUseHighPriority",
             base::FEATURE_ENABLED_BY_DEFAULT);

using BrowserTaskPriority = ::content::internal::BrowserTaskPriority;
using QueueName = ::perfetto::protos::pbzero::SequenceManagerTask::QueueName;
using InsertFencePosition =
    ::base::sequence_manager::TaskQueue::InsertFencePosition;
using QueueEnabledVoter = base::sequence_manager::TaskQueue::QueueEnabledVoter;

QueueName GetControlTaskQueueName(BrowserThread::ID thread_id) {
  switch (thread_id) {
    case BrowserThread::UI:
      return QueueName::UI_CONTROL_TQ;
    case BrowserThread::IO:
      return QueueName::IO_CONTROL_TQ;
    case BrowserThread::ID_COUNT:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return QueueName::UNKNOWN_TQ;
}

QueueName GetRunAllPendingTaskQueueName(BrowserThread::ID thread_id) {
  switch (thread_id) {
    case BrowserThread::UI:
      return QueueName::UI_RUN_ALL_PENDING_TQ;
    case BrowserThread::IO:
      return QueueName::IO_RUN_ALL_PENDING_TQ;
    case BrowserThread::ID_COUNT:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return QueueName::UNKNOWN_TQ;
}

QueueName GetUITaskQueueName(BrowserTaskQueues::QueueType queue_type) {
  switch (queue_type) {
    case BrowserTaskQueues::QueueType::kBestEffort:
      return QueueName::UI_BEST_EFFORT_TQ;
    case BrowserTaskQueues::QueueType::kDefault:
      return QueueName::UI_DEFAULT_TQ;
    case BrowserTaskQueues::QueueType::kDeferrableUserBlocking:
      return QueueName::UI_USER_BLOCKING_DEFERRABLE_TQ;
    case BrowserTaskQueues::QueueType::kUserBlocking:
      return QueueName::UI_USER_BLOCKING_TQ;
    case BrowserTaskQueues::QueueType::kUserVisible:
      return QueueName::UI_USER_VISIBLE_TQ;
    case BrowserTaskQueues::QueueType::kUserInput:
      return QueueName::UI_USER_INPUT_TQ;
    case BrowserTaskQueues::QueueType::kNavigationNetworkResponse:
      return QueueName::UI_NAVIGATION_NETWORK_RESPONSE_TQ;
    case BrowserTaskQueues::QueueType::kServiceWorkerStorageControlResponse:
      return QueueName::UI_SERVICE_WORKER_STORAGE_CONTROL_RESPONSE_TQ;
    case BrowserTaskQueues::QueueType::kBeforeUnloadBrowserResponse:
      return QueueName::UI_BEFORE_UNLOAD_BROWSER_RESPONSE_TQ;
  }
}

QueueName GetIOTaskQueueName(BrowserTaskQueues::QueueType queue_type) {
  switch (queue_type) {
    case BrowserTaskQueues::QueueType::kBestEffort:
      return QueueName::IO_BEST_EFFORT_TQ;
    case BrowserTaskQueues::QueueType::kDefault:
      return QueueName::IO_DEFAULT_TQ;
    case BrowserTaskQueues::QueueType::kDeferrableUserBlocking:
      return QueueName::IO_USER_BLOCKING_DEFERRABLE_TQ;
    case BrowserTaskQueues::QueueType::kUserBlocking:
      return QueueName::IO_USER_BLOCKING_TQ;
    case BrowserTaskQueues::QueueType::kUserVisible:
      return QueueName::IO_USER_VISIBLE_TQ;
    case BrowserTaskQueues::QueueType::kUserInput:
      return QueueName::IO_USER_INPUT_TQ;
    case BrowserTaskQueues::QueueType::kNavigationNetworkResponse:
      return QueueName::IO_NAVIGATION_NETWORK_RESPONSE_TQ;
    case BrowserTaskQueues::QueueType::kServiceWorkerStorageControlResponse:
      return QueueName::IO_SERVICE_WORKER_STORAGE_CONTROL_RESPONSE_TQ;
    case BrowserTaskQueues::QueueType::kBeforeUnloadBrowserResponse:
      return QueueName::IO_BEFORE_UNLOAD_BROWSER_RESPONSE_TQ;
  }
}

QueueName GetTaskQueueName(BrowserThread::ID thread_id,
                           BrowserTaskQueues::QueueType queue_type) {
  switch (thread_id) {
    case BrowserThread::UI:
      return GetUITaskQueueName(queue_type);
    case BrowserThread::IO:
      return GetIOTaskQueueName(queue_type);
    case BrowserThread::ID_COUNT:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return QueueName::UNKNOWN_TQ;
}

}  // namespace

BrowserTaskQueues::QueueData::QueueData() = default;
BrowserTaskQueues::QueueData::~QueueData() = default;
BrowserTaskQueues::QueueData::QueueData(BrowserTaskQueues::QueueData&& other) =
    default;

BrowserTaskQueues::Handle::~Handle() = default;

BrowserTaskQueues::Handle::Handle(BrowserTaskQueues* outer)
    : outer_(outer),
      control_task_runner_(outer_->control_queue_->task_runner()),
      default_task_runner_(outer_->GetDefaultTaskQueue()->task_runner()),
      browser_task_runners_(outer_->CreateBrowserTaskRunners()) {}

void BrowserTaskQueues::Handle::OnStartupComplete() {
  control_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BrowserTaskQueues::OnStartupComplete,
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

BrowserTaskQueues::BrowserTaskQueues(
    BrowserThread::ID thread_id,
    base::sequence_manager::SequenceManager* sequence_manager) {
  for (size_t i = 0; i < queue_data_.size(); ++i) {
    queue_data_[i].task_queue = sequence_manager->CreateTaskQueue(
        base::sequence_manager::TaskQueue::Spec(
            GetTaskQueueName(thread_id, static_cast<QueueType>(i))));
    queue_data_[i].voter = queue_data_[i].task_queue->CreateQueueEnabledVoter();
    if (static_cast<QueueType>(i) != QueueType::kDefault) {
      queue_data_[i].voter->SetVoteToEnable(false);
    }
  }

  GetBrowserTaskQueue(QueueType::kUserVisible)
      ->SetQueuePriority(BrowserTaskPriority::kLowPriority);

  // Best effort queue
  GetBrowserTaskQueue(QueueType::kBestEffort)
      ->SetQueuePriority(BrowserTaskPriority::kBestEffortPriority);

  // User Input queue
  GetBrowserTaskQueue(QueueType::kUserInput)
      ->SetQueuePriority(BrowserTaskPriority::kHighestPriority);

  GetBrowserTaskQueue(QueueType::kNavigationNetworkResponse)
      ->SetQueuePriority(BrowserTaskPriority::kHighPriority);

  GetBrowserTaskQueue(QueueType::kServiceWorkerStorageControlResponse)
      ->SetQueuePriority(BrowserTaskPriority::kHighestPriority);

  GetBrowserTaskQueue(QueueType::kBeforeUnloadBrowserResponse)
      ->SetQueuePriority(BrowserTaskPriority::kHighPriority);

  // Control queue
  control_queue_ =
      sequence_manager->CreateTaskQueue(base::sequence_manager::TaskQueue::Spec(
          GetControlTaskQueueName(thread_id)));
  control_queue_->SetQueuePriority(BrowserTaskPriority::kControlPriority);

  // Run all pending queue
  run_all_pending_tasks_queue_ =
      sequence_manager->CreateTaskQueue(base::sequence_manager::TaskQueue::Spec(
          GetRunAllPendingTaskQueueName(thread_id)));
  run_all_pending_tasks_queue_->SetQueuePriority(
      BrowserTaskPriority::kBestEffortPriority);

  handle_ = base::AdoptRef(new Handle(this));
}

BrowserTaskQueues::~BrowserTaskQueues() {
  for (auto& queue : queue_data_) {
    queue.task_queue.reset();
  }
  control_queue_.reset();
  run_all_pending_tasks_queue_.reset();
  handle_->OnTaskQueuesDestroyed();
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

std::array<std::unique_ptr<QueueEnabledVoter>,
           BrowserTaskQueues::kNumQueueTypes>
BrowserTaskQueues::CreateQueueEnabledVoters() const {
  std::array<std::unique_ptr<QueueEnabledVoter>, kNumQueueTypes> voters;
  for (size_t i = 0; i < voters.size(); ++i) {
    voters[i] = queue_data_[i].task_queue->CreateQueueEnabledVoter();
  }
  return voters;
}

void BrowserTaskQueues::OnStartupComplete() {
  // Enable all queues
  for (const auto& queue : queue_data_) {
    queue.voter->SetVoteToEnable(true);
  }

  // Update ServiceWorker task queue priority.
  DCHECK_EQ(
      static_cast<BrowserTaskPriority>(
          GetBrowserTaskQueue(QueueType::kServiceWorkerStorageControlResponse)
              ->GetQueuePriority()),
      BrowserTaskPriority::kHighestPriority);
  GetBrowserTaskQueue(QueueType::kServiceWorkerStorageControlResponse)
      ->SetQueuePriority(
          base::FeatureList::IsEnabled(
              kServiceWorkerStorageControlResponseUseHighPriority)
              ? BrowserTaskPriority::kHighPriority
              : BrowserTaskPriority::kNormalPriority);
}

void BrowserTaskQueues::EnableAllExceptBestEffortQueues() {
  for (size_t i = 0; i < queue_data_.size(); ++i) {
    if (i != static_cast<size_t>(QueueType::kBestEffort)) {
      queue_data_[i].voter->SetVoteToEnable(true);
    }
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
  }
}

}  // namespace content
