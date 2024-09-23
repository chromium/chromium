// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POSITION_BROWSER_AGENT_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POSITION_BROWSER_AGENT_OBSERVER_BRIDGE_H_

#include "base/scoped_observation.h"
#include "ios/chrome/browser/omnibox/model/omnibox_position_browser_agent.h"

@protocol OmniboxPositionBrowserAgentObserving;

// Bridge for observing OmniboxPositionBrowserAgent in Objective-C.
class OmniboxPositionBrowserAgentObserverBridge final
    : public OmniboxPositionBrowserAgentObserver {
 public:
  OmniboxPositionBrowserAgentObserverBridge(
      id<OmniboxPositionBrowserAgentObserving> observing,
      OmniboxPositionBrowserAgent* browser_agent);
  ~OmniboxPositionBrowserAgentObserverBridge() final;

  // OmniboxPositionBrowserAgentObserver implementation.
  void OmniboxPositionBrowserAgentHasNewBottomLayout(
      OmniboxPositionBrowserAgent* browser_agent,
      bool is_current_layout_bottom_omnibox) override;

 private:
  __weak id<OmniboxPositionBrowserAgentObserving> observing_;
  base::ScopedObservation<OmniboxPositionBrowserAgent,
                          OmniboxPositionBrowserAgentObserver>
      observation_{this};
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POSITION_BROWSER_AGENT_OBSERVER_BRIDGE_H_
