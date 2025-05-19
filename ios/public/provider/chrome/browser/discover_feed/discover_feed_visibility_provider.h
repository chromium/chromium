// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_VISIBILITY_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_VISIBILITY_PROVIDER_H_

enum class DiscoverFeedEligibility;
@protocol DiscoverFeedVisibilityObserver;

/// Provider protocol that manages the visibility of the Discover feed.
@protocol DiscoverFeedVisibilityProvider <NSObject>

/// `YES` if the user has turned the Discover feed on. Usually can be toggled
/// through the UI.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

/// `YES` if the user is eligible to view the feed. If `NO`, the Discover feed
/// should not be available to the user, regardless of their preference.
- (DiscoverFeedEligibility)eligibility;

/// Adding and removing Discover feed visibility observer from the provider.
- (void)addObserver:(id<DiscoverFeedVisibilityObserver>)observer;
- (void)removeObserver:(id<DiscoverFeedVisibilityObserver>)observer;

/// Shutdown. Should be called before the instance is deallocated.
- (void)shutdown;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISCOVER_FEED_DISCOVER_FEED_VISIBILITY_PROVIDER_H_
