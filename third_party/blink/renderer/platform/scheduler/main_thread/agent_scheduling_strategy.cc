// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/agent_scheduling_strategy.h"

#include <algorithm>
#include <memory>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/pollable_thread_safe_flag.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"

namespace blink {
namespace scheduler {
namespace {

using ::base::sequence_manager::TaskQueue;

using PrioritisationType =
    ::blink::scheduler::MainThreadTaskQueue::QueueTraits::PrioritisationType;

// Scheduling strategy that does nothing. This emulates the "current" shipped
// behavior, and is the default unless overridden. Corresponds to the
// |kNoOpStrategy| feature.
class NoOpStrategy final : public AgentSchedulingStrategy {
 public:
  NoOpStrategy() = default;

  ShouldUpdatePolicy OnFrameAdded(const FrameSchedulerImpl&) override {
    VerifyValidSequence();
    return ShouldUpdatePolicy::kNo;
  }
  ShouldUpdatePolicy OnFrameRemoved(const FrameSchedulerImpl&) override {
    VerifyValidSequence();
    return ShouldUpdatePolicy::kNo;
  }
  ShouldUpdatePolicy OnMainFrameFirstMeaningfulPaint(
      const FrameSchedulerImpl&) override {
    VerifyValidSequence();
    return ShouldUpdatePolicy::kNo;
  }
  ShouldUpdatePolicy OnInputEvent() override {
    VerifyValidSequence();
    return ShouldUpdatePolicy::kNo;
  }
  ShouldUpdatePolicy OnDocumentChangedInMainFrame(
      const FrameSchedulerImpl&) override {
    VerifyValidSequence();
    return ShouldUpdatePolicy::kNo;
  }
  ShouldUpdatePolicy OnMainFrameLoad(const FrameSchedulerImpl&) override {
    VerifyValidSequence();
    return ShouldUpdatePolicy::kNo;
  }
  ShouldUpdatePolicy OnDelayPassed(const FrameSchedulerImpl&) override {
    VerifyValidSequence();
    return ShouldUpdatePolicy::kNo;
  }

  absl::optional<bool> QueueEnabledState(
      const MainThreadTaskQueue& task_queue) const override {
    VerifyValidSequence();
    return absl::nullopt;
  }
  absl::optional<TaskQueue::QueuePriority> QueuePriority(
      const MainThreadTaskQueue& task_queue) const override {
    VerifyValidSequence();
    return absl::nullopt;
  }

  bool ShouldNotifyOnInputEvent() const override { return false; }
};

// Strategy that keeps track of main frames reaching a certain signal to make
// scheduling decisions. The exact behavior will be determined by parameter
// values.
class TrackMainFrameSignal final : public AgentSchedulingStrategy {
 public:
  TrackMainFrameSignal(Delegate& delegate,
                       PerAgentAffectedQueues affected_queue_types,
                       PerAgentSlowDownMethod method,
                       PerAgentSignal signal,
                       base::TimeDelta delay)
      : delegate_(delegate),
        affected_queue_types_(affected_queue_types),
        method_(method),
        signal_(signal),
        delay_(delay),
        waiting_for_input_(&waiting_for_input_lock_) {
    DCHECK(signal != PerAgentSignal::kDelayOnly || !delay.is_zero())
        << "Delay duration can not be zero when using |kDelayOnly|.";
  }

  ShouldUpdatePolicy OnFrameAdded(
      const FrameSchedulerImpl& frame_scheduler) override {
    VerifyValidSequence();
    return OnNewDocument(frame_scheduler);
  }

  ShouldUpdatePolicy OnFrameRemoved(
      const FrameSchedulerImpl& frame_scheduler) override {
    VerifyValidSequence();
    if (frame_scheduler.GetFrameType() !=
        FrameScheduler::FrameType::kMainFrame) {
      return ShouldUpdatePolicy::kNo;
    }

    main_frames_.erase(&frame_scheduler);
    main_frames_waiting_for_signal_.erase(&frame_scheduler);
    if (main_frames_waiting_for_signal_.IsEmpty())
      SetWaitingForInput(false);

    // TODO(talp): If the frame wasn't in the set to begin with (e.g.: because
    //  it already hit FMP), or if there are still other frames in the set,
    //  then we may not have to trigger a policy update. (But what about cases
    //  where the current agent just changed from main to non-main?)
    return ShouldUpdatePolicy::kYes;
  }

  ShouldUpdatePolicy OnMainFrameFirstMeaningfulPaint(
      const FrameSchedulerImpl& frame_scheduler) override {
    VerifyValidSequence();
    DCHECK(frame_scheduler.GetFrameType() ==
           FrameScheduler::FrameType::kMainFrame);

    return OnSignal(frame_scheduler, PerAgentSignal::kFirstMeaningfulPaint);
  }

  ShouldUpdatePolicy OnInputEvent() override {
    VerifyValidSequence();

    // We only use input as a fail-safe for FMP, other signals are more
    // reliable.
    DCHECK_EQ(signal_, PerAgentSignal::kFirstMeaningfulPaint)
        << "OnInputEvent should only be called for FMP-based strategies.";

    if (main_frames_waiting_for_signal_.IsEmpty())
      return ShouldUpdatePolicy::kNo;

    // Ideally we would like to only remove the frame the input event is related
    // to, but we don't currently have that information. One suggestion (by
    // altimin@) is to attribute it to a widget, and apply it to all frames on
    // the page the widget is on.
    main_frames_waiting_for_signal_.clear();
    SetWaitingForInput(false);
    return ShouldUpdatePolicy::kYes;
  }

  ShouldUpdatePolicy OnDocumentChangedInMainFrame(
      const FrameSchedulerImpl& frame_scheduler) override {
    VerifyValidSequence();
    return OnNewDocument(frame_scheduler);
  }

  ShouldUpdatePolicy OnMainFrameLoad(
      const FrameSchedulerImpl& frame_scheduler) override {
    VerifyValidSequence();
    DCHECK(frame_scheduler.GetFrameType() ==
           FrameScheduler::FrameType::kMainFrame);

    return OnSignal(frame_scheduler, PerAgentSignal::kOnLoad);
  }

  ShouldUpdatePolicy OnDelayPassed(
      const FrameSchedulerImpl& frame_scheduler) override {
    VerifyValidSequence();
    return SignalReached(frame_scheduler);
  }

  absl::optional<bool> QueueEnabledState(
      const MainThreadTaskQueue& task_queue) const override {
    VerifyValidSequence();

    if (method_ == PerAgentSlowDownMethod::kDisable &&
        ShouldAffectQueue(task_queue)) {
      return false;
    }

    return absl::nullopt;
  }

  absl::optional<TaskQueue::QueuePriority> QueuePriority(
      const MainThreadTaskQueue& task_queue) const override {
    VerifyValidSequence();

    if (method_ == PerAgentSlowDownMethod::kBestEffort &&
        ShouldAffectQueue(task_queue)) {
      return TaskQueue::QueuePriority::kBestEffortPriority;
    }

    return absl::nullopt;
  }

  bool ShouldNotifyOnInputEvent() const override {
    if (signal_ != PerAgentSignal::kFirstMeaningfulPaint)
      return false;

    return waiting_for_input_.IsSet();
  }

 private:
  ShouldUpdatePolicy OnNewDocument(const FrameSchedulerImpl& frame_scheduler) {
    // For now we *always* return kYes here. It might be possible to optimize
    // this, but there are a number of tricky cases that need to be taken into
    // account here: (i) a non-main frame could have navigated between a main
    // and a non-main agent, possibly requiring policy update for that frame, or
    // (ii) main frame navigated to a different agent, potentially changing the
    // main/non-main classification for both the "previous" and "current" agents
    // and requiring their policies be updated.

    if (frame_scheduler.GetFrameType() !=
        FrameScheduler::FrameType::kMainFrame) {
      return ShouldUpdatePolicy::kYes;
    }

    if (signal_ == PerAgentSignal::kDelayOnly)
      delegate_.OnSetTimer(frame_scheduler, delay_);
    else if (signal_ == PerAgentSignal::kFirstMeaningfulPaint)
      SetWaitingForInput(true);

    main_frames_.insert(&frame_scheduler);

    // Only add ordinary page frames to the set of waiting frames, as
    // non-ordinary ones don't report any signals.
    if (frame_scheduler.IsOrdinary())
      main_frames_waiting_for_signal_.insert(&frame_scheduler);

    return ShouldUpdatePolicy::kYes;
  }

  bool ShouldAffectQueue(const MainThreadTaskQueue& task_queue) const {
    // Queues that don't have a frame scheduler are, by definition, not
    // associated with a frame (or agent).
    if (!task_queue.GetFrameScheduler())
      return false;

    if (affected_queue_types_ == PerAgentAffectedQueues::kTimerQueues &&
        task_queue.GetPrioritisationType() !=
            PrioritisationType::kJavaScriptTimer) {
      return false;
    }

    // Don't do anything if all main frames have reached the signal.
    if (main_frames_waiting_for_signal_.IsEmpty())
      return false;

    // Otherwise, affect the queue only if it doesn't belong to any main agent.
    base::UnguessableToken agent_cluster_id =
        task_queue.GetFrameScheduler()->GetAgentClusterId();
    return std::all_of(main_frames_.begin(), main_frames_.end(),
                       [agent_cluster_id](const FrameSchedulerImpl* frame) {
                         return frame->GetAgentClusterId() != agent_cluster_id;
                       });
  }

  ShouldUpdatePolicy OnSignal(const FrameSchedulerImpl& frame_scheduler,
                              PerAgentSignal signal) {
    if (signal != signal_)
      return ShouldUpdatePolicy::kNo;

    // If there is no delay, then we have reached the awaited signal.
    if (delay_.is_zero()) {
      return SignalReached(frame_scheduler);
    }

    // No need to update policy if we have to wait for a delay.
    delegate_.OnSetTimer(frame_scheduler, delay_);
    return ShouldUpdatePolicy::kNo;
  }

  ShouldUpdatePolicy SignalReached(const FrameSchedulerImpl& frame_scheduler) {
    main_frames_waiting_for_signal_.erase(&frame_scheduler);
    if (main_frames_waiting_for_signal_.IsEmpty())
      SetWaitingForInput(false);

    // TODO(talp): If the frame wasn't in the set to begin with (e.g.: because
    //  an input event cleared it), or if there are still other frames in the
    //  set, then we may not have to trigger a policy update.
    return ShouldUpdatePolicy::kYes;
  }

  Delegate& delegate_;
  const PerAgentAffectedQueues affected_queue_types_;
  const PerAgentSlowDownMethod method_;
  const PerAgentSignal signal_;
  const base::TimeDelta delay_;

  WTF::HashSet<const FrameSchedulerImpl*> main_frames_;
  WTF::HashSet<const FrameSchedulerImpl*> main_frames_waiting_for_signal_;

  base::Lock waiting_for_input_lock_;
  PollableThreadSafeFlag waiting_for_input_;
  void SetWaitingForInput(bool waiting_for_input) {
    if (waiting_for_input_.IsSet() != waiting_for_input) {
      base::AutoLock lock(waiting_for_input_lock_);
      waiting_for_input_.SetWhileLocked(waiting_for_input);
    }
  }
};
}  // namespace

AgentSchedulingStrategy::~AgentSchedulingStrategy() {
  VerifyValidSequence();
}

std::unique_ptr<AgentSchedulingStrategy> AgentSchedulingStrategy::Create(
    Delegate& delegate) {
  if (!base::FeatureList::IsEnabled(kPerAgentSchedulingExperiments))
    return std::make_unique<NoOpStrategy>();

  return std::make_unique<TrackMainFrameSignal>(
      delegate, kPerAgentQueues.Get(), kPerAgentMethod.Get(),
      kPerAgentSignal.Get(),
      base::TimeDelta::FromMilliseconds(kPerAgentDelayMs.Get()));
}

void AgentSchedulingStrategy::VerifyValidSequence() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
}

}  // namespace scheduler
}  // namespace blink
