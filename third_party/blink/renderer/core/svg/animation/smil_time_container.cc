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

#include "third_party/blink/renderer/core/svg/animation/smil_time_container.h"

#include <algorithm>
#include "third_party/blink/renderer/core/animation/animation_clock.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/svg/animation/smil_time.h"
#include "third_party/blink/renderer/core/svg/animation/svg_smil_element.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

struct SMILTimeContainer::NextIntervalTimeLess {
  bool operator()(const SVGSMILElement& a, const SVGSMILElement& b) {
    return a.NextIntervalTime() < b.NextIntervalTime();
  }
};

class ScheduledAnimationsMutationsForbidden {
  STACK_ALLOCATED();

 public:
  explicit ScheduledAnimationsMutationsForbidden(
      SMILTimeContainer* time_container)
#if DCHECK_IS_ON()
      : flag_reset_(&time_container->prevent_scheduled_animations_changes_,
                    true)
#endif
  {
  }

 private:
#if DCHECK_IS_ON()
  base::AutoReset<bool> flag_reset_;
#endif
};

static constexpr base::TimeDelta kAnimationPolicyOnceDuration =
    base::TimeDelta::FromSeconds(3);

SMILTimeContainer::SMILTimeContainer(SVGSVGElement& owner)
    : frame_scheduling_state_(kIdle),
      started_(false),
      paused_(false),
      document_order_indexes_dirty_(false),
      is_updating_intervals_(false),
      wakeup_timer_(
          owner.GetDocument().GetTaskRunner(TaskType::kInternalDefault),
          this,
          &SMILTimeContainer::WakeupTimerFired),
      animation_policy_once_timer_(
          owner.GetDocument().GetTaskRunner(TaskType::kInternalDefault),
          this,
          &SMILTimeContainer::AnimationPolicyTimerFired),
      owner_svg_element_(&owner) {}

SMILTimeContainer::~SMILTimeContainer() {
  CancelAnimationFrame();
  CancelAnimationPolicyTimer();
  DCHECK(!wakeup_timer_.IsActive());
  DCHECK(ScheduledAnimationsMutationsAllowed());
}

void SMILTimeContainer::Schedule(SVGSMILElement* animation,
                                 SVGElement* target,
                                 const QualifiedName& attribute_name) {
  DCHECK_EQ(animation->TimeContainer(), this);
  DCHECK(target);
  DCHECK(animation->HasValidTarget());
  DCHECK(ScheduledAnimationsMutationsAllowed());

  // Separate out Discard and AnimateMotion
  QualifiedName name = (animation->HasTagName(svg_names::kAnimateMotionTag) ||
                        animation->HasTagName(svg_names::kDiscardTag))
                           ? animation->TagQName()
                           : attribute_name;

  auto key = std::make_pair(target, name);
  auto& sandwich =
      scheduled_animations_.insert(key, nullptr).stored_value->value;
  if (!sandwich)
    sandwich = MakeGarbageCollected<SMILAnimationSandwich>();

  sandwich->Add(animation);

  priority_queue_.Insert(animation);
}

void SMILTimeContainer::Unschedule(SVGSMILElement* animation,
                                   SVGElement* target,
                                   const QualifiedName& attribute_name) {
  DCHECK_EQ(animation->TimeContainer(), this);
  DCHECK(ScheduledAnimationsMutationsAllowed());

  // Separate out Discard and AnimateMotion
  QualifiedName name = (animation->HasTagName(svg_names::kAnimateMotionTag) ||
                        animation->HasTagName(svg_names::kDiscardTag))
                           ? animation->TagQName()
                           : attribute_name;

  auto key = std::make_pair(target, name);
  AnimationsMap::iterator it = scheduled_animations_.find(key);
  CHECK(it != scheduled_animations_.end());

  auto& sandwich = *(it->value);
  sandwich.Remove(animation);

  if (sandwich.IsEmpty())
    scheduled_animations_.erase(it);

  priority_queue_.Remove(animation);
}

void SMILTimeContainer::Reschedule(SVGSMILElement* animation) {
  // TODO(fs): We trigger this sometimes at the moment - for example when
  // removing the entire fragment that the timed element is in.
  if (!priority_queue_.Contains(animation))
    return;
  priority_queue_.Update(animation);
  // We're inside a call to UpdateIntervals() or ResetIntervals(), so
  // we don't need to request an update - that will happen after the regular
  // update has finished (if needed).
  if (is_updating_intervals_)
    return;
  if (!IsStarted())
    return;
  // Schedule UpdateAnimations...() to be called asynchronously so multiple
  // intervals can change with UpdateAnimations...() only called once at the
  // end.
  if (HasPendingSynchronization())
    return;
  CancelAnimationFrame();
  ScheduleWakeUp(base::TimeDelta(), kSynchronizeAnimations);
}

bool SMILTimeContainer::HasAnimations() const {
  return !scheduled_animations_.IsEmpty();
}

bool SMILTimeContainer::HasPendingSynchronization() const {
  return frame_scheduling_state_ == kSynchronizeAnimations &&
         wakeup_timer_.IsActive() && wakeup_timer_.NextFireInterval().is_zero();
}

SMILTime SMILTimeContainer::Elapsed() const {
  if (!IsStarted())
    return SMILTime();

  if (IsPaused())
    return presentation_time_;

  base::TimeDelta time_offset =
      GetDocument().Timeline().CurrentTimeInternal().value_or(
          base::TimeDelta()) -
      reference_time_;
  DCHECK_GE(time_offset, base::TimeDelta());
  SMILTime elapsed = presentation_time_ +
                     SMILTime::FromMicroseconds(time_offset.InMicroseconds());
  DCHECK_GE(elapsed, SMILTime());
  return elapsed;
}

void SMILTimeContainer::ResetDocumentTime() {
  DCHECK(IsStarted());
  // TODO(edvardt): We actually want to check if
  // the document is active and we don't have any special
  // conditions and such, but they require more fixing,
  // probably in SVGSVGElement. I suspect there's a large
  // bug buried here somewhere. This is enough to "paper over"
  // it, but it's not really a solution.
  //
  // Bug: 996196

  SynchronizeToDocumentTimeline();
}

SMILTime SMILTimeContainer::CurrentDocumentTime() const {
  return latest_update_time_;
}

void SMILTimeContainer::SynchronizeToDocumentTimeline() {
  reference_time_ = GetDocument().Timeline().CurrentTimeInternal().value_or(
      base::TimeDelta());
}

bool SMILTimeContainer::IsPaused() const {
  // If animation policy is "none", the timeline is always paused.
  return paused_ || AnimationPolicy() == kImageAnimationPolicyNoAnimation;
}

bool SMILTimeContainer::IsStarted() const {
  return started_;
}

bool SMILTimeContainer::IsTimelineRunning() const {
  return IsStarted() && !IsPaused();
}

void SMILTimeContainer::Start() {
  CHECK(!IsStarted());

  if (!GetDocument().IsActive())
    return;

  if (!HandleAnimationPolicy(kRestartOnceTimerIfNotPaused))
    return;

  // Sample the document timeline to get a time reference for the "presentation
  // time".
  SynchronizeToDocumentTimeline();
  started_ = true;

  UpdateAnimationsAndScheduleFrameIfNeeded(presentation_time_);
}

void SMILTimeContainer::Pause() {
  if (!HandleAnimationPolicy(kCancelOnceTimer))
    return;
  DCHECK(!IsPaused());

  if (IsStarted()) {
    presentation_time_ = Elapsed();
    CancelAnimationFrame();
  }
  // Update the flag after sampling elapsed().
  paused_ = true;
}

void SMILTimeContainer::Unpause() {
  if (!HandleAnimationPolicy(kRestartOnceTimer))
    return;
  DCHECK(IsPaused());

  paused_ = false;

  if (!IsStarted())
    return;

  SynchronizeToDocumentTimeline();
  ScheduleWakeUp(base::TimeDelta(), kSynchronizeAnimations);
}

void SMILTimeContainer::SetElapsed(SMILTime elapsed) {
  presentation_time_ = elapsed;

  if (!GetDocument().IsActive())
    return;

  // If the document hasn't finished loading, |presentation_time_| will be
  // used as the start time to seek to once it's possible.
  if (!IsStarted())
    return;

  if (!HandleAnimationPolicy(kRestartOnceTimerIfNotPaused))
    return;

  CancelAnimationFrame();

  if (!IsPaused())
    SynchronizeToDocumentTimeline();

  // If we are rewinding the timeline, we need to start from 0 and then move
  // forward to the new presentation time. If we're moving forward we can just
  // perform the update in the normal fashion.
  if (elapsed < latest_update_time_) {
    ResetIntervals();
    latest_update_time_ = SMILTime();
  }
  UpdateAnimationsAndScheduleFrameIfNeeded(elapsed);
}

void SMILTimeContainer::ScheduleAnimationFrame(base::TimeDelta delay_time) {
  DCHECK(IsTimelineRunning());
  DCHECK(!wakeup_timer_.IsActive());
  DCHECK(GetDocument().IsActive());

  const base::TimeDelta kLocalMinimumDelay =
      base::TimeDelta::FromSecondsD(DocumentTimeline::kMinimumDelay);
  if (delay_time < kLocalMinimumDelay) {
    ServiceOnNextFrame();
  } else {
    ScheduleWakeUp(delay_time - kLocalMinimumDelay, kFutureAnimationFrame);
  }
}

void SMILTimeContainer::CancelAnimationFrame() {
  frame_scheduling_state_ = kIdle;
  wakeup_timer_.Stop();
}

void SMILTimeContainer::ScheduleWakeUp(
    base::TimeDelta delay_time,
    FrameSchedulingState frame_scheduling_state) {
  DCHECK(frame_scheduling_state == kSynchronizeAnimations ||
         frame_scheduling_state == kFutureAnimationFrame);
  wakeup_timer_.StartOneShot(delay_time, FROM_HERE);
  frame_scheduling_state_ = frame_scheduling_state;
}

void SMILTimeContainer::WakeupTimerFired(TimerBase*) {
  DCHECK(frame_scheduling_state_ == kSynchronizeAnimations ||
         frame_scheduling_state_ == kFutureAnimationFrame);
  FrameSchedulingState previous_frame_scheduling_state =
      frame_scheduling_state_;
  frame_scheduling_state_ = kIdle;
  // TODO(fs): The timeline should not be running if we're in an inactive
  // document, so this should be turned into a DCHECK.
  if (!GetDocument().IsActive())
    return;
  if (previous_frame_scheduling_state == kFutureAnimationFrame) {
    DCHECK(IsTimelineRunning());
    ServiceOnNextFrame();
  } else {
    UpdateAnimationsAndScheduleFrameIfNeeded(Elapsed());
  }
}

void SMILTimeContainer::ScheduleAnimationPolicyTimer() {
  animation_policy_once_timer_.StartOneShot(kAnimationPolicyOnceDuration,
                                            FROM_HERE);
}

void SMILTimeContainer::CancelAnimationPolicyTimer() {
  animation_policy_once_timer_.Stop();
}

void SMILTimeContainer::AnimationPolicyTimerFired(TimerBase*) {
  Pause();
}

ImageAnimationPolicy SMILTimeContainer::AnimationPolicy() const {
  Settings* settings = GetDocument().GetSettings();
  if (!settings)
    return kImageAnimationPolicyAllowed;

  return settings->GetImageAnimationPolicy();
}

bool SMILTimeContainer::HandleAnimationPolicy(
    AnimationPolicyOnceAction once_action) {
  ImageAnimationPolicy policy = AnimationPolicy();
  // If the animation policy is "none", control is not allowed.
  // returns false to exit flow.
  if (policy == kImageAnimationPolicyNoAnimation)
    return false;
  // If the animation policy is "once",
  if (policy == kImageAnimationPolicyAnimateOnce) {
    switch (once_action) {
      case kRestartOnceTimerIfNotPaused:
        if (IsPaused())
          break;
        FALLTHROUGH;
      case kRestartOnceTimer:
        ScheduleAnimationPolicyTimer();
        break;
      case kCancelOnceTimer:
        CancelAnimationPolicyTimer();
        break;
    }
  }
  if (policy == kImageAnimationPolicyAllowed) {
    // When the SVG owner element becomes detached from its document,
    // the policy defaults to ImageAnimationPolicyAllowed; there's
    // no way back. If the policy had been "once" prior to that,
    // ensure cancellation of its timer.
    if (once_action == kCancelOnceTimer)
      CancelAnimationPolicyTimer();
  }
  return true;
}

void SMILTimeContainer::UpdateDocumentOrderIndexes() {
  unsigned timing_element_count = 0;
  for (SVGSMILElement& element :
       Traversal<SVGSMILElement>::DescendantsOf(OwnerSVGElement()))
    element.SetDocumentOrderIndex(timing_element_count++);
  document_order_indexes_dirty_ = false;
}

SVGSVGElement& SMILTimeContainer::OwnerSVGElement() const {
  return *owner_svg_element_;
}

Document& SMILTimeContainer::GetDocument() const {
  return OwnerSVGElement().GetDocument();
}

void SMILTimeContainer::ServiceOnNextFrame() {
  if (GetDocument().View()) {
    GetDocument().View()->ScheduleAnimation();
    frame_scheduling_state_ = kAnimationFrame;
  }
}

void SMILTimeContainer::ServiceAnimations() {
  if (frame_scheduling_state_ != kAnimationFrame)
    return;
  frame_scheduling_state_ = kIdle;
  // TODO(fs): The timeline should not be running if we're in an inactive
  // document, so this should be turned into a DCHECK.
  if (!GetDocument().IsActive())
    return;
  UpdateAnimationsAndScheduleFrameIfNeeded(Elapsed());
}

bool SMILTimeContainer::CanScheduleFrame(SMILTime earliest_fire_time) const {
  // If there's synchronization pending (most likely due to syncbases), then
  // let that complete first before attempting to schedule a frame.
  if (HasPendingSynchronization())
    return false;
  if (!IsTimelineRunning())
    return false;
  return earliest_fire_time.IsFinite();
}

void SMILTimeContainer::UpdateAnimationsAndScheduleFrameIfNeeded(
    SMILTime elapsed) {
  DCHECK(GetDocument().IsActive());
  DCHECK(!wakeup_timer_.IsActive());

  UpdateAnimationTimings(elapsed);
  ApplyAnimationValues(elapsed);

  SMILTime next_progress_time = NextProgressTime(elapsed);
  DCHECK(!wakeup_timer_.IsActive());

  if (!CanScheduleFrame(next_progress_time))
    return;
  SMILTime delay_time = next_progress_time - elapsed;
  DCHECK(delay_time.IsFinite());
  ScheduleAnimationFrame(
      base::TimeDelta::FromMicroseconds(delay_time.InMicroseconds()));
}

SMILTime SMILTimeContainer::NextProgressTime(SMILTime presentation_time) const {
  SMILTime next_progress_time = SMILTime::Unresolved();
  for (const auto& element : priority_queue_) {
    next_progress_time = std::min(next_progress_time,
                                  element->NextProgressTime(presentation_time));
    if (next_progress_time <= presentation_time)
      break;
  }
  return next_progress_time;
}

void SMILTimeContainer::RemoveUnusedKeys() {
  Vector<AnimationId> invalid_keys;
  for (auto& entry : scheduled_animations_) {
    if (entry.value->IsEmpty()) {
      invalid_keys.push_back(entry.key);
    }
  }
  scheduled_animations_.RemoveAll(invalid_keys);
}

void SMILTimeContainer::ResetIntervals() {
  base::AutoReset<bool> updating_intervals_scope(&is_updating_intervals_, true);
  ScheduledAnimationsMutationsForbidden scope(this);
  for (auto& element : priority_queue_)
    element->Reset();
}

SVGSMILElement* SMILTimeContainer::GetNextReady(
    SMILTime presentation_time) const {
  DCHECK(!priority_queue_.IsEmpty());
  SVGSMILElement* next_element = priority_queue_.MinElement();
  if (next_element->NextIntervalTime() > presentation_time)
    return nullptr;
  return next_element;
}

void SMILTimeContainer::UpdateIntervals(SMILTime document_time) {
  DCHECK(document_time.IsFinite());
  DCHECK_GE(document_time, SMILTime());

  base::AutoReset<bool> updating_intervals_scope(&is_updating_intervals_, true);
  while (SVGSMILElement* element = GetNextReady(document_time)) {
    element->UpdateInterval(document_time);
    element->UpdateActiveState(document_time);
    element->UpdateNextIntervalTime(document_time);
    priority_queue_.Update(element);
  }
}

void SMILTimeContainer::UpdateAnimationTimings(SMILTime presentation_time) {
  DCHECK(GetDocument().IsActive());

  ScheduledAnimationsMutationsForbidden scope(this);

  if (document_order_indexes_dirty_)
    UpdateDocumentOrderIndexes();

  RemoveUnusedKeys();

  if (priority_queue_.IsEmpty())
    return;

  // Flush any "late" interval updates.
  UpdateIntervals(latest_update_time_);

  while (latest_update_time_ < presentation_time) {
    const SMILTime interval_time =
        priority_queue_.MinElement()->NextIntervalTime();
    if (interval_time <= presentation_time) {
      latest_update_time_ = interval_time;
      UpdateIntervals(latest_update_time_);
    } else {
      latest_update_time_ = presentation_time;
    }
  }
}

void SMILTimeContainer::ApplyAnimationValues(SMILTime elapsed) {
  HeapVector<Member<SVGSMILElement>> animations_to_apply;
  {
    ScheduledAnimationsMutationsForbidden scope(this);
    for (auto& sandwich : scheduled_animations_.Values()) {
      sandwich->UpdateActiveAnimationStack(elapsed);
      if (SVGSMILElement* animation = sandwich->ApplyAnimationValues())
        animations_to_apply.push_back(animation);
    }
  }

  if (animations_to_apply.IsEmpty())
    return;

  // Everything bellow handles "discard" elements.
  UseCounter::Count(&GetDocument(), WebFeature::kSVGSMILAnimationAppliedEffect);

  // Sort by location in the document. (Should be based on the target rather
  // than the timed element, but often enough they will order the same.)
  std::sort(
      animations_to_apply.begin(), animations_to_apply.end(),
      [](const Member<SVGSMILElement>& a, const Member<SVGSMILElement>& b) {
        return a->DocumentOrderIndex() < b->DocumentOrderIndex();
      });

  for (const auto& timed_element : animations_to_apply) {
    if (timed_element->isConnected() && timed_element->IsSVGDiscardElement()) {
      SVGElement* target_element = timed_element->targetElement();
      if (target_element && target_element->isConnected()) {
        UseCounter::Count(&GetDocument(),
                          WebFeature::kSVGSMILDiscardElementTriggered);
        target_element->remove(IGNORE_EXCEPTION_FOR_TESTING);
        DCHECK(!target_element->isConnected());
      }

      if (timed_element->isConnected()) {
        timed_element->remove(IGNORE_EXCEPTION_FOR_TESTING);
        DCHECK(!timed_element->isConnected());
      }
    }
  }
  return;
}

void SMILTimeContainer::AdvanceFrameForTesting() {
  const SMILTime kFrameDuration = SMILTime::FromSecondsD(0.025);
  SetElapsed(Elapsed() + kFrameDuration);
}

void SMILTimeContainer::Trace(blink::Visitor* visitor) {
  visitor->Trace(scheduled_animations_);
  visitor->Trace(priority_queue_);
  visitor->Trace(owner_svg_element_);
}

}  // namespace blink
