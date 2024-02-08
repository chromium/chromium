// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigation_transitions/back_forward_transition_animator.h"

#include "content/browser/navigation_transitions/back_forward_transition_animation_manager_android.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/public/browser/navigation_handle.h"

namespace content {

namespace {

using HistoryNavType = BackForwardTransitionAnimationManager::NavigationType;
using SwipeEdge = ui::BackGestureEventSwipeEdge;

}  // namespace

std::unique_ptr<BackForwardTransitionAnimator>
BackForwardTransitionAnimator::Factory::Create(
    WebContentsViewAndroid* web_contents_view_android,
    NavigationControllerImpl* controller,
    const ui::BackGestureEvent& gesture,
    HistoryNavType nav_type,
    SwipeEdge edge,
    BackForwardTransitionAnimationManagerAndroid* animation_manager) {
  return base::WrapUnique(new BackForwardTransitionAnimator(
      web_contents_view_android, controller, gesture, nav_type, edge,
      animation_manager));
}

BackForwardTransitionAnimator::BackForwardTransitionAnimator(
    WebContentsViewAndroid* web_contents_view_android,
    NavigationControllerImpl* controller,
    const ui::BackGestureEvent& gesture,
    HistoryNavType nav_type,
    SwipeEdge edge,
    BackForwardTransitionAnimationManagerAndroid* animation_manager)
    : nav_type_(nav_type),
      edge_(edge),
      animation_manager_(animation_manager),
      latest_progress_gesture_(gesture) {
  CHECK_EQ(nav_type_, HistoryNavType::kBackward);
  CHECK_EQ(edge_, SwipeEdge::LEFT);
  // TODO(https://crbug.com/1509888): Directly advance to the start state.
}

BackForwardTransitionAnimator::~BackForwardTransitionAnimator() {
  // TODO(https://crbug.com/1509888):
  // - Reset the transformation on WCVA::parent_for_web_page_widgets_;
  // - Remove UIResource for the screenshot;
  // - Detach and destroy the screenshot layer.
}

void BackForwardTransitionAnimator::OnGestureProgressed(
    const ui::BackGestureEvent& gesture) {
  CHECK_GE(gesture.progress(), 0.f);
  CHECK_LE(gesture.progress(), 1.f);

  latest_progress_gesture_ = gesture;

  // TODO(https://crbug.com/1509888):
  // - Ask physics model for transforms.
  // - Set the layers' transforms per `result`.
  // - Tick `effect_` with a fitted timestamp.
}

void BackForwardTransitionAnimator::OnGestureCancelled() {
  // TODO(https://crbug.com/1509888): Advance to displaying the cancel
  // animation.
}

void BackForwardTransitionAnimator::OnGestureInvoked() {
  // TODO(https://crbug.com/1509888): Set the request ID immediately after
  // calling `NavigationController::GoToIndex()`. After GoToIndex():
  // - If the controller doesn't have a pending entry, then we must have failed
  //   creating a `NavigationRequest`. Play the cancel animation.
  // - If the primary `FrameTreeNode` has a `NavigationRequest`, set its ID to
  //   `navigation_request_id_`. If the main frame is navigating away, we will
  //   listen to the request of the main frame.
  // - TODO(https://crbug.com/1517736) Else, traverse the entire `FrameTree` and
  //   collect all the navigation requests .
  //
  // TODO(crbug.com/1515916): Handle the subframe navigations where we have
  // multiple navigation requests for the subframes. Fow now the subframe
  // navigations are implicitly not animated because we don't capture
  // screenshots for subframe navigations.

  // TODO(https://crbug.com/1509888): Advance to displaying the invoke
  // animation.
}

void BackForwardTransitionAnimator::OnRenderWidgetHostViewSwapped(
    RenderWidgetHost* old_widget_host,
    RenderWidgetHost* new_widget_host) {
  auto* old_view =
      static_cast<RenderWidgetHostViewAndroid*>(old_widget_host->GetView());
  if (old_view) {
    // TODO(https://crbug.com/1488075): There might be a visual glitch if the
    // old page is unloaded while we are still displaying the invoke animation.
    // For now, make a deep copy of the old surface layer from `old_rwhva` and
    // put the deep copy on top of the `WCVA::parent_for_web_page_widgets_`.
    //
    // Ideally, we need a way to preserve a minimal visual state of the old
    // page.
  } else {
    // If we do a back navigation from a crashed page, we won't have an
    // `old_view`.
    //
    // TODO(https://crbug.com/1488075): The Clank's interstitial page isn't
    // drawn by `old_view`. We need to address as part of "navigating from NTP"
    // animation.
  }

  // We must have a live new widget.
  CHECK(new_widget_host);
  // `render_frame_metadata_provider()` is guaranteed non-null.
  std::optional<viz::LocalSurfaceId> last_frame_local_surface_id =
      static_cast<RenderWidgetHostImpl*>(new_widget_host)
          ->render_frame_metadata_provider()
          ->LastRenderFrameMetadata()
          .local_surface_id;
  auto* new_view =
      static_cast<RenderWidgetHostViewBase*>(new_widget_host->GetView());
  if (last_frame_local_surface_id.has_value() &&
      last_frame_local_surface_id.value().is_valid() &&
      last_frame_local_surface_id.value().embed_token() ==
          new_view->GetLocalSurfaceId().embed_token() &&
      last_frame_local_surface_id.value().IsSameOrNewerThan(
          new_view->GetLocalSurfaceId())) {
    // This can happen where the renderer's main thread is very busy to reply
    // `DidCommitNavigation()` back to the browser, but viz has already
    // activated the first frame. Because the browser hasn't received the
    // `DidCommitNavigation()` message, this animation manager hasn't subscribed
    // to the new widget host, therefore missed out on the first
    // `OnRenderFrameMetadataChangedAfterActivation()`. The screenshot will stay
    // until the next `OnRenderFrameMetadataChangedAfterActivation()`
    // notification.
    //
    // In this case we inspect the `LocalSurfaceId` of activated frame. If the
    // ID is greater than what the browser is embedding, we know viz has already
    // activated a frame. We don't need to subscribe to the new widget host
    // for `OnRenderFrameMetadataChangedAfterActivation()` at all.
    CHECK(!viz_has_activated_first_frame_);
    viz_has_activated_first_frame_ = true;
    return;
  }

  // We subscribe to `new_widget_host` to get notified when the new renderer
  // draws a new frame, so we can start cross-fading from the preview screenshot
  // to the new page's live content.
  CHECK(!new_render_widget_host_);
  new_render_widget_host_ = RenderWidgetHostImpl::From(new_widget_host);
  new_render_widget_host_->AddObserver(this);
  new_render_widget_host_->render_frame_metadata_provider()->AddObserver(this);
}

// This is only called after we subscribe to the new `RenderWidgetHost` in
// `OnRenderWidgetHostViewSwapped()`, meaning this method, just like
// `OnRenderWidgetHostViewSwapped()`, won't be called for 204/205/Download
// navigations, and won't be called if the navigation is cancelled.
//
// The manager won't be notified by the
// `OnRenderFrameMetadataChangedAfterActivation()`s that arrive earlier than
// `DidCommitNavigation()` either if the renderer is too busy to reply the
// DidCommit message while viz has already activated a new frame for the new
// page. See `OnRenderWidgetHostViewSwapped()` on how we guard this case.
void BackForwardTransitionAnimator::OnRenderFrameMetadataChangedAfterActivation(
    base::TimeTicks activation_time) {
  // `OnRenderWidgetHostViewSwapped()` is the prerequisite for this API.
  CHECK(new_render_widget_host_);

  // `DidFinishNavigation()` must have been called, because the swap of
  // `RenderWidgetHostView`s and `DidFinishNavigation()` happen in the same
  // atomic callstack (all part of the `DidCommitNavigation`).
  //
  // The navigation must have successfully committed, resulting us swapping the
  // `RenderWidgetHostView`s thus getting this notification.
  CHECK_EQ(navigation_state_, NavigationTerminalState::kCommitted);

  CHECK(!viz_has_activated_first_frame_)
      << "OnRenderFrameMetadataChangedAfterActivation can only be called once.";
  viz_has_activated_first_frame_ = true;

  // No longer interested in any other compositor frame submission
  // notifications. We can safely dismiss the previewed screenshot now.
  new_render_widget_host_->render_frame_metadata_provider()->RemoveObserver(
      this);
  new_render_widget_host_->RemoveObserver(this);
  new_render_widget_host_ = nullptr;

  // TODO(https://crbug.com/1509888): Advance to the next state to display the
  // cross-fade animation.
}

void BackForwardTransitionAnimator::OnRootWindowVisibilityChanged(
    bool visible) {
  if (!visible) {
    ui::WindowAndroid* window_android =
        animation_manager_->web_contents_view_android()
            ->GetTopLevelNativeWindow();
    CHECK(window_android);
    window_android->RemoveObserver(this);

    ui::WindowAndroidCompositor* compositor = window_android->GetCompositor();
    CHECK(compositor);

    animation_manager_->OnAnimationsFinished();
  }
}

void BackForwardTransitionAnimator::OnDetachCompositor() {
  ui::WindowAndroid* window_android =
      animation_manager_->web_contents_view_android()
          ->GetTopLevelNativeWindow();
  window_android->RemoveObserver(this);

  ui::WindowAndroidCompositor* compositor = window_android->GetCompositor();
  CHECK(compositor);

  animation_manager_->OnAnimationsFinished();
}

void BackForwardTransitionAnimator::OnAnimate(
    base::TimeTicks frame_begin_time) {
  // TODO(https://crbug.com/1509888): Implement this.
  //
  // if (crossfade) { Tick `effect_` }
  // else { ask `physics_model_` to compute the offsets }
  //
  // if (done) { advance to next state }
  // else { WindowAndroid::SetNeedsAnimate }
}

void BackForwardTransitionAnimator::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!primary_main_frame_navigation_request_id_of_gesture_nav_.has_value()) {
    // This accounts for when the user is in the middle of a swipe but a
    // navigation occurs. While the user is swiping, the current page is
    // partially out of the viewport. Since a navigation has just committed in
    // the browser, we must recenter the current page.
    //
    // TODO(https://crbug.com/1509888):
    // - Advance to the `kDisplayingCancelAnimation`.
    // - Also put the screenshot back to it's navigation entry.
    return;
  }
  if (primary_main_frame_navigation_request_id_of_gesture_nav_.value() !=
      navigation_handle->GetNavigationId()) {
    // Ignore any other request's destruction. We are only interested in the
    // primary main frame's request created by this animation manager, as a
    // result of a user-initiated session history navigation.
    return;
  }

  CHECK_EQ(navigation_state_, NavigationTerminalState::kNotSet)
      << "DidFinishNavigation can only be called once. We might have observed "
         "the wrong navigation request.";

  navigation_state_ = navigation_handle->HasCommitted()
                          ? NavigationTerminalState::kCommitted
                          : NavigationTerminalState::kCancelled;
  WebContentsObserver::Observe(nullptr);

  // TODO(https://crbug.com/1509888): OnDidFinishNavigation on physics model.

  if (navigation_handle->IsErrorPage()) {
    CHECK_EQ(navigation_state_, NavigationTerminalState::kCommitted);
    // TODO(https://crbug.com/1509887): Implement a different UX if we decide
    // not show the animation at all (i.e. abort animation early when we
    // receive the response header).
  } else {
    if (UNLIKELY(navigation_state_ == NavigationTerminalState::kCancelled)) {
      // 204/205/Download, or the ongoing navigation is cancelled. We need to
      // animate the old page back.
      //
      // TODO(https://crbug.com/1509886): We might need a better UX than just
      // display the cancel animation.
      //
      // TODO(https://crbug.com/1509888): Manually advance to the cancel state,
      // since the gesture navigation is being cancelled, meaning this animation
      // manager won't receive a `OnGestureCancelled()`.
    }
  }
  // TODO(https://crbug.com/1519149): Handle the cross-origin server redirect.
  // We cannot show a cross-origin fullscreen overlay of a.com if a.com redirect
  // the user to b.com.
}

void BackForwardTransitionAnimator::RenderWidgetHostDestroyed(
    RenderWidgetHost* widget_host) {
  if (widget_host == new_render_widget_host_) {
    // Our new widget host is about to be destroyed. This can happen for a
    // client redirect, where we never get the
    // `OnRenderFrameMetadataChangedAfterActivation()` of any frame of a
    // committed renderer. The screenshot isn't dismissed even after the gesture
    // navigation is committed.
    //
    // In such cases we simply advance to play the cross-fade from the
    // screenshot to whichever surface underneath the screenshot.
    //
    // TODO(https://crbug.com/1509888): Also CHECK `state_` that `this` is
    // waiting for new frame submissions.
    CHECK_EQ(navigation_state_, NavigationTerminalState::kCommitted);
    new_render_widget_host_ = nullptr;
    // TODO(https://crbug.com/1509888): Advance to play the cross-fade
    // animation.
  }
}

}  // namespace content
