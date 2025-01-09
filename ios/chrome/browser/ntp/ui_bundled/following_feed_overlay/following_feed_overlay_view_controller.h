// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FOLLOWING_FEED_OVERLAY_FOLLOWING_FEED_OVERLAY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FOLLOWING_FEED_OVERLAY_FOLLOWING_FEED_OVERLAY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol FollowingFeedOverlayViewControllerDelegate;

/// Container view controller that hosts the following feed. Will be displayed
/// as an overlay modal.
@interface FollowingFeedOverlayViewController : UIViewController

/// Delegate object that handles view events of the following feed overlay.
@property(nonatomic, weak) id<FollowingFeedOverlayViewControllerDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_FOLLOWING_FEED_OVERLAY_FOLLOWING_FEED_OVERLAY_VIEW_CONTROLLER_H_
