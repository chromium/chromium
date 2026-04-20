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

// Called before the obscured inset range updates.
- (void)fullscreenWillUpdateObscuredInsetRange:(FullscreenBrowserAgent*)agent;

// Called after the obscured inset range updates.
- (void)fullscreenDidUpdateObscuredInsetRange:(FullscreenBrowserAgent*)agent;

// Called when the fullscreen transition completes.
- (void)fullscreen:(FullscreenBrowserAgent*)agent
     didTransition:(FullscreenTransition)transition;

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
  void WillUpdateObscuredInsetRange(FullscreenBrowserAgent* agent) override;
  void DidUpdateObscuredInsetRange(FullscreenBrowserAgent* agent) override;
  void FullscreenDidTransition(FullscreenBrowserAgent* agent,
                               FullscreenTransition transition) override;

  __weak id<FullscreenBrowserAgentObserving> observer_;
  base::ScopedObservation<FullscreenBrowserAgent,
                          FullscreenBrowserAgentObserver>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_BROWSER_AGENT_OBSERVER_BRIDGE_H_
