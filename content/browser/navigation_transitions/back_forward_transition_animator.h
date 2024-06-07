// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NAVIGATION_TRANSITIONS_BACK_FORWARD_TRANSITION_ANIMATOR_H_
#define CONTENT_BROWSER_NAVIGATION_TRANSITIONS_BACK_FORWARD_TRANSITION_ANIMATOR_H_

#include "cc/resources/ui_resource_client.h"
#include "content/browser/navigation_transitions/physics_model.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/back_forward_transition_animation_manager.h"
#include "content/public/browser/render_frame_metadata_provider.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/android/view_android_observer.h"
#include "ui/android/window_android_observer.h"
#include "ui/events/back_gesture_event.h"
#include "ui/gfx/animation/keyframe/keyframe_effect.h"

namespace cc::slim {
class SolidColorLayer;
class SurfaceLayer;
class UIResourceLayer;
}

namespace content {

class NavigationControllerImpl;
class NavigationEntryScreenshot;
class WebContentsViewAndroid;
class BackForwardTransitionAnimationManagerAndroid;
class RenderWidgetHostImpl;
class RenderFrameHostImpl;

// This class listens to gesture events for navigating the session history and
// updates the UI in response. It is 1:1 with a single gesture, i.e. each time
// the user touches the screen to start a gesture a new instance is created.
class CONTENT_EXPORT BackForwardTransitionAnimator
    : public RenderFrameMetadataProvider::Observer,
      public ui::ViewAndroidObserver,
      public ui::WindowAndroidObserver,
      public WebContentsObserver,
      public RenderWidgetHostObserver,
      public gfx::FloatAnimationCurve::Target {
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
        BackForwardTransitionAnimationManager::NavigationDirection
            nav_direction,
        int destination_entry_id,
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
  void OnDidNavigatePrimaryMainFramePreCommit(
      NavigationRequest* navigation_request,
      RenderFrameHostImpl* old_host,
      RenderFrameHostImpl* new_host);
  void OnNavigationCancelledBeforeStart(NavigationHandle* navigation_handle);
  void OnContentForNavigationEntryShown();
  BackForwardTransitionAnimationManager::AnimationStage
  GetCurrentAnimationStage();

 protected:
  BackForwardTransitionAnimator(
      WebContentsViewAndroid* web_contents_view_android,
      NavigationControllerImpl* controller,
      const ui::BackGestureEvent& gesture,
      BackForwardTransitionAnimationManager::NavigationDirection nav_type,
      int destination_entry_id,
      BackForwardTransitionAnimationManagerAndroid* animation_manager);

  // `RenderFrameMetadataProvider::Observer`:
  void OnRenderFrameMetadataChangedBeforeActivation(
      const cc::RenderFrameMetadata& metadata) override {}
  void OnRenderFrameMetadataChangedAfterActivation(
      base::TimeTicks activation_time) override;
  void OnRenderFrameSubmission() override {}
  void OnLocalSurfaceIdChanged(
      const cc::RenderFrameMetadata& metadata) override {}

  // `ui::ViewAndroidObserver`:
  void OnAttachedToWindow() override {}
  void OnDetachedFromWindow() override;

  // `ui::WindowAndroidObserver`:
  void OnRootWindowVisibilityChanged(bool visible) override;
  void OnAttachCompositor() override {}
  void OnDetachCompositor() override;
  void OnAnimate(base::TimeTicks frame_begin_time) override;
  void OnActivityStopped() override {}
  void OnActivityStarted() override {}

  // `WebContentsObserver`:
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  // `RenderWidgetHostObserver`:
  void RenderWidgetHostDestroyed(RenderWidgetHost* widget_host) override;

  // `gfx::FloatAnimationCurve::Target`:
  void OnFloatAnimated(const float& value,
                       int target_property_id,
                       gfx::KeyframeModel* keyframe_model) override;

  // Called when each animation finishes. Advances `this` into the next state.
  // Being virtual for testing.
  virtual void OnCancelAnimationDisplayed();
  virtual void OnInvokeAnimationDisplayed();
  virtual void OnCrossFadeAnimationDisplayed();

  // Identifies the different stages of the animation that this manager is in.
  enum class State {
    // Set immediately when `OnGestureStarted` is called. Indicates that the
    // user has started swiping from the edge of the screen. The manager remains
    // in this state until the user has lifted the finger from the screen, to
    // either start the history navigation or not start it.
    kStarted = 0,

    // No explicit state while the user swipes across the screen, since there
    // is no stateful changes during the in-progress transition period.

    // Set when `OnGestureCancelled` is called, signaling the user has decided
    // to not start the history navigation.
    //
    // Also set when the gesture-initiated navigation is aborted or cancelled.
    // In this state, an animation is being displayed to dismiss the screenshot
    // and bring the old page back to the viewport.
    //
    // Also set when the active page has a BeforeUnload handler and we need to
    // animate the active page back so the user can interact with the
    // BeforeUnload prompt. TODO(liuwilliam): Worth considering a
    // `kDisplayingCancelAnimationForBeforeUnload` to reduce the compexity in
    // the `State`'s transition.
    kDisplayingCancelAnimation,

    // Set after the browser has dispatched BeforeUnload IPC to the renderer and
    // is waiting for the response, and the cancel animation has brought back
    // the active page to the center of the viewport. This is an optional state:
    // if the cancel animation hasn't finished before the renderer has
    // responded, we will skip this state.
    kWaitingForBeforeUnloadResponse,

    // TODO(crbug.com/40896070): If we were to bring the active page back
    // to let the user interact with the prompt (e.g., camera access), we need a
    // state for that.

    // Set when `OnGestureInvoked` is called, signaling the user has decided
    // to start the history navigation. Animations are displayed to bring the
    // screenshot to the center of the viewport, and to bring the old page
    // completely out of the viewport.
    //
    // The gesture-initiated history navigation starts at the beginning of this
    // state. The same navigation is finished in the browser at the end of this
    // state.
    //
    // Internally, this state covers `PhysicsModel`'s commit-pending spring
    // and invoke spring. We don't differentiate commit-pending vs invoke as
    // commit-pending is designed to be a `PhysicsModel` internal state.
    kDisplayingInvokeAnimation,

    // An optional state only reachable from `kDisplayingInvokeAnimation`: at
    // the end of the invoke animation, the screenshot is centered at the
    // viewport. Before the new page is ready to be presented the user, the
    // screenshot will persist at the viewport center. The screenshot is only
    // crossfaded out after the new renderer is ready to be presented to the
    // user, which is signalled via
    // `OnRenderFrameMetadataChangedAfterActivation()`, meaning viz has
    // processed a new compositor frame submitted by the new renderer.
    //
    // If `OnRenderFrameMetadataChangedAfterActivation()` is received before
    // the end of `kDisplayingInvokeAnimation`, this state will be skipped
    // completely.
    kWaitingForNewRendererToDraw,

    // A state only reachable from `kDisplayingInvokeAnimation`: at
    // the end of the invoke animation, the animator is waiting for the
    // embedder content to be fully visible. The animator will continue or
    // end after the content becomes fully visible.
    kWaitingForContentForNavigationEntryShown,

    // Reachable from the end of `kDisplayingInvokeAnimation` or from
    // `kWaitingForNewRendererToDraw`. Cross-fading from the screenshot to the
    // new page.
    kDisplayingCrossFadeAnimation,

    // The terminal state of the animation manager. We reach this state when
    // all the animations are finished in the UI. The manager remains in this
    // state until it is destroyed.
    kAnimationFinished,
  };
  State state() const { return state_; }

  static bool CanAdvanceTo(State from, State to);
  static std::string ToString(State state);

  enum class NavigationState {
    // Navigation has not begun.
    kNotStarted = 0,

    // Two states to track the BeforeUnload handler. They are optional if the
    // active page does not have a BeforeUnload handler.
    kBeforeUnloadDispatched,
    // This state functions as a boolean flag to distinguish how we get to
    // `kStarted`:
    // - From `kNotStarted` as regular navigations, or;
    // - From `kBeforeUnloadAckedProceed` as navigations with BeforeUnload
    //   handlers.
    // It's only set when the browser receives the renderer's ack with proceed,
    // and advances to `kStarted` when the navigation starts, which happens
    // within an atomic stack.
    kBeforeUnloadAckedProceed,

    // The navigation is cancelled before it starts. Terminal state 1/3.
    // Reachable from `kNotStarted` and `kBeforeUnloadDispatched`.
    kCancelledBeforeStart,
    // The navigation has started in the browser.
    kStarted,
    // The navigation has either committed to a new doc, or an error page.
    // Terminal state 2/3.
    kCommitted,
    // The navigation has been cancelled (cancelled by a secondary navigation,
    // or aborted by the user). Terminal state 3/3.
    kCancelled,
  };
  static std::string ToString(NavigationState state);

 private:
  // Initializes `effect_` for the scrim and cross-fade animation.
  void InitializeEffectForGestureProgressAnimation();
  void InitializeEffectForCrossfadeAnimation();

  // Advance current `state_` to `state`.
  void AdvanceAndProcessState(State state);

  // Let this manager respond to the current `state_`.
  void ProcessState();

  // Initializes the `ui_resource_layer_` and sets up the layer tree.
  void SetupForScreenshotPreview(
      std::unique_ptr<NavigationEntryScreenshot> screenshot);

  // Start the session history navigation, and start tracking the created
  // `NavigationRequests` by their IDs. Returns true if the requests are
  // successfully created and false otherwise. The caller should play the invoke
  // or cancel animation based on the return value.
  [[nodiscard]] bool StartNavigationAndTrackRequest();

  // Forwards the calls to `CompositorImpl`.
  cc::UIResourceId CreateUIResource(cc::UIResourceClient* client);
  void DeleteUIResource(cc::UIResourceId resource_id);

  // Apply the `result` to the screenshot and the web page, and tick the
  // animation effector. Returns a boolean indicating if both the `PhysicsModel`
  // and the `gfx::KeyFrameModels` have finished playing.
  [[nodiscard]] bool SetLayerTransformationAndTickEffect(
      const PhysicsModel::Result& result);

  void CloneOldSurfaceLayerAndRegisterNewFrameActivationObserver(
      RenderFrameHostImpl* old_host,
      RenderFrameHostImpl* new_host);

  void CloneOldSurfaceLayer(RenderWidgetHostViewBase* old_main_frame_view);

  void UnregisterNewFrameActivationObserver();

  const BackForwardTransitionAnimationManager::NavigationDirection
      nav_direction_;

  // The ID of the destination `NavigationEntry`. Constant through out the
  // lifetime of a gesture so we are guaranteed to target the correct entry.
  // This is also guaranteed to be equal to `screenshot_->navigation_entry_id()`
  // once `screenshot_` is set.
  const int destination_entry_id_;

  // The manager back-pointer. Guaranteed to outlive the impl.
  const raw_ptr<BackForwardTransitionAnimationManagerAndroid>
      animation_manager_;

  // Tracks the `NavigationRequest` created by the gesture back navigation of a
  // primary main frame.
  std::optional<int64_t>
      primary_main_frame_navigation_request_id_of_gesture_nav_;

  // The unique id assigned to `screenshot_`.
  cc::UIResourceId ui_resource_id_ =
      cc::UIResourceClient::kUninitializedUIResourceId;

  // New layer for the scrim. Always on top of the `ui_resource_layer_`.
  scoped_refptr<cc::slim::SolidColorLayer> screenshot_scrim_;

  // New layer for `screenshot_`.
  scoped_refptr<cc::slim::UIResourceLayer> ui_resource_layer_;

  // A copy of old surface, covering the entire old page from when the
  // navigation commits to the end of the invoke animation (where the old page
  // is completely out of the viewport).
  scoped_refptr<cc::slim::SurfaceLayer> old_surface_clone_;

  // The pre-captured screenshot used for previewing. The ownership of the
  // screenshot is transferred from the cache to this manager when the gesture
  // starts. If the user decides not to start the history navigation, or the
  // gesture navigation starts but cancelled by another navigation, the
  // screenshot will be placed back into the cache.
  //
  // Let this animation manager take ownership of the screenshot during the
  // animation. This is to keep the cache from evicting the screenshot while
  // it's being displayed in the UI.
  std::unique_ptr<NavigationEntryScreenshot> screenshot_;

  // Tracks various state of the navigation request associated with this
  // gesture. Only set if the navigation request is successfully created.
  NavigationState navigation_state_ = NavigationState::kNotStarted;

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

  // Responsible for the non-transformational animations (scrim and
  // cross-fade).
  gfx::KeyframeEffect effect_;

  // Responsible for the transformational animations.
  PhysicsModel physics_model_;

  // Set by the latest `OnGestureProgressed()`.
  ui::BackGestureEvent latest_progress_gesture_;

  // A transition always suppresses sending input events to the renderer.
  WebContentsImpl::ScopedIgnoreInputEvents ignore_input_scope_;

  State state_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NAVIGATION_TRANSITIONS_BACK_FORWARD_TRANSITION_ANIMATOR_H_
