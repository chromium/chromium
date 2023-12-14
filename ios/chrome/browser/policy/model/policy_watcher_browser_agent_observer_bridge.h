// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_POLICY_WATCHER_BROWSER_AGENT_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_POLICY_WATCHER_BROWSER_AGENT_OBSERVER_BRIDGE_H_

#import "ios/chrome/browser/policy/model/policy_watcher_browser_agent_observer.h"

// Protocol that corresponds to the PolicyWatcherBrowserAgentObserver API.
@protocol PolicyWatcherBrowserAgentObserving

// Invoked when the policy has changed and it is no longer possible to SignIn.
- (void)policyWatcherBrowserAgentNotifySignInDisabled:
    (PolicyWatcherBrowserAgent*)policyWatcher;

@end

// Observer that bridges PolicyWatcherBrowserAgentObserver events to an
// Objective-C observer.
class PolicyWatcherBrowserAgentObserverBridge
    : public PolicyWatcherBrowserAgentObserver {
 public:
  explicit PolicyWatcherBrowserAgentObserverBridge(
      id<PolicyWatcherBrowserAgentObserving> observer);
  ~PolicyWatcherBrowserAgentObserverBridge() override = default;

  // PolicyWatcherBrowserAgentObserver implementation.
  void OnSignInDisallowed(PolicyWatcherBrowserAgent* policy_watcher) final;

 private:
  __weak id<PolicyWatcherBrowserAgentObserving> observer_ = nil;
};

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_POLICY_WATCHER_BROWSER_AGENT_OBSERVER_BRIDGE_H_
