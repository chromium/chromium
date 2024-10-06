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
#include "ui/android/modal_dialog_manager_bridge.h"
#include "ui/android/view_android_observer.h"
#include "ui/android/window_android_observer.h"
#include "ui/events/back_gesture_event.h"
#include "ui/gfx/animation/keyframe/keyframe_effect.h"

namespace cc::slim {
class Layer;
class SolidColorLayer;
class SurfaceLayer;
class UIResourceLayer;
}

namespace content {

class NavigationControllerImpl;
class NavigationEntryScreenshot;
class WebContentsViewAndroid;
class BackForwardTransitionAnimationManagerAndroid;
class ProgressBar;
class RenderWidgetHostImpl;
class RenderFrameHostImpl;

// This class listens to gesture events for navigating the session history and
// updates the UI in response. It is 1:1 with a single gesture, i.e. each time
// the user touches the screen to start a gesture a new instance is created.
class CONTENT_EXPORT BackForwardTransitionAnimator
    : public gfx::FloatAnimationCurve::Target,
      public gfx::TransformAnimationCurve::Target {
 public:
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

    // One of the two terminal states of the animation manager. We reach this
    // state when all the animations are finished in the UI. The manager remains
    // in this state until it is destroyed.
    kAnimationFinished,

    // Another terminal state indicating that we have to abort the animated
    // transition. This can happen, for example, when a secondary navigation
    // commits mid-animation, or when Chrome is backgrounded during a
    // transition.
    kAnimationAborted,
  };
  static const char* StateToString(State state);

  // Indicates the animation abort reason for UMA metrics.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Upon adding a new value, add it
  // to `tools/metrics/histograms/metadata/navigation/enums.xml` as well.
  enum class AnimationAbortReason {
    // The subscribed `RenderWidgetHost` was destroyed.
    kRenderWidgetHostDestroyed = 0,

    kMainCommitOnSubframeTransition = 1,
    kNewCommitInPrimaryMainFrame = 2,
    kCrossOriginRedirect = 3,
    kNewCommitWhileDisplayingInvokeAnimation = 4,
    kNewCommitWhileDisplayingCanceledAnimation = 5,
    kNewCommitWhileWaitingForNewRendererToDraw = 6,
    kNewCommitWhileWaitingForContentForNavigationEntryShown = 7,
    kNewCommitWhileDisplayingCrossFadeAnimation = 8,
    kNewCommitWhileWaitingForBeforeUnloadResponse = 9,
    kMultipleNavigationRequestsCreated = 10,

    // The navigation entry was deleted when the navigation was ready to commit.
    kNavigationEntryDeletedBeforeCommit = 11,

    // The new frame is not activated in time.
    kPostNavigationFirstFrameTimeout = 12,

    // The user started a new gesture while the first one is still on-going.
    kChainedBack = 13,

    // Set when the native view is detached from the native window. Clank can
    // sometimes detach the view without detaching the compositor first. See
    // https://crbug.com/344761329.
    kDetachedFromWindow = 14,

    // Set when the native window becomes invisible.
    kRootWindowVisibilityChanged = 15,

    kCompositorDetached = 16,

    // The animation manager is destroyed. This can happen when a visible
    // `WebContents` is destroyed when its NativeView, layer and compositor are
    // still intact, thus bypassing other observer hooks.
    kAnimationManagerDestroyed = 17,

    // Abort the animation when the physical size of the screen has changed.
    // This can happen when the user rotates the phone mid-animation.
    kPhysicalSizeChanged = 18,

    kMaxValue = kPhysicalSizeChanged
  };

  // Indicates what animation state caused input event suppression.
  enum class IgnoringInputReason {
    kAnimationInvokedOccurred = 0,
    kAnimationCanceledOccurred = 1,
    kNoOccurrence = 2
  };

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
        ui::BackGestureEventSwipeEdge initiating_edge,
        NavigationEntryImpl* destination_entry,
        SkBitmap embedder_content,
        BackForwardTransitionAnimationManagerAndroid* animation_manager);
  };

  BackForwardTransitionAnimator(const BackForwardTransitionAnimator&) = delete;
  BackForwardTransitionAnimator& operator=(
      const BackForwardTransitionAnimator&) = delete;
  ~BackForwardTransitionAnimator() override;

  // Mirrors the APIs on `BackForwardTransitionAnimationManager`.
  // Some of them are virtual for testing purposes.
  void OnGestureProgressed(const ui::BackGestureEvent& gesture);
  void OnGestureCancelled();
  void OnGestureInvoked();
  void OnContentForNavigationEntryShown();
  BackForwardTransitionAnimationManager::AnimationStage
  GetCurrentAnimationStage();
  virtual void OnAnimate(base::TimeTicks frame_begin_time);
  void OnRenderWidgetHostDestroyed(RenderWidgetHost* widget_host);
  virtual void OnRenderFrameMetadataChangedAfterActivation(
      base::TimeTicks activation_time);
  virtual void DidStartNavigation(NavigationHandle* navigation_handle);
  virtual void ReadyToCommitNavigation(NavigationHandle* navigation_handle);
  virtual void DidFinishNavigation(NavigationHandle* navigation_handle);
  void OnDidNavigatePrimaryMainFramePreCommit(
      NavigationRequest* navigation_request,
      RenderFrameHostImpl* old_host,
      RenderFrameHostImpl* new_host);
  void OnNavigationCancelledBeforeStart(NavigationHandle* navigation_handle);
  void MaybeRecordIgnoredInput(const blink::WebInputEvent& event);

  // Notifies when the transition needs to be aborted.
  void AbortAnimation(AnimationAbortReason abort_reason);

  [[nodiscard]] bool IsTerminalState();

  cc::slim::Layer* screenshot_layer_for_testing() const {
    return screenshot_layer_.get();
  }
  cc::slim::SolidColorLayer* scrim_layer_for_testing() const {
    return screenshot_scrim_.get();
  }
  cc::slim::SurfaceLayer* clone_layer_for_testing() const {
    return old_surface_clone_.get();
  }
  cc::slim::SolidColorLayer* rrect_layer_for_testing() const {
    return rounded_rectangle_.get();
  }
  ProgressBar* progress_bar_for_testing() const { return progress_bar_.get(); }
  cc::slim::UIResourceLayer* embedder_live_content_clone_for_testing() const {
    return embedder_live_content_clone_.get();
  }

  base::OneShotTimer* dismiss_screenshot_timer_for_testing() {
    return &dismiss_screenshot_timer_;
  }

 protected:
  BackForwardTransitionAnimator(
      WebContentsViewAndroid* web_contents_view_android,
      NavigationControllerImpl* controller,
      const ui::BackGestureEvent& gesture,
      BackForwardTransitionAnimationManager::NavigationDirection nav_direction,
      ui::BackGestureEventSwipeEdge initiating_edge,
      NavigationEntryImpl* destination_entry,
      SkBitmap embedder_content,
      BackForwardTransitionAnimationManagerAndroid* animation_manager);

  // `gfx::FloatAnimationCurve::Target`:
  void OnFloatAnimated(const float& value,
                       int target_property_id,
                       gfx::KeyframeModel* keyframe_model) override;

  // `gfx::TransformAnimationCurve::Target`:
  void OnTransformAnimated(const gfx::TransformOperations& transform,
                           int target_property_id,
                           gfx::KeyframeModel* keyframe_model) override;

  // Called when each animation finishes. Advances `this` into the next state.
  // Being virtual for testing.
  virtual void OnCancelAnimationDisplayed();
  virtual void OnInvokeAnimationDisplayed();
  virtual void OnCrossFadeAnimationDisplayed();

  static bool CanAdvanceTo(State from, State to);

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
  static const char* NavigationStateToString(NavigationState state);

  ui::BackGestureEventSwipeEdge initiating_edge() const {
    return initiating_edge_;
  }

  State state_;

  // The destination `FrameNavigationEntry::item_sequence_number()` of the
  // gesture back navigation in the primary main frame. Set when the browser
  // tells the renderer to commit the navigation.
  int64_t primary_main_frame_navigation_entry_item_sequence_number_ =
      cc::RenderFrameMetadata::kInvalidItemSequenceNumber;

 private:
  // Initializes `effect_` for the scrim and cross-fade animation.
  void InitializeEffectForGestureProgressAnimation();
  void InitializeEffectForCrossfadeAnimation();

  // Advance current `state_` to `state`.
  void AdvanceAndProcessState(State state);

  // Let this manager respond to the current `state_`.
  void ProcessState();

  // Initializes the `ui_resource_layer_` and sets up the layer tree.
  void SetupForScreenshotPreview(SkBitmap embedder_content);

  // Sets the progress bar shown during the invoke phase of the animation.
  void SetupProgressBar();

  // Start the session history navigation, and start tracking the created
  // `NavigationRequests` by their IDs. Returns true if the requests are
  // successfully created and false otherwise. The caller should play the invoke
  // or cancel animation based on the return value.
  [[nodiscard]] bool StartNavigationAndTrackRequest();
  void TrackRequest(base::WeakPtr<NavigationRequest> created_request);

  struct ComputedAnimationValues {
    // The offset that will be applied to the live, outgoing page.
    float live_page_offset_px = 0.f;
    // The offset that will be applied to the incoming screenshot layer.
    float screenshot_offset_px = 0.f;
    // The current progress of the animation, running from 0 to 1.
    float progress = 0.f;
  };

  // The physics model is agnostic of UI writing mode (LTR vs RTL) as well as
  // navigation direction and functions in terms of a spring on the left side
  // applied to a layer moving to the right. This method transforms the physics
  // result values into values usable by the animator.
  ComputedAnimationValues ComputeAnimationValues(
      const PhysicsModel::Result& result);

  // Forwards the calls to `CompositorImpl`.
  cc::UIResourceId CreateUIResource(cc::UIResourceClient* client);
  void DeleteUIResource(cc::UIResourceId resource_id);

  // Apply the `result` to the screenshot and the web page, and tick the
  // animation effector. Returns a boolean indicating if both the `PhysicsModel`
  // and the `gfx::KeyFrameModels` have finished playing.
  [[nodiscard]] bool SetLayerTransformationAndTickEffect(
      const PhysicsModel::Result& result);

  void MaybeCloneOldSurfaceLayer(RenderWidgetHostViewBase* old_main_frame_view);

  void SetUpEmbedderContentLayerIfNeeded(SkBitmap embedder_content);

  // Called when the navigation is ready to be committed in the renderer.
  void SubscribeToNewRenderWidgetHost(NavigationRequest* navigation_request);

  void UnregisterNewFrameActivationObserver();

  int GetViewportWidthPx() const;
  int GetViewportHeightPx() const;

  void StartInputSuppression(IgnoringInputReason ignoring_input_reason);

  void InsertLayersInOrder();

  // Dispatched when `dismiss_screenshot_timer_` fires, to remove the stale
  // screenshot after the screenshot is fully centered because the new Document
  // hasn't produced a frame yet.
  void OnPostNavigationFirstFrameTimeout();

  void ResetLiveOverlayLayer();

  // Calculate the start and end position of the rrect for the fallback UX, in
  // physical pixels.
  gfx::PointF CalculateRRectStartPx() const;
  gfx::PointF CalculateRRectEndPx() const;

  int DipToPx(int dip) const;

  void DeferDialogs();
  void ResumeDialogs();

  const BackForwardTransitionAnimationManager::NavigationDirection
      nav_direction_;

  const ui::BackGestureEventSwipeEdge initiating_edge_;

  // The ID of the destination `NavigationEntry`. Constant through out the
  // lifetime of a gesture so we are guaranteed to target the correct entry.
  // This is also guaranteed to be equal to `screenshot_->navigation_entry_id()`
  // once `screenshot_` is set.
  const int destination_entry_id_;

  // The manager back-pointer. Guaranteed to outlive the impl.
  const raw_ptr<BackForwardTransitionAnimationManagerAndroid>
      animation_manager_;

  // Track the ID of the `NavigationRequest` created by the gesture back
  // navigation in the primary main frame or in the subframe:
  // - If a request is created in the primary main frame, we won't track any of
  // the subframe requests (i.e., a fragment navigation in the primary main
  // frame and cross-doc navigations in the subframes).
  // - Else, we track the subframe request.
  // - For any navigation with more than one subframe requests, the transition
  // is aborted.
  struct TrackedRequest {
    int64_t navigation_id;
    bool is_primary_main_frame;
  };
  std::optional<TrackedRequest> tracked_request_;

  // Set when a navigation is being started.
  bool is_starting_navigation_ = false;

  // The unique id assigned to `screenshot_`.
  cc::UIResourceId ui_resource_id_ =
      cc::UIResourceClient::kUninitializedUIResourceId;

  // New layer for the scrim. Always on top of the `ui_resource_layer_`.
  scoped_refptr<cc::slim::SolidColorLayer> screenshot_scrim_;

  // New layer for `screenshot_`.
  scoped_refptr<cc::slim::Layer> screenshot_layer_;

  // A copy of old surface, covering the entire old page from when the
  // navigation commits to the end of the invoke animation (where the old page
  // is completely out of the viewport).
  // - For cross-RFH navigations, it is cloned before RFH swap;
  // - For same-RFH and same-doc navigations, it is cloned immediately after we
  //   tell the renderer to commit the navigation.
  scoped_refptr<cc::slim::SurfaceLayer> old_surface_clone_;

  // A copy of the embedder content to show the content from the embedder side.
  // Only one of old_surface_clone_ or embedder_live_content_clone_ will be set.
  scoped_refptr<cc::slim::UIResourceLayer> embedder_live_content_clone_;

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

  // If `screenshot_` is supplied by the embedder.
  const bool is_copied_from_embedder_;

  // The scale factor is constant per gesture.
  const float device_scale_factor_;

  // Color and positional information to compose a fallback screenshot.
  struct FallbackUX {
    BackForwardTransitionAnimationManager::FallbackUXConfig color_config;
    // The start and stop positions of the rounded rectangle, with respect to
    // its parent (the screenshot layer).
    gfx::PointF start_px;
    gfx::PointF end_px;
  };
  std::optional<FallbackUX> fallback_ux_;
  // The rounded rectangle specified by `fallback_ux_`. It embeds the favicon,
  // and is child of the screenshot layer. Need the reference here because the
  // animation timeline of the rounded rectangle and the favicon is different
  // from the screenshot.
  scoped_refptr<cc::slim::SolidColorLayer> rounded_rectangle_;

  // Tracks various state of the navigation request associated with this
  // gesture. Only set if the navigation request is successfully created.
  NavigationState navigation_state_ = NavigationState::kNotStarted;

  // If viz has already activated a frame for the new page before the invoke
  // animation finishes, we set this bit so we can start the crossfade animation
  // immediately after the invoke animation.
  bool viz_has_activated_first_frame_ = false;

  // The widget host for the new page.
  // - For cross-doc navigations, it is set when the browser receives the
  //   "DidCommit" message.
  // - For same-doc navigations, it is set immediately after we tell the
  //   renderer to commit the navigation.
  //
  // It listens to the first
  // `OnRenderFrameMetadataChangedAfterActivation()` on the new widget host.
  // This first notification signals that viz has processed a frame submitted by
  // the renderer, at which we can safely cross-fade from the screenshot to the
  // new page.
  //
  // Stays null for 204/205/Download, or for cancelled navigations. Also reset
  // to null when the tracked `RenderWidgetHost` is destroyed.
  raw_ptr<RenderWidgetHostImpl> new_render_widget_host_;

  // Responsible for the non-transformational animations (e.g., scrim and
  // cross-fade), and the position of the rounded rectangle (when fallback UX is
  // used).
  gfx::KeyframeEffect effect_;

  // Responsible for the transformational animations of the live page and the
  // screenshot.
  PhysicsModel physics_model_;

  // Set by the latest `OnGestureProgressed()`.
  ui::BackGestureEvent latest_progress_gesture_;

  // The indeterminate progress bar shown during the invoke animation.
  std::unique_ptr<ProgressBar> progress_bar_;

  // A transition suppresses sending input events to the renderer during the
  // animation.
  std::optional<WebContentsImpl::ScopedIgnoreInputEvents> ignore_input_scope_;

  // A timer to dismiss the potentially stale screenshot, after screenshot is
  // fully centered (at the end of the invoke animation).
  base::OneShotTimer dismiss_screenshot_timer_;

  // Counter for different combinations of reason and position of ignored
  // inputs.
  struct IgnoredReasonCategoryAndCount {
    int animation_invoked_on_source = 0;
    int animation_invoked_on_destination = 0;
    int animation_canceled_on_source = 0;
    int animation_canceled_on_destination = 0;
  };
  IgnoredReasonCategoryAndCount ignored_inputs_count_;

  IgnoringInputReason ignoring_input_reason_ =
      IgnoringInputReason::kNoOccurrence;

  // Stores the token that identify the deferred dialogs. During the animated
  // transition, the live page could show the user some permission prompts or
  // alerts before unloaded. We suppress these dialogs during the transition.
  // These dialogs will be re-presented if the swipe gesture does not unload the
  // live page.
  int deferred_dialog_token_ =
      ui::ModalDialogManagerBridge::kInvalidDialogToken;

  base::WeakPtrFactory<BackForwardTransitionAnimator> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_NAVIGATION_TRANSITIONS_BACK_FORWARD_TRANSITION_ANIMATOR_H_
