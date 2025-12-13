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
#include "net/base/request_priority.h"

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
  // Creates task queues and task runners using the provided `sequence_manager`.
  // The `sequence_manager` must outlive this `NetworkServiceTaskQueues`
  // instance.
  explicit NetworkServiceTaskQueues(
      base::sequence_manager::SequenceManager* sequence_manager);

  // Destroys all managed task queues.
  ~NetworkServiceTaskQueues();

  // Returns the task runner that should be returned by
  // SingleThreadTaskRunner::GetCurrentDefault().
  // This is typically the task runner for the DEFAULT priority.
  const scoped_refptr<base::SingleThreadTaskRunner>& GetDefaultTaskRunner()
      const {
    return GetTaskRunner(net::RequestPriority::DEFAULT_PRIORITY);
  }

  // Returns the task runner for the specified `RequestPriority`.
  const scoped_refptr<base::SingleThreadTaskRunner>& GetTaskRunner(
      net::RequestPriority priority) const {
    return task_runners_[static_cast<size_t>(priority)];
  }

  // Sets a handler to be called when a task is completed on any of the
  // managed task queues.
  void SetOnTaskCompletedHandler(
      base::sequence_manager::TaskQueue::OnTaskCompletedHandler handler);

 private:
  // Helper to get the underlying `TaskQueue` for a given priority.
  base::sequence_manager::TaskQueue* GetTaskQueue(
      net::RequestPriority priority) const {
    return task_queues_[static_cast<size_t>(priority)].get();
  }

  void CreateTaskQueues(
      base::sequence_manager::SequenceManager* sequence_manager);

  void CreateNetworkServiceTaskRunners();

  // Array of handles to the underlying task queues.
  std::array<base::sequence_manager::TaskQueue::Handle, net::NUM_PRIORITIES>
      task_queues_;

  // Array of task observers, one for each `TaskQueue` in `task_queues_`. There
  // is a 1:1 correspondence: `task_observers_[i]` is the task observer for
  // `task_queues_[i]`.
  std::array<std::unique_ptr<NetworkServiceTaskObserver>, net::NUM_PRIORITIES>
      task_observers_;

  // Array of task runners, one for each `TaskQueue` in `task_queues_`. There is
  // a 1:1 correspondence: `task_runners_[i]` is the runner for
  // `task_queues_[i]`.
  std::array<scoped_refptr<base::SingleThreadTaskRunner>, net::NUM_PRIORITIES>
      task_runners_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SCHEDULER_NETWORK_SERVICE_TASK_QUEUES_H_
