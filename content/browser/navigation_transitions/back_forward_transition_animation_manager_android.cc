// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigation_transitions/back_forward_transition_animation_manager_android.h"

#include "content/browser/navigation_transitions/back_forward_transition_animator.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/public/browser/back_forward_transition_animation_manager.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents_delegate.h"

namespace content {

namespace {

using NavigationDirection =
    BackForwardTransitionAnimationManager::NavigationDirection;

using AnimationStage = BackForwardTransitionAnimationManager::AnimationStage;
using SwipeEdge = ui::BackGestureEventSwipeEdge;

bool ShouldSkipDefaultNavTransitionForPendingUX(
    NavigationDirection nav_direction,
    SwipeEdge edge) {
  // Currently we only have approved UX for the history back navigation on the
  // left edge, in both gesture mode and 3-button mode.
  if (nav_direction == NavigationDirection::kBackward &&
      edge == SwipeEdge::LEFT) {
    return false;
  }
  return true;
}

// TODO(crbug.com/40260440): We shouldn't skip any transitions. Use a
// fallback UX instead.
bool ShouldSkipDefaultNavTransition(const gfx::Size& physical_backing_size,
                                    NavigationEntry* destination_entry) {
  auto* data =
      destination_entry->GetUserData(NavigationEntryScreenshot::kUserDataKey);
  if (!data) {
    // No screenshot at the destination.
    //
    // TODO(crbug.com/40260440): We should show the animation using the
    // favicon and the background color of the destination page.
    return true;
  }
  // TODO(crbug.com/41482490): We should skip if `physical_backing_size`
  // != screenshot's dimension (except for Clank native views).

  return false;
}

}  // namespace

BackForwardTransitionAnimationManagerAndroid::
    BackForwardTransitionAnimationManagerAndroid(
        WebContentsViewAndroid* web_contents_view_android,
        NavigationControllerImpl* navigation_controller)
    : web_contents_view_android_(web_contents_view_android),
      navigation_controller_(navigation_controller),
      animator_factory_(
          std::make_unique<BackForwardTransitionAnimator::Factory>()) {}

BackForwardTransitionAnimationManagerAndroid::
    ~BackForwardTransitionAnimationManagerAndroid() = default;

void BackForwardTransitionAnimationManagerAndroid::OnGestureStarted(
    const ui::BackGestureEvent& gesture,
    SwipeEdge edge,
    NavigationDirection navigation_direction) {
  std::optional<int> index =
      navigation_direction == NavigationDirection::kForward
          ? navigation_controller_->GetIndexForGoForward()
          : navigation_controller_->GetIndexForGoBack();
  CHECK(index.has_value());
  auto* destination_entry = navigation_controller_->GetEntryAtIndex(*index);

  CHECK(destination_entry)
      << "The embedder should only delegate the history navigation task "
         "to this manager if there is a destination entry.";

  // Each previous gesture should finished with `OnGestureCancelled()` or
  // `OnGestureInvoked()`. In both cases we reset `destination_entry_index_` to
  // -1.
  CHECK_EQ(destination_entry_index_, -1);
  destination_entry_index_ = *index;

  if (animator_) {
    // It's possible for a user to start a second gesture when the first gesture
    // animation is still on-going (aka "chained back"). For now, abort the
    // previous animation (impl's dtor will reset the layer's position and
    // reclaim all the resources).
    //
    // TODO(crbug.com/40261105): We need a proper UX to support this.
    animator_.reset();
  }

  if (ShouldSkipDefaultNavTransitionForPendingUX(navigation_direction, edge) ||
      ShouldSkipDefaultNavTransition(
          web_contents_view_android_->GetNativeView()->GetPhysicalBackingSize(),
          destination_entry)) {
    return;
  }

  CHECK(animator_factory_);
  animator_ = animator_factory_->Create(
      web_contents_view_android_.get(), navigation_controller_.get(), gesture,
      navigation_direction, destination_entry->GetUniqueID(), this);
  OnAnimationStageChanged();
}

void BackForwardTransitionAnimationManagerAndroid::OnGestureProgressed(
    const ui::BackGestureEvent& gesture) {
  if (animator_) {
    animator_->OnGestureProgressed(gesture);
  }
}

void BackForwardTransitionAnimationManagerAndroid::OnGestureCancelled() {
  CHECK_NE(destination_entry_index_, -1);
  if (animator_) {
    animator_->OnGestureCancelled();
  }
  destination_entry_index_ = -1;
}

void BackForwardTransitionAnimationManagerAndroid::OnGestureInvoked() {
  CHECK_NE(destination_entry_index_, -1);
  if (animator_) {
    animator_->OnGestureInvoked();
  } else {
    navigation_controller_->GoToIndex(destination_entry_index_);
  }
  destination_entry_index_ = -1;
}

void BackForwardTransitionAnimationManagerAndroid::
    OnContentForNavigationEntryShown() {
  if (animator_) {
    animator_->OnContentForNavigationEntryShown();
  }
}

AnimationStage
BackForwardTransitionAnimationManagerAndroid::GetCurrentAnimationStage() {
  return animator_ ? animator_->GetCurrentAnimationStage()
                   : AnimationStage::kNone;
}

void BackForwardTransitionAnimationManagerAndroid::OnAnimationStageChanged() {
  web_contents_view_android()
      ->web_contents()
      ->GetDelegate()
      ->DidBackForwardTransitionAnimationChange();
}

void BackForwardTransitionAnimationManagerAndroid::
    OnDidNavigatePrimaryMainFramePreCommit(
        NavigationRequest* navigation_request,
        RenderFrameHostImpl* old_host,
        RenderFrameHostImpl* new_host) {
  if (animator_) {
    animator_->OnDidNavigatePrimaryMainFramePreCommit(navigation_request,
                                                      old_host, new_host);
  }
}

void BackForwardTransitionAnimationManagerAndroid::
    OnNavigationCancelledBeforeStart(NavigationHandle* navigation_handle) {
  if (animator_) {
    animator_->OnNavigationCancelledBeforeStart(navigation_handle);
  }
}

void BackForwardTransitionAnimationManagerAndroid::
    SynchronouslyDestroyAnimator() {
  CHECK(animator_);
  animator_.reset();
  OnAnimationStageChanged();
}

}  // namespace content
