// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACK_FORWARD_TRANSITION_ANIMATION_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_BACK_FORWARD_TRANSITION_ANIMATION_MANAGER_H_

#include "content/common/content_export.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/events/back_gesture_event.h"

namespace ui {
class BackGestureEvent;
}  // namespace ui

namespace content {

// This class manages the back/forward page preview animation during a
// back/forward session history navigation, triggered by a user gesture.
// Instances of this class can not outlive the associated `WebContentsView`. The
// caller of all the APIs must ensure that the associated `WebContents` is
// visible and is being composited.
class CONTENT_EXPORT BackForwardTransitionAnimationManager {
 public:
  // Indicates the direction of the session history navigation. This is required
  // as the swipe-edge alone is not sufficient to deduce the direction of the
  // history navigation.
  enum class NavigationDirection { kForward, kBackward };

  // GENERATED_JAVA_ENUM_PACKAGE: (
  //   org.chromium.content_public.browser.back_forward_transition)
  enum class AnimationStage {
    // Indicates no gesture is active or no animation is being done for a
    // gesture.
    kNone = 0,
    // Indicates that an ongoing gesture has invoked. An animation to remove the
    // content of the entry the user is navigation away from is in progress.
    kInvokeAnimation,
    // Indicates that animation manager is waiting for the embedder to show
    // its content.
    kWaitingForEmbedderContentForCommittedEntry,
    // All other stages for an active animation.
    kOther,
  };

  // Provides drawing information about the fallback UX for navigation
  // transitions, when no valid screenshot is available.
  struct FallbackUXConfig {
    // The color of the rounded rectangle that embeds the favicon.
    SkColor4f rounded_rectangle_color = SkColors::kTransparent;
    // The background color of the fallback UX.
    SkColor4f background_color = SkColors::kTransparent;
  };

  virtual ~BackForwardTransitionAnimationManager() = default;

  // Called when the user gesture for showing the history preview starts. This
  // is the starting point of the animation. The caller must end a gesture
  // sequence by calling `OnGestureCancelled()` or `OnGestureInvoked()` after
  // this API. It is invalid to start another gesture by calling
  // `OnGestureStarted()` before an existing gesture ends.
  //
  // `navigation_direction` indicates which direction the gesture should
  // navigate the user. The caller must ensure the requested history navigation
  // is feasible (i.e., there is a `NavigationEntry` for the type requested by
  // the user).
  virtual void OnGestureStarted(const ui::BackGestureEvent& gesture,
                                ui::BackGestureEventSwipeEdge edge,
                                NavigationDirection navigation_direction) = 0;

  // Called when the user swipes across the screen. This method updates the
  // animation and makes sure the animation responds to the user's finger
  // movement.
  virtual void OnGestureProgressed(const ui::BackGestureEvent& gesture) = 0;

  // Called when the user finishes the gesture but does not signal the start of
  // a session history navigation. This API is responsible for displaying a
  // "cancel animation" that puts the current page back to the center of the
  // viewport.
  virtual void OnGestureCancelled() = 0;

  // Called when the user finishes the gesture and signals the start of a
  // session history navigation. This navigation destination is specified by
  // `navigation_direction` argument of `OnGestureStarted()`. This API is
  // responsible for displaying an "invoke animation" that moves the old page
  // completely out of the viewport, and puts the new page to the center of the
  // viewport. This API also starts the session history navigation.
  virtual void OnGestureInvoked() = 0;

  // For cases where the content for a navigation entry is being drawn by the
  // embedder (instead of the web page), this notifies when the embedder has
  // rendered the UI at its final state.
  virtual void OnContentForNavigationEntryShown() = 0;

  // Get current stage of the back forward transition.
  virtual AnimationStage GetCurrentAnimationStage() = 0;

  // Sets the favicon for navigation transitions. The favicon is associated with
  // the last committed entry for the current WebContents. The favicon will be
  // used to compose a fallback UX when needed.
  virtual void SetFavicon(const SkBitmap& favicon) = 0;

  // Returns true if a gesture navigation from the screen edge `edge`, towards
  // the session history direction `navigation_direction` should be animated.
  static bool ShouldAnimateNavigationTransition(
      NavigationDirection navigation_direction,
      ui::BackGestureEventSwipeEdge edge);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACK_FORWARD_TRANSITION_ANIMATION_MANAGER_H_
