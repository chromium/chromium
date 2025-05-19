// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_FAKES_FAKE_DISCOVER_FEED_ELIGIBILITY_HANDLER_H_
#define IOS_CHROME_TEST_FAKES_FAKE_DISCOVER_FEED_ELIGIBILITY_HANDLER_H_

#import <Foundation/Foundation.h>

#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_visibility_provider.h"

enum class DiscoverFeedEligibility;

/// Fake eligibility handler for Discover feed for testing use.
@interface FakeDiscoverFeedEligibilityHandler
    : NSObject <DiscoverFeedVisibilityProvider>

/// Sets eligibilty; should be called by the test case to verify eligibility
/// change behavior.
- (void)setEligibility:(DiscoverFeedEligibility)eligibility;

/// Number of observers registered.
- (int)observerCount;

/// Returns `YES` if `-shutdown` is invoked.
- (BOOL)isShutdown;

@end

#endif  // IOS_CHROME_TEST_FAKES_FAKE_DISCOVER_FEED_ELIGIBILITY_HANDLER_H_
