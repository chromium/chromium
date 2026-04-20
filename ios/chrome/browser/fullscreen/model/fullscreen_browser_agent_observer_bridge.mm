// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent_observer_bridge.h"

FullscreenBrowserAgentObserverBridge::FullscreenBrowserAgentObserverBridge(
    id<FullscreenBrowserAgentObserving> observer,
    FullscreenBrowserAgent* agent)
    : observer_(observer) {
  if (agent) {
    scoped_observation_.Observe(agent);
  }
}

FullscreenBrowserAgentObserverBridge::~FullscreenBrowserAgentObserverBridge() =
    default;

void FullscreenBrowserAgentObserverBridge::WillUpdateState(
    FullscreenBrowserAgent* agent) {
  if ([observer_ respondsToSelector:@selector(fullscreenWillUpdateState:)]) {
    [observer_ fullscreenWillUpdateState:agent];
  }
}

void FullscreenBrowserAgentObserverBridge::DidUpdateState(
    FullscreenBrowserAgent* agent) {
  if ([observer_ respondsToSelector:@selector(fullscreenDidUpdateState:)]) {
    [observer_ fullscreenDidUpdateState:agent];
  }
}

void FullscreenBrowserAgentObserverBridge::WillUpdateObscuredInsetRange(
    FullscreenBrowserAgent* agent) {
  if ([observer_ respondsToSelector:@selector
                 (fullscreenWillUpdateObscuredInsetRange:)]) {
    [observer_ fullscreenWillUpdateObscuredInsetRange:agent];
  }
}

void FullscreenBrowserAgentObserverBridge::DidUpdateObscuredInsetRange(
    FullscreenBrowserAgent* agent) {
  if ([observer_ respondsToSelector:@selector
                 (fullscreenDidUpdateObscuredInsetRange:)]) {
    [observer_ fullscreenDidUpdateObscuredInsetRange:agent];
  }
}

void FullscreenBrowserAgentObserverBridge::FullscreenDidTransition(
    FullscreenBrowserAgent* agent,
    FullscreenTransition transition) {
  if ([observer_ respondsToSelector:@selector(fullscreen:didTransition:)]) {
    [observer_ fullscreen:agent didTransition:transition];
  }
}
