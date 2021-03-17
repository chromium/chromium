// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_BROWSER_TASK_QUEUES_H_
#define CONTENT_BROWSER_SCHEDULER_BROWSER_TASK_QUEUES_H_

#include <array>

#include "base/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequence_manager/task_queue.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"

namespace base {
namespace sequence_manager {
class SequenceManager;
class TimeDomain;
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
// from a different thread use the a Handle instance instead.
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

    // For tasks on the critical path up to issuing the initial navigation.
    kBootstrap,

    // For preconnection-related tasks.
    kPreconnection,

    // base::TaskPriority::kUserBlocking maps to this task queue. It's for tasks
    // that affect the UI immediately after a user interaction. Has the same
    // priority as kDefault.
    kUserBlocking,

    // base::TaskPriority::kUserVisible maps to this task queue. The result of
    // these tasks are visible to the user (in the UI or as a side-effect on the
    // system) but they are not an immediate response to a user interaction.
    kUserVisible,

    kMaxValue = kUserVisible
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
    // ThreadTaskRunnerHandle::Get().
    const scoped_refptr<base::SingleThreadTaskRunner>& GetDefaultTaskRunner() {
      return default_task_runner_;
    }

    const scoped_refptr<base::SingleThreadTaskRunner>& GetBrowserTaskRunner(
        QueueType queue_type) const {
      return browser_task_runners_[static_cast<size_t>(queue_type)];
    }

    // Initializes any scheduler experiments. Should be called after
    // FeatureLists have been initialized (which usually happens after task
    // queues are set up).
    void PostFeatureListInitializationSetup();

    // Enables all tasks queues. Can be called multiple times.
    void EnableAllQueues();

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

   private:
    friend base::RefCountedThreadSafe<Handle>;

    // Only BrowserTaskQueues can create new instances
    friend class BrowserTaskQueues;

    ~Handle();

    explicit Handle(BrowserTaskQueues* task_queues);

    // |outer_| can only be safely used from a task posted to one of the
    // runners.
    BrowserTaskQueues* outer_ = nullptr;
    scoped_refptr<base::SingleThreadTaskRunner> control_task_runner_;
    scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
    std::array<scoped_refptr<base::SingleThreadTaskRunner>, kNumQueueTypes>
        browser_task_runners_;
  };

  // |sequence_manager| and |time_domain| must outlive this instance.
  explicit BrowserTaskQueues(
      BrowserThread::ID thread_id,
      base::sequence_manager::SequenceManager* sequence_manager,
      base::sequence_manager::TimeDomain* time_domain);

  // Destroys all queues.
  ~BrowserTaskQueues();

  scoped_refptr<Handle> GetHandle() { return handle_; }

 private:
  // All these methods can only be called from the associated thread. To make
  // sure that is the case they will always be called from a task posted to the
  // |control_queue_|.
  void StartRunAllPendingTasksForTesting(
      base::ScopedClosureRunner on_pending_task_ran);
  void EndRunAllPendingTasksForTesting(
      base::ScopedClosureRunner on_pending_task_ran);
  void EnableAllQueues();
  void EnableAllExceptBestEffortQueues();
  void PostFeatureListInitializationSetup();

  base::sequence_manager::TaskQueue* GetBrowserTaskQueue(QueueType type) const {
    return queue_data_[static_cast<size_t>(type)].task_queue.get();
  }

  std::array<scoped_refptr<base::SingleThreadTaskRunner>, kNumQueueTypes>
  CreateBrowserTaskRunners() const;

  struct QueueData {
    QueueData();
    ~QueueData();
    scoped_refptr<base::sequence_manager::TaskQueue> task_queue;
    std::unique_ptr<base::sequence_manager::TaskQueue::QueueEnabledVoter> voter;
  };
  std::array<QueueData, kNumQueueTypes> queue_data_;

  // Helper queue to make sure private methods run on the associated thread. the
  // control queue has maximum priority and will never be disabled.
  scoped_refptr<base::sequence_manager::TaskQueue> control_queue_;

  // Queue that backs the default TaskRunner registered with SequenceManager.
  // This will be the one returned by ThreadTaskRunnerHandle::Get(). Note this
  // is different from QueueType:kDefault as this queue needs to be enabled from
  // the beginning.
  scoped_refptr<base::sequence_manager::TaskQueue> default_task_queue_;

  // Helper queue to run all pending tasks.
  scoped_refptr<base::sequence_manager::TaskQueue> run_all_pending_tasks_queue_;
  int run_all_pending_nesting_level_ = 0;

  scoped_refptr<Handle> handle_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_BROWSER_TASK_QUEUES_H_
