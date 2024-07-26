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

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/svg/animation/element_smil_animations.h"
#include "third_party/blink/renderer/core/svg/animation/svg_smil_element.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/svg/svg_component_transfer_function_element.h"
#include "third_party/blink/renderer/core/svg/svg_fe_light_element.h"
#include "third_party/blink/renderer/core/svg/svg_fe_merge_node_element.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

class AnimationTargetsMutationsForbidden {
  STACK_ALLOCATED();

 public:
  explicit AnimationTargetsMutationsForbidden(SMILTimeContainer* time_container)
#if DCHECK_IS_ON()
      : flag_reset_(&time_container->prevent_animation_targets_changes_, true)
#endif
  {
  }

 private:
#if DCHECK_IS_ON()
  base::AutoReset<bool> flag_reset_;
#endif
};

class SMILTimeContainer::TimingUpdate {
  STACK_ALLOCATED();

 public:
  // The policy used when performing the timing update.
  enum MovePolicy {
    // Used for regular updates, i.e when time is running. All events will be
    // dispatched.
    kNormal,
    // Used for seeking updates, i.e when time is explicitly
    // set/changed. Events are not dispatched for skipped intervals, and no
    // repeats are generated.
    kSeek,
  };
  TimingUpdate(SMILTimeContainer& time_container,
               SMILTime target_time,
               MovePolicy policy)
      : target_time_(target_time),
        policy_(policy),
        time_container_(&time_container) {
    DCHECK_LE(target_time_, time_container_->max_presentation_time_);
  }
  ~TimingUpdate();

  const SMILTime& Time() const { return time_container_->latest_update_time_; }
  bool TryAdvanceTime(SMILTime next_time) {
    if (time_container_->latest_update_time_ >= target_time_)
      return false;
    if (next_time > target_time_) {
      time_container_->latest_update_time_ = target_time_;
      return false;
    }
    time_container_->latest_update_time_ = next_time;
    return true;
  }
  void RewindTimeToZero() { time_container_->latest_update_time_ = SMILTime(); }
  const SMILTime& TargetTime() const { return target_time_; }
  bool IsSeek() const { return policy_ == kSeek; }
  void AddActiveElement(SVGSMILElement*, const SMILInterval&);
  void HandleEvents(SVGSMILElement*, SVGSMILElement::EventDispatchMask);
  bool ShouldDispatchEvents() const {
    return time_container_->should_dispatch_events_;
  }

  using UpdatedElementsMap = HeapHashMap<Member<SVGSMILElement>, SMILInterval>;
  UpdatedElementsMap& UpdatedElements() { return updated_elements_; }

  TimingUpdate(const TimingUpdate&) = delete;
  TimingUpdate& operator=(const TimingUpdate&) = delete;

 private:
  SMILTime target_time_;
  MovePolicy policy_;
  SMILTimeContainer* time_container_;
  UpdatedElementsMap updated_elements_;
};

SMILTimeContainer::TimingUpdate::~TimingUpdate() {
  if (!ShouldDispatchEvents())
    return;
  DCHECK(IsSeek() || updated_elements_.empty());
  for (const auto& entry : updated_elements_) {
    SVGSMILElement* element = entry.key;
    if (auto events_to_dispatch = element->ComputeSeekEvents(entry.value))
      element->DispatchEvents(events_to_dispatch);
  }
}

void SMILTimeContainer::TimingUpdate::AddActiveElement(
    SVGSMILElement* element,
    const SMILInterval& interval) {
  DCHECK(IsSeek());
  DCHECK(ShouldDispatchEvents());
  updated_elements_.insert(element, interval);
}

void SMILTimeContainer::TimingUpdate::HandleEvents(
    SVGSMILElement* element,
    SVGSMILElement::EventDispatchMask events_to_dispatch) {
  if (!IsSeek()) {
    if (ShouldDispatchEvents() && events_to_dispatch)
      element->DispatchEvents(events_to_dispatch);
    return;
  }
  // Even if no events will be dispatched, we still need to track the elements
  // that has been updated so that we can adjust their next interval time when
  // we're done. (If we tracked active elements separately this would not be
  // necessary.)
  updated_elements_.insert(element, SMILInterval::Unresolved());
}

SMILTimeContainer::SMILTimeContainer(SVGSVGElement& owner)
    : frame_scheduling_state_(kIdle),
      started_(false),
      paused_(false),
      should_dispatch_events_(!SVGImage::IsInSVGImage(&owner)),
      document_order_indexes_dirty_(false),
      is_updating_intervals_(false),
      wakeup_timer_(
          owner.GetDocument().GetTaskRunner(TaskType::kInternalDefault),
          this,
          &SMILTimeContainer::WakeupTimerFired),
      owner_svg_element_(&owner) {
  // Update the max presentation time based on the animation policy in effect.
  SetPresentationTime(presentation_time_);
}

SMILTimeContainer::~SMILTimeContainer() {
  CancelAnimationFrame();
  DCHECK(!wakeup_timer_.IsActive());
  DCHECK(AnimationTargetsMutationsAllowed());
}

void SMILTimeContainer::Schedule(SVGSMILElement* animation) {
  DCHECK_EQ(animation->TimeContainer(), this);
  DCHECK(animation->HasValidTarget());
  DCHECK(AnimationTargetsMutationsAllowed());

  animated_targets_.insert(animation->targetElement());
  // Enter the element into the queue with the "latest" possible time. The
  // timed element will update its position in the queue when (re)evaluating
  // its current interval.
  priority_queue_.Insert(SMILTime::Unresolved(), animation);
}

void SMILTimeContainer::Unschedule(SVGSMILElement* animation) {
  DCHECK_EQ(animation->TimeContainer(), this);
  DCHECK(AnimationTargetsMutationsAllowed());
  DCHECK(animated_targets_.Contains(animation->targetElement()));

  animated_targets_.erase(animation->targetElement());
  priority_queue_.Remove(animation);
}

void SMILTimeContainer::Reschedule(SVGSMILElement* animation,
                                   SMILTime interval_time) {
  // TODO(fs): We trigger this sometimes at the moment - for example when
  // removing the entire fragment that the timed element is in.
  if (!priority_queue_.Contains(animation))
    return;
  priority_queue_.Update(interval_time, animation);
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
  return !animated_targets_.empty();
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
      GetDocument().Timeline().CurrentPhaseAndTime().time.value_or(
          base::TimeDelta()) -
      reference_time_;
  DCHECK_GE(time_offset, base::TimeDelta());
  SMILTime elapsed = presentation_time_ + SMILTime::FromTimeDelta(time_offset);
  DCHECK_GE(elapsed, SMILTime());
  return ClampPresentationTime(elapsed);
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

SMILTime SMILTimeContainer::LatestUpdatePresentationTime() const {
  return latest_update_time_;
}

void SMILTimeContainer::SynchronizeToDocumentTimeline() {
  reference_time_ =
      GetDocument().Timeline().CurrentPhaseAndTime().time.value_or(
          base::TimeDelta());
}

bool SMILTimeContainer::IsPaused() const {
  // If animation policy is "none", the timeline is always paused.
  return paused_ || AnimationsDisabled();
}

bool SMILTimeContainer::IsStarted() const {
  return started_;
}

bool SMILTimeContainer::IsTimelineRunning() const {
  return IsStarted() && !IsPaused();
}

void SMILTimeContainer::Start() {
  CHECK(!IsStarted());

  if (AnimationsDisabled())
    return;

  // Sample the document timeline to get a time reference for the "presentation
  // time".
  SynchronizeToDocumentTimeline();
  started_ = true;

  TimingUpdate update(*this, presentation_time_, TimingUpdate::kSeek);
  UpdateAnimationsAndScheduleFrameIfNeeded(update);
}

void SMILTimeContainer::Pause() {
  if (AnimationsDisabled())
    return;
  DCHECK(!IsPaused());

  if (IsStarted()) {
    SetPresentationTime(Elapsed());
    CancelAnimationFrame();
  }

  // Update the flag after sampling elapsed().
  paused_ = true;
}

void SMILTimeContainer::Unpause() {
  if (AnimationsDisabled())
    return;
  DCHECK(IsPaused());

  paused_ = false;

  if (!IsStarted())
    return;

  SynchronizeToDocumentTimeline();
  ScheduleWakeUp(base::TimeDelta(), kSynchronizeAnimations);
}

void SMILTimeContainer::SetPresentationTime(SMILTime new_presentation_time) {
  // Start by resetting the max presentation time, because if the
  // animation-policy is "once" we'll set a new limit below regardless, and for
  // the other cases it's the right thing to do.
  //
  // We can't seek beyond this time, because at Latest() any additions will
  // yield the same value.
  max_presentation_time_ = SMILTime::Latest() - SMILTime::Epsilon();
  presentation_time_ = ClampPresentationTime(new_presentation_time);
  if (AnimationPolicy() !=
      mojom::blink::ImageAnimationPolicy::kImageAnimationPolicyAnimateOnce)
    return;
  const SMILTime kAnimationPolicyOnceDuration = SMILTime::FromSecondsD(3);
  max_presentation_time_ =
      ClampPresentationTime(presentation_time_ + kAnimationPolicyOnceDuration);
}

SMILTime SMILTimeContainer::ClampPresentationTime(
    SMILTime presentation_time) const {
  return std::min(presentation_time, max_presentation_time_);
}

void SMILTimeContainer::SetElapsed(SMILTime elapsed) {
  SetPresentationTime(elapsed);

  if (AnimationsDisabled())
    return;

  // If the document hasn't finished loading, |presentation_time_| will be
  // used as the start time to seek to once it's possible.
  if (!IsStarted())
    return;

  CancelAnimationFrame();

  if (!IsPaused())
    SynchronizeToDocumentTimeline();

  TimingUpdate update(*this, presentation_time_, TimingUpdate::kSeek);
  PrepareSeek(update);
  UpdateAnimationsAndScheduleFrameIfNeeded(update);
}

void SMILTimeContainer::ScheduleAnimationFrame(base::TimeDelta delay_time,
                                               bool disable_throttling) {
  DCHECK(IsTimelineRunning());
  DCHECK(!wakeup_timer_.IsActive());
  DCHECK(GetDocument().IsActive());

  // Skip the comparison against kLocalMinimumDelay if an animation is
  // not visible.
  if (!disable_throttling) {
    ScheduleWakeUp(delay_time, kFutureAnimationFrame);
    return;
  }

  const base::TimeDelta kLocalMinimumDelay =
      base::Seconds(DocumentTimeline::kMinimumDelay);
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
  TimingUpdate update(*this, Elapsed(), TimingUpdate::kNormal);
  if (previous_frame_scheduling_state == kFutureAnimationFrame) {
    DCHECK(IsTimelineRunning());
    if (RuntimeEnabledFeatures::SmilAutoSuspendOnLagEnabled()) {
      // Advance time to just before the next event.
      const SMILTime next_event_time =
          !priority_queue_.IsEmpty()
              ? priority_queue_.Min() - SMILTime::Epsilon()
              : SMILTime::Unresolved();
      update.TryAdvanceTime(next_event_time);
    }
    ServiceOnNextFrame();
  } else {
    UpdateAnimationsAndScheduleFrameIfNeeded(update);
  }
}

mojom::blink::ImageAnimationPolicy SMILTimeContainer::AnimationPolicy() const {
  const Settings* settings = GetDocument().GetSettings();
  return settings
             ? settings->GetImageAnimationPolicy()
             : mojom::blink::ImageAnimationPolicy::kImageAnimationPolicyAllowed;
}

bool SMILTimeContainer::AnimationsDisabled() const {
  return !GetDocument().IsActive() || AnimationPolicy() ==
                                          mojom::blink::ImageAnimationPolicy::
                                              kImageAnimationPolicyNoAnimation;
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

bool SMILTimeContainer::ServiceAnimations() {
  // If a synchronization is pending, we can flush it now.
  FrameSchedulingState previous_frame_scheduling_state =
      frame_scheduling_state_;
  if (frame_scheduling_state_ == kSynchronizeAnimations) {
    DCHECK(wakeup_timer_.IsActive());
    wakeup_timer_.Stop();
    frame_scheduling_state_ = kAnimationFrame;
  }
  if (frame_scheduling_state_ != kAnimationFrame)
    return false;
  frame_scheduling_state_ = kIdle;
  // TODO(fs): The timeline should not be running if we're in an inactive
  // document, so this should be turned into a DCHECK.
  if (!GetDocument().IsActive())
    return false;
  SMILTime elapsed = Elapsed();
  if (RuntimeEnabledFeatures::SmilAutoSuspendOnLagEnabled()) {
    // If an unexpectedly long amount of time has passed since we last
    // ticked animations, behave as if we paused the timeline after
    // |kMaxAnimationLag| and now automatically resume the animation.
    constexpr SMILTime kMaxAnimationLag = SMILTime::FromSecondsD(60);
    const SMILTime elapsed_limit = latest_update_time_ + kMaxAnimationLag;
    if (previous_frame_scheduling_state == kAnimationFrame &&
        elapsed > elapsed_limit) {
      // We've passed the lag limit. Compute the excess lag and then
      // rewind/adjust the timeline by that amount to make it appear as if only
      // kMaxAnimationLag has passed.
      const SMILTime excess_lag = elapsed - elapsed_limit;
      // Since Elapsed() is clamped, the limit should fall within the clamped
      // time range as well.
      DCHECK_EQ(ClampPresentationTime(presentation_time_ - excess_lag),
                presentation_time_ - excess_lag);
      presentation_time_ = presentation_time_ - excess_lag;
      elapsed = Elapsed();
    }
  }
  TimingUpdate update(*this, elapsed, TimingUpdate::kNormal);
  return UpdateAnimationsAndScheduleFrameIfNeeded(update);
}

bool SMILTimeContainer::UpdateAnimationsAndScheduleFrameIfNeeded(
    TimingUpdate& update) {
  DCHECK(GetDocument().IsActive());
  DCHECK(!wakeup_timer_.IsActive());
  // If the priority queue is empty, there are no timed elements to process and
  // no animations to apply, so we are done.
  if (priority_queue_.IsEmpty())
    return false;
  AnimationTargetsMutationsForbidden scope(this);
  UpdateTimedElements(update);
  bool disable_throttling = ApplyTimedEffects(update.TargetTime());
  DCHECK(!wakeup_timer_.IsActive());
  DCHECK(!HasPendingSynchronization());

  if (!IsTimelineRunning())
    return false;
  SMILTime next_progress_time =
      NextProgressTime(update.TargetTime(), disable_throttling);
  if (!next_progress_time.IsFinite())
    return false;
  SMILTime delay_time = next_progress_time - update.TargetTime();
  DCHECK(delay_time.IsFinite());
  ScheduleAnimationFrame(delay_time.ToTimeDelta(), disable_throttling);
  return true;
}

SMILTime SMILTimeContainer::NextProgressTime(SMILTime presentation_time,
                                             bool disable_throttling) const {
  if (presentation_time == max_presentation_time_)
    return SMILTime::Unresolved();

  // If the element is not rendered, skip any updates within the active
  // intervals and step to the next "event" time (begin, repeat or end).
  if (!disable_throttling) {
    return priority_queue_.Min();
  }

  SMILTime next_progress_time = SMILTime::Unresolved();
  for (const auto& entry : priority_queue_) {
    next_progress_time = std::min(
        next_progress_time, entry.second->NextProgressTime(presentation_time));
    if (next_progress_time <= presentation_time)
      break;
  }
  return next_progress_time;
}

void SMILTimeContainer::PrepareSeek(TimingUpdate& update) {
  DCHECK(update.IsSeek());
  if (update.ShouldDispatchEvents()) {
    // Record which elements are active at the current time so that we can
    // correctly determine the transitions when the seek finishes.
    // TODO(fs): Maybe keep track of the set of active timed elements and use
    // that here (and in NextProgressTime).
    for (auto& entry : priority_queue_) {
      SVGSMILElement* element = entry.second;
      const SMILInterval& active_interval =
          element->GetActiveInterval(update.Time());
      if (!active_interval.Contains(update.Time()))
        continue;
      update.AddActiveElement(element, active_interval);
    }
  }
  // If we are rewinding the timeline, we need to start from 0 and then move
  // forward to the new presentation time. If we're moving forward we can just
  // perform the update in the normal fashion.
  if (update.TargetTime() < update.Time()) {
    ResetIntervals();
    // TODO(fs): Clear resolved end times.
    update.RewindTimeToZero();
  }
}

void SMILTimeContainer::ResetIntervals() {
  base::AutoReset<bool> updating_intervals_scope(&is_updating_intervals_, true);
  AnimationTargetsMutationsForbidden scope(this);
  for (auto& entry : priority_queue_)
    entry.second->Reset();
  // (Re)set the priority of all the elements in the queue to the earliest
  // possible, so that a later call to UpdateIntervals() will run an update for
  // all of them.
  priority_queue_.ResetAllPriorities(SMILTime::Earliest());
}

void SMILTimeContainer::UpdateIntervals(TimingUpdate& update) {
  const SMILTime document_time = update.Time();
  DCHECK(document_time.IsFinite());
  DCHECK_GE(document_time, SMILTime());
  DCHECK(!priority_queue_.IsEmpty());

  const size_t kMaxIterations = std::max(priority_queue_.size() * 16, 1000000u);
  size_t current_iteration = 0;

  SVGSMILElement::IncludeRepeats repeat_handling =
      update.IsSeek() ? SVGSMILElement::kExcludeRepeats
                      : SVGSMILElement::kIncludeRepeats;

  base::AutoReset<bool> updating_intervals_scope(&is_updating_intervals_, true);
  while (priority_queue_.Min() <= document_time) {
    SVGSMILElement* element = priority_queue_.MinElement();
    element->UpdateInterval(document_time);
    auto events_to_dispatch =
        element->UpdateActiveState(document_time, update.IsSeek());
    update.HandleEvents(element, events_to_dispatch);
    SMILTime next_interval_time =
        element->ComputeNextIntervalTime(document_time, repeat_handling);
    priority_queue_.Update(next_interval_time, element);
    // Debugging signal for crbug.com/1021630.
    CHECK_LT(current_iteration++, kMaxIterations);
  }
}

void SMILTimeContainer::UpdateTimedElements(TimingUpdate& update) {
  // Flush any "late" interval updates.
  UpdateIntervals(update);

  while (update.TryAdvanceTime(priority_queue_.Min()))
    UpdateIntervals(update);

  // Update the next interval time for all affected elements to compensate for
  // any ignored repeats.
  const SMILTime presentation_time = update.TargetTime();
  for (const auto& element : update.UpdatedElements().Keys()) {
    SMILTime next_interval_time = element->ComputeNextIntervalTime(
        presentation_time, SVGSMILElement::kIncludeRepeats);
    priority_queue_.Update(next_interval_time, element);
  }
}

namespace {

bool NonRenderedElementThatAffectsContent(const SVGElement& target) {
  return IsA<SVGFELightElement>(target) ||
         IsA<SVGComponentTransferFunctionElement>(target) ||
         IsA<SVGFEMergeNodeElement>(target);
}

bool CanThrottleTarget(const SVGElement& target) {
  // Don't throttle if the target is in the layout tree.
  if (target.GetLayoutObject()) {
    return false;
  }
  // Don't throttle if the target has computed style (for example <stop>
  // elements).
  if (ComputedStyle::NullifyEnsured(target.GetComputedStyle())) {
    return false;
  }
  // Don't throttle if the target has use instances.
  if (!target.InstancesForElement().empty()) {
    return false;
  }
  // Don't throttle if the target is a non-rendered element that affects
  // content.
  if (NonRenderedElementThatAffectsContent(target)) {
    return false;
  }

  return true;
}

}  // namespace

bool SMILTimeContainer::ApplyTimedEffects(SMILTime elapsed) {
  if (document_order_indexes_dirty_)
    UpdateDocumentOrderIndexes();

  bool did_apply_effects = false;
  bool disable_throttling =
      !RuntimeEnabledFeatures::InvisibleSVGAnimationThrottlingEnabled();
  for (auto& entry : animated_targets_) {
    ElementSMILAnimations* animations = entry.key->GetSMILAnimations();
    if (animations && animations->Apply(elapsed)) {
      did_apply_effects = true;

      if (!disable_throttling && !CanThrottleTarget(*entry.key)) {
        disable_throttling = true;
      }
    }
  }

  if (did_apply_effects) {
    UseCounter::Count(&GetDocument(),
                      WebFeature::kSVGSMILAnimationAppliedEffect);
  }

  return disable_throttling;
}

void SMILTimeContainer::AdvanceFrameForTesting() {
  const SMILTime kFrameDuration = SMILTime::FromSecondsD(0.025);
  SetElapsed(Elapsed() + kFrameDuration);
}

void SMILTimeContainer::Trace(Visitor* visitor) const {
  visitor->Trace(wakeup_timer_);
  visitor->Trace(animated_targets_);
  visitor->Trace(priority_queue_);
  visitor->Trace(owner_svg_element_);
}

void SMILTimeContainer::DidAttachLayoutObject() {
  if (!IsTimelineRunning()) {
    return;
  }
  // If we're waiting on a scheduled timer to fire, trigger an animation
  // update on the next visual update.
  if (frame_scheduling_state_ != kFutureAnimationFrame) {
    return;
  }
  CancelAnimationFrame();
  ServiceOnNextFrame();
}

}  // namespace blink
