// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_BROWSER_UI_THREAD_SCHEDULER_H_
#define CONTENT_BROWSER_SCHEDULER_BROWSER_UI_THREAD_SCHEDULER_H_

#include <memory>

#include "base/task/sequence_manager/task_queue.h"
#include "content/browser/scheduler/browser_task_queues.h"
#include "content/common/content_export.h"

namespace base {
namespace sequence_manager {
class SequenceManager;
class TimeDomain;
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

  using Handle = BrowserTaskQueues::Handle;

  BrowserUIThreadScheduler();
  ~BrowserUIThreadScheduler();

  // Setting the DefaultTaskRunner is up to the caller.
  static std::unique_ptr<BrowserUIThreadScheduler> CreateForTesting(
      base::sequence_manager::SequenceManager* sequence_manager,
      base::sequence_manager::TimeDomain* time_domain);

  using QueueType = BrowserTaskQueues::QueueType;

  scoped_refptr<Handle> GetHandle() const { return handle_; }

 private:
  friend class BrowserTaskExecutor;

  BrowserUIThreadScheduler(
      base::sequence_manager::SequenceManager* sequence_manager,
      base::sequence_manager::TimeDomain* time_domain);

  void CommonSequenceManagerSetup(
      base::sequence_manager::SequenceManager* sequence_manager);

  // Called after the feature list is ready and we can set up any policy
  // experiments.
  void PostFeatureListSetup();
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

  // These four variables are used in the BrowserPrioritizeNativeWork finch
  // experiment. False ensures this feature is disabled by default.
  int user_input_active_handle_count = 0;
  bool browser_prioritize_native_work_ = false;
  base::TimeDelta browser_prioritize_native_work_after_input_end_ms_;

  DISALLOW_COPY_AND_ASSIGN(BrowserUIThreadScheduler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_BROWSER_UI_THREAD_SCHEDULER_H_
