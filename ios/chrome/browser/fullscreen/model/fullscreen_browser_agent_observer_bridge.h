// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_BROWSER_AGENT_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_BROWSER_AGENT_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/scoped_observation.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent_observer.h"

// Objective-C protocol for observing FullscreenBrowserAgent events.
@protocol FullscreenBrowserAgentObserving <NSObject>

@optional

// Called before the fullscreen state updates.
- (void)fullscreenWillUpdateState:(FullscreenBrowserAgent*)agent;

// Called after the fullscreen state updates.
- (void)fullscreenDidUpdateState:(FullscreenBrowserAgent*)agent;

// Called once per display refresh while an animated transition is running,
// plus a final time with the exact target value. Read the value from
// `agent->interpolated_progress()`.
//
// Implementing this method is itself the opt-in: observers that do not
// implement it are never called, and no display link is scheduled unless at
// least one observer does. Only implement it for UI that is a *non-linear*
// function of progress; affine UI is already interpolated correctly by the
// concurrent UIKit animation, for free and off the main thread.
//
// See FullscreenBrowserAgentObserver::DidUpdateInterpolatedProgress() for the
// constraint on obscured insets.
- (void)fullscreenDidUpdateInterpolatedProgress:(FullscreenBrowserAgent*)agent;

// Called before the obscured inset range updates.
- (void)fullscreenWillUpdateObscuredInsetRange:(FullscreenBrowserAgent*)agent;

// Called after the obscured inset range updates.
- (void)fullscreenDidUpdateObscuredInsetRange:(FullscreenBrowserAgent*)agent;

// Called when the fullscreen transition completes.
- (void)fullscreen:(FullscreenBrowserAgent*)agent
     didTransition:(FullscreenTransition)transition;

// Called when the FullscreenBrowserAgent is shutting down.
- (void)fullscreenWillShutDown:(FullscreenBrowserAgent*)agent;

@end

// Bridge class that listens for `FullscreenBrowserAgent` notifications and
// passes them to its Objective-C delegate.
class FullscreenBrowserAgentObserverBridge
    : public FullscreenBrowserAgentObserver {
 public:
  FullscreenBrowserAgentObserverBridge(
      id<FullscreenBrowserAgentObserving> observer,
      FullscreenBrowserAgent* agent);
  ~FullscreenBrowserAgentObserverBridge() override;

 private:
  // FullscreenBrowserAgentObserver:
  void WillUpdateState(FullscreenBrowserAgent* agent) override;
  void DidUpdateState(FullscreenBrowserAgent* agent) override;
  void DidUpdateInterpolatedProgress(FullscreenBrowserAgent* agent) override;
  void WillUpdateObscuredInsetRange(FullscreenBrowserAgent* agent) override;
  void DidUpdateObscuredInsetRange(FullscreenBrowserAgent* agent) override;
  void FullscreenDidTransition(FullscreenBrowserAgent* agent,
                               FullscreenTransition transition) override;
  void WillShutDown(FullscreenBrowserAgent* agent) override;

  __weak id<FullscreenBrowserAgentObserving> observer_;

  // Whether `observer_` implements the optional per-frame selector. Resolved
  // once at construction rather than per frame, since the callback fires on
  // every display refresh and an object's method table cannot change for the
  // bridge's lifetime.
  const bool wants_interpolated_progress_;

  base::ScopedObservation<FullscreenBrowserAgent,
                          FullscreenBrowserAgentObserver>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_BROWSER_AGENT_OBSERVER_BRIDGE_H_
