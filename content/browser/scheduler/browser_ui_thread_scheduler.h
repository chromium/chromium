// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_BROWSER_UI_THREAD_SCHEDULER_H_
#define CONTENT_BROWSER_SCHEDULER_BROWSER_UI_THREAD_SCHEDULER_H_

#include <memory>

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
    BrowserUIThreadScheduler* scheduler_ = nullptr;
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

  explicit BrowserUIThreadScheduler(
      base::sequence_manager::SequenceManager* sequence_manager);

  void CommonSequenceManagerSetup(
      base::sequence_manager::SequenceManager* sequence_manager);

  // Called after the feature list is ready and we can set up any policy
  // experiments.
  void PostFeatureListSetup();
  void EnableBrowserPrioritizesNativeWork();
  void EnableAlternatingScheduler();
  // Used in the BrowserPrioritizeNativeWork experiment, when we want to
  // prioritize yielding to java when user input starts and for a short period
  // after it ends.
  BrowserUIThreadScheduler::UserInputActiveHandle OnUserInputStart();
  void DidStartUserInput();
  void DidEndUserInput();
  // After user input has ended CancelNativePriority will be called to inform
  // the SequenceManager to stop prioritizing yielding to native tasks.
  void CancelNativePriority();

  // In production the BrowserUIThreadScheduler will own its SequenceManager,
  // but in tests it may not.
  std::unique_ptr<base::sequence_manager::SequenceManager>
      owned_sequence_manager_;

  BrowserTaskQueues task_queues_;
  scoped_refptr<Handle> handle_;

  // These three variables are used in the BrowserPrioritizeNativeWork finch
  // experiment. False ensures this feature is disabled by default.
  int user_input_active_handle_count = 0;
  bool browser_prioritize_native_work_ = false;
  base::TimeDelta browser_prioritize_native_work_after_input_end_ms_;

  // There five variables are used in the kBrowserPeriodicYieldingToNative finch
  // experiment, |scroll_state_| should indicate the scroll state upton which
  // the yielding to looper delay will depend.
  bool browser_enable_periodic_yielding_native_ = false;
  ScrollState scroll_state_;
  base::TimeDelta yield_to_native_for_normal_input_after_ms_;
  base::TimeDelta yield_to_native_for_fling_input_after_ms_;
  base::TimeDelta yield_to_native_for_default_after_ms_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_BROWSER_UI_THREAD_SCHEDULER_H_
