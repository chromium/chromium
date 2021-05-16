// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_AGENT_SCHEDULING_STRATEGY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_AGENT_SCHEDULING_STRATEGY_H_

#include "base/sequence_checker.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {
namespace scheduler {

// Abstract class that can be consulted to determine task queue priorities and
// scheduling policies, that are based on the queue's Agent.
// Strategies should only be accessed from the main thread.
class PLATFORM_EXPORT AgentSchedulingStrategy {
 public:
  enum class ShouldUpdatePolicy {
    kNo,
    kYes,
  };

  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    // Delegate should call OnDelayPassed after |delay| has passed, and pass
    // |frame_scheduler| as a parameter.
    virtual void OnSetTimer(const FrameSchedulerImpl& frame_scheduler,
                            base::TimeDelta delay) = 0;
  };

  AgentSchedulingStrategy(const AgentSchedulingStrategy&) = delete;
  AgentSchedulingStrategy(AgentSchedulingStrategy&&) = delete;

  virtual ~AgentSchedulingStrategy();

  static std::unique_ptr<AgentSchedulingStrategy> Create(Delegate& delegate);

  // The following functions need to be called as appropriate to manage the
  // strategy's internal state. Will return |kYes| when a policy update should
  // be triggered.
  virtual ShouldUpdatePolicy OnFrameAdded(
      const FrameSchedulerImpl& frame_scheduler) WARN_UNUSED_RESULT = 0;
  virtual ShouldUpdatePolicy OnFrameRemoved(
      const FrameSchedulerImpl& frame_scheduler) WARN_UNUSED_RESULT = 0;
  virtual ShouldUpdatePolicy OnMainFrameFirstMeaningfulPaint(
      const FrameSchedulerImpl& frame_scheduler) WARN_UNUSED_RESULT = 0;
  // FMP is not reported consistently, so input events are used as a failsafe
  // to make sure frames aren't considered waiting for FMP indefinitely. Should
  // not be called for mouse move events.
  virtual ShouldUpdatePolicy OnInputEvent() WARN_UNUSED_RESULT = 0;
  virtual ShouldUpdatePolicy OnDocumentChangedInMainFrame(
      const FrameSchedulerImpl& frame_scheduler) WARN_UNUSED_RESULT = 0;
  virtual ShouldUpdatePolicy OnMainFrameLoad(
      const FrameSchedulerImpl& frame_scheduler) WARN_UNUSED_RESULT = 0;
  // OnDelayPassed should be called by Delegate after the appropriate delay.
  virtual ShouldUpdatePolicy OnDelayPassed(
      const FrameSchedulerImpl& frame_scheduler) WARN_UNUSED_RESULT = 0;

  // The following functions should be consulted when making scheduling
  // decisions. Will return |absl::optional| containing the desired value, or
  // |nullopt| to signify that the original scheduler's decision should not be
  // changed.
  virtual absl::optional<bool> QueueEnabledState(
      const MainThreadTaskQueue& task_queue) const = 0;
  virtual absl::optional<base::sequence_manager::TaskQueue::QueuePriority>
  QueuePriority(const MainThreadTaskQueue& task_queue) const = 0;

  // Returns true if the strategy is interested in getting input event
  // notifications. This is *the only* method that may be called from different
  // threads.
  virtual bool ShouldNotifyOnInputEvent() const = 0;

 protected:
  AgentSchedulingStrategy() = default;

  // Check that the strategy is used from the right (= main) thread. Should be
  // called from all public methods except ShouldNotifyOnInput().
  void VerifyValidSequence() const;

 private:
  SEQUENCE_CHECKER(main_thread_sequence_checker_);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_AGENT_SCHEDULING_STRATEGY_H_
