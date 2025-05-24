// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_VISIBILITY_PROVIDER_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_VISIBILITY_PROVIDER_CONFIGURATION_H_

#import <Foundation/Foundation.h>

class DiscoverFeedService;
class PrefService;
class TemplateURLService;

namespace regional_capabilities {
class RegionalCapabilitiesService;
}

/// Configuration object for the Discover visibility provider.
@interface DiscoverFeedVisibilityProviderConfiguration : NSObject

/// The service to get the discover feed.
@property(nonatomic, assign) DiscoverFeedService* discoverFeedService;
/// The per Profile PrefService.
@property(nonatomic, assign) PrefService* prefService;
/// The service to get the default search engine.
@property(nonatomic, assign) TemplateURLService* templateURLService;
/// The service to check location.
@property(nonatomic, assign) regional_capabilities::RegionalCapabilitiesService*
    regionalCapabilitiesService;
/// `YES` if the app is resumed from safe mode.
@property(nonatomic, assign) BOOL resumedFromSafeMode;

@end

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_VISIBILITY_PROVIDER_CONFIGURATION_H_
