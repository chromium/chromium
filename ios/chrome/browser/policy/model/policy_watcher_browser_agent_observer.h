// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_POLICY_WATCHER_BROWSER_AGENT_OBSERVER_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_POLICY_WATCHER_BROWSER_AGENT_OBSERVER_H_

#import <Foundation/Foundation.h>

#include "base/observer_list_types.h"

class PolicyWatcherBrowserAgent;

// Interface for listening to events occurring to PolicyWatcherBrowserAgent.
class PolicyWatcherBrowserAgentObserver : public base::CheckedObserver {
 public:
  // Notify the observer that SignIn is no longer allowed.
  virtual void OnSignInDisallowed(PolicyWatcherBrowserAgent* policy_watcher) {}

 protected:
  PolicyWatcherBrowserAgentObserver() = default;
};

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_POLICY_WATCHER_BROWSER_AGENT_OBSERVER_H_
