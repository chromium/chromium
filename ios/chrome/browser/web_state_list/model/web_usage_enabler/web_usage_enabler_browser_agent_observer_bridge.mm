// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent_observer_bridge.h"

WebUsageEnablerBrowserAgentObserverBridge::
    WebUsageEnablerBrowserAgentObserverBridge(
        WebUsageEnablerBrowserAgent* web_usage_enabler,
        id<WebUsageEnablerBrowserAgentObserving> observer)
    : observer_(observer) {
  scoped_observation_.Observe(web_usage_enabler);
}

WebUsageEnablerBrowserAgentObserverBridge::
    ~WebUsageEnablerBrowserAgentObserverBridge() {}

void WebUsageEnablerBrowserAgentObserverBridge::WebUsageEnablerValueChanged(
    WebUsageEnablerBrowserAgent* web_usage_enabler) {
  CHECK(scoped_observation_.IsObservingSource(web_usage_enabler));

  [observer_ webUsageEnablerValueChanged:web_usage_enabler];
}
