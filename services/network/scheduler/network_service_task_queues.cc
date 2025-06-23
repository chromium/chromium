// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/scheduler/network_service_task_queues.h"

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "services/network/scheduler/network_service_task_priority.h"

namespace network {

// Observes task execution on a specific network service task queue and records
// metrics.
class NetworkServiceTaskObserver : public base::TaskObserver {
 public:
  explicit NetworkServiceTaskObserver(
      std::string queue_name,
      base::sequence_manager::TaskQueue::Handle* queue)
      : queue_name_(std::move(queue_name)), queue_(queue) {}

  void WillProcessTask(const base::PendingTask& pending_task,
                       bool was_blocked_or_low_priority) override {
    // Sample with a 0.001 probability to reduce metrics overhead.
    if (sampler_.ShouldSample(0.001)) {
      base::UmaHistogramCounts100(
          base::StrCat(
              {"NetworkService.Scheduler.IOThread.NumberOfPendingTasks.",
               queue_name_, "Queue"}),
          (*queue_)->GetNumberOfPendingTasks());
      base::UmaHistogramTimes(
          base::StrCat({"NetworkService.Scheduler.IOThread.QueuingTime.",
                        queue_name_, "Queue"}),
          base::TimeTicks::Now() - pending_task.queue_time);
    }
  }
  void DidProcessTask(const base::PendingTask& pending_task) override {}

 private:
  const std::string queue_name_;
  // `queue_` outlives this task observer.
  raw_ptr<base::sequence_manager::TaskQueue::Handle> queue_;
  const base::MetricsSubSampler sampler_;
};

namespace {

using NetworkServiceTaskPriority =
    ::network::internal::NetworkServiceTaskPriority;
using QueueName = ::perfetto::protos::pbzero::SequenceManagerTask::QueueName;
using QueueType = ::network::NetworkServiceTaskQueues::QueueType;

QueueName GetTaskQueueName(NetworkServiceTaskQueues::QueueType queue_type) {
  switch (queue_type) {
    case NetworkServiceTaskQueues::QueueType::kDefault:
      return QueueName::NETWORK_SERVICE_THREAD_DEFAULT_TQ;
    case NetworkServiceTaskQueues::QueueType::kHighPriority:
      return QueueName::NETWORK_SERVICE_THREAD_HIGH_TQ;
    default:
      NOTREACHED();
  }
}

const char* QueueTypeToString(QueueType type) {
  switch (type) {
    case QueueType::kDefault:
      return "Default";
    case QueueType::kHighPriority:
      return "High";
  }
  NOTREACHED();
}

}  // namespace

NetworkServiceTaskQueues::NetworkServiceTaskQueues(
    base::sequence_manager::SequenceManager* sequence_manager) {
  CreateTaskQueues(sequence_manager);
  CreateNetworkServiceTaskRunners();
}

NetworkServiceTaskQueues::~NetworkServiceTaskQueues() = default;

void NetworkServiceTaskQueues::CreateTaskQueues(
    base::sequence_manager::SequenceManager* sequence_manager) {
  for (size_t i = 0; i < task_queues_.size(); ++i) {
    task_queues_[i] = sequence_manager->CreateTaskQueue(
        base::sequence_manager::TaskQueue::Spec(
            GetTaskQueueName(static_cast<QueueType>(i))));
    task_observers_[i] = std::make_unique<NetworkServiceTaskObserver>(
        QueueTypeToString(static_cast<QueueType>(i)), &task_queues_[i]);
    task_queues_[i]->AddTaskObserver(task_observers_[i].get());
  }

  // Default queue
  GetTaskQueue(QueueType::kDefault)
      ->SetQueuePriority(NetworkServiceTaskPriority::kDefaultPriority);

  // High Priority queue
  GetTaskQueue(QueueType::kHighPriority)
      ->SetQueuePriority(NetworkServiceTaskPriority::kHighPriority);
}

void NetworkServiceTaskQueues::CreateNetworkServiceTaskRunners() {
  for (size_t i = 0; i < task_queues_.size(); ++i) {
    task_runners_[i] = task_queues_[i]->task_runner();
  }
}

void NetworkServiceTaskQueues::SetOnTaskCompletedHandler(
    base::sequence_manager::TaskQueue::OnTaskCompletedHandler handler) {
  for (auto& queue : task_queues_) {
    queue->SetOnTaskCompletedHandler(handler);
  }
}

}  // namespace network
