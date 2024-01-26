// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigation_transitions/back_forward_transition_animation_manager_android.h"

#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_cache.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "ui/android/window_android.h"
#include "ui/events/back_gesture_event.h"

namespace content {

namespace {

using HistoryNavType = BackForwardTransitionAnimationManager::NavigationType;
using SwipeEdge = ui::BackGestureEventSwipeEdge;

bool ShouldSkipDefaultNavTransitionForPendingUX(HistoryNavType nav_type,
                                                SwipeEdge edge) {
  // Currently we only have approved UX for the history back navigation on the
  // left edge, in both gesture mode and 3-button mode.
  if (nav_type == HistoryNavType::kBackward && edge == SwipeEdge::LEFT) {
    return false;
  }
  return true;
}

// TODO(https://crbug.com/1424477): We shouldn't skip any transitions. Use a
// fallback UX instead.
bool ShouldSkipDefaultNavTransition(const gfx::Size& physical_backing_size,
                                    NavigationEntry* destination_entry) {
  // TODO(https://crbug.com/1509888): Implement this method. We should skip if:
  // - `destination_entry` doesn't have a screenshot.
  // - `physical_backing_size` != screenshot's dimension (except for Clank
  //    native views).
  //
  // TODO(crbug.com/1516956): We should also *explicitly* skip subframes navs
  // before they are supported. Subframes are currently skipped implicitly as we
  // don't capture screenshot for subframe navigations.
  return true;
}

}  // namespace

//=========================== `AnimationManagerImpl` ===========================

class BackForwardTransitionAnimationManagerAndroid::AnimationManagerImpl
    : public RenderFrameMetadataProvider::Observer,
      ui::WindowAndroidObserver,
      WebContentsObserver,
      RenderWidgetHostObserver {
 public:
  AnimationManagerImpl(
      WebContentsViewAndroid* web_contents_view_android,
      NavigationControllerImpl* controller,
      const ui::BackGestureEvent& gesture,
      HistoryNavType nav_type,
      SwipeEdge edge,
      BackForwardTransitionAnimationManagerAndroid* animation_manager);
  AnimationManagerImpl(const AnimationManagerImpl&) = delete;
  AnimationManagerImpl& operator=(const AnimationManagerImpl&) = delete;
  ~AnimationManagerImpl() override;

  // `RenderFrameMetadataProvider::Observer`:
  void OnRenderFrameMetadataChangedBeforeActivation(
      const cc::RenderFrameMetadata& metadata) override {}
  void OnRenderFrameMetadataChangedAfterActivation(
      base::TimeTicks activation_time) override;
  void OnRenderFrameSubmission() override {}
  void OnLocalSurfaceIdChanged(
      const cc::RenderFrameMetadata& metadata) override {}

  // `ui::WindowAndroidObserver`:
  void OnRootWindowVisibilityChanged(bool visible) override;
  void OnAttachCompositor() override {}
  void OnDetachCompositor() override;
  void OnAnimate(base::TimeTicks frame_begin_time) override;
  void OnActivityStopped() override {}
  void OnActivityStarted() override {}

  // `WebContentsObserver`:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  // `RenderWidgetHostObserver`:
  void RenderWidgetHostDestroyed(RenderWidgetHost* widget_host) override;

  // Mirrors the APIs on `BackForwardTransitionAnimationManager`.
  void OnGestureProgressed(const ui::BackGestureEvent& gesture);
  void OnGestureCancelled();
  void OnGestureInvoked();
  void OnRenderWidgetHostViewSwapped(RenderWidgetHost* old_widget_host,
                                     RenderWidgetHost* new_widget_host);

 private:
  const HistoryNavType nav_type_;
  const SwipeEdge edge_;

  // The manager back-pointer. Guaranteed to outlive the impl.
  const raw_ptr<BackForwardTransitionAnimationManagerAndroid>
      animation_manager_;

  // Tracks the `NavigationRequest` created by the gesture back navigation of a
  // primary main frame.
  absl::optional<int64_t>
      primary_main_frame_navigation_request_id_of_gesture_nav_;

  enum class NavigationTerminalState {
    // Navigation has not begun, or not yet committed.
    kNotSet = 0,
    // The navigation has either committed to a new doc, or an error page.
    kCommitted,
    // The navigation has been cancelled (replaced by a secondary navigation, or
    // aborted by the user).
    kCancelled,
  };
  // Set via `DidFinishNavigation()`. Records if the navigation has successfully
  // committed.
  NavigationTerminalState navigation_state_ = NavigationTerminalState::kNotSet;

  // If viz has already activated a frame for the new page before the invoke
  // animation finishes, we set this bit so we can start the crossfade animation
  // immediately after the invoke animation.
  bool viz_has_activated_first_frame_ = false;

  // The widget host for the new page. Only set after the new page's widget is
  // swapped in. This class listens to the first
  // `OnRenderFrameMetadataChangedAfterActivation()` on the new widget host.
  // This first notification signals that viz has processed a frame submitted by
  // the renderer, at which we can safely cross-fade from the screenshot to the
  // new page.
  //
  // Stays null for 204/205/Download, or for cancelled navigations. Also reset
  // to null when the tracked `RenderWidgetHost` is destroyed.
  raw_ptr<RenderWidgetHostImpl> new_render_widget_host_;

  // Set by the latest `OnGestureProgressed()`.
  ui::BackGestureEvent latest_progress_gesture_;
};

BackForwardTransitionAnimationManagerAndroid::AnimationManagerImpl::
    AnimationManagerImpl(
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

BackForwardTransitionAnimationManagerAndroid::AnimationManagerImpl::
    ~AnimationManagerImpl() {
  // TODO(https://crbug.com/1509888):
  // - Reset the transformation on WCVA::parent_for_web_page_widgets_;
  // - Remove UIResource for the screenshot;
  // - Detach and destroy the screenshot layer.
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
void BackForwardTransitionAnimationManagerAndroid::AnimationManagerImpl::
    OnRenderFrameMetadataChangedAfterActivation(
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

void BackForwardTransitionAnimationManagerAndroid::AnimationManagerImpl::
    OnRootWindowVisibilityChanged(bool visible) {
  if (!visible) {
    ui::WindowAndroid* window_android =
        animation_manager_->web_contents_view_android_
            ->GetTopLevelNativeWindow();
    CHECK(window_android);
    window_android->RemoveObserver(this);

    ui::WindowAndroidCompositor* compositor = window_android->GetCompositor();
    CHECK(compositor);

    animation_manager_->OnAnimationsFinished();
  }
}

void BackForwardTransitionAnimationManagerAndroid::AnimationManagerImpl::
    OnDetachCompositor() {
  ui::WindowAndroid* window_android =
      animation_manager_->web_contents_view_android_->GetTopLevelNativeWindow();
  window_android->RemoveObserver(this);

  ui::WindowAndroidCompositor* compositor = window_android->GetCompositor();
  CHECK(compositor);

  animation_manager_->OnAnimationsFinished();
}

void BackForwardTransitionAnimationManagerAndroid::AnimationManagerImpl::
    OnAnimate(base::TimeTicks frame_begin_time) {
  // TODO(https://crbug.com/1509888): Implement this.
  //
  // if (crossfade) { Tick `effect_` }
  // else { ask `physics_model_` to compute the offsets }
  //
  // if (done) { advance to next state }
  // else { WindowAndroid::SetNeedsAnimate }
}

void BackForwardTransitionAnimationManagerAndroid::AnimationManagerImpl::
    DidFinishNavigation(NavigationHandle* navigation_handle) {
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

void BackForwardTransitionAnimationManagerAndroid::AnimationManagerImpl::
    RenderWidgetHostDestroyed(RenderWidgetHost* widget_host) {
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

void BackForwardTransitionAnimationManagerAndroid::AnimationManagerImpl::
    OnGestureProgressed(const ui::BackGestureEvent& gesture) {
  CHECK_GE(gesture.progress(), 0.f);
  CHECK_LE(gesture.progress(), 1.f);

  latest_progress_gesture_ = gesture;

  // TODO(https://crbug.com/1509888):
  // - Ask physics model for transforms.
  // - Set the layers' transforms per `result`.
  // - Tick `effect_` with a fitted timestamp.
}

void BackForwardTransitionAnimationManagerAndroid::AnimationManagerImpl::
    OnGestureCancelled() {
  // TODO(https://crbug.com/1509888): Advance to displaying the cancel
  // animation.
}

void BackForwardTransitionAnimationManagerAndroid::AnimationManagerImpl::
    OnGestureInvoked() {
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

void BackForwardTransitionAnimationManagerAndroid::AnimationManagerImpl::
    OnRenderWidgetHostViewSwapped(RenderWidgetHost* old_widget_host,
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
  absl::optional<viz::LocalSurfaceId> last_frame_local_surface_id =
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

//=============== `BackForwardTransitionAnimationManagerAndroid` ===============

BackForwardTransitionAnimationManagerAndroid::
    BackForwardTransitionAnimationManagerAndroid(
        WebContentsViewAndroid* web_contents_view_android,
        NavigationControllerImpl* navigation_controller)
    : web_contents_view_android_(web_contents_view_android),
      navigation_controller_(navigation_controller) {}

BackForwardTransitionAnimationManagerAndroid::
    ~BackForwardTransitionAnimationManagerAndroid() = default;

void BackForwardTransitionAnimationManagerAndroid::OnGestureStarted(
    const ui::BackGestureEvent& gesture,
    SwipeEdge edge,
    HistoryNavType navigation_type) {
  absl::optional<int> index =
      navigation_type == HistoryNavType::kForward
          ? navigation_controller_->GetIndexForGoForward()
          : navigation_controller_->GetIndexForGoBack();
  CHECK(index.has_value());
  auto* destination_entry = navigation_controller_->GetEntryAtIndex(*index);

  CHECK(destination_entry)
      << "The embedder should only delegate the history navigation task "
         "to this manager if there is a destination entry.";

  if (impl_) {
    // It's possible for a user to start a second gesture when the first gesture
    // is still on-going (aka "chained back"). For now, abort the previous
    // animation (impl's dtor will reset the layer's position and reclaim all
    // the resources).
    //
    // TODO(https://crbug.com/1425943): We need a proper UX to support this.
    impl_.reset();
  }

  if (ShouldSkipDefaultNavTransitionForPendingUX(navigation_type, edge) ||
      ShouldSkipDefaultNavTransition(
          web_contents_view_android_->GetNativeView()->GetPhysicalBackingSize(),
          destination_entry)) {
    CHECK(!destination_entry_index_.has_value());
    // Cache the index here so that when `OnGestureInvoked()` is called this
    // animation manager knows which navigation entry to navigate to.
    destination_entry_index_ = index;
    return;
  }

  impl_ = std::make_unique<AnimationManagerImpl>(
      web_contents_view_android_.get(), navigation_controller_.get(), gesture,
      navigation_type, edge, this);
}

void BackForwardTransitionAnimationManagerAndroid::OnGestureProgressed(
    const ui::BackGestureEvent& gesture) {
  if (impl_) {
    impl_->OnGestureProgressed(gesture);
  }
}

void BackForwardTransitionAnimationManagerAndroid::OnGestureCancelled() {
  if (impl_) {
    impl_->OnGestureCancelled();
  }
  destination_entry_index_.reset();
}

void BackForwardTransitionAnimationManagerAndroid::OnGestureInvoked() {
  if (impl_) {
    CHECK(!destination_entry_index_.has_value());
    impl_->OnGestureInvoked();
  } else {
    CHECK(destination_entry_index_.has_value());
    navigation_controller_->GoToIndex(*destination_entry_index_);
    destination_entry_index_.reset();
  }
}

void BackForwardTransitionAnimationManagerAndroid::
    OnRenderWidgetHostViewSwapped(RenderWidgetHost* old_widget_host,
                                  RenderWidgetHost* new_widget_host) {
  if (impl_) {
    impl_->OnRenderWidgetHostViewSwapped(old_widget_host, new_widget_host);
  }
}

void BackForwardTransitionAnimationManagerAndroid::OnAnimationsFinished() {
  CHECK(impl_);
  impl_.reset();
}

}  // namespace content
