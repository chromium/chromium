// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/policy_watcher_browser_agent_observer_bridge.h"

PolicyWatcherBrowserAgentObserverBridge::
    PolicyWatcherBrowserAgentObserverBridge(
        id<PolicyWatcherBrowserAgentObserving> observer)
    : observer_(observer) {}

void PolicyWatcherBrowserAgentObserverBridge::OnSignInDisallowed(
    PolicyWatcherBrowserAgent* policy_watcher) {
  [observer_ policyWatcherBrowserAgentNotifySignInDisabled:policy_watcher];
}
