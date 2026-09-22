// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_BROWSER_AGENT_OBSERVER_H_
#define IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_BROWSER_AGENT_OBSERVER_H_

#import "base/observer_list_types.h"

class FullscreenBrowserAgent;

// Enum representing the type of fullscreen transition.
enum class FullscreenTransition { kEnterFullscreen, kExitFullscreen };

// Observer interface for the FullscreenBrowserAgent.
class FullscreenBrowserAgentObserver : public base::CheckedObserver {
 public:
  // Called before the fullscreen state updates.
  virtual void WillUpdateState(FullscreenBrowserAgent* agent) {}

  // Called after the fullscreen state updates.
  virtual void DidUpdateState(FullscreenBrowserAgent* agent) {}

  // Called once per display refresh while an animated transition is running,
  // plus a final time with the exact target value. Read the value from
  // FullscreenBrowserAgent::interpolated_progress(); top_progress() and
  // bottom_progress() stay pinned to the transition target for the whole
  // animation and are not useful here.
  //
  // Do not override this whenever the observer's UI is an *affine* function of
  // progress. UIKit already interpolates such UI correctly and off the main
  // thread for free, so overriding would burn main thread time every frame
  // without changing a single pixel. Override only for UI that cannot be
  // expressed that way: a non-linear easing curve, a blur radius, a discrete
  // crossfade, or any value that is not an animatable UIView property.
  //
  // An override MUST NOT let its per-frame work change the observer's obscured
  // inset contribution. Insets are collected once, at the transition target,
  // during WillUpdateState(), and the web content then interpolates between
  // its start and target insets independently. An observer whose *layout*
  // varied non-linearly per frame would drift away from the web content and
  // open a visible gap at the content edge. Keep per-frame work purely
  // presentational (transform, alpha, blur, corner radius, ...).
  virtual void DidUpdateInterpolatedProgress(FullscreenBrowserAgent* agent) {}

  // Called before the obscured inset range updates.
  virtual void WillUpdateObscuredInsetRange(FullscreenBrowserAgent* agent) {}

  // Called after the obscured inset range updates.
  virtual void DidUpdateObscuredInsetRange(FullscreenBrowserAgent* agent) {}

  // Called when the fullscreen transition completes.
  virtual void FullscreenDidTransition(FullscreenBrowserAgent* agent,
                                       FullscreenTransition transition) {}

  // Called when the FullscreenBrowserAgent is shutting down.
  virtual void WillShutDown(FullscreenBrowserAgent* agent) {}
};

#endif  // IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_BROWSER_AGENT_OBSERVER_H_
