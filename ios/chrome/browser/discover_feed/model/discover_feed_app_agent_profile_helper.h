// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_APP_AGENT_PROFILE_HELPER_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_APP_AGENT_PROFILE_HELPER_H_

#import <Foundation/Foundation.h>

@class ProfileState;

// Helper object handling the per-Profile operations for DiscoverFeedAppAgent.
@interface DiscoverFeedAppAgentProfileHelper : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithProfileState:(ProfileState*)profileState
    NS_DESIGNATED_INITIALIZER;

// Invoked when the DiscoverFeedAppAgent wants the feed for the Profile
// to be refreshed (usually when the application enter background).
- (void)refreshFeedInBackground;

// Invokeb when the ProfileState is about to be disconnected.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_APP_AGENT_PROFILE_HELPER_H_
