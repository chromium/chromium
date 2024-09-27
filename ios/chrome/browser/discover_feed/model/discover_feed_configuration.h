// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_CONFIGURATION_H_

#import <Foundation/Foundation.h>

#import "ios/public/provider/chrome/browser/signin/signin_sso_api.h"

class AuthenticationService;
@class FeedMetricsRecorder;
class PrefService;
class TemplateURLService;

namespace signin {
class IdentityManager;
}

namespace syncer {
class SyncService;
}

// Configuration object used by the DiscoverFeedService.
@interface DiscoverFeedConfiguration : NSObject

// AuthenticationService used by DiscoverFeedService.
@property(nonatomic, assign) AuthenticationService* authService;

// The per Profile PrefService.
@property(nonatomic, assign) PrefService* profilePrefService;

// The global PrefService.
@property(nonatomic, assign) PrefService* localStatePrefService;

// IdentityManager used by DiscoverFeedService.
@property(nonatomic, assign) signin::IdentityManager* identityManager;

// SingleSignOnService used by DiscoverFeedService.
@property(nonatomic, strong) id<SingleSignOnService> singleSignOnService;

// Feed metrics recorder used by DiscoverFeedService.
@property(nonatomic, strong) FeedMetricsRecorder* metricsRecorder;

// The service to get the default search engine.
@property(nonatomic, assign) TemplateURLService* templateURLService;

// The service exposing sync state.
@property(nonatomic, assign) syncer::SyncService* syncService;

@end

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_CONFIGURATION_H_
