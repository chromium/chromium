// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_discover_feed_eligibility_handler.h"

#import "ios/chrome/browser/discover_feed/model/discover_feed_visibility_observer.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_visibility_provider_configuration.h"
#import "ios/chrome/browser/discover_feed/model/feed_constants.h"

@implementation FakeDiscoverFeedEligibilityHandler {
  DiscoverFeedEligibility _eligibility;
  BOOL _shutdown;
  int _observerCount;
}

/// Property is declared in a protocol and not autosynthesized.
@synthesize enabled = _enabled;

- (instancetype)init {
  self = [super init];
  if (self) {
    _enabled = YES;
    _eligibility = DiscoverFeedEligibility::kEligible;
    _observerCount = 0;
  }
  return self;
}

- (void)setEligibility:(DiscoverFeedEligibility)eligibility {
  _eligibility = eligibility;
}

- (int)observerCount {
  return _observerCount;
}

- (BOOL)isShutdown {
  return _shutdown;
}

- (void)setEnabled:(BOOL)enabled {
  _enabled = enabled;
}

#pragma mark - DiscoverFeedVisibilityProvider

- (DiscoverFeedEligibility)eligibility {
  return _eligibility;
}

- (void)addObserver:(id<DiscoverFeedVisibilityObserver>)observer {
  if (observer) {
    _observerCount++;
  }
}

- (void)removeObserver:(id<DiscoverFeedVisibilityObserver>)observer {
  if (observer) {
    _observerCount--;
  }
}

- (void)shutdown {
  _shutdown = YES;
}

@end
