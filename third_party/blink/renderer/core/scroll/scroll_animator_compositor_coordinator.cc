// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scroll_animator_compositor_coordinator.h"

#include <memory>

#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation.h"

namespace blink {

ScrollAnimatorCompositorCoordinator::ScrollAnimatorCompositorCoordinator()
    : element_id_(),
      run_state_(RunState::kIdle),
      impl_only_animation_takeover_(false),
      compositor_animation_id_(0),
      compositor_animation_group_id_(0) {
  compositor_animation_ = CompositorAnimation::Create();
  DCHECK(compositor_animation_);
  compositor_animation_->SetAnimationDelegate(this);
}

// TODO(dbaron): This should probably DCHECK(element_detached_), but too
// many unittests would fail such a DCHECK().
ScrollAnimatorCompositorCoordinator::~ScrollAnimatorCompositorCoordinator() =
    default;

void ScrollAnimatorCompositorCoordinator::Dispose() {
  compositor_animation_->SetAnimationDelegate(nullptr);
  compositor_animation_.reset();
}

void ScrollAnimatorCompositorCoordinator::DetachElement() {
  DCHECK(!element_detached_);
  element_detached_ = true;
  ReattachCompositorAnimationIfNeeded(
      GetScrollableArea()->GetCompositorAnimationTimeline());
}

void ScrollAnimatorCompositorCoordinator::ResetAnimationState() {
  run_state_ = RunState::kIdle;
  RemoveAnimation();
}

bool ScrollAnimatorCompositorCoordinator::HasAnimationThatRequiresService()
    const {
  if (HasImplOnlyAnimationUpdate())
    return true;

  switch (run_state_) {
    case RunState::kIdle:
    case RunState::kRunningOnCompositor:
      return false;
    case RunState::kWaitingToCancelOnCompositorButNewScroll:
    case RunState::kPostAnimationCleanup:
    case RunState::kWaitingToSendToCompositor:
    case RunState::kRunningOnMainThread:
    case RunState::kRunningOnCompositorButNeedsUpdate:
    case RunState::kRunningOnCompositorButNeedsTakeover:
    case RunState::kRunningOnCompositorButNeedsAdjustment:
    case RunState::kWaitingToCancelOnCompositor:
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool ScrollAnimatorCompositorCoordinator::AddAnimation(
    std::unique_ptr<cc::KeyframeModel> keyframe_model) {
  RemoveAnimation();
  if (compositor_animation_->IsElementAttached()) {
    compositor_animation_id_ = keyframe_model->id();
    compositor_animation_group_id_ = keyframe_model->group();
    compositor_animation_->AddKeyframeModel(std::move(keyframe_model));
    return true;
  }
  return false;
}

void ScrollAnimatorCompositorCoordinator::RemoveAnimation() {
  if (compositor_animation_id_) {
    compositor_animation_->RemoveKeyframeModel(compositor_animation_id_);
    compositor_animation_id_ = 0;
    compositor_animation_group_id_ = 0;
  }
}

void ScrollAnimatorCompositorCoordinator::AbortAnimation() {
  if (compositor_animation_id_) {
    compositor_animation_->AbortKeyframeModel(compositor_animation_id_);
    compositor_animation_id_ = 0;
    compositor_animation_group_id_ = 0;
  }
}

void ScrollAnimatorCompositorCoordinator::CancelAnimation() {
  switch (run_state_) {
    case RunState::kIdle:
    case RunState::kWaitingToCancelOnCompositor:
    case RunState::kPostAnimationCleanup:
      break;
    case RunState::kWaitingToSendToCompositor:
      if (compositor_animation_id_) {
        // We still have a previous animation running on the compositor.
        run_state_ = RunState::kWaitingToCancelOnCompositor;
      } else {
        ResetAnimationState();
      }
      break;
    case RunState::kRunningOnMainThread:
      run_state_ = RunState::kPostAnimationCleanup;
      break;
    case RunState::kWaitingToCancelOnCompositorButNewScroll:
    case RunState::kRunningOnCompositorButNeedsAdjustment:
    case RunState::kRunningOnCompositorButNeedsTakeover:
    case RunState::kRunningOnCompositorButNeedsUpdate:
    case RunState::kRunningOnCompositor:
      run_state_ = RunState::kWaitingToCancelOnCompositor;

      // Get serviced the next time compositor updates are allowed.
      GetScrollableArea()->RegisterForAnimation();
  }
}

void ScrollAnimatorCompositorCoordinator::TakeOverCompositorAnimation() {
  switch (run_state_) {
    case RunState::kIdle:
      TakeOverImplOnlyScrollOffsetAnimation();
      break;
    case RunState::kWaitingToCancelOnCompositor:
    case RunState::kWaitingToCancelOnCompositorButNewScroll:
    case RunState::kPostAnimationCleanup:
    case RunState::kRunningOnCompositorButNeedsTakeover:
    case RunState::kWaitingToSendToCompositor:
    case RunState::kRunningOnMainThread:
      break;
    case RunState::kRunningOnCompositorButNeedsAdjustment:
    case RunState::kRunningOnCompositorButNeedsUpdate:
    case RunState::kRunningOnCompositor:
      // We call abortAnimation that makes changes to the animation running on
      // the compositor. Thus, this function should only be called when in
      // CompositingClean state.
      AbortAnimation();

      run_state_ = RunState::kRunningOnCompositorButNeedsTakeover;

      // Get serviced the next time compositor updates are allowed.
      GetScrollableArea()->RegisterForAnimation();
  }
}

void ScrollAnimatorCompositorCoordinator::CompositorAnimationFinished(
    int group_id) {
  if (compositor_animation_group_id_ != group_id)
    return;

  // TODO(crbug.com/992437) We should not need to remove completed animations
  // however they are sometimes accidentally restarted if we don't explicitly
  // remove them.
  RemoveAnimation();

  switch (run_state_) {
    case RunState::kIdle:
    case RunState::kPostAnimationCleanup:
    case RunState::kRunningOnMainThread:
      NOTREACHED_IN_MIGRATION();
      break;
    case RunState::kWaitingToSendToCompositor:
    case RunState::kWaitingToCancelOnCompositorButNewScroll:
      break;
    case RunState::kRunningOnCompositor:
    case RunState::kRunningOnCompositorButNeedsAdjustment:
    case RunState::kRunningOnCompositorButNeedsUpdate:
    case RunState::kRunningOnCompositorButNeedsTakeover:
    case RunState::kWaitingToCancelOnCompositor:
      run_state_ = RunState::kPostAnimationCleanup;
      // Get serviced the next time compositor updates are allowed.
      if (GetScrollableArea())
        GetScrollableArea()->RegisterForAnimation();
      else
        ResetAnimationState();
  }
}

bool ScrollAnimatorCompositorCoordinator::ReattachCompositorAnimationIfNeeded(
    cc::AnimationTimeline* timeline) {
  bool reattached = false;
  CompositorElementId element_id;
  if (!element_detached_) {
    element_id = GetScrollElementId();
  }
  if (element_id != element_id_) {
    if (compositor_animation_ && timeline) {
      // Detach from old layer (if any).
      if (element_id_) {
        if (compositor_animation_->IsElementAttached())
          compositor_animation_->DetachElement();
        if (GetCompositorAnimation())
          timeline->DetachAnimation(GetCompositorAnimation()->CcAnimation());
      }
      // Attach to new layer (if any).
      if (element_id) {
        DCHECK(!compositor_animation_->IsElementAttached());
        if (GetCompositorAnimation())
          timeline->AttachAnimation(GetCompositorAnimation()->CcAnimation());
        compositor_animation_->AttachElement(element_id);
        reattached = true;
      }
      element_id_ = element_id;
    }
  }

  return reattached;
}

void ScrollAnimatorCompositorCoordinator::NotifyAnimationStarted(
    base::TimeDelta monotonic_time,
    int group) {}

void ScrollAnimatorCompositorCoordinator::NotifyAnimationFinished(
    base::TimeDelta monotonic_time,
    int group) {
  NotifyCompositorAnimationFinished(group);
}

void ScrollAnimatorCompositorCoordinator::NotifyAnimationAborted(
    base::TimeDelta monotonic_time,
    int group) {
  // An animation aborted by the compositor is treated as a finished
  // animation.
  NotifyCompositorAnimationFinished(group);
}

CompositorAnimation*
ScrollAnimatorCompositorCoordinator::GetCompositorAnimation() const {
  return compositor_animation_.get();
}

gfx::PointF
ScrollAnimatorCompositorCoordinator::CompositorOffsetFromBlinkOffset(
    ScrollOffset offset) {
  return GetScrollableArea()->ScrollOffsetToPosition(offset);
}

ScrollOffset
ScrollAnimatorCompositorCoordinator::BlinkOffsetFromCompositorOffset(
    gfx::PointF position) {
  return GetScrollableArea()->ScrollPositionToOffset(position);
}

bool ScrollAnimatorCompositorCoordinator::HasImplOnlyAnimationUpdate() const {
  return !impl_only_animation_adjustment_.IsZero() ||
         impl_only_animation_takeover_;
}

CompositorElementId ScrollAnimatorCompositorCoordinator::GetScrollElementId()
    const {
  return GetScrollableArea()->GetScrollElementId();
}

void ScrollAnimatorCompositorCoordinator::UpdateImplOnlyCompositorAnimations() {
  if (!HasImplOnlyAnimationUpdate())
    return;

  cc::AnimationHost* host = GetScrollableArea()->GetCompositorAnimationHost();
  CompositorElementId element_id = GetScrollElementId();
  if (host && element_id) {
    if (!impl_only_animation_adjustment_.IsZero()) {
      host->scroll_offset_animations().AddAdjustmentUpdate(
          element_id, gfx::Vector2dF(impl_only_animation_adjustment_));
    }
    if (impl_only_animation_takeover_)
      host->scroll_offset_animations().AddTakeoverUpdate(element_id);
  }
  impl_only_animation_adjustment_ = gfx::Vector2d();
  impl_only_animation_takeover_ = false;
}

void ScrollAnimatorCompositorCoordinator::UpdateCompositorAnimations() {
  if (!GetScrollableArea()->ScrollAnimatorEnabled())
    return;

  UpdateImplOnlyCompositorAnimations();
}

void ScrollAnimatorCompositorCoordinator::ScrollOffsetChanged(
    const ScrollOffset& offset,
    mojom::blink::ScrollType scroll_type) {
  ScrollOffset clamped_offset = GetScrollableArea()->ClampScrollOffset(offset);
  GetScrollableArea()->ScrollOffsetChanged(clamped_offset, scroll_type);
}

void ScrollAnimatorCompositorCoordinator::AdjustImplOnlyScrollOffsetAnimation(
    const gfx::Vector2d& adjustment) {
  if (!GetScrollableArea()->ScrollAnimatorEnabled())
    return;

  impl_only_animation_adjustment_ += adjustment;
  GetScrollableArea()->RegisterForAnimation();
}

void ScrollAnimatorCompositorCoordinator::
    TakeOverImplOnlyScrollOffsetAnimation() {
  if (!GetScrollableArea()->ScrollAnimatorEnabled())
    return;

  impl_only_animation_takeover_ = true;

  // Update compositor animations right away to avoid skipping a frame.
  // This imposes the constraint that this function should only be called
  // from or after DocumentLifecycle::LifecycleState::CompositingClean state.
  UpdateImplOnlyCompositorAnimations();

  GetScrollableArea()->RegisterForAnimation();
}

String ScrollAnimatorCompositorCoordinator::RunStateAsText() const {
  switch (run_state_) {
    case RunState::kIdle:
      return String("Idle");
    case RunState::kWaitingToSendToCompositor:
      return String("WaitingToSendToCompositor");
    case RunState::kRunningOnCompositor:
      return String("RunningOnCompositor");
    case RunState::kRunningOnMainThread:
      return String("RunningOnMainThread");
    case RunState::kRunningOnCompositorButNeedsUpdate:
      return String("RunningOnCompositorButNeedsUpdate");
    case RunState::kWaitingToCancelOnCompositor:
      return String("WaitingToCancelOnCompositor");
    case RunState::kPostAnimationCleanup:
      return String("PostAnimationCleanup");
    case RunState::kRunningOnCompositorButNeedsTakeover:
      return String("RunningOnCompositorButNeedsTakeover");
    case RunState::kWaitingToCancelOnCompositorButNewScroll:
      return String("WaitingToCancelOnCompositorButNewScroll");
    case RunState::kRunningOnCompositorButNeedsAdjustment:
      return String("RunningOnCompositorButNeedsAdjustment");
  }
  NOTREACHED_IN_MIGRATION();
  return String();
}

}  // namespace blink
