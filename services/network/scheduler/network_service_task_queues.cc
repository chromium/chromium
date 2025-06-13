// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/scheduler/network_service_task_queues.h"

#include "base/notreached.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "services/network/scheduler/network_service_task_priority.h"

namespace network {
namespace {

using NetworkServiceTaskPriority =
    ::network::internal::NetworkServiceTaskPriority;
using QueueName = ::perfetto::protos::pbzero::SequenceManagerTask::QueueName;

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
