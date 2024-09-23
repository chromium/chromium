// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/programmatic_scroll_animator.h"

#include <memory>

#include "cc/animation/animation_id_provider.h"
#include "cc/animation/scroll_offset_animation_curve_factory.h"
#include "cc/trees/target_property.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

ProgrammaticScrollAnimator::ProgrammaticScrollAnimator(
    ScrollableArea* scrollable_area)
    : scrollable_area_(scrollable_area) {}

ProgrammaticScrollAnimator::~ProgrammaticScrollAnimator() {
  if (on_finish_)
    std::move(on_finish_).Run(ScrollableArea::ScrollCompletionMode::kFinished);
}

void ProgrammaticScrollAnimator::ResetAnimationState() {
  ScrollAnimatorCompositorCoordinator::ResetAnimationState();
  animation_curve_.reset();
  start_time_ = base::TimeTicks();
  if (on_finish_)
    std::move(on_finish_).Run(ScrollableArea::ScrollCompletionMode::kFinished);
}

mojom::blink::ScrollType ProgrammaticScrollAnimator::GetScrollType() const {
  return is_sequenced_scroll_ ? mojom::blink::ScrollType::kSequenced
                              : mojom::blink::ScrollType::kProgrammatic;
}

void ProgrammaticScrollAnimator::ScrollToOffsetWithoutAnimation(
    const ScrollOffset& offset,
    bool is_sequenced_scroll) {
  CancelAnimation();
  is_sequenced_scroll_ = is_sequenced_scroll;
  ScrollOffsetChanged(offset, GetScrollType());
  is_sequenced_scroll_ = false;
  if (SmoothScrollSequencer* sequencer =
          GetScrollableArea()->GetSmoothScrollSequencer()) {
    sequencer->RunQueuedAnimations();
  }
}

void ProgrammaticScrollAnimator::AnimateToOffset(
    const ScrollOffset& offset,
    bool is_sequenced_scroll,
    ScrollableArea::ScrollCallback on_finish) {
  if (run_state_ == RunState::kPostAnimationCleanup)
    ResetAnimationState();

  is_sequenced_scroll_ = is_sequenced_scroll;
  if (on_finish_) {
    std::move(on_finish_)
        .Run(ScrollableArea::ScrollCompletionMode::kInterruptedByScroll);
  }
  on_finish_ = std::move(on_finish);
  // Ideally, if an ongoing animation exists when we receive a request to
  // animate to a different offset, instead of cancelling the current
  // animation, we could retarget the current animation to the new
  // scroll offset, keeping the velocity of the current animation.
  // When doing this, we'd need to be careful to handle the possibility
  // of repeatedly retargeting to a drastically different location such that
  // the scroll never settles.
  if (animation_curve_ && target_offset_ == offset) {
    return;
  }
  start_time_ = base::TimeTicks();
  target_offset_ = offset;
  animation_curve_ = cc::ScrollOffsetAnimationCurveFactory::CreateAnimation(
      CompositorOffsetFromBlinkOffset(target_offset_),
      cc::ScrollOffsetAnimationCurveFactory::ScrollType::kProgrammatic);

  scrollable_area_->RegisterForAnimation();
  if (!scrollable_area_->ScheduleAnimation()) {
    ResetAnimationState();
    ScrollOffsetChanged(offset, GetScrollType());
  }
  run_state_ = RunState::kWaitingToSendToCompositor;
}

void ProgrammaticScrollAnimator::CancelAnimation() {
  DCHECK_NE(run_state_, RunState::kRunningOnCompositorButNeedsUpdate);
  ScrollAnimatorCompositorCoordinator::CancelAnimation();
  if (on_finish_)
    std::move(on_finish_).Run(ScrollableArea::ScrollCompletionMode::kFinished);
}

void ProgrammaticScrollAnimator::TickAnimation(base::TimeTicks monotonic_time) {
  if (run_state_ != RunState::kRunningOnMainThread)
    return;

  if (start_time_ == base::TimeTicks())
    start_time_ = monotonic_time;
  base::TimeDelta elapsed_time = monotonic_time - start_time_;
  bool is_finished = (elapsed_time > animation_curve_->Duration());
  ScrollOffset offset =
      BlinkOffsetFromCompositorOffset(animation_curve_->GetValue(elapsed_time));
  ScrollOffsetChanged(offset, GetScrollType());

  if (is_finished) {
    run_state_ = RunState::kPostAnimationCleanup;
    AnimationFinished();
  } else if (!scrollable_area_->ScheduleAnimation()) {
    ResetAnimationState();
  }
}

void ProgrammaticScrollAnimator::UpdateCompositorAnimations() {
  if (run_state_ == RunState::kPostAnimationCleanup) {
    // No special cleanup, simply reset animation state. We have this state
    // here because the state machine is shared with ScrollAnimator which
    // has to do some cleanup that requires the compositing state to be clean.
    return ResetAnimationState();
  }

  if (compositor_animation_id() &&
      run_state_ != RunState::kRunningOnCompositor) {
    // If the current run state is WaitingToSendToCompositor but we have a
    // non-zero compositor animation id, there's a currently running
    // compositor animation that needs to be removed here before the new
    // animation is added below.
    DCHECK(run_state_ == RunState::kWaitingToCancelOnCompositor ||
           run_state_ == RunState::kWaitingToSendToCompositor);

    RemoveAnimation();

    if (run_state_ == RunState::kWaitingToCancelOnCompositor) {
      ResetAnimationState();
      return;
    }
  }

  if (run_state_ == RunState::kWaitingToSendToCompositor) {
    if (!element_id_)
      ReattachCompositorAnimationIfNeeded(
          GetScrollableArea()->GetCompositorAnimationTimeline());

    bool sent_to_compositor = false;
    if (!scrollable_area_->ShouldScrollOnMainThread()) {
      // If MultiSmoothScrollIntoView is not enabled, we use sequenced scrolls
      // which we cannot send to the compositor thread. crbug.com/730705.
      if (!is_sequenced_scroll_ ||
          RuntimeEnabledFeatures::MultiSmoothScrollIntoViewEnabled()) {
        auto animation = cc::KeyframeModel::Create(
            animation_curve_->Clone(),
            cc::AnimationIdProvider::NextKeyframeModelId(),
            cc::AnimationIdProvider::NextGroupId(),
            cc::KeyframeModel::TargetPropertyId(
                cc::TargetProperty::SCROLL_OFFSET));

        if (AddAnimation(std::move(animation))) {
          sent_to_compositor = true;
          run_state_ = RunState::kRunningOnCompositor;
        }
      }
    }

    if (!sent_to_compositor) {
      run_state_ = RunState::kRunningOnMainThread;
      animation_curve_->SetInitialValue(
          CompositorOffsetFromBlinkOffset(scrollable_area_->GetScrollOffset()));
      if (!scrollable_area_->ScheduleAnimation()) {
        ScrollOffsetChanged(target_offset_, GetScrollType());
        ResetAnimationState();
      }
    }
  }

  // If the scrollable area switched to require main thread scrolling during a
  // composited animation, continue the animation on the main thread.
  if (run_state_ == RunState::kRunningOnCompositor &&
      scrollable_area_->ShouldScrollOnMainThread()) {
    RemoveAnimation();
    run_state_ = RunState::kRunningOnMainThread;
    animation_curve_->SetInitialValue(
        CompositorOffsetFromBlinkOffset(scrollable_area_->GetScrollOffset()));
    scrollable_area_->RegisterForAnimation();
    if (!scrollable_area_->ScheduleAnimation()) {
      ResetAnimationState();
      ScrollOffsetChanged(target_offset_, GetScrollType());
    }
  }
}

void ProgrammaticScrollAnimator::NotifyCompositorAnimationFinished(
    int group_id) {
  DCHECK_NE(run_state_, RunState::kRunningOnCompositorButNeedsUpdate);
  ScrollAnimatorCompositorCoordinator::CompositorAnimationFinished(group_id);
  AnimationFinished();
}

void ProgrammaticScrollAnimator::AnimationFinished() {
  if (on_finish_)
    std::move(on_finish_).Run(ScrollableArea::ScrollCompletionMode::kFinished);
  if (is_sequenced_scroll_) {
    is_sequenced_scroll_ = false;
    if (SmoothScrollSequencer* sequencer =
            GetScrollableArea()->GetSmoothScrollSequencer())
      sequencer->RunQueuedAnimations();
  }
}

void ProgrammaticScrollAnimator::Trace(Visitor* visitor) const {
  visitor->Trace(scrollable_area_);
  ScrollAnimatorCompositorCoordinator::Trace(visitor);
}

}  // namespace blink
