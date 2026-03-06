// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NAVIGATION_TRANSITIONS_BLUR_TRANSITION_ANIMATION_MANAGER_H_
#define CONTENT_BROWSER_NAVIGATION_TRANSITIONS_BLUR_TRANSITION_ANIMATION_MANAGER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "cc/slim/surface_layer.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/android/window_android_observer.h"
#include "ui/gfx/native_ui_types.h"

namespace ui {
class WindowAndroid;
}  // namespace ui

namespace cc::slim {
class SolidColorLayer;
class SurfaceLayer;
}  // namespace cc::slim

namespace content {

class BackForwardTransitionAnimationManager;
class WebContents;
class WebContentsViewAndroid;

/**
 * Manages a navigation blur transition on Android by overlaying a blurred
 * reference to the previous page's live surface during a load.
 *
 * 1. ReadyToCommitNavigation: Triggered immediately prior to navigation
 *    commitment. The manager obtains a reference to the surface of the
 *    outgoing page and initializes the transition layers.
 *    `animation_state_` is initialized to `kNone`.
 *
 * 2. RenderFrameHostStateChanged: Triggered upon navigation commitment once
 *    the new frame becomes active. The blurred layer is integrated into the
 *    view hierarchy and `animation_state_` transitions to `kBlurShown`.
 *    A timer (GetBlurHoldDuration) is initialized to ensure the transition is
 *    dismissed should the new page fail to render within the expected duration.
 *
 * 3. Fallback Phase (Timer Expiration): Should the new page fail to render
 *    content before the timer expires, the manager transitions to a solid
 *    color fallback layer to prevent the persistent display of a stale blur
 *    or an unpopulated new page.
 *    - `animation_state_` transitions to `kFadeToFallbackColor`.
 *    - Following the completion of the fallback layer's opacity transition,
 *      `animation_state_` transitions to `kFallbackShown`, and the blur layer
 *      is subsequently released.
 *
 * 4. Reveal Phase (DidFirstVisuallyNonEmptyPaint): Triggered when the new page
 *    performs its initial visually non-empty paint.
 *    - `animation_state_` transitions to `kFadeOut`.
 *    - All visible transition layers fade out to reveal the newly committed
 *      page.
 *
 * 5. Cleanup: Upon completion of the fade-out or when the transition is
 *    interrupted, all layers are destroyed and `animation_state_` is reset to
 *    `kNone`.
 */
class CONTENT_EXPORT BlurTransitionAnimationManager
    : public WebContentsObserver,
      public base::SupportsUserData::Data,
      public ui::WindowAndroidObserver {
 public:
  // Interface to delegate the Android-specific view operations.
  // This decoupling allows tests to mock these operations without needing
  // to link against the internal WebContentsViewAndroid class.
  class WebContentsViewAndroidDelegate {
   public:
    virtual ~WebContentsViewAndroidDelegate() = default;
    virtual bool ShouldShowBlurTransitionAnimation(
        NavigationHandle* navigation_handle) = 0;
    virtual BackForwardTransitionAnimationManager*
    GetBackForwardTransitionAnimationManager() = 0;
    virtual gfx::NativeView GetNativeView() = 0;
    virtual ui::WindowAndroid* GetWindowAndroid() = 0;
    virtual viz::SurfaceId GetCurrentSurfaceId() = 0;
    virtual std::optional<SkColor> GetThemeColor() = 0;
  };

  ~BlurTransitionAnimationManager() override;

  static void CreateForWebContents(WebContents* web_contents);
  static BlurTransitionAnimationManager* FromWebContents(
      WebContents* web_contents);

  // WebContentsObserver:
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void RenderFrameHostStateChanged(
      RenderFrameHost* render_frame_host,
      RenderFrameHost::LifecycleState old_state,
      RenderFrameHost::LifecycleState new_state) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void DidFirstVisuallyNonEmptyPaint() override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  // ui::WindowAndroidObserver:
  void OnRootWindowVisibilityChanged(bool visible) override {}
  void OnAttachCompositor() override {}
  void OnDetachCompositor() override {}
  void OnAnimate(base::TimeTicks frame_begin_time) override;
  void OnActivityStopped() override {}
  void OnActivityStarted() override {}

  // Represents the reason why the navigation blur layer is hidden.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class TransitionExitReason {
    kFinished = 0,
    kNavigationInterrupted = 1,
    kRenderProcessGone = 2,
    kAnimationTimerExpired = 3,
    kMaxValue = kAnimationTimerExpired,
  };

  // Stops the animation state machine.
  // |should_animate_out|: If true, and the layer is currently visible, a
  // fade-out animation will play before destruction. If false, or if the layer
  // is not ready, the transition is aborted immediately.
  virtual void SignalExit(TransitionExitReason exit_reason,
                          bool should_animate_out);

 protected:
  // Virtual for testing.
  virtual WebContentsViewAndroidDelegate* GetWebContentsViewAndroidDelegate();

 private:
  friend class BlurTransitionAnimationManagerTest;
  friend class TestBlurTransitionAnimationManager;

  enum class AnimationState {
    kNone,
    kBlurShown,
    kFadeToFallbackColor,
    kFallbackShown,
    kFadeOut,
  };

  explicit BlurTransitionAnimationManager(WebContents* web_contents);

  static const void* UserDataKey();

  void SetAnimationState(AnimationState state);
  // Resets the navigation-specific state (ID and target RFH) and records the
  // exit reason if a navigation was active.
  void ResetNavigationState(TransitionExitReason exit_reason);
  void RecordExitReason(TransitionExitReason exit_reason);

  void StartFadeOut();
  void RequestAnimate();
  void OnBlurHoldTimerExpired();

  void ShowBlurTransitionLayer();
  void HideBlurTransitionLayer();
  // Removes the layer from the tree, releases the reference (freeing memory),
  // and unregisters the window observer.
  void DestroyLayer();

  void RegisterWindowObserver();
  void UnregisterWindowObserver();

  // The ID of the navigation handle that triggered the blur transition.
  int64_t navigation_id_ = 0;
  // The ID of the RenderFrameHost that we expect to commit for this navigation.
  GlobalRenderFrameHostId target_rfh_id_;

  AnimationState animation_state_ = AnimationState::kNone;
  // This captures the moment that the current animation phase actually begins.
  // It is anchored to the first frame received in OnAnimate for the current
  // phase.
  base::TimeTicks animation_phase_start_time_;
  // Add a timer to ensure that even if no event triggers the fade out, the
  // blurred layer will not persist indefinitely.
  base::OneShotTimer blur_hold_timer_;

  scoped_refptr<cc::slim::SurfaceLayer> blur_layer_;
  // This is a defensive measure against the viz compositor still displaying the
  // old pixels even though the render frame host state has changed and the
  // animation timer has expired. We do not want to show the user the previous
  // page, so we default back to status quo behaviour with a white screen before
  // pixels of the new page are populated and trigger the fade out via
  // DidFirstVisuallyNonEmptyPaint.
  scoped_refptr<cc::slim::SolidColorLayer> fallback_color_layer_;

  // Capture values to ensure smooth transitions between animation phases.
  float initial_blur_opacity_ = 0.0f;
  float initial_fallback_opacity_ = 0.0f;

  bool is_window_observer_registered_ = false;
  TransitionExitReason last_exit_reason_ =
      TransitionExitReason::kAnimationTimerExpired;
  std::unique_ptr<WebContentsViewAndroidDelegate>
      web_contents_view_android_delegate_;

  base::WeakPtrFactory<BlurTransitionAnimationManager> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_NAVIGATION_TRANSITIONS_BLUR_TRANSITION_ANIMATION_MANAGER_H_
