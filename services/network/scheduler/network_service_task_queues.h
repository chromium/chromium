// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SCHEDULER_NETWORK_SERVICE_TASK_QUEUES_H_
#define SERVICES_NETWORK_SCHEDULER_NETWORK_SERVICE_TASK_QUEUES_H_

#include <array>

#include "base/component_export.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/single_thread_task_runner.h"

namespace base::sequence_manager {
class SequenceManager;
}  // namespace base::sequence_manager

namespace network {

class NetworkServiceTaskObserver;

// Task queues for the network service thread.
//
// Instances must be created and destroyed on the same thread as the
// underlying SequenceManager and instances are not allowed to outlive this
// SequenceManager. All methods of this class must be called from the
// associated thread unless noted otherwise.
//
// This class creates and manages a set of `base::sequence_manager::TaskQueue`s
// with different priorities for the network service thread. It provides
// `base::SingleThreadTaskRunner`s for each of these queues.
class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkServiceTaskQueues {
 public:
  // Defines the types of task queues available.
  enum class QueueType {
    kDefault,
    kHighPriority,
    kMaxValue = kHighPriority,
  };

  // Creates task queues and task runners using the provided `sequence_manager`.
  // The `sequence_manager` must outlive this `NetworkServiceTaskQueues`
  // instance.
  explicit NetworkServiceTaskQueues(
      base::sequence_manager::SequenceManager* sequence_manager);

  // Destroys all managed task queues.
  ~NetworkServiceTaskQueues();

  // Returns the underlying `TaskQueue` for the default priority.
  base::sequence_manager::TaskQueue* GetDefaultTaskQueue() const {
    return GetTaskQueue(QueueType::kDefault);
  }

  // Returns the task runner that should be returned by
  // SingleThreadTaskRunner::GetCurrentDefault().
  // This is typically the task runner for the `QueueType::kDefault`.
  const scoped_refptr<base::SingleThreadTaskRunner>& GetDefaultTaskRunner()
      const {
    return GetTaskRunner(QueueType::kDefault);
  }

  // Returns the task runner for the specified `QueueType`.
  const scoped_refptr<base::SingleThreadTaskRunner>& GetTaskRunner(
      QueueType type) const {
    return task_runners_[static_cast<size_t>(type)];
  }

  // Sets a handler to be called when a task is completed on any of the
  // managed task queues.
  void SetOnTaskCompletedHandler(
      base::sequence_manager::TaskQueue::OnTaskCompletedHandler handler);

 private:
  static constexpr size_t kNumQueueTypes =
      static_cast<size_t>(QueueType::kMaxValue) + 1;

  // Helper to get the underlying `TaskQueue` for a given `QueueType`.
  base::sequence_manager::TaskQueue* GetTaskQueue(QueueType type) const {
    return task_queues_[static_cast<size_t>(type)].get();
  }

  void CreateTaskQueues(
      base::sequence_manager::SequenceManager* sequence_manager);

  void CreateNetworkServiceTaskRunners();

  // Array of handles to the underlying task queues.
  // The index corresponds to the integer value of `QueueType`.
  std::array<base::sequence_manager::TaskQueue::Handle, kNumQueueTypes>
      task_queues_;

  // Array of task observers, one for each `TaskQueue` in `task_queues_`. There
  // is a 1:1 correspondence: `task_observers_[i]` is the task observer for
  // `task_queues_[i]`.
  std::array<std::unique_ptr<NetworkServiceTaskObserver>, kNumQueueTypes>
      task_observers_;

  // Array of task runners, one for each `TaskQueue` in `task_queues_`. There is
  // a 1:1 correspondence: `task_runners_[i]` is the runner for
  // `task_queues_[i]`.
  std::array<scoped_refptr<base::SingleThreadTaskRunner>, kNumQueueTypes>
      task_runners_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SCHEDULER_NETWORK_SERVICE_TASK_QUEUES_H_
