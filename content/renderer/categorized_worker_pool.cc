// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/categorized_worker_pool.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "cc/raster/task_category.h"

namespace content {
namespace {

// A thread which forwards to CategorizedWorkerPool::Run with the runnable
// categories.
class CategorizedWorkerPoolThread : public base::SimpleThread {
 public:
  CategorizedWorkerPoolThread(
      const std::string& name_prefix,
      const Options& options,
      CategorizedWorkerPool* pool,
      std::vector<cc::TaskCategory> categories,
      base::ConditionVariable* has_ready_to_run_tasks_cv)
      : SimpleThread(name_prefix, options),
        pool_(pool),
        categories_(categories),
        has_ready_to_run_tasks_cv_(has_ready_to_run_tasks_cv) {}

  void SetBackgroundingCallback(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::OnceCallback<void(base::PlatformThreadId)> callback) {
    DCHECK(!HasStartBeenAttempted());
    background_task_runner_ = std::move(task_runner);
    backgrounding_callback_ = std::move(callback);
  }

  // base::SimpleThread:
  void BeforeRun() override {
    if (backgrounding_callback_) {
      DCHECK(background_task_runner_);
      background_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(backgrounding_callback_), tid()));
    }
  }

  void Run() override { pool_->Run(categories_, has_ready_to_run_tasks_cv_); }

 private:
  CategorizedWorkerPool* const pool_;
  const std::vector<cc::TaskCategory> categories_;
  base::ConditionVariable* const has_ready_to_run_tasks_cv_;

  base::OnceCallback<void(base::PlatformThreadId)> backgrounding_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner_;
};

}  // namespace

// A sequenced task runner which posts tasks to a CategorizedWorkerPool.
class CategorizedWorkerPool::CategorizedWorkerPoolSequencedTaskRunner
    : public base::SequencedTaskRunner {
 public:
  explicit CategorizedWorkerPoolSequencedTaskRunner(
      cc::TaskGraphRunner* task_graph_runner)
      : task_graph_runner_(task_graph_runner),
        namespace_token_(task_graph_runner->GenerateNamespaceToken()) {}

  // Overridden from base::TaskRunner:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    return PostNonNestableDelayedTask(from_here, std::move(task), delay);
  }
  bool RunsTasksInCurrentSequence() const override { return true; }

  // Overridden from base::SequencedTaskRunner:
  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
    // for details.
    CHECK(task);
    base::AutoLock lock(lock_);

    // Remove completed tasks.
    DCHECK(completed_tasks_.empty());
    task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                              &completed_tasks_);

    tasks_.erase(tasks_.begin(), tasks_.begin() + completed_tasks_.size());

    tasks_.push_back(base::MakeRefCounted<ClosureTask>(std::move(task)));
    graph_.Reset();
    for (const auto& graph_task : tasks_) {
      int dependencies = 0;
      if (!graph_.nodes.empty())
        dependencies = 1;

      // Treat any tasks that are enqueued through the SequencedTaskRunner as
      // FOREGROUND priority. We don't have enough information to know the
      // actual priority of such tasks, so we run them as soon as possible.
      cc::TaskGraph::Node node(graph_task, cc::TASK_CATEGORY_FOREGROUND,
                               0u /* priority */, dependencies);
      if (dependencies) {
        graph_.edges.push_back(cc::TaskGraph::Edge(
            graph_.nodes.back().task.get(), node.task.get()));
      }
      graph_.nodes.push_back(std::move(node));
    }
    task_graph_runner_->ScheduleTasks(namespace_token_, &graph_);
    completed_tasks_.clear();
    return true;
  }

 private:
  ~CategorizedWorkerPoolSequencedTaskRunner() override {
    {
      base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
      task_graph_runner_->WaitForTasksToFinishRunning(namespace_token_);
    }
    task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                              &completed_tasks_);
  }

  // Lock to exclusively access all the following members that are used to
  // implement the SequencedTaskRunner interfaces.
  base::Lock lock_;

  cc::TaskGraphRunner* task_graph_runner_;
  // Namespace used to schedule tasks in the task graph runner.
  cc::NamespaceToken namespace_token_;
  // List of tasks currently queued up for execution.
  cc::Task::Vector tasks_;
  // Graph object used for scheduling tasks.
  cc::TaskGraph graph_;
  // Cached vector to avoid allocation when getting the list of complete
  // tasks.
  cc::Task::Vector completed_tasks_;
};

CategorizedWorkerPool::CategorizedWorkerPool()
    : namespace_token_(GenerateNamespaceToken()),
      has_ready_to_run_foreground_tasks_cv_(&lock_),
      has_ready_to_run_background_tasks_cv_(&lock_),
      has_namespaces_with_finished_running_tasks_cv_(&lock_),
      shutdown_(false) {
  // Declare the two ConditionVariables which are used by worker threads to
  // sleep-while-idle as such to avoid throwing off //base heuristics.
  has_ready_to_run_foreground_tasks_cv_.declare_only_used_while_idle();
  has_ready_to_run_background_tasks_cv_.declare_only_used_while_idle();
}

void CategorizedWorkerPool::Start(int num_threads) {
  DCHECK(threads_.empty());

  // Start |num_threads| threads for foreground work, including nonconcurrent
  // foreground work.
  std::vector<cc::TaskCategory> foreground_categories;
  foreground_categories.push_back(cc::TASK_CATEGORY_NONCONCURRENT_FOREGROUND);
  foreground_categories.push_back(cc::TASK_CATEGORY_FOREGROUND);

  for (int i = 0; i < num_threads; i++) {
    base::SimpleThread::Options thread_options;
    // Use same priority for foreground workers as compositor thread.
    thread_options.priority = base::PlatformThread::GetCurrentThreadPriority();
    std::unique_ptr<base::SimpleThread> thread(new CategorizedWorkerPoolThread(
        base::StringPrintf("CompositorTileWorker%d", i + 1).c_str(),
        base::SimpleThread::Options(), this, foreground_categories,
        &has_ready_to_run_foreground_tasks_cv_));
    thread->StartAsync();
    threads_.push_back(std::move(thread));
  }

  // Start a single thread for background work.
  std::vector<cc::TaskCategory> background_categories;
  background_categories.push_back(cc::TASK_CATEGORY_BACKGROUND);

  // Use background priority for background thread.
  base::SimpleThread::Options thread_options;
#if !defined(OS_MACOSX)
  thread_options.priority = base::ThreadPriority::BACKGROUND;
#endif

  auto thread = std::make_unique<CategorizedWorkerPoolThread>(
      "CompositorTileWorkerBackground", thread_options, this,
      background_categories, &has_ready_to_run_background_tasks_cv_);
  if (backgrounding_callback_) {
    thread->SetBackgroundingCallback(std::move(background_task_runner_),
                                     std::move(backgrounding_callback_));
  }
  thread->StartAsync();
  threads_.push_back(std::move(thread));
}

void CategorizedWorkerPool::Shutdown() {
  {
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    WaitForTasksToFinishRunning(namespace_token_);
  }

  CollectCompletedTasks(namespace_token_, &completed_tasks_);
  // Shutdown raster threads.
  {
    base::AutoLock lock(lock_);

    DCHECK(!work_queue_.HasReadyToRunTasks());
    DCHECK(!work_queue_.HasAnyNamespaces());

    DCHECK(!shutdown_);
    shutdown_ = true;

    // Wake up all workers so they exit.
    has_ready_to_run_foreground_tasks_cv_.Broadcast();
    has_ready_to_run_background_tasks_cv_.Broadcast();
  }
  while (!threads_.empty()) {
    threads_.back()->Join();
    threads_.pop_back();
  }
}

// Overridden from base::TaskRunner:
bool CategorizedWorkerPool::PostDelayedTask(const base::Location& from_here,
                                            base::OnceClosure task,
                                            base::TimeDelta delay) {
  base::AutoLock lock(lock_);

  // Remove completed tasks.
  DCHECK(completed_tasks_.empty());
  CollectCompletedTasksWithLockAcquired(namespace_token_, &completed_tasks_);

  base::EraseIf(tasks_, [this](const scoped_refptr<cc::Task>& e) {
    return base::Contains(this->completed_tasks_, e);
  });

  tasks_.push_back(base::MakeRefCounted<ClosureTask>(std::move(task)));
  graph_.Reset();
  for (const auto& graph_task : tasks_) {
    // Delayed tasks are assigned FOREGROUND category, ensuring that they run as
    // soon as possible once their delay has expired.
    graph_.nodes.push_back(
        cc::TaskGraph::Node(graph_task.get(), cc::TASK_CATEGORY_FOREGROUND,
                            0u /* priority */, 0u /* dependencies */));
  }

  ScheduleTasksWithLockAcquired(namespace_token_, &graph_);
  completed_tasks_.clear();
  return true;
}

bool CategorizedWorkerPool::RunsTasksInCurrentSequence() const {
  return true;
}

void CategorizedWorkerPool::Run(
    const std::vector<cc::TaskCategory>& categories,
    base::ConditionVariable* has_ready_to_run_tasks_cv) {
  base::AutoLock lock(lock_);

  while (true) {
    if (!RunTaskWithLockAcquired(categories)) {
      // We are no longer running tasks, which may allow another category to
      // start running. Signal other worker threads.
      SignalHasReadyToRunTasksWithLockAcquired();

      // Exit when shutdown is set and no more tasks are pending.
      if (shutdown_)
        break;

      // Wait for more tasks.
      has_ready_to_run_tasks_cv->Wait();
      continue;
    }
  }
}

void CategorizedWorkerPool::FlushForTesting() {
  base::AutoLock lock(lock_);

  while (!work_queue_.HasFinishedRunningTasksInAllNamespaces()) {
    has_namespaces_with_finished_running_tasks_cv_.Wait();
  }
}

scoped_refptr<base::SequencedTaskRunner>
CategorizedWorkerPool::CreateSequencedTaskRunner() {
  return new CategorizedWorkerPoolSequencedTaskRunner(this);
}

void CategorizedWorkerPool::SetBackgroundingCallback(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::OnceCallback<void(base::PlatformThreadId)> callback) {
  // The callback must be set before the threads have been created.
  DCHECK(threads_.empty());
  backgrounding_callback_ = std::move(callback);
  background_task_runner_ = std::move(task_runner);
}

CategorizedWorkerPool::~CategorizedWorkerPool() {}

cc::NamespaceToken CategorizedWorkerPool::GenerateNamespaceToken() {
  base::AutoLock lock(lock_);
  return work_queue_.GenerateNamespaceToken();
}

void CategorizedWorkerPool::ScheduleTasks(cc::NamespaceToken token,
                                          cc::TaskGraph* graph) {
  TRACE_EVENT2("disabled-by-default-cc.debug",
               "CategorizedWorkerPool::ScheduleTasks", "num_nodes",
               graph->nodes.size(), "num_edges", graph->edges.size());
  {
    base::AutoLock lock(lock_);
    ScheduleTasksWithLockAcquired(token, graph);
  }
}

void CategorizedWorkerPool::ScheduleTasksWithLockAcquired(
    cc::NamespaceToken token,
    cc::TaskGraph* graph) {
  DCHECK(token.IsValid());
  DCHECK(!cc::TaskGraphWorkQueue::DependencyMismatch(graph));
  DCHECK(!shutdown_);

  work_queue_.ScheduleTasks(token, graph);

  // There may be more work available, so wake up another worker thread.
  SignalHasReadyToRunTasksWithLockAcquired();
}

void CategorizedWorkerPool::WaitForTasksToFinishRunning(
    cc::NamespaceToken token) {
  TRACE_EVENT0("disabled-by-default-cc.debug",
               "CategorizedWorkerPool::WaitForTasksToFinishRunning");

  DCHECK(token.IsValid());

  {
    base::AutoLock lock(lock_);

    auto* task_namespace = work_queue_.GetNamespaceForToken(token);

    if (!task_namespace)
      return;

    while (!work_queue_.HasFinishedRunningTasksInNamespace(task_namespace))
      has_namespaces_with_finished_running_tasks_cv_.Wait();

    // There may be other namespaces that have finished running tasks, so wake
    // up another origin thread.
    has_namespaces_with_finished_running_tasks_cv_.Signal();
  }
}

void CategorizedWorkerPool::CollectCompletedTasks(
    cc::NamespaceToken token,
    cc::Task::Vector* completed_tasks) {
  TRACE_EVENT0("disabled-by-default-cc.debug",
               "CategorizedWorkerPool::CollectCompletedTasks");

  {
    base::AutoLock lock(lock_);
    CollectCompletedTasksWithLockAcquired(token, completed_tasks);
  }
}

void CategorizedWorkerPool::CollectCompletedTasksWithLockAcquired(
    cc::NamespaceToken token,
    cc::Task::Vector* completed_tasks) {
  DCHECK(token.IsValid());
  work_queue_.CollectCompletedTasks(token, completed_tasks);
}

bool CategorizedWorkerPool::RunTaskWithLockAcquired(
    const std::vector<cc::TaskCategory>& categories) {
  for (const auto& category : categories) {
    if (ShouldRunTaskForCategoryWithLockAcquired(category)) {
      RunTaskInCategoryWithLockAcquired(category);
      return true;
    }
  }
  return false;
}

void CategorizedWorkerPool::RunTaskInCategoryWithLockAcquired(
    cc::TaskCategory category) {

  lock_.AssertAcquired();

  auto prioritized_task = work_queue_.GetNextTaskToRun(category);

  TRACE_EVENT1("toplevel", "TaskGraphRunner::RunTask", "source_frame_number_",
               prioritized_task.task->frame_number());
  // There may be more work available, so wake up another worker thread.
  SignalHasReadyToRunTasksWithLockAcquired();

  {
    base::AutoUnlock unlock(lock_);

    prioritized_task.task->RunOnWorkerThread();
  }

  auto* task_namespace = prioritized_task.task_namespace;
  work_queue_.CompleteTask(std::move(prioritized_task));

  // If namespace has finished running all tasks, wake up origin threads.
  if (work_queue_.HasFinishedRunningTasksInNamespace(task_namespace))
    has_namespaces_with_finished_running_tasks_cv_.Signal();
}

bool CategorizedWorkerPool::ShouldRunTaskForCategoryWithLockAcquired(
    cc::TaskCategory category) {
  lock_.AssertAcquired();

  if (!work_queue_.HasReadyToRunTasksForCategory(category))
    return false;

  if (category == cc::TASK_CATEGORY_BACKGROUND) {
    // Only run background tasks if there are no foreground tasks running or
    // ready to run.
    size_t num_running_foreground_tasks =
        work_queue_.NumRunningTasksForCategory(
            cc::TASK_CATEGORY_NONCONCURRENT_FOREGROUND) +
        work_queue_.NumRunningTasksForCategory(cc::TASK_CATEGORY_FOREGROUND);
    bool has_ready_to_run_foreground_tasks =
        work_queue_.HasReadyToRunTasksForCategory(
            cc::TASK_CATEGORY_NONCONCURRENT_FOREGROUND) ||
        work_queue_.HasReadyToRunTasksForCategory(cc::TASK_CATEGORY_FOREGROUND);

    if (num_running_foreground_tasks > 0 || has_ready_to_run_foreground_tasks)
      return false;
  }

  // Enforce that only one nonconcurrent task runs at a time.
  if (category == cc::TASK_CATEGORY_NONCONCURRENT_FOREGROUND &&
      work_queue_.NumRunningTasksForCategory(
          cc::TASK_CATEGORY_NONCONCURRENT_FOREGROUND) > 0) {
    return false;
  }

  return true;
}

void CategorizedWorkerPool::SignalHasReadyToRunTasksWithLockAcquired() {
  lock_.AssertAcquired();

  if (ShouldRunTaskForCategoryWithLockAcquired(cc::TASK_CATEGORY_FOREGROUND) ||
      ShouldRunTaskForCategoryWithLockAcquired(
          cc::TASK_CATEGORY_NONCONCURRENT_FOREGROUND)) {
    has_ready_to_run_foreground_tasks_cv_.Signal();
  }

  if (ShouldRunTaskForCategoryWithLockAcquired(cc::TASK_CATEGORY_BACKGROUND)) {
    has_ready_to_run_background_tasks_cv_.Signal();
  }
}

CategorizedWorkerPool::ClosureTask::ClosureTask(base::OnceClosure closure)
    : closure_(std::move(closure)) {}

// Overridden from cc::Task:
void CategorizedWorkerPool::ClosureTask::RunOnWorkerThread() {
  std::move(closure_).Run();
}

CategorizedWorkerPool::ClosureTask::~ClosureTask() {}

}  // namespace content
