// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SCHEDULER_NETWORK_SERVICE_SCHEDULER_H_
#define SERVICES_NETWORK_SCHEDULER_NETWORK_SERVICE_SCHEDULER_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequence_manager/task_queue.h"
#include "services/network/scheduler/network_service_task_queues.h"

namespace base {
class LazyNow;
namespace sequence_manager {
class SequenceManager;
struct Task;
}  // namespace sequence_manager
}  // namespace base

namespace network {

// Manages the task scheduling for the network service. It sets up a
// `base::sequence_manager::SequenceManager` on the current thread with specific
// priorities defined by NetworkServiceTaskPriority` and manages
// `NetworkServiceTaskQueues` (e.g., default, high priority).
//
// This scheduler is responsible for:
// - Creating and configuring the sequence manager for the network thread.
// - Providing task runners for different priority levels.
// - Integrating with `net::GetTaskRunner` by setting up the task runners in
//   `net::internal::TaskRunnerGlobals`.
// - Optionally handling task completion notifications for metrics.
class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkServiceScheduler {
 public:
  NetworkServiceScheduler();

  NetworkServiceScheduler(const NetworkServiceScheduler&) = delete;
  NetworkServiceScheduler& operator=(const NetworkServiceScheduler&) = delete;

  ~NetworkServiceScheduler();

  using QueueType = NetworkServiceTaskQueues::QueueType;

  // Returns the task runner for the specified `QueueType`.
  const scoped_refptr<base::SingleThreadTaskRunner>& GetTaskRunner(
      QueueType type) const;

  // Sets up the global task runners in `net::internal::TaskRunnerGlobals` so
  // that `net::GetTaskRunner(net::RequestPriority)` returns the appropriate
  // task runner managed by this scheduler.
  void SetUpNetTaskRunners();

  // Similar to `SetUpNetTaskRunners`, but specifically for testing.
  // It saves the current global high-priority task runner and restores it
  // when this `NetworkServiceScheduler` instance is destructed.
  void SetUpNetTaskRunnersForTesting();

 private:
  // Callback for when a task completes on one of the managed queues.
  // Used for recording metrics.
  void OnTaskCompleted(
      const base::sequence_manager::Task& task,
      base::sequence_manager::TaskQueue::TaskTiming* task_timing,
      base::LazyNow* lazy_now);

  std::unique_ptr<base::sequence_manager::SequenceManager> sequence_manager_;
  NetworkServiceTaskQueues task_queues_;

  // Stores the original global high-priority task runner when
  // `SetUpNetTaskRunnersForTesting()` is called, so it can be restored on
  // destruction.
  std::optional<scoped_refptr<base::SingleThreadTaskRunner>>
      original_high_priority_task_runner_for_testing_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SCHEDULER_NETWORK_SERVICE_SCHEDULER_H_
