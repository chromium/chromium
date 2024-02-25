// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/model/follow_browser_agent_observer_bridge.h"

#import "ios/chrome/browser/follow/model/follow_browser_agent_observing.h"
#import "ios/chrome/browser/follow/model/followed_web_site.h"

FollowBrowserAgentObserverBridge::FollowBrowserAgentObserverBridge(
    id<FollowBrowserAgentObserving> observing,
    FollowBrowserAgent* browser_agent)
    : observing_(observing) {
  observation_.Observe(browser_agent);
}

FollowBrowserAgentObserverBridge::~FollowBrowserAgentObserverBridge() = default;

void FollowBrowserAgentObserverBridge::OnWebSiteFollowed(
    FollowedWebSite* web_site) {
  [observing_ followedWebSite:web_site];
}

void FollowBrowserAgentObserverBridge::OnWebSiteUnfollowed(
    FollowedWebSite* web_site) {
  [observing_ unfollowedWebSite:web_site];
}

void FollowBrowserAgentObserverBridge::OnFollowedWebSitesLoaded() {
  [observing_ followedWebSitesLoaded];
}
