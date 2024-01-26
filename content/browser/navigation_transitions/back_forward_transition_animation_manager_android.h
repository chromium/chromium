// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NAVIGATION_TRANSITIONS_BACK_FORWARD_TRANSITION_ANIMATION_MANAGER_ANDROID_H_
#define CONTENT_BROWSER_NAVIGATION_TRANSITIONS_BACK_FORWARD_TRANSITION_ANIMATION_MANAGER_ANDROID_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/back_forward_transition_animation_manager.h"
#include "content/public/browser/render_frame_metadata_provider.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/android/window_android_observer.h"

namespace ui {
class BackGestureEvent;
}

namespace content {

class NavigationControllerImpl;
class RenderWidgetHost;
class WebContentsViewAndroid;

// A wrapper class that forwards the gesture event APIs to the `impl_`. It
// limits the APIs explosed to the embedder. Owned by `WebContentsViewAndroid`.
//
// If for some reason the history navigation couldn't be animated, this class
// won't create an `impl_`, and will start the history navigation via the
// `NavigationController`.
// TODO(https://crbug.com/1424477): We should always animate a gesture history
// navigation.
class CONTENT_EXPORT BackForwardTransitionAnimationManagerAndroid
    : public BackForwardTransitionAnimationManager {
 public:
  BackForwardTransitionAnimationManagerAndroid(
      WebContentsViewAndroid* web_contents_view_android,
      NavigationControllerImpl* navigation_controller);
  BackForwardTransitionAnimationManagerAndroid(
      const BackForwardTransitionAnimationManagerAndroid&) = delete;
  BackForwardTransitionAnimationManagerAndroid& operator=(
      const BackForwardTransitionAnimationManagerAndroid&) = delete;
  ~BackForwardTransitionAnimationManagerAndroid() override;

  // `NavigationTransitionAnimationManager`:
  void OnGestureStarted(const ui::BackGestureEvent& gesture,
                        ui::BackGestureEventSwipeEdge edge,
                        NavigationType navigation_type) override;
  void OnGestureProgressed(const ui::BackGestureEvent& gesture) override;
  void OnGestureCancelled() override;
  void OnGestureInvoked() override;

  // This is called when `RenderWidgetHost` is swapped: that is the old
  // `RenderWidgetHostView` is removed from the View tree but the new
  // `RenderWidgetHostView` has not yet been inserted.
  //
  // Note: This API won't get called if the navigation does not commit
  // (204/205/Download), or the navigation is cancelled (aborted by the user) or
  // replaced (by another browser-initiated navigation).
  //
  // TODO(https://crbug.com/1510570): This won't work for same-doc navigations.
  // We need to listen to `OnLocalSurfaceIdChanged` when we bump the `SurfaceId`
  // for same-doc navigations.
  //
  // TODO(https://crbug.com/1515412): This also won't work for the initial
  // navigation away from "about:blank". We might be able to treat this
  // navigation as a same-doc one.
  //
  // TODO(https://crbug.com/936696): Check the status of RD when it is close to
  // launch. Without RD we need to make sure the LocalSurfaceId is updated for
  // every navigation.
  //
  // TODO(https://crbug.com/1515590): Should consider subscribe to FCP. FCP
  // works mainframe as well as subframes navigations, with the exceptions of
  // same-doc navigations.
  void OnRenderWidgetHostViewSwapped(RenderWidgetHost* old_widget_host,
                                     RenderWidgetHost* new_widget_host);

 private:
  // The actual implementation of the animation manager that manages the history
  // navigation animation. One instance per gesture.
  class AnimationManagerImpl;

  // `impl_` invokes this callback to erase itself, when all the animation has
  // finished in the browser UI.
  void OnAnimationsFinished();

  // The owning `WebContentsViewAndroid`. Guaranteed to outlive `this`.
  const raw_ptr<WebContentsViewAndroid> web_contents_view_android_;

  // The navigation controller of the primary `FrameTree` of this `WebContents`.
  // Its life time is bound to this `WebContents`, thus guaranteed to outlive
  // this manager.
  const raw_ptr<NavigationControllerImpl> navigation_controller_;

  // The index of the destination entry in the history list. Only set if we are
  // not able to show an animated session history preview. When the feature is
  // enabled, Clank will delegate the navigation task to this AnimatinoManager
  // completely. This optional field helps the manager to memorize where to
  // navigate. This covers all the cases where we don't show an animation (e.g.,
  // LtR language right-edge swipe).
  //
  // Use an index instead of an offset, in case during the animated transition
  // the session history is updated (e.g., history.pushState()) and we don't
  // want to lead the user to the wrong entry.
  absl::optional<int> destination_entry_index_;

  // Only instantiated if the user gesture will trigger an animated session
  // history preview. Created when the eligible `OnGestureStarted()` arrives,
  // and destroyed when the animations finish (`OnAnimationsFinished()`).
  std::unique_ptr<AnimationManagerImpl> impl_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NAVIGATION_TRANSITIONS_BACK_FORWARD_TRANSITION_ANIMATION_MANAGER_ANDROID_H_
