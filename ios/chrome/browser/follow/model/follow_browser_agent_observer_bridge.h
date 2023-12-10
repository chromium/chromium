// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_BROWSER_AGENT_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_BROWSER_AGENT_OBSERVER_BRIDGE_H_

#include "base/scoped_observation.h"
#include "ios/chrome/browser/follow/model/follow_browser_agent.h"

@protocol FollowBrowserAgentObserving;

// Bridge for observing FollowBrowserAgent in Objective-C.
class FollowBrowserAgentObserverBridge final
    : public FollowBrowserAgent::Observer {
 public:
  FollowBrowserAgentObserverBridge(id<FollowBrowserAgentObserving> observing,
                                   FollowBrowserAgent* browser_agent);
  ~FollowBrowserAgentObserverBridge() final;

  // FollowServiceObserver implementation.
  void OnWebSiteFollowed(FollowedWebSite* web_site) final;
  void OnWebSiteUnfollowed(FollowedWebSite* web_site) final;
  void OnFollowedWebSitesLoaded() final;

 private:
  __weak id<FollowBrowserAgentObserving> observing_;
  base::ScopedObservation<FollowBrowserAgent, FollowBrowserAgent::Observer>
      observation_{this};
};

#endif  // IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_BROWSER_AGENT_OBSERVER_BRIDGE_H_
