// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_SHARE_DOWNLOAD_OVERLAY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SHARING_SHARE_DOWNLOAD_OVERLAY_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator to present download overlay.
@interface ShareDownloadOverlayCoordinator : ChromeCoordinator

// Initializes a coordinator for displaying an overlay on the current displayed
// `webView` which must not be null.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                   webView:(UIView*)webView
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_BROWSER_UI_SHARING_SHARE_DOWNLOAD_OVERLAY_COORDINATOR_H_
