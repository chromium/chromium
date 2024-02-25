// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_BROWSER_UI_THREAD_SCHEDULER_H_
#define CONTENT_BROWSER_SCHEDULER_BROWSER_UI_THREAD_SCHEDULER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/time/time.h"
#include "content/browser/scheduler/browser_task_queues.h"
#include "content/common/content_export.h"

namespace base {
namespace sequence_manager {
class SequenceManager;
}  // namespace sequence_manager
}  // namespace base

namespace content {
class BrowserTaskExecutor;

// The BrowserUIThreadScheduler vends TaskQueues and manipulates them to
// implement scheduling policy. This class is never deleted in production.
class CONTENT_EXPORT BrowserUIThreadScheduler {
 public:
  class UserInputActiveHandle {
   public:
    explicit UserInputActiveHandle(BrowserUIThreadScheduler* scheduler);
    ~UserInputActiveHandle();

    // This is a move only type.
    UserInputActiveHandle(const UserInputActiveHandle&) = delete;
    UserInputActiveHandle& operator=(const UserInputActiveHandle&) = delete;
    UserInputActiveHandle& operator=(UserInputActiveHandle&&);
    UserInputActiveHandle(UserInputActiveHandle&& other);

   private:
    void MoveFrom(UserInputActiveHandle* other);
    // Only this constructor actually creates a UserInputActiveHandle that will
    // inform scheduling decisions.
    raw_ptr<BrowserUIThreadScheduler> scheduler_ = nullptr;
  };

  enum ScrollState { kGestureScrollActive, kFlingActive, kNone };

  using Handle = BrowserTaskQueues::Handle;

  BrowserUIThreadScheduler();

  BrowserUIThreadScheduler(const BrowserUIThreadScheduler&) = delete;
  BrowserUIThreadScheduler& operator=(const BrowserUIThreadScheduler&) = delete;

  ~BrowserUIThreadScheduler();

  static BrowserUIThreadScheduler* Get();

  // Setting the DefaultTaskRunner is up to the caller.
  static std::unique_ptr<BrowserUIThreadScheduler> CreateForTesting(
      base::sequence_manager::SequenceManager* sequence_manager);

  using QueueType = BrowserTaskQueues::QueueType;

  scoped_refptr<Handle> GetHandle() const { return handle_; }
  void OnScrollStateUpdate(ScrollState scroll_state);

 private:
  friend class BrowserTaskExecutor;
  friend class BrowserUIThreadSchedulerTest;

  using QueueEnabledVoter =
      base::sequence_manager::TaskQueue::QueueEnabledVoter;

  explicit BrowserUIThreadScheduler(
      base::sequence_manager::SequenceManager* sequence_manager);

  void CommonSequenceManagerSetup(
      base::sequence_manager::SequenceManager* sequence_manager);

  // Called after the feature list is ready and we can set up any policy
  // experiments.
  void PostFeatureListSetup();
  void EnableBrowserPrioritizesNativeWork();
  void EnableDeferringBrowserUIThreadTasks();

  // Used in the BrowserPrioritizeNativeWork experiment, when we want to
  // prioritize yielding to java when user input starts and for a short period
  // after it ends.
  BrowserUIThreadScheduler::UserInputActiveHandle OnUserInputStart();
  void DidStartUserInput();
  void DidEndUserInput();
  // After user input has ended CancelNativePriority will be called to inform
  // the SequenceManager to stop prioritizing yielding to native tasks.
  void CancelNativePriority();

  // Update the scheduling policy when a scroll becomes active or stops.
  void UpdatePolicyOnScrollStateUpdate(ScrollState old_state,
                                       ScrollState new_state);
  // Updates task queues' state to allow/disallow some queues from running
  // during certain events.
  // Can be expanded to modify queue priorities as well.
  void UpdateTaskQueueStates();

  QueueEnabledVoter& GetBrowserTaskRunnerVoter(QueueType queue_type) {
    return *queue_enabled_voters_[static_cast<size_t>(queue_type)].get();
  }

  // Policy controls the scheduling policy for UI main thread, like which
  // queues get to run at what priority, depending on system state.
  class Policy {
   public:
    Policy() = default;
    ~Policy() = default;

    bool operator==(const Policy& other) const {
      return should_defer_task_queues_ == other.should_defer_task_queues_ &&
             defer_normal_or_lower_priority_tasks_ ==
                 other.defer_normal_or_lower_priority_tasks_ &&
             defer_known_long_running_tasks_ ==
                 other.defer_known_long_running_tasks_;
    }

    bool IsQueueEnabled(BrowserTaskQueues::QueueType task_queue) const;

    // Currently used to defer task queues during scrolls.
    bool should_defer_task_queues_ = false;

    // Those are temporary finch flags used to control different experiment
    // groups inside the |BrowserDeferUIThreadTasks| finch experiment.
    // Each flag signals deferring a different set of task queues.
    // For group 1, |defer_normal_or_lower_priority_tasks_| controls deferring
    // all tasks queues with normal priority or lower during a scroll.
    bool defer_normal_or_lower_priority_tasks_ = false;
    // For group 2, |defer_known_long_running_tasks_| means that some tasks
    // will be posted to the |kDeferrableUserBlocking| and those are the only
    // tasks that should be deferred.
    bool defer_known_long_running_tasks_ = false;
  };

  // In production the BrowserUIThreadScheduler will own its SequenceManager,
  // but in tests it may not.
  std::unique_ptr<base::sequence_manager::SequenceManager>
      owned_sequence_manager_;

  BrowserTaskQueues task_queues_;
  std::array<std::unique_ptr<QueueEnabledVoter>,
             BrowserTaskQueues::kNumQueueTypes>
      queue_enabled_voters_;

  scoped_refptr<Handle> handle_;

  // These three variables are used in the BrowserPrioritizeNativeWork finch
  // experiment. False ensures this feature is disabled by default.
  int user_input_active_handle_count = 0;
  bool browser_prioritize_native_work_ = false;
  base::TimeDelta browser_prioritize_native_work_after_input_end_ms_;

  ScrollState scroll_state_ = ScrollState::kNone;
  Policy current_policy_;

  // This variable is used to control the kBrowserDeferUIThreadTasks finch
  // experiment, false indicates it is disabled by default.
  bool browser_enable_deferring_ui_thread_tasks_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_BROWSER_UI_THREAD_SCHEDULER_H_
