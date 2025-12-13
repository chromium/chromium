// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SCHEDULER_NETWORK_SERVICE_TASK_SCHEDULER_H_
#define SERVICES_NETWORK_SCHEDULER_NETWORK_SERVICE_TASK_SCHEDULER_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "net/base/request_priority.h"
#include "services/network/scheduler/network_service_task_queues.h"

namespace base {
class LazyNow;
namespace sequence_manager {
class SequenceManager;
struct Task;
}  // namespace sequence_manager
}  // namespace base

namespace network {

// Manages task scheduling for the network service. This scheduler assumes
// that `SequenceManager` is already correctly constructed on the current thread
// with specific priorities defined in `NetworkServiceTaskPriority`. This
// scheduler creates and manages `NetworkServiceTaskQueues` (e.g., default, high
// priority).
//
// This scheduler is responsible for:
// - Providing task runners for different priority levels.
// - Integrating with `net::GetTaskRunner` by setting up the task runners in
//   `net::internal::TaskRunnerGlobals`.
// - Handling task completion notifications for metrics.
//
// The scheduler does not own the sequence manager. This scheduler's lifetime
// should outlive the network service.
class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkServiceTaskScheduler {
 public:
  NetworkServiceTaskScheduler(const NetworkServiceTaskScheduler&) = delete;
  NetworkServiceTaskScheduler& operator=(const NetworkServiceTaskScheduler&) =
      delete;

  // Creates and registers a `NetworkServiceTaskScheduler` for the current
  // thread. This function is typically called during NetworkService
  // initialization and will only create a scheduler if
  // `SetSequenceManagerSettings()` was called beforehand (e.g. during child
  // process startup) to configure the thread's sequence manager with the
  // required priority settings.
  static void MaybeCreate();

  // Creates a NetworkServiceTaskScheduler for testing.
  static std::unique_ptr<NetworkServiceTaskScheduler> CreateForTesting();

  ~NetworkServiceTaskScheduler();

  // Returns the task runner for the specified `RequestPriority`.
  const scoped_refptr<base::SingleThreadTaskRunner>& GetTaskRunner(
      net::RequestPriority priority) const;

  // Returns the default task runner.
  const scoped_refptr<base::SingleThreadTaskRunner>& GetDefaultTaskRunner()
      const;

  // Sets up the global `net` task runners to point to this scheduler's task
  // runners. This test-only version saves the original global task runners
  // and restores them upon this scheduler's destruction to prevent side-effects
  // between tests.
  void SetUpNetTaskRunnersForTesting();

 private:
  // Constructor for production, borrows the existing sequence manager.
  explicit NetworkServiceTaskScheduler(
      base::sequence_manager::SequenceManager* sequence_manager);

  // Constructor for testing, takes ownership of `sequence manager_for_testing`.
  explicit NetworkServiceTaskScheduler(
      std::unique_ptr<base::sequence_manager::SequenceManager>
          sequence_manager_for_testing);

  // Sets up the global `net` task runners to point to this scheduler's task
  // runners.
  void SetUpNetTaskRunners();

  // Callback for when a task completes on one of the managed queues.
  // Used for recording metrics.
  void OnTaskCompleted(
      const base::sequence_manager::Task& task,
      base::sequence_manager::TaskQueue::TaskTiming* task_timing,
      base::LazyNow* lazy_now);

  // Sequence manager used only for testing scenarios where the scheduler
  // owns the sequence manager.
  std::unique_ptr<base::sequence_manager::SequenceManager>
      sequence_manager_for_testing_;

  NetworkServiceTaskQueues task_queues_;

  // Stores the original global task runners when
  // `SetUpNetTaskRunnersForTesting()` is called, so it can be restored on
  // destruction.
  std::optional<std::array<scoped_refptr<base::SingleThreadTaskRunner>,
                           net::NUM_PRIORITIES>>
      original_task_runners_for_testing_;

  // Stores the original default task runner before `CreateForTesting()`
  // is called, so it can be restored on destruction.
  std::optional<scoped_refptr<base::SingleThreadTaskRunner>>
      original_default_task_runner_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SCHEDULER_NETWORK_SERVICE_TASK_SCHEDULER_H_
