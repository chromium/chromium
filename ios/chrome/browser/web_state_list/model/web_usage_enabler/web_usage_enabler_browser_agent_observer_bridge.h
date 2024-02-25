// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_WEB_USAGE_ENABLER_WEB_USAGE_ENABLER_BROWSER_AGENT_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_WEB_USAGE_ENABLER_WEB_USAGE_ENABLER_BROWSER_AGENT_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/scoped_observation.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent_observer.h"

// Protocol that corresponds to the WebUsageEnablerBrowserAgentObserver API.
// Allows registering Objective-C objects to listen to
// WebUsageEnablerBrowserAgent events.
@protocol WebUsageEnablerBrowserAgentObserving
- (void)webUsageEnablerValueChanged:
    (WebUsageEnablerBrowserAgent*)webUsageEnabler;
@end

// Observer that bridges WebUsageEnablerBrowserAgent events to an Objective-C
// observer that implements the WebUsageEnablerBrowserAgentObserver protocol
// (the observer is *not* owned).
class WebUsageEnablerBrowserAgentObserverBridge final
    : public WebUsageEnablerBrowserAgentObserver {
 public:
  // Creates a bridge which observes `WebUsageEnablerBrowserAgent`, forwarding
  // events to `observer`.
  WebUsageEnablerBrowserAgentObserverBridge(
      WebUsageEnablerBrowserAgent* WebUsageEnablerBrowserAgent,
      id<WebUsageEnablerBrowserAgentObserving> observer);
  ~WebUsageEnablerBrowserAgentObserverBridge() final;

  // Not copyable or moveable.
  WebUsageEnablerBrowserAgentObserverBridge(
      const WebUsageEnablerBrowserAgentObserverBridge&) = delete;
  WebUsageEnablerBrowserAgentObserverBridge& operator=(
      const WebUsageEnablerBrowserAgentObserverBridge&) = delete;

  // WebUsageEnablerBrowserAgentObserver.
  void WebUsageEnablerValueChanged(
      WebUsageEnablerBrowserAgent* web_usage_enabler) override;

 private:
  __weak id<WebUsageEnablerBrowserAgentObserving> observer_ = nil;
  base::ScopedObservation<WebUsageEnablerBrowserAgent,
                          WebUsageEnablerBrowserAgentObserver>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_WEB_USAGE_ENABLER_WEB_USAGE_ENABLER_BROWSER_AGENT_OBSERVER_BRIDGE_H_
