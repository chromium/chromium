// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/scheduler/network_service_task_scheduler.h"

#include "base/memory/ptr_util.h"
#include "base/task/current_thread.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/task/task_runner.h"
#include "services/network/scheduler/network_service_task_priority.h"

namespace network {

namespace {

// `g_network_service_task_scheduler` is intentionally leaked on shutdown.
NetworkServiceTaskScheduler* g_network_service_task_scheduler = nullptr;

// Set to true if the current thread's SequenceManager is configured correctly
// to support the NetworkServiceTaskScheduler priorities.
//
// TODO(crbug.com/421051258): Make this flag thread local. Currently this flag
// is set on the main thread which starts IO thread.
bool g_is_sequence_manager_configured = false;

// Network service thread extension of CurrentThread.
class CurrentNetworkServiceThread : public ::base::CurrentThread {
 public:
  static base::sequence_manager::internal::SequenceManagerImpl*
  GetCurrentSequenceManagerImpl() {
    return CurrentThread::GetCurrentSequenceManagerImpl();
  }
};

}  // namespace

// static
void NetworkServiceTaskScheduler::MaybeCreate() {
  if (!g_is_sequence_manager_configured) {
    return;
  }
  // For testing scenarios, `MaybeCreate` can be called multiple times.
  if (g_network_service_task_scheduler) {
    return;
  }
  auto* sequence_manager =
      CurrentNetworkServiceThread::GetCurrentSequenceManagerImpl();
  CHECK_EQ(static_cast<size_t>(sequence_manager->GetPriorityCount()),
           static_cast<size_t>(
               internal::NetworkServiceTaskPriority::kPriorityCount));
  g_network_service_task_scheduler =
      new NetworkServiceTaskScheduler(sequence_manager);
  g_network_service_task_scheduler->SetUpNetTaskRunners();
}

// static
std::unique_ptr<NetworkServiceTaskScheduler>
NetworkServiceTaskScheduler::CreateForTesting() {
  return base::WrapUnique(new NetworkServiceTaskScheduler(
      // Use a custom sequence manager for testing, as the current thread might
      // not have one set up with the correct priority settings, and we take
      // ownership of it for cleanup.
      base::sequence_manager::CreateSequenceManagerOnCurrentThread(
          base::sequence_manager::SequenceManager::Settings::Builder()
              .SetPrioritySettings(
                  internal::CreateNetworkServiceTaskPrioritySettings())
              .Build())));
}

// The NetworkServiceTaskScheduler's lifetime is effectively static when
// assigned to `g_network_service_task_scheduler` (in non-testing scenarios).
// Therefore, explicit cleanup of resources like
// `net::internal::TaskRunnerGlobals` (for non-test scenarios) is not required
// here as they are managed elsewhere or their lifetime is tied to the process.
//
// For testing scenarios created via `CreateForTesting()`, the original task
// runners for the thread are restored upon destruction.
NetworkServiceTaskScheduler::~NetworkServiceTaskScheduler() {
  if (original_high_priority_task_runner_for_testing_.has_value()) {
    net::internal::GetTaskRunnerGlobals().high_priority_task_runner =
        *original_high_priority_task_runner_for_testing_;
  }
  if (original_default_task_runner_.has_value()) {
    CurrentNetworkServiceThread::GetCurrentSequenceManagerImpl()
        ->SetDefaultTaskRunner(*original_default_task_runner_);
  }
}

NetworkServiceTaskScheduler::NetworkServiceTaskScheduler(
    base::sequence_manager::SequenceManager* sequence_manager)
    : task_queues_(sequence_manager) {
  // Enable crash keys for the sequence manager to help debug scheduler related
  // crashes.
  sequence_manager->EnableCrashKeys(
      "network_service_task_scheduler_async_stack");
  // Set the default task runner for the current thread.
  sequence_manager->SetDefaultTaskRunner(task_queues_.GetDefaultTaskRunner());
}

NetworkServiceTaskScheduler::NetworkServiceTaskScheduler(
    std::unique_ptr<base::sequence_manager::SequenceManager>
        sequence_manager_for_testing)
    : sequence_manager_for_testing_(std::move(sequence_manager_for_testing)),
      task_queues_(sequence_manager_for_testing_.get()) {
  // Saves the default task runner to restore it later for testing scenarios.
  original_default_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  // Set the default task runner for this scheduler.
  sequence_manager_for_testing_->SetDefaultTaskRunner(
      task_queues_.GetDefaultTaskRunner());
}

// static
void NetworkServiceTaskScheduler::ConfigureSequenceManager(
    base::Thread::Options& options) {
  options.sequence_manager_settings =
      std::make_unique<base::sequence_manager::SequenceManagerSettings>(
          base::sequence_manager::SequenceManager::Settings::Builder()
              .SetPrioritySettings(
                  network::internal::CreateNetworkServiceTaskPrioritySettings())
              .SetMessagePumpType(options.message_pump_type)
              .SetCanRunTasksByBatches(true)
              .SetAddQueueTimeToTasks(true)
              .SetShouldSampleCPUTime(true)
              .Build());
  g_is_sequence_manager_configured = true;
}

void NetworkServiceTaskScheduler::OnTaskCompleted(
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

void NetworkServiceTaskScheduler::SetUpNetTaskRunners() {
  net::internal::TaskRunnerGlobals& globals =
      net::internal::GetTaskRunnerGlobals();
  globals.high_priority_task_runner = GetTaskRunner(QueueType::kHighPriority);
}

void NetworkServiceTaskScheduler::SetUpNetTaskRunnersForTesting() {
  CHECK(!original_high_priority_task_runner_for_testing_.has_value());
  original_high_priority_task_runner_for_testing_ =
      net::internal::GetTaskRunnerGlobals().high_priority_task_runner;
  SetUpNetTaskRunners();
}

const scoped_refptr<base::SingleThreadTaskRunner>&
NetworkServiceTaskScheduler::GetTaskRunner(QueueType type) const {
  return task_queues_.GetTaskRunner(type);
}

}  // namespace network
