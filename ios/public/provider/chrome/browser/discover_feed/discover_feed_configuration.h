// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_CONFIGURATION_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_CONFIGURATION_H_

#import <Foundation/Foundation.h>

class AuthenticationService;
class ChromeBrowserState;
@class DiscoverFeedMetricsRecorder;
class PrefService;

// Configuration object used by the DiscoverFeedProvider.
@interface DiscoverFeedConfiguration : NSObject

// BrowserState used by DiscoverFeedProvider. DEPRECATED.
@property(nonatomic, assign) ChromeBrowserState* browserState;

// AuthenticationService used by DiscoverFeedProvider.
@property(nonatomic, assign) AuthenticationService* authService;

// PrefService used by DiscoverFeedProvider.
@property(nonatomic, assign) PrefService* prefService;

// DiscoverFeed metrics recorder used by DiscoverFeedProvider.
@property(nonatomic, strong) DiscoverFeedMetricsRecorder* metricsRecorder;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_CONFIGURATION_H_
