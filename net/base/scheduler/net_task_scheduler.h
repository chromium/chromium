// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SCHEDULER_NET_TASK_SCHEDULER_H_
#define NET_BASE_SCHEDULER_NET_TASK_SCHEDULER_H_

#include <array>
#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/base/scheduler/net_task_queues.h"

namespace base {
class LazyNow;
namespace sequence_manager {
class SequenceManager;
struct Task;
}  // namespace sequence_manager
}  // namespace base

namespace net {

// Manages task scheduling for the network thread. This scheduler assumes
// that `SequenceManager` is already correctly constructed on the current thread
// with specific priorities defined in `NetTaskPriority`. This
// scheduler creates and manages `NetTaskQueues` (e.g., default, high
// priority).
//
// This scheduler is responsible for:
// - Providing task runners for different priority levels.
// - Integrating with `net::GetTaskRunner` by setting up the task runners in
//   `net::internal::TaskRunnerGlobals`.
// - Handling task completion notifications for metrics.
//
// The scheduler does not own the sequence manager. This scheduler's lifetime
// should cover all network stack operations using it on this thread.
class NET_EXPORT NetTaskScheduler {
 public:
  NetTaskScheduler(const NetTaskScheduler&) = delete;
  NetTaskScheduler& operator=(const NetTaskScheduler&) = delete;

  // Creates and registers a `NetTaskScheduler` for the current
  // thread. This function is typically called during embedder
  // initialization (e.g. in the Network Service) and will only create a
  // scheduler if `SetSequenceManagerSettings()` was called beforehand (e.g.
  // during child process startup) to configure the thread's sequence manager
  // with the required priority settings.
  static void MaybeCreate();

  // Creates a NetTaskScheduler for testing.
  static std::unique_ptr<NetTaskScheduler> CreateForTesting(
      base::sequence_manager::SequenceManager* manager);

  // Creates a NetTaskScheduler for NetTaskEnvironment. This borrows an
  // existing `sequence_manager` (which must be configured with network priority
  // settings) and redirects global task runners for testing.
  static std::unique_ptr<NetTaskScheduler> CreateForNetTaskEnvironment(
      base::sequence_manager::SequenceManager* sequence_manager);

  ~NetTaskScheduler();

  // Returns the task runner for the specified `RequestPriority`.
  const scoped_refptr<base::SingleThreadTaskRunner>& GetTaskRunner(
      RequestPriority priority) const;

  // Returns the default task runner.
  const scoped_refptr<base::SingleThreadTaskRunner>& GetDefaultTaskRunner()
      const;

  base::sequence_manager::TaskQueue* GetDefaultTaskQueue();

  // Sets up the global `net` task runners to point to this scheduler's task
  // runners. This test-only version saves the original global task runners
  // and restores them upon this scheduler's destruction to prevent side-effects
  // between tests.
  void SetUpNetTaskRunnersForTesting();

 private:
  // Constructor for production, borrows the existing sequence manager.
  explicit NetTaskScheduler(
      base::sequence_manager::SequenceManager* sequence_manager);

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

  NetTaskQueues task_queues_;

  // Stores the original global task runners when
  // `SetUpNetTaskRunnersForTesting()` is called, so it can be restored on
  // destruction.
  std::optional<
      std::array<scoped_refptr<base::SingleThreadTaskRunner>, NUM_PRIORITIES>>
      original_task_runners_for_testing_;
};

}  // namespace net

#endif  // NET_BASE_SCHEDULER_NET_TASK_SCHEDULER_H_
