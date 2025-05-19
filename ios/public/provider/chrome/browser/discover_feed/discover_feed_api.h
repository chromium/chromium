// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_API_H_

#import <memory>

#import "ios/chrome/browser/discover_feed/model/discover_feed_configuration.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_visibility_provider_configuration.h"

@protocol DiscoverFeedVisibilityProvider;

namespace ios {
namespace provider {

// Creates a new instance of DiscoverFeedService.
std::unique_ptr<DiscoverFeedService> CreateDiscoverFeedService(
    DiscoverFeedConfiguration* configuration);

// Creates a new instance of DiscoverFeedVisibilityProvider.
id<DiscoverFeedVisibilityProvider> CreateDiscoverFeedVisibilityProvider(
    DiscoverFeedVisibilityProviderConfiguration* configuration);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_API_H_
