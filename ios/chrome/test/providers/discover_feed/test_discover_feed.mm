// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_api.h"

#import "ios/chrome/browser/discover_feed/model/discover_feed_visibility_provider_configuration.h"
#import "ios/chrome/test/fakes/fake_discover_feed_eligibility_handler.h"
#import "ios/chrome/test/providers/discover_feed/test_discover_feed_service.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_visibility_provider.h"

namespace ios {
namespace provider {

std::unique_ptr<DiscoverFeedService> CreateDiscoverFeedService(
    DiscoverFeedConfiguration* configuration) {
  return std::make_unique<TestDiscoverFeedService>();
}

id<DiscoverFeedVisibilityProvider> CreateDiscoverFeedVisibilityProvider(
    DiscoverFeedVisibilityProviderConfiguration* configuration) {
  DiscoverFeedService* discover_feed_service =
      configuration.discoverFeedService;
  if (!discover_feed_service) {
    return nil;
  }
  TestDiscoverFeedService* test_discover_feed_service =
      static_cast<TestDiscoverFeedService*>(discover_feed_service);
  FakeDiscoverFeedEligibilityHandler* handler =
      [[FakeDiscoverFeedEligibilityHandler alloc] init];
  test_discover_feed_service->set_eligibility_handler(handler);
  return handler;
}

}  // namespace provider
}  // namespace ios
