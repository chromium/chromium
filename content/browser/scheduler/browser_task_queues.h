// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_BROWSER_TASK_QUEUES_H_
#define CONTENT_BROWSER_SCHEDULER_BROWSER_TASK_QUEUES_H_

#include <array>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"

namespace base {
namespace sequence_manager {
class SequenceManager;
}  // namespace sequence_manager
}  // namespace base

namespace content {

// Common task queues for browser threads. This class holds all the queues
// needed by browser threads. This makes it easy for all browser threads to have
// the same queues. Thic class also provides a Handler to act on the queues from
// any thread.
//
// Instances must be created and destroyed on the same thread as the
// underlying SequenceManager and instances are not allowed to outlive this
// SequenceManager. All methods of this class must be called from the
// associated thread unless noted otherwise. If you need to perform operations
// from a different thread use a Handle instance instead.
//
// Attention: All queues are initially disabled, that is, tasks will not be run
// for them.
class CONTENT_EXPORT BrowserTaskQueues {
 public:
  enum class QueueType {
    // Catch all for tasks that don't fit other categories.
    // TODO(alexclarke): Introduce new semantic types as needed to minimize the
    // number of default tasks. Has the same priority as kUserBlocking.
    kDefault,

    // For non-urgent work, that will only execute if there's nothing else to
    // do. Can theoretically be starved indefinitely although that's unlikely in
    // practice.
    kBestEffort,

    // Those are tasks that affect the UI, but not urgent enough to run
    // immediately, those tasks are either deferred or run based on the
    // scheduling policy.
    kDeferrableUserBlocking,

    // base::TaskPriority::kUserBlocking maps to this task queue. It's for tasks
    // that affect the UI immediately after a user interaction. Has the same
    // priority as kDefault.
    kUserBlocking,

    // base::TaskPriority::kUserVisible maps to this task queue. The result of
    // these tasks are visible to the user (in the UI or as a side-effect on the
    // system) but they are not an immediate response to a user interaction.
    kUserVisible,

    // For tasks directly related to handling input events. This also changes
    // the priority of yielding to native (to get the user input events faster).
    // This is higher priority than kUserBlocking.
    kUserInput,

    // For tasks processing navigation network request's response from the
    // network service.
    kNavigationNetworkResponse,

    // For tasks processing ServiceWorker's storage control's response. This has
    // the highest priority during startup, and is updated to normal priority
    // after startup.
    kServiceWorkerStorageControlResponse,

    // For before unload navigation continuation tasks.
    kBeforeUnloadBrowserResponse,

    kMaxValue = kBeforeUnloadBrowserResponse
  };

  static constexpr size_t kNumQueueTypes =
      static_cast<size_t>(QueueType::kMaxValue) + 1;

  // Handle to a BrowserTaskQueues instance that can be used from any thread
  // as all operations are thread safe.
  //
  // If the underlying BrowserTaskQueues is destroyed all methods of this
  // class become no-ops, that is it is safe for this class to outlive its
  // parent BrowserTaskQueues.
  class CONTENT_EXPORT Handle : public base::RefCountedThreadSafe<Handle> {
   public:
    REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

    // Returns the task runner that should be returned by
    // SingleThreadTaskRunner::GetCurrentDefault().
    const scoped_refptr<base::SingleThreadTaskRunner>& GetDefaultTaskRunner() {
      return default_task_runner_;
    }

    const scoped_refptr<base::SingleThreadTaskRunner>& GetBrowserTaskRunner(
        QueueType queue_type) const {
      return browser_task_runners_[static_cast<size_t>(queue_type)];
    }

    // Called after startup is complete, enables all task queues and can
    // be called multiple times.
    void OnStartupComplete();

    // Called quite early in startup after initialising the owning thread's
    // scheduler, before we call RunLoop::Run on the thread.
    // Note: default_task_queue_ doesn't need to be enabled as it is not
    // disabled during startup.
    // Enables all task queues except the effort ones. Can be called multiple
    // times.
    void EnableAllExceptBestEffortQueues();

    // Schedules |on_pending_task_ran| to run when all pending tasks (at the
    // time this method was invoked) have run. Only "runnable" tasks are taken
    // into account, that is tasks from disabled queues are ignored, also this
    // only works reliably for immediate tasks, delayed tasks might or might not
    // run depending on timing.
    //
    // The callback will run on the thread associated with this Handle, unless
    // that thread is no longer accepting tasks; in which case it will be run
    // inline immediately.
    //
    // The recommended usage pattern is:
    // RunLoop run_loop;
    // handle.ScheduleRunAllPendingTasksForTesting(run_loop.QuitClosure());
    // run_loop.Run();
    void ScheduleRunAllPendingTasksForTesting(
        base::OnceClosure on_pending_task_ran);

    // Drop back-pointer to resource about to be freed.
    void OnTaskQueuesDestroyed() { outer_ = nullptr; }

   private:
    friend base::RefCountedThreadSafe<Handle>;

    // Only BrowserTaskQueues can create new instances
    friend class BrowserTaskQueues;

    ~Handle();

    explicit Handle(BrowserTaskQueues* task_queues);

    // |outer_| can only be safely used from a task posted to one of the
    // runners.
    raw_ptr<BrowserTaskQueues> outer_ = nullptr;
    scoped_refptr<base::SingleThreadTaskRunner> control_task_runner_;
    scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
    std::array<scoped_refptr<base::SingleThreadTaskRunner>, kNumQueueTypes>
        browser_task_runners_;
  };

  // Creates queue voters for all task queues created within this
  // BrowserTaskQueues object.
  // NOTE: You can only call this function from the thread that owns the
  // task queues, and you can only use the voters on the same thread.
  std::array<
      std::unique_ptr<base::sequence_manager::TaskQueue::QueueEnabledVoter>,
      kNumQueueTypes>
  CreateQueueEnabledVoters() const;

  // |sequence_manager| must outlive this instance.
  explicit BrowserTaskQueues(
      BrowserThread::ID thread_id,
      base::sequence_manager::SequenceManager* sequence_manager);

  // Destroys all queues.
  ~BrowserTaskQueues();

  scoped_refptr<Handle> GetHandle() { return handle_; }

 private:
  struct QueueData {
   public:
    QueueData();
    ~QueueData();
    QueueData(QueueData&& other);

    base::sequence_manager::TaskQueue::Handle task_queue;
    std::unique_ptr<base::sequence_manager::TaskQueue::QueueEnabledVoter> voter;
  };

  // All these methods can only be called from the associated thread. To make
  // sure that is the case they will always be called from a task posted to the
  // |control_queue_|.
  void StartRunAllPendingTasksForTesting(
      base::ScopedClosureRunner on_pending_task_ran);
  void EndRunAllPendingTasksForTesting(
      base::ScopedClosureRunner on_pending_task_ran);
  void OnStartupComplete();
  void EnableAllExceptBestEffortQueues();

  base::sequence_manager::TaskQueue* GetBrowserTaskQueue(QueueType type) const {
    return queue_data_[static_cast<size_t>(type)].task_queue.get();
  }

  base::sequence_manager::TaskQueue* GetDefaultTaskQueue() const {
    return GetBrowserTaskQueue(QueueType::kDefault);
  }

  std::array<scoped_refptr<base::SingleThreadTaskRunner>, kNumQueueTypes>
  CreateBrowserTaskRunners() const;

  std::array<QueueData, kNumQueueTypes> queue_data_;

  // Helper queue to make sure private methods run on the associated thread. the
  // control queue has maximum priority and will never be disabled.
  base::sequence_manager::TaskQueue::Handle control_queue_;

  // Helper queue to run all pending tasks.
  base::sequence_manager::TaskQueue::Handle run_all_pending_tasks_queue_;
  int run_all_pending_nesting_level_ = 0;

  scoped_refptr<Handle> handle_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_BROWSER_TASK_QUEUES_H_
