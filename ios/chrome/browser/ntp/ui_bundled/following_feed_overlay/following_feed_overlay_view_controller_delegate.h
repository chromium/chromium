// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FOLLOWING_FEED_OVERLAY_FOLLOWING_FEED_OVERLAY_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FOLLOWING_FEED_OVERLAY_FOLLOWING_FEED_OVERLAY_VIEW_CONTROLLER_DELEGATE_H_

@class FollowingFeedOverlayViewController;

/// Protocol to handle user interactions in a
/// FollowingFeedOverlayViewController.
@protocol FollowingFeedOverlayViewControllerDelegate

/// Delegate method that handles the dismissal of the following feed overlay
/// view.
- (void)followingFeedOverlayViewControllerDidDismiss:
    (FollowingFeedOverlayViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FOLLOWING_FEED_OVERLAY_FOLLOWING_FEED_OVERLAY_VIEW_CONTROLLER_DELEGATE_H_
