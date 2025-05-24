// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_VISIBILITY_OBSERVER_H_
#define IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_VISIBILITY_OBSERVER_H_

@class DiscoverFeedVisibilityProvider;

/// Protocol declaration for objects that need to know about Discover feed
/// visibility changes.
@protocol DiscoverFeedVisibilityObserver

@optional

/// Notifies observer that the feed eligibility has changed. Observer should
/// display or hide the feed preference toggles accordingly.
- (void)didChangeDiscoverFeedEligibility;

/// Notifies observer that the feed visibility should be updated.
- (void)didChangeDiscoverFeedVisibility;

@end

#endif  // IOS_CHROME_BROWSER_DISCOVER_FEED_MODEL_DISCOVER_FEED_VISIBILITY_OBSERVER_H_
