// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NAVIGATION_TRANSITIONS_BLUR_TRANSITION_ANIMATION_MANAGER_H_
#define CONTENT_BROWSER_NAVIGATION_TRANSITIONS_BLUR_TRANSITION_ANIMATION_MANAGER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "cc/slim/surface_layer.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/common/content_export.h"
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
class Layer;
}  // namespace cc::slim

namespace content {

class BackForwardTransitionAnimationManager;
class WebContents;
class WebContentsViewAndroid;

/**
 * Manages a navigation blur transition on Android by overlaying a blurred
 * reference to the previous page's live surface during a load.
 *
 * 1. ReadyToCommitNavigation: References the previous page's live surface
 *    (SurfaceId), applies a blur, and starts a "Fade In" (0% -> 100% opacity).
 * 2. DidFirstVisuallyNonEmptyPaint: Starts a "Fade Out" (100% -> 0% opacity)
 *    to reveal the new page once it's content is ready.
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
  };

  ~BlurTransitionAnimationManager() override;

  static void CreateForWebContents(WebContents* web_contents);
  static BlurTransitionAnimationManager* FromWebContents(
      WebContents* web_contents);

  // WebContentsObserver:
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
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
    kMaxValue = kRenderProcessGone,
  };

  // Stops the animation state machine.
  // |should_animate_out|: If true, and the layer is currently visible, a
  // fade-out animation will play before destruction. If false, or if the layer
  // is not ready, the transition is aborted immediately.
  virtual void StopFadeInAnimation(TransitionExitReason exit_reason,
                                   bool should_animate_out);

 protected:
  // Virtual for testing.
  virtual WebContentsViewAndroidDelegate* GetWebContentsViewAndroidDelegate();

 private:
  friend class BlurTransitionAnimationManagerTest;
  friend class TestBlurTransitionAnimationManager;

  // Animation state.
  enum class AnimationState {
    kNone,
    kFadeIn,
    kFadeOut,
  };

  explicit BlurTransitionAnimationManager(WebContents* web_contents);

  static const void* UserDataKey();

  void RegisterWindowObserver();
  void UnregisterWindowObserver();

  void StartFadeOut();

  // Removes the layer from the tree, releases the reference (freeing memory),
  // and unregisters the window observer.
  void DestroyLayer();

  void ShowBlurTransitionLayer(scoped_refptr<cc::slim::Layer> layer);
  void HideBlurTransitionLayer();

  void RecordExitReason(TransitionExitReason exit_reason);

  // The ID of the navigation handle that triggered the blur transition.
  int64_t navigation_id_ = 0;

  AnimationState animation_state_ = AnimationState::kNone;
  // This captures the moment that the animation actually begins.
  // It is anchored to the first frame received in OnAnimate.
  base::TimeTicks animation_start_time_;

  // Tracks if the next OnAnimate call is the very first frame of a new
  // animation state.
  bool is_first_frame_of_animation_ = false;

  // Offsets the animation start time to ensure visual continuity.
  // When a state change (e.g., Fade-In -> Fade-Out) occurs mid-animation, this
  // allows the new animation to begin at the equivalent opacity of the previous
  // state.
  base::TimeDelta initial_animation_offset_;

  bool is_window_observer_registered_ = false;

  scoped_refptr<cc::slim::SurfaceLayer> blur_layer_;

  TransitionExitReason last_exit_reason_ =
      TransitionExitReason::kRenderProcessGone;

  std::unique_ptr<WebContentsViewAndroidDelegate>
      web_contents_view_android_delegate_;

  base::WeakPtrFactory<BlurTransitionAnimationManager> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_NAVIGATION_TRANSITIONS_BLUR_TRANSITION_ANIMATION_MANAGER_H_
