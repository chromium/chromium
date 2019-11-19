/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_ANIMATION_SMIL_TIME_CONTAINER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_ANIMATION_SMIL_TIME_CONTAINER_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/svg/animation/priority_queue.h"
#include "third_party/blink/renderer/core/svg/animation/smil_animation_sandwich.h"
#include "third_party/blink/renderer/platform/graphics/image_animation_policy.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Document;
class SMILTime;
class SVGElement;
class SVGSVGElement;

class SMILTimeContainer final : public GarbageCollected<SMILTimeContainer> {
 public:
  // Sorted list
  using AnimationId = std::pair<WeakMember<SVGElement>, QualifiedName>;
  using AnimationsMap = HeapHashMap<AnimationId, Member<SMILAnimationSandwich>>;

  explicit SMILTimeContainer(SVGSVGElement& owner);
  ~SMILTimeContainer();

  void Schedule(SVGSMILElement*, SVGElement*, const QualifiedName&);
  void Reschedule(SVGSMILElement*);
  void Unschedule(SVGSMILElement*, SVGElement*, const QualifiedName&);

  // Returns the time we are currently updating.
  SMILTime Elapsed() const;
  // Returns the current time in the document.
  SMILTime CurrentDocumentTime() const;

  bool IsPaused() const;
  bool IsStarted() const;

  void Start();
  void Pause();
  void Unpause();
  void SetElapsed(SMILTime);

  void ServiceAnimations();
  bool HasAnimations() const;

  void ResetDocumentTime();
  void SetDocumentOrderIndexesDirty() { document_order_indexes_dirty_ = true; }

  // Advance the animation timeline a single frame.
  void AdvanceFrameForTesting();

  void Trace(blink::Visitor*);

 private:
  enum FrameSchedulingState {
    // No frame scheduled.
    kIdle,
    // Scheduled a wakeup to update the animation values.
    kSynchronizeAnimations,
    // Scheduled a wakeup to trigger an animation frame.
    kFutureAnimationFrame,
    // Scheduled a animation frame for continuous update.
    kAnimationFrame
  };

  enum AnimationPolicyOnceAction {
    // Restart OnceTimer if the timeline is not paused.
    kRestartOnceTimerIfNotPaused,
    // Restart OnceTimer.
    kRestartOnceTimer,
    // Cancel OnceTimer.
    kCancelOnceTimer
  };

  bool IsTimelineRunning() const;
  void SynchronizeToDocumentTimeline();
  void ScheduleAnimationFrame(base::TimeDelta delay_time);
  void CancelAnimationFrame();
  void WakeupTimerFired(TimerBase*);
  void ScheduleAnimationPolicyTimer();
  void CancelAnimationPolicyTimer();
  void AnimationPolicyTimerFired(TimerBase*);
  ImageAnimationPolicy AnimationPolicy() const;
  bool HandleAnimationPolicy(AnimationPolicyOnceAction);
  bool CanScheduleFrame(SMILTime earliest_fire_time) const;
  void UpdateAnimationsAndScheduleFrameIfNeeded(SMILTime elapsed);
  void RemoveUnusedKeys();
  void ResetIntervals();
  SVGSMILElement* GetNextReady(SMILTime presentation_time) const;
  void UpdateIntervals(SMILTime presentation_time);
  void UpdateAnimationTimings(SMILTime elapsed);
  void ApplyAnimationValues(SMILTime elapsed);
  SMILTime NextProgressTime(SMILTime presentation_time) const;
  void ServiceOnNextFrame();
  void ScheduleWakeUp(base::TimeDelta delay_time, FrameSchedulingState);
  bool HasPendingSynchronization() const;

  void UpdateDocumentOrderIndexes();

  SVGSVGElement& OwnerSVGElement() const;
  Document& GetDocument() const;

  // The latest "restart" time for the time container's timeline. If the
  // timeline has not been manipulated (seeked, paused) this will be zero.
  SMILTime presentation_time_;
  // The state all SVGSMILElements should be at.
  SMILTime latest_update_time_;
  // The time on the document timeline corresponding to |presentation_time_|.
  base::TimeDelta reference_time_;

  FrameSchedulingState frame_scheduling_state_;
  bool started_ : 1;  // The timeline has been started.
  bool paused_ : 1;   // The timeline is paused.

  bool document_order_indexes_dirty_ : 1;
  bool is_updating_intervals_;

  TaskRunnerTimer<SMILTimeContainer> wakeup_timer_;
  TaskRunnerTimer<SMILTimeContainer> animation_policy_once_timer_;

  AnimationsMap scheduled_animations_;

  struct NextIntervalTimeLess;
  PriorityQueue<SVGSMILElement, NextIntervalTimeLess> priority_queue_;

  Member<SVGSVGElement> owner_svg_element_;

#if DCHECK_IS_ON()
  friend class ScheduledAnimationsMutationsForbidden;
  // This boolean will catch any attempts to mutate (schedule/unschedule)
  // |scheduled_animations_| when it is set to true.
  bool prevent_scheduled_animations_changes_ = false;
#endif

  bool ScheduledAnimationsMutationsAllowed() const {
#if DCHECK_IS_ON()
    return !prevent_scheduled_animations_changes_;
#else
    return true;
#endif
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_ANIMATION_SMIL_TIME_CONTAINER_H_
