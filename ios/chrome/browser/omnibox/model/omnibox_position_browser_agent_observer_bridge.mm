// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_position_browser_agent_observer_bridge.h"

#import "ios/chrome/browser/omnibox/model/omnibox_position_browser_agent.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position_browser_agent_observing.h"

OmniboxPositionBrowserAgentObserverBridge::
    OmniboxPositionBrowserAgentObserverBridge(
        id<OmniboxPositionBrowserAgentObserving> observing,
        OmniboxPositionBrowserAgent* browser_agent)
    : observing_(observing) {
  observation_.Observe(browser_agent);
}

OmniboxPositionBrowserAgentObserverBridge::
    ~OmniboxPositionBrowserAgentObserverBridge() = default;

void OmniboxPositionBrowserAgentObserverBridge::
    OmniboxPositionBrowserAgentHasNewBottomLayout(
        OmniboxPositionBrowserAgent* browser_agent,
        bool is_current_layout_bottom_omnibox) {
  [observing_ omniboxPositionBrowserAgent:browser_agent
             isCurrentLayoutBottomOmnibox:is_current_layout_bottom_omnibox];
}
