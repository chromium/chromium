// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_timeline.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_double.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"

namespace blink {

AnimationTimeline::AnimationTimeline(Document* document)
    : document_(document), outdated_animation_count_(0) {
  document_->GetDocumentAnimations().AddTimeline(*this);
}

void AnimationTimeline::AnimationAttached(Animation* animation) {
  DCHECK(!animations_.Contains(animation));
  animations_.insert(animation);
  animation->ResolveTimelineOffsets(GetTimelineRange());
}

void AnimationTimeline::AnimationDetached(Animation* animation) {
  animations_.erase(animation);
  animations_needing_update_.erase(animation);
  if (animation->Outdated())
    outdated_animation_count_--;
  animation->ResolveTimelineOffsets(GetTimelineRange());
}

bool CompareAnimations(const Member<Animation>& left,
                       const Member<Animation>& right) {
  // This uses pointer order comparision because it is less expensive and
  // element order doesn't affect the animation result(http://crbug.com/1047316)
  return Animation::HasLowerCompositeOrdering(
      left.Get(), right.Get(),
      Animation::CompareAnimationsOrdering::kPointerOrder);
}

V8CSSNumberish* AnimationTimeline::currentTime() {
  const std::optional<base::TimeDelta>& result = CurrentPhaseAndTime().time;
  if (result)
    return MakeGarbageCollected<V8CSSNumberish>(result->InMillisecondsF());
  return nullptr;
}

std::optional<AnimationTimeDelta> AnimationTimeline::CurrentTime() {
  std::optional<base::TimeDelta> result = CurrentPhaseAndTime().time;
  return result ? std::make_optional(AnimationTimeDelta(result.value()))
                : std::nullopt;
}

std::optional<double> AnimationTimeline::CurrentTimeMilliseconds() {
  std::optional<base::TimeDelta> result = CurrentPhaseAndTime().time;
  return result ? std::make_optional(result->InMillisecondsF()) : std::nullopt;
}

std::optional<double> AnimationTimeline::CurrentTimeSeconds() {
  std::optional<base::TimeDelta> result = CurrentPhaseAndTime().time;
  return result ? std::make_optional(result->InSecondsF()) : std::nullopt;
}

V8CSSNumberish* AnimationTimeline::duration() {
  return nullptr;
}

void AnimationTimeline::ClearOutdatedAnimation(Animation* animation) {
  DCHECK(!animation->Outdated());
  outdated_animation_count_--;
}

wtf_size_t AnimationTimeline::AnimationsNeedingUpdateCount() const {
  wtf_size_t count = 0;
  for (const auto& animation : animations_needing_update_) {
    // Exclude animations which are not actively generating frames.
    if ((!animation->CompositorPending() && !animation->Playing() &&
         !IsProgressBased()) ||
        animation->AnimationHasNoEffect()) {
      continue;
    }
    count++;
  }
  return count;
}

bool AnimationTimeline::NeedsAnimationTimingUpdate() {
  PhaseAndTime current_phase_and_time = CurrentPhaseAndTime();
  if (current_phase_and_time == last_current_phase_and_time_)
    return false;

  // We allow |last_current_phase_and_time_| to advance here when there
  // are no animations to allow animations spawned during style
  // recalc to not invalidate this flag.
  if (animations_needing_update_.empty())
    last_current_phase_and_time_ = current_phase_and_time;

  return !animations_needing_update_.empty();
}

void AnimationTimeline::ServiceAnimations(TimingUpdateReason reason) {
  TRACE_EVENT0("blink", "AnimationTimeline::serviceAnimations");

  auto current_phase_and_time = CurrentPhaseAndTime();

  if (IsProgressBased() &&
      last_current_phase_and_time_ != current_phase_and_time) {
    UpdateCompositorTimeline();
  }

  last_current_phase_and_time_ = current_phase_and_time;

  HeapVector<Member<Animation>> animations;
  animations.ReserveInitialCapacity(animations_needing_update_.size());
  for (Animation* animation : animations_needing_update_)
    animations.push_back(animation);

  std::sort(animations.begin(), animations.end(), CompareAnimations);

  for (Animation* animation : animations) {
    if (!animation->Update(reason))
      animations_needing_update_.erase(animation);
  }

  DCHECK_EQ(outdated_animation_count_, 0U);
  DCHECK(last_current_phase_and_time_ == CurrentPhaseAndTime());

#if DCHECK_IS_ON()
  for (const auto& animation : animations_needing_update_)
    DCHECK(!animation->Outdated());
#endif
  // Explicitly free the backing store to avoid memory regressions.
  // TODO(bikineev): Revisit when young generation is done.
  animations.clear();
}

// https://drafts.csswg.org/web-animations-1/#removing-replaced-animations
void AnimationTimeline::getReplaceableAnimations(
    AnimationTimeline::ReplaceableAnimationsMap* replaceable_animations_map) {
  for (Animation* animation : animations_) {
    // Initial conditions for removal:
    // * has an associated animation effect whose effect target is a descendant
    //    of doc, and
    // * is replaceable
    if (!animation->IsReplaceable())
      continue;
    DCHECK(animation->effect());
    Element* target = To<KeyframeEffect>(animation->effect())->EffectTarget();
    DCHECK(target);
    if (target->GetDocument() != animation->GetDocument())
      continue;

    auto inserted = replaceable_animations_map->insert(target, nullptr);
    if (inserted.is_new_entry) {
      inserted.stored_value->value =
          MakeGarbageCollected<HeapVector<Member<Animation>>>();
    }
    inserted.stored_value->value->push_back(animation);
  }
}

void AnimationTimeline::SetOutdatedAnimation(Animation* animation) {
  DCHECK(animation->Outdated());
  outdated_animation_count_++;
  animations_needing_update_.insert(animation);
  if (IsActive() && document_->GetPage() &&
      !document_->GetPage()->Animator().IsServicingAnimations()) {
    ScheduleServiceOnNextFrame();
  }
}

void AnimationTimeline::ScheduleServiceOnNextFrame() {
  if (document_->View())
    document_->View()->ScheduleAnimation();
}

Animation* AnimationTimeline::Play(AnimationEffect* child,
                                   ExceptionState& exception_state) {
  Animation* animation = Animation::Create(child, this, exception_state);
  if (animation) {
    DCHECK(animations_.Contains(animation));
    animation->play();
    DCHECK(animations_needing_update_.Contains(animation));
  }

  return animation;
}

void AnimationTimeline::MarkAnimationsCompositorPending(bool source_changed) {
  Animation::CompositorPendingReason reason =
      source_changed ? Animation::CompositorPendingReason::kPendingEffectChange
                     : Animation::CompositorPendingReason::kPendingUpdate;
  for (const auto& animation : animations_) {
    animation->SetCompositorPending(reason);
  }
}

void AnimationTimeline::MarkPendingIfCompositorPropertyAnimationChanges(
    const PaintArtifactCompositor* paint_artifact_compositor) {
  for (const auto& animation : animations_) {
    animation->MarkPendingIfCompositorPropertyAnimationChanges(
        paint_artifact_compositor);
  }
}

void AnimationTimeline::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(animations_needing_update_);
  visitor->Trace(animations_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
