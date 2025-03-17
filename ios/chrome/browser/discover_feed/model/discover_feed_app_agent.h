// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_APP_AGENT_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_APP_AGENT_H_

#import "ios/chrome/app/application_delegate/observing_app_state_agent.h"

@protocol DiscoverFeedProfileHelper;

// This agent manages refreshing the Discover feed when Chrome enters the
// background.
@interface DiscoverFeedAppAgent : SceneObservingAppAgent

// Adds a per-Profile helper.
- (void)addHelper:(id<DiscoverFeedProfileHelper>)helper;

// Removes a per-Profile helper.
- (void)removeHelper:(id<DiscoverFeedProfileHelper>)helper;

@end

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_APP_AGENT_H_
