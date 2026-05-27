// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/scheduler/net_task_scheduler.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/task/current_thread.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/features.h"
#include "net/base/request_priority.h"
#include "net/base/scheduler/net_task_priority.h"
#include "net/base/scheduler/sequence_manager_configurator.h"
#include "net/base/task/task_runner.h"

namespace net {

namespace {

// `g_net_task_scheduler` is intentionally leaked on shutdown.
NetTaskScheduler* g_net_task_scheduler = nullptr;

// Network thread extension of CurrentThread.
class CurrentNetworkThread : public ::base::CurrentThread {
 public:
  static base::sequence_manager::internal::SequenceManagerImpl*
  GetCurrentSequenceManagerImpl() {
    return CurrentThread::GetCurrentSequenceManagerImpl();
  }
};

}  // namespace

// static
void NetTaskScheduler::MaybeCreate() {
  if (!IsSequenceManagerConfigured()) {
    return;
  }
  // For testing scenarios, `MaybeCreate` can be called multiple times.
  if (g_net_task_scheduler) {
    return;
  }
  auto* sequence_manager =
      CurrentNetworkThread::GetCurrentSequenceManagerImpl();
  CHECK_EQ(static_cast<size_t>(sequence_manager->GetPriorityCount()),
           static_cast<size_t>(internal::NetTaskPriority::kPriorityCount));
  g_net_task_scheduler = new NetTaskScheduler(sequence_manager);
  g_net_task_scheduler->SetUpNetTaskRunners();
}

// static
std::unique_ptr<NetTaskScheduler>
NetTaskScheduler::CreateForTesting(  // IN-TEST
    base::sequence_manager::SequenceManager* manager) {
  return base::WrapUnique(new NetTaskScheduler(manager));
}

// static
std::unique_ptr<NetTaskScheduler> NetTaskScheduler::CreateForNetTaskEnvironment(
    base::sequence_manager::SequenceManager* sequence_manager) {
  auto scheduler = base::WrapUnique(new NetTaskScheduler(sequence_manager));
  scheduler->SetUpNetTaskRunnersForTesting();  // IN-TEST
  return scheduler;
}

// The NetTaskScheduler's lifetime is effectively static when
// assigned to `g_net_task_scheduler` (in non-testing scenarios).
// Therefore, explicit cleanup of resources like
// `net::internal::TaskRunnerGlobals` (for non-test scenarios) is not required
// here as they are managed elsewhere or their lifetime is tied to the process.
//
// For testing scenarios created via `CreateForTesting()`, the original task
// runners for the thread are restored upon destruction.
NetTaskScheduler::~NetTaskScheduler() {
  if (original_task_runners_for_testing_.has_value()) {
    net::internal::GetTaskRunnerGlobals().task_runners =
        *original_task_runners_for_testing_;
  }
}

NetTaskScheduler::NetTaskScheduler(
    base::sequence_manager::SequenceManager* sequence_manager)
    : task_queues_(sequence_manager) {
  // Enable crash keys for the sequence manager to help debug scheduler related
  // crashes.
  sequence_manager->EnableCrashKeys("network_scheduler_async_stack");
  // Set the default task runner for the current thread.
  sequence_manager->SetDefaultTaskQueue(task_queues_.GetDefaultTaskQueue());
}

void NetTaskScheduler::OnTaskCompleted(
    const base::sequence_manager::Task& task,
    base::sequence_manager::TaskQueue::TaskTiming* task_timing,
    base::LazyNow* lazy_now) {
  // Records the end time of the task.
  task_timing->RecordTaskEnd(lazy_now);

  // Records CPU usage for the completed task.
  //
  // Note: Thread time is already subsampled in sequence manager by a factor of
  // `kTaskSamplingRateForRecordingCPUTime`.
  //
  // TODO(crbug.com/463794414): Rename this histogram prefix to "Net.Scheduler"
  // once the current "NetworkService.Scheduler" metrics transition is fully
  // coordinated with metrics users to preserve telemetry durability.
  task_timing->RecordUmaOnCpuMetrics("NetworkService.Scheduler.IOThread");
}

void NetTaskScheduler::SetUpNetTaskRunners() {
  net::internal::TaskRunnerGlobals& globals =
      net::internal::GetTaskRunnerGlobals();
  if (base::FeatureList::IsEnabled(
          features::kNetworkServicePerPriorityTaskQueues)) {
    for (int i = 0; i < NUM_PRIORITIES; ++i) {
      globals.task_runners[i] = GetTaskRunner(static_cast<RequestPriority>(i));
    }
    return;
  }

  // Unless the feature is enabled, we use two task queues, DEFAULT and
  // HIGHEST.
  static_assert(DEFAULT_PRIORITY < HIGHEST);
  for (int i = 0; i < NUM_PRIORITIES; ++i) {
    globals.task_runners[i] = GetTaskRunner(DEFAULT_PRIORITY);
  }
  // Highest is used only for net::RequestPriority::HIGHEST.
  globals.task_runners[HIGHEST] = GetTaskRunner(HIGHEST);
}

void NetTaskScheduler::SetUpNetTaskRunnersForTesting() {
  CHECK(!original_task_runners_for_testing_.has_value());
  original_task_runners_for_testing_ =
      net::internal::GetTaskRunnerGlobals().task_runners;
  SetUpNetTaskRunners();
}

const scoped_refptr<base::SingleThreadTaskRunner>&
NetTaskScheduler::GetTaskRunner(RequestPriority priority) const {
  return task_queues_.GetTaskRunner(priority);
}

const scoped_refptr<base::SingleThreadTaskRunner>&
NetTaskScheduler::GetDefaultTaskRunner() const {
  return task_queues_.GetDefaultTaskRunner();
}

base::sequence_manager::TaskQueue* NetTaskScheduler::GetDefaultTaskQueue() {
  return task_queues_.GetDefaultTaskQueue();
}

}  // namespace net
