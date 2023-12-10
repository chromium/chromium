// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_BROWSER_AGENT_OBSERVING_H_
#define IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_BROWSER_AGENT_OBSERVING_H_

#import <Foundation/Foundation.h>

@class FollowedWebSite;

// Objective-C protocol for observing FollowBrowserAgent.
@protocol FollowBrowserAgentObserving <NSObject>

// Invoked when `webSite` is followed.
- (void)followedWebSite:(FollowedWebSite*)webSite;

// Invoked when `webSite` is unfollowed.
- (void)unfollowedWebSite:(FollowedWebSite*)webSite;

// Invoked when followed websites are loaded.
- (void)followedWebSitesLoaded;

@end

#endif  // IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_BROWSER_AGENT_OBSERVING_H_
