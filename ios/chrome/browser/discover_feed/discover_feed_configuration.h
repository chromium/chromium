// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_CONFIGURATION_H_

#import <Foundation/Foundation.h>

#import "ios/public/provider/chrome/browser/signin/signin_sso_api.h"

class AuthenticationService;
@class FeedMetricsRecorder;
class PrefService;
class TemplateURLService;

namespace signin {
class IdentityManager;
}

// Configuration object used by the DiscoverFeedService.
// TODO(crbug.com/1277504): Rename this to FeedConfiguration.
@interface DiscoverFeedConfiguration : NSObject

// AuthenticationService used by DiscoverFeedService.
@property(nonatomic, assign) AuthenticationService* authService;

// PrefService used by DiscoverFeedService.
@property(nonatomic, assign) PrefService* prefService;

// IdentityManager used by DiscoverFeedService.
@property(nonatomic, assign) signin::IdentityManager* identityManager;

// SingleSignOnService used by DiscoverFeedService.
@property(nonatomic, strong) id<SingleSignOnService> ssoService;

// Feed metrics recorder used by DiscoverFeedService.
@property(nonatomic, strong) FeedMetricsRecorder* metricsRecorder;

// The service to get the default search engine.
@property(nonatomic, assign) TemplateURLService* templateURLService;

@end

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_CONFIGURATION_H_
