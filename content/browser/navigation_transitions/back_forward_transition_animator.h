// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NAVIGATION_TRANSITIONS_BACK_FORWARD_TRANSITION_ANIMATOR_H_
#define CONTENT_BROWSER_NAVIGATION_TRANSITIONS_BACK_FORWARD_TRANSITION_ANIMATOR_H_

#include "content/public/browser/back_forward_transition_animation_manager.h"
#include "content/public/browser/render_frame_metadata_provider.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/android/window_android_observer.h"
#include "ui/events/back_gesture_event.h"

namespace content {

class NavigationControllerImpl;
class WebContentsViewAndroid;
class BackForwardTransitionAnimationManagerAndroid;
class RenderWidgetHostImpl;

// This class listens to gesture events for navigating the session history and
// updates the UI in response. It is 1:1 with a single gesture, i.e. each time
// the user touches the screen to start a gesture a new instance is created.
class CONTENT_EXPORT BackForwardTransitionAnimator
    : public RenderFrameMetadataProvider::Observer,
      public ui::WindowAndroidObserver,
      public WebContentsObserver,
      public RenderWidgetHostObserver {
 public:
  // To create the `BackForwardTransitionAnimator`. Tests can override this
  // factory to supply a customized version of `BackForwardTransitionAnimator`.
  class Factory {
   public:
    Factory() = default;
    Factory(const Factory&) = delete;
    Factory& operator=(const Factory&) = delete;
    virtual ~Factory() = default;

    virtual std::unique_ptr<BackForwardTransitionAnimator> Create(
        WebContentsViewAndroid* web_contents_view_android,
        NavigationControllerImpl* controller,
        const ui::BackGestureEvent& gesture,
        BackForwardTransitionAnimationManager::NavigationType nav_type,
        ui::BackGestureEventSwipeEdge edge,
        BackForwardTransitionAnimationManagerAndroid* animation_manager);
  };

  BackForwardTransitionAnimator(const BackForwardTransitionAnimator&) = delete;
  BackForwardTransitionAnimator& operator=(
      const BackForwardTransitionAnimator&) = delete;
  ~BackForwardTransitionAnimator() override;

  // Mirrors the APIs on `BackForwardTransitionAnimationManager`.
  void OnGestureProgressed(const ui::BackGestureEvent& gesture);
  void OnGestureCancelled();
  void OnGestureInvoked();
  void OnRenderWidgetHostViewSwapped(RenderWidgetHost* old_widget_host,
                                     RenderWidgetHost* new_widget_host);

 protected:
  BackForwardTransitionAnimator(
      WebContentsViewAndroid* web_contents_view_android,
      NavigationControllerImpl* controller,
      const ui::BackGestureEvent& gesture,
      BackForwardTransitionAnimationManager::NavigationType nav_type,
      ui::BackGestureEventSwipeEdge edge,
      BackForwardTransitionAnimationManagerAndroid* animation_manager);

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

 private:
  const BackForwardTransitionAnimationManager::NavigationType nav_type_;
  const ui::BackGestureEventSwipeEdge edge_;

  // The manager back-pointer. Guaranteed to outlive the impl.
  const raw_ptr<BackForwardTransitionAnimationManagerAndroid>
      animation_manager_;

  // Tracks the `NavigationRequest` created by the gesture back navigation of a
  // primary main frame.
  std::optional<int64_t>
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

}  // namespace content

#endif  // CONTENT_BROWSER_NAVIGATION_TRANSITIONS_BACK_FORWARD_TRANSITION_ANIMATOR_H_
