// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_CATEGORIZED_WORKER_POOL_H_
#define CONTENT_RENDERER_CATEGORIZED_WORKER_POOL_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/condition_variable.h"
#include "base/task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/simple_thread.h"
#include "cc/raster/task_category.h"
#include "cc/raster/task_graph_runner.h"
#include "cc/raster/task_graph_work_queue.h"
#include "content/common/content_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

// A pool of threads used to run categorized work. The work can be scheduled on
// the threads using different interfaces.
// 1. The pool itself implements TaskRunner interface and tasks posted via that
//    interface might run in parallel.
// 2. The pool also implements TaskGraphRunner interface which allows to
//    schedule a graph of tasks with their dependencies.
// 3. CreateSequencedTaskRunner() creates a sequenced task runner that might run
//    in parallel with other instances of sequenced task runners.
class CONTENT_EXPORT CategorizedWorkerPool : public base::TaskRunner,
                                             public cc::TaskGraphRunner {
 public:
  CategorizedWorkerPool();

  // Overridden from base::TaskRunner:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override;

  // Overridden from cc::TaskGraphRunner:
  cc::NamespaceToken GenerateNamespaceToken() override;
  void ScheduleTasks(cc::NamespaceToken token, cc::TaskGraph* graph) override;
  void WaitForTasksToFinishRunning(cc::NamespaceToken token) override;
  void CollectCompletedTasks(cc::NamespaceToken token,
                             cc::Task::Vector* completed_tasks) override;

  // Runs a task from one of the provided categories. Categories listed first
  // have higher priority.
  void Run(const std::vector<cc::TaskCategory>& categories,
           base::ConditionVariable* has_ready_to_run_tasks_cv);

  void FlushForTesting();

  // Spawn |num_threads| normal threads and 1 background thread and start
  // running work on the worker threads.
  void Start(int num_normal_threads);

  // Finish running all the posted tasks (and nested task posted by those tasks)
  // of all the associated task runners.
  // Once all the tasks are executed the method blocks until the threads are
  // terminated.
  void Shutdown();

  cc::TaskGraphRunner* GetTaskGraphRunner() { return this; }

  // Create a new sequenced task graph runner.
  scoped_refptr<base::SequencedTaskRunner> CreateSequencedTaskRunner();

  // Runs the callback on the specified task-runner once the background worker
  // thread is initialized.
  void SetBackgroundingCallback(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::OnceCallback<void(base::PlatformThreadId)> callback);

 protected:
  ~CategorizedWorkerPool() override;

 private:
  class CategorizedWorkerPoolSequencedTaskRunner;
  friend class CategorizedWorkerPoolSequencedTaskRunner;

  // Simple Task for the TaskGraphRunner that wraps a closure.
  // This class is used to schedule TaskRunner tasks on the
  // |task_graph_runner_|.
  class ClosureTask : public cc::Task {
   public:
    explicit ClosureTask(base::OnceClosure closure);

    // Overridden from cc::Task:
    void RunOnWorkerThread() override;

   protected:
    ~ClosureTask() override;

   private:
    base::OnceClosure closure_;

    DISALLOW_COPY_AND_ASSIGN(ClosureTask);
  };

  void ScheduleTasksWithLockAcquired(cc::NamespaceToken token,
                                     cc::TaskGraph* graph)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void CollectCompletedTasksWithLockAcquired(cc::NamespaceToken token,
                                             cc::Task::Vector* completed_tasks)
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

  // Determines if we should run a new task for the given category. This factors
  // in whether a task is available and whether the count of running tasks is
  // low enough to start a new one.
  bool ShouldRunTaskForCategoryWithLockAcquired(cc::TaskCategory category)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // The actual threads where work is done.
  std::vector<std::unique_ptr<base::SimpleThread>> threads_;

  // Lock to exclusively access all the following members that are used to
  // implement the TaskRunner and TaskGraphRunner interfaces.
  base::Lock lock_;
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
  // Condition variables for foreground and background threads.
  base::ConditionVariable has_task_for_normal_priority_thread_cv_;
  base::ConditionVariable has_task_for_background_priority_thread_cv_;
  // Condition variable that is waited on by origin threads until a namespace
  // has finished running all associated tasks.
  base::ConditionVariable has_namespaces_with_finished_running_tasks_cv_;
  // Set during shutdown. Tells Run() to return when no more tasks are pending.
  bool shutdown_ GUARDED_BY(lock_);

  base::OnceCallback<void(base::PlatformThreadId)> backgrounding_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_CATEGORIZED_WORKER_POOL_H_
