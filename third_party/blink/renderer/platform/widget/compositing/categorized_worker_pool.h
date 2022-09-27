// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_COMPOSITING_CATEGORIZED_WORKER_POOL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_COMPOSITING_CATEGORIZED_WORKER_POOL_H_

#include "base/callback.h"
#include "base/containers/span.h"
#include "base/synchronization/condition_variable.h"
#include "base/task/post_job.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/simple_thread.h"
#include "cc/raster/task_category.h"
#include "cc/raster/task_graph_runner.h"
#include "cc/raster/task_graph_work_queue.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// A pool of threads used to run categorized work. The work can be scheduled on
// the threads using different interfaces.
// 1. The pool itself implements TaskRunner interface and tasks posted via that
//    interface might run in parallel.
// 2. The pool also implements TaskGraphRunner interface which allows to
//    schedule a graph of tasks with their dependencies.
// 3. CreateSequencedTaskRunner() creates a sequenced task runner that might run
//    in parallel with other instances of sequenced task runners.
class PLATFORM_EXPORT CategorizedWorkerPool : public base::TaskRunner,
                                              public cc::TaskGraphRunner {
 public:
  CategorizedWorkerPool();
  ~CategorizedWorkerPool() override;

  // Get or create the singleton worker pool.
  static CategorizedWorkerPool* GetOrCreate();

  // Overridden from cc::TaskGraphRunner:
  cc::NamespaceToken GenerateNamespaceToken() override;
  void WaitForTasksToFinishRunning(cc::NamespaceToken token) override;
  void CollectCompletedTasks(cc::NamespaceToken token,
                             cc::Task::Vector* completed_tasks) override;

  virtual void FlushForTesting() = 0;

  virtual void Start(int max_concurrency_foreground) = 0;

  // Finish running all the posted tasks (and nested task posted by those tasks)
  // of all the associated task runners.
  // Once all the tasks are executed the method blocks until the threads are
  // terminated.
  virtual void Shutdown() = 0;

  cc::TaskGraphRunner* GetTaskGraphRunner() { return this; }

  // Create a new sequenced task graph runner.
  scoped_refptr<base::SequencedTaskRunner> CreateSequencedTaskRunner();

 protected:
  class CategorizedWorkerPoolSequencedTaskRunner;
  friend class CategorizedWorkerPoolSequencedTaskRunner;

  // Simple Task for the TaskGraphRunner that wraps a closure.
  // This class is used to schedule TaskRunner tasks on the
  // |task_graph_runner_|.
  class ClosureTask : public cc::Task {
   public:
    explicit ClosureTask(base::OnceClosure closure);

    ClosureTask(const ClosureTask&) = delete;
    ClosureTask& operator=(const ClosureTask&) = delete;

    // Overridden from cc::Task:
    void RunOnWorkerThread() override;

   protected:
    ~ClosureTask() override;

   private:
    base::OnceClosure closure_;
  };

  void CollectCompletedTasksWithLockAcquired(cc::NamespaceToken token,
                                             cc::Task::Vector* completed_tasks)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Determines if we should run a new task for the given category. This factors
  // in whether a task is available and whether the count of running tasks is
  // low enough to start a new one.
  bool ShouldRunTaskForCategoryWithLockAcquired(cc::TaskCategory category)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Lock to exclusively access all the following members that are used to
  // implement the TaskRunner and TaskGraphRunner interfaces.
  mutable base::Lock lock_;
  // Stores the tasks to be run, sorted by priority.
  cc::TaskGraphWorkQueue work_queue_ GUARDED_BY(lock_);
  // Namespace used to schedule tasks in the task graph runner.
  const cc::NamespaceToken namespace_token_;
  // List of tasks currently queued up for execution.
  cc::Task::Vector tasks_ GUARDED_BY(lock_);
  // Graph object used for scheduling tasks.
  cc::TaskGraph graph_ GUARDED_BY(lock_);
  // Cached vector to avoid allocation when getting the list of complete
  // tasks.
  cc::Task::Vector completed_tasks_ GUARDED_BY(lock_);
  // Condition variable that is waited on by origin threads until a namespace
  // has finished running all associated tasks.
  base::ConditionVariable has_namespaces_with_finished_running_tasks_cv_;
};

class PLATFORM_EXPORT CategorizedWorkerPoolImpl : public CategorizedWorkerPool {
 public:
  CategorizedWorkerPoolImpl();
  ~CategorizedWorkerPoolImpl() override;

  // Overridden from base::TaskRunner:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override;

  // Overridden from cc::TaskGraphRunner:
  void ScheduleTasks(cc::NamespaceToken token, cc::TaskGraph* graph) override;

  // Runs a task from one of the provided categories. Categories listed first
  // have higher priority.
  void Run(const std::vector<cc::TaskCategory>& categories,
           base::ConditionVariable* has_ready_to_run_tasks_cv);

  // Overridden from CategorizedWorkerPool:
  void FlushForTesting() override;
  void Start(int max_concurrency_foreground) override;
  void Shutdown() override;

 private:
  void ScheduleTasksWithLockAcquired(cc::NamespaceToken token,
                                     cc::TaskGraph* graph)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  // Runs a task from one of the provided categories. Categories listed first
  // have higher priority. Returns false if there were no tasks to run.
  bool RunTaskWithLockAcquired(const std::vector<cc::TaskCategory>& categories)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Run next task for the given category. Caller must acquire |lock_| prior to
  // calling this function and make sure at least one task is ready to run.
  void RunTaskInCategoryWithLockAcquired(cc::TaskCategory category)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Helper function which signals worker threads if tasks are ready to run.
  void SignalHasReadyToRunTasksWithLockAcquired()
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // The actual threads where work is done.
  std::vector<std::unique_ptr<base::SimpleThread>> threads_;

  // Condition variables for foreground and background threads.
  base::ConditionVariable has_task_for_normal_priority_thread_cv_;
  base::ConditionVariable has_task_for_background_priority_thread_cv_;

  // Set during shutdown. Tells Run() to return when no more tasks are pending.
  bool shutdown_ GUARDED_BY(lock_);
};

class PLATFORM_EXPORT CategorizedWorkerPoolJob : public CategorizedWorkerPool {
 public:
  CategorizedWorkerPoolJob();
  ~CategorizedWorkerPoolJob() override;

  // Overridden from base::TaskRunner:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override;

  // Overridden from cc::TaskGraphRunner:
  void ScheduleTasks(cc::NamespaceToken token, cc::TaskGraph* graph) override;

  // Runs a task from one of the provided categories. Categories listed first
  // have higher priority.
  void Run(base::span<const cc::TaskCategory> categories,
           base::JobDelegate* job_delegate);

  // Overridden from CategorizedWorkerPool:
  void FlushForTesting() override;
  void Start(int max_concurrency_foreground) override;
  void Shutdown() override;

 private:
  absl::optional<cc::TaskGraphWorkQueue::PrioritizedTask>
  GetNextTaskToRunWithLockAcquired(
      base::span<const cc::TaskCategory> categories);

  base::JobHandle* ScheduleTasksWithLockAcquired(cc::NamespaceToken token,
                                                 cc::TaskGraph* graph)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Helper function which signals worker threads if tasks are ready to run.
  base::JobHandle* GetJobHandleToNotifyWithLockAcquired()
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  size_t GetMaxJobConcurrency(
      base::span<const cc::TaskCategory> categories) const;

  size_t max_concurrency_foreground_ = 0;

  base::JobHandle background_job_handle_;
  base::JobHandle foreground_job_handle_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WIDGET_COMPOSITING_CATEGORIZED_WORKER_POOL_H_
