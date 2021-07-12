// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_DISCOVER_FEED_PREVIEW_DISCOVER_FEED_PREVIEW_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_NTP_DISCOVER_FEED_PREVIEW_DISCOVER_FEED_PREVIEW_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

class GURL;

// Coordinator for the discover feed preview. Provides the preview view
// controller and handles its actions.
@interface DiscoverFeedPreviewCoordinator : ChromeCoordinator

// Inits the coordinator with |browser| and the |URL| of the feed article.
- (instancetype)initWithBrowser:(Browser*)browser
                            URL:(const GURL)URL NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Returns the viewController for the discover feed preview. It displays a
// loaded webState UIView.
- (UIViewController*)discoverFeedPreviewViewController;

// Handles the tap action of the feed preview. Called when the user taps on
// the discover feed preview on the context menu.
- (void)handlePreviewAction;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_DISCOVER_FEED_PREVIEW_DISCOVER_FEED_PREVIEW_COORDINATOR_H_
