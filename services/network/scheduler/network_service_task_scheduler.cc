// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/scheduler/network_service_task_scheduler.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/task/current_thread.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/request_priority.h"
#include "net/base/task/task_runner.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_service_task_priority.h"
#include "services/network/public/cpp/sequence_manager_configurator.h"

namespace network {

namespace {

// `g_network_service_task_scheduler` is intentionally leaked on shutdown.
NetworkServiceTaskScheduler* g_network_service_task_scheduler = nullptr;

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
  if (!IsSequenceManagerConfigured()) {
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
              .SetPrioritySettings(CreateNetworkServiceTaskPrioritySettings())
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
  if (original_task_runners_for_testing_.has_value()) {
    net::internal::GetTaskRunnerGlobals().task_runners =
        *original_task_runners_for_testing_;
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
  sequence_manager->EnableCrashKeys("network_service_scheduler_async_stack");
  // Set the default task runner for the current thread.
  sequence_manager->SetDefaultTaskRunner(GetDefaultTaskRunner());
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
  if (base::FeatureList::IsEnabled(
          features::kNetworkServicePerPriorityTaskQueues)) {
    for (int i = 0; i < net::NUM_PRIORITIES; ++i) {
      globals.task_runners[i] =
          GetTaskRunner(static_cast<net::RequestPriority>(i));
    }
    return;
  }

  // Unless the feature is enabled, we use two task queues, DEFAULT and
  // HIGHEST.
  static_assert(net::RequestPriority::DEFAULT_PRIORITY <
                net::RequestPriority::HIGHEST);
  for (int i = 0; i < net::NUM_PRIORITIES; ++i) {
    globals.task_runners[i] =
        GetTaskRunner(net::RequestPriority::DEFAULT_PRIORITY);
  }
  // Highest is used only for net::RequestPriority::HIGHEST.
  globals.task_runners[net::RequestPriority::HIGHEST] =
      GetTaskRunner(net::RequestPriority::HIGHEST);
}

void NetworkServiceTaskScheduler::SetUpNetTaskRunnersForTesting() {
  CHECK(!original_task_runners_for_testing_.has_value());
  original_task_runners_for_testing_ =
      net::internal::GetTaskRunnerGlobals().task_runners;
  SetUpNetTaskRunners();
}

const scoped_refptr<base::SingleThreadTaskRunner>&
NetworkServiceTaskScheduler::GetTaskRunner(
    net::RequestPriority priority) const {
  return task_queues_.GetTaskRunner(priority);
}

const scoped_refptr<base::SingleThreadTaskRunner>&
NetworkServiceTaskScheduler::GetDefaultTaskRunner() const {
  return task_queues_.GetDefaultTaskRunner();
}

}  // namespace network
