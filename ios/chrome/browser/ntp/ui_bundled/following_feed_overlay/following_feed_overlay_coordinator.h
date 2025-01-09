// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FOLLOWING_FEED_OVERLAY_FOLLOWING_FEED_OVERLAY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FOLLOWING_FEED_OVERLAY_FOLLOWING_FEED_OVERLAY_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol FeedControlDelegate;

/// Coordinator that manages the overlay modal containing the following feed.
@interface FollowingFeedOverlayCoordinator : ChromeCoordinator

/// Delegate for controlling the presented feed.
@property(nonatomic, weak) id<FeedControlDelegate> feedControlDelegate;

/// Whether the following feed overlay should animate its presentation. Must be
/// set before starting the coordinator.
@property(nonatomic, assign) BOOL animatePresentation;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FOLLOWING_FEED_OVERLAY_FOLLOWING_FEED_OVERLAY_COORDINATOR_H_
