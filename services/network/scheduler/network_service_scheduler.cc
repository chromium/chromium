// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/scheduler/network_service_scheduler.h"

#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "net/base/task/task_runner.h"
#include "services/network/scheduler/network_service_task_priority.h"

namespace network {

// The NetworkServiceScheduler's lifetime is coupled with the Network Service
// itself. Therefore, explicit cleanup of resources like
// `net::internal::TaskRunnerGlobals` (for non-test scenarios) is not required
// here as they are managed elsewhere or their lifetime is tied to the process.
//
// For testing scenarios using `SetUpNetTaskRunnersForTesting`, the original
// high-priority task runner is restored.
NetworkServiceScheduler::~NetworkServiceScheduler() {
  if (original_high_priority_task_runner_for_testing_.has_value()) {
    net::internal::GetTaskRunnerGlobals().high_priority_task_runner =
        *original_high_priority_task_runner_for_testing_;
  }
}

NetworkServiceScheduler::NetworkServiceScheduler()
    : sequence_manager_(
          base::sequence_manager::CreateSequenceManagerOnCurrentThread(
              base::sequence_manager::SequenceManager::Settings::Builder()
                  .SetAddQueueTimeToTasks(true)
                  .SetCanRunTasksByBatches(true)
                  .SetPrioritySettings(
                      internal::CreateNetworkServiceTaskPrioritySettings())
                  .SetShouldSampleCPUTime(true)
                  .Build())),
      task_queues_(sequence_manager_.get()) {
  CHECK_EQ(static_cast<size_t>(sequence_manager_->GetPriorityCount()),
           static_cast<size_t>(
               internal::NetworkServiceTaskPriority::kPriorityCount));

  // Set a handler to be called upon completion of each task.
  task_queues_.SetOnTaskCompletedHandler(base::BindRepeating(
      &NetworkServiceScheduler::OnTaskCompleted, base::Unretained(this)));

  // Enable crash keys for the sequence manager to help debug scheduler related
  // crashes.
  sequence_manager_->EnableCrashKeys("network_service_scheduler_async_stack");

  // Set the default task runner for the current thread.
  sequence_manager_->SetDefaultTaskRunner(task_queues_.GetDefaultTaskRunner());
}

void NetworkServiceScheduler::OnTaskCompleted(
    const base::sequence_manager::Task& task,
    base::sequence_manager::TaskQueue::TaskTiming* task_timing,
    base::LazyNow* lazy_now) {
  // Records the end time of the task.
  task_timing->RecordTaskEnd(lazy_now);

  // Records CPU usage for the completed task.
  //
  // Note: Thread time is already subsampled in sequence manager by a factor of
  // `kTaskSamplingRateForRecordingCPUTime`.
  task_timing->RecordUmaOnCpuMetrics("NetworkService.Scheduler.IOThread");
}

void NetworkServiceScheduler::SetUpNetTaskRunners() {
  net::internal::TaskRunnerGlobals& globals =
      net::internal::GetTaskRunnerGlobals();
  globals.high_priority_task_runner = GetTaskRunner(QueueType::kHighPriority);
}

void NetworkServiceScheduler::SetUpNetTaskRunnersForTesting() {
  CHECK(!original_high_priority_task_runner_for_testing_.has_value());
  original_high_priority_task_runner_for_testing_ =
      net::internal::GetTaskRunnerGlobals().high_priority_task_runner;
  SetUpNetTaskRunners();
}

const scoped_refptr<base::SingleThreadTaskRunner>&
NetworkServiceScheduler::GetTaskRunner(QueueType type) const {
  return task_queues_.GetTaskRunner(type);
}

}  // namespace network
