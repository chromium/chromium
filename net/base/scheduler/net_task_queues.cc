// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/scheduler/net_task_queues.h"

#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/pending_task.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/types/pass_key.h"
#include "net/base/request_priority.h"
#include "net/base/scheduler/net_task_priority.h"

namespace net {

namespace {

using internal::NetTaskPriority;
using QueueName = ::perfetto::protos::pbzero::SequenceManagerTask::QueueName;

QueueName GetTaskQueueName(RequestPriority priority) {
  switch (priority) {
    case RequestPriority::THROTTLED:
      return QueueName::NETWORK_SERVICE_THREAD_THROTTLED_TQ;
    case RequestPriority::IDLE:
      return QueueName::NETWORK_SERVICE_THREAD_IDLE_TQ;
    case RequestPriority::LOWEST:
      return QueueName::NETWORK_SERVICE_THREAD_LOWEST_TQ;
    case RequestPriority::LOW:
      return QueueName::NETWORK_SERVICE_THREAD_LOW_TQ;
    case RequestPriority::MEDIUM:
      return QueueName::NETWORK_SERVICE_THREAD_MEDIUM_TQ;
    case RequestPriority::HIGHEST:
      return QueueName::NETWORK_SERVICE_THREAD_HIGHEST_TQ;
  }
}

// LINT.IfChange(PriorityToString)
const char* PriorityToString(RequestPriority priority) {
  switch (priority) {
    case RequestPriority::THROTTLED:
      return "Throttled";
    case RequestPriority::IDLE:
      return "Idle";
    case RequestPriority::LOWEST:
      return "Lowest";
    case RequestPriority::LOW:
      return "Low";
    case RequestPriority::MEDIUM:
      return "Medium";
    case RequestPriority::HIGHEST:
      return "Highest";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/network/histograms.xml:QueueName)

}  // namespace

// Observes task execution on a specific net task queue and notifies
// NetTaskQueues.
class NetworkTaskObserver : public base::TaskObserver {
 public:
  NetworkTaskObserver(RequestPriority priority, NetTaskQueues& net_task_queues)
      : priority_(priority), net_task_queues_(net_task_queues) {}

  void WillProcessTask(const base::PendingTask& pending_task,
                       bool was_blocked_or_low_priority) override {
    net_task_queues_->OnWillProcessTask(base::PassKey<NetworkTaskObserver>(),
                                        priority_, pending_task);
  }
  void DidProcessTask(const base::PendingTask& pending_task) override {}

 private:
  const RequestPriority priority_;
  // `net_task_queues_` outlives this task observer.
  const raw_ref<NetTaskQueues> net_task_queues_;
};

NetTaskQueues::NetTaskQueues(
    base::sequence_manager::SequenceManager* sequence_manager) {
  CreateTaskQueues(sequence_manager);
  CreateNetworkTaskRunners();
}

NetTaskQueues::~NetTaskQueues() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NetTaskQueues::CreateTaskQueues(
    base::sequence_manager::SequenceManager* sequence_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (size_t i = 0; i < task_queues_.size(); ++i) {
    task_queues_[i] = sequence_manager->CreateTaskQueue(
        base::sequence_manager::TaskQueue::Spec(
            GetTaskQueueName(static_cast<RequestPriority>(i))));

    // Set the priority for each task queue.
    //
    // Note the differing priority conventions:
    // base::sequence_manager::TaskQueue::QueuePriority, which
    // NetTaskPriority extends, must be in descending order (e.g.,
    // kHighest is 0), while net::RequestPriority is ascending (a higher index
    // means higher priority). Therefore, we must invert the priority value.
    task_queues_[i]->SetQueuePriority(
        static_cast<size_t>(NetTaskPriority::kPriorityCount) - 1 - i);

    // Create and attach a task observer for each queue.
    task_observers_[i] = std::make_unique<NetworkTaskObserver>(
        static_cast<RequestPriority>(i), *this);
    task_queues_[i]->AddTaskObserver(task_observers_[i].get());
  }
}

void NetTaskQueues::CreateNetworkTaskRunners() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (size_t i = 0; i < task_queues_.size(); ++i) {
    task_runners_[i] = task_queues_[i]->task_runner();
  }
}

void NetTaskQueues::SetOnTaskCompletedHandler(
    base::sequence_manager::TaskQueue::OnTaskCompletedHandler handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& queue : task_queues_) {
    queue->SetOnTaskCompletedHandler(handler);
  }
}

void NetTaskQueues::OnWillProcessTask(base::PassKey<NetworkTaskObserver>,
                                      RequestPriority priority,
                                      const base::PendingTask& pending_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The track name for TRACE_COUNTER must be a const expression.
  switch (priority) {
    case RequestPriority::THROTTLED:
      TRACE_COUNTER(TRACE_DISABLED_BY_DEFAULT("network"),
                    "NumberOfPendingTasksThrottledQueue",
                    GetTaskQueue(priority)->GetNumberOfPendingTasks());
      break;
    case RequestPriority::IDLE:
      TRACE_COUNTER(TRACE_DISABLED_BY_DEFAULT("network"),
                    "NumberOfPendingTasksIdleQueue",
                    GetTaskQueue(priority)->GetNumberOfPendingTasks());
      break;
    case RequestPriority::LOWEST:
      TRACE_COUNTER(TRACE_DISABLED_BY_DEFAULT("network"),
                    "NumberOfPendingTasksLowestQueue",
                    GetTaskQueue(priority)->GetNumberOfPendingTasks());
      break;
    case RequestPriority::LOW:
      TRACE_COUNTER(TRACE_DISABLED_BY_DEFAULT("network"),
                    "NumberOfPendingTasksLowQueue",
                    GetTaskQueue(priority)->GetNumberOfPendingTasks());
      break;
    case RequestPriority::MEDIUM:
      TRACE_COUNTER(TRACE_DISABLED_BY_DEFAULT("network"),
                    "NumberOfPendingTasksMediumQueue",
                    GetTaskQueue(priority)->GetNumberOfPendingTasks());
      break;
    case RequestPriority::HIGHEST:
      TRACE_COUNTER(TRACE_DISABLED_BY_DEFAULT("network"),
                    "NumberOfPendingTasksHighestQueue",
                    GetTaskQueue(priority)->GetNumberOfPendingTasks());
      break;
  }

  // Sample with a 0.001 probability to reduce metrics overhead.
  //
  // TODO(crbug.com/463794414): Rename these histogram prefixes to
  // "Net.Scheduler" once the current "NetworkService.Scheduler" transition is
  // coordinated with metrics users to preserve telemetry durability.
  if (base::ShouldRecordSubsampledMetric(0.001)) {
    size_t total_pending_tasks = 0;
    size_t pending_tasks = 0;
    for (size_t i = 0; i < task_queues_.size(); ++i) {
      const size_t count = task_queues_[i]->GetNumberOfPendingTasks();
      total_pending_tasks += count;
      if (static_cast<RequestPriority>(i) == priority) {
        pending_tasks = count;
      }
    }
    base::UmaHistogramCounts1000(
        "NetworkService.Scheduler.IOThread.NumberOfPendingTasks.Total",
        total_pending_tasks);
    const char* queue_name = PriorityToString(priority);
    base::UmaHistogramCounts1000(
        base::StrCat({"NetworkService.Scheduler.IOThread.NumberOfPendingTasks.",
                      queue_name, "Queue"}),
        pending_tasks);
    base::UmaHistogramTimes(
        base::StrCat({"NetworkService.Scheduler.IOThread.QueuingTime.",
                      queue_name, "Queue"}),
        base::TimeTicks::Now() - pending_task.queue_time);
  }
}

}  // namespace net
