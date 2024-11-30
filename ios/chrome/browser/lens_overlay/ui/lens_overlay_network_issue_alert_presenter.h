// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_NETWORK_ISSUE_ALERT_PRESENTER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_NETWORK_ISSUE_ALERT_PRESENTER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_error_handler.h"

@protocol LensOverlayNetworkIssueDelegate;

// Utility class for presenting the connectivity alert in Lens Overlay.
@interface LensOverlayNetworkIssueAlertPresenter
    : NSObject <LensOverlayErrorHandler>

- (instancetype)initWithBaseViewController:
    (UIViewController*)baseViewController;

// Delegate for events of this presenter.
@property(nonatomic, weak) id<LensOverlayNetworkIssueDelegate> delegate;

@end

// Delegate for events related to issues in connectivity.
@protocol LensOverlayNetworkIssueDelegate <NSObject>

/// Called immediately before the alert is shown.
- (void)onNetworkIssueAlertWillShow;

/// The user acknowledged the connectivity issue.
- (void)onNetworkIssueAlertAcknowledged;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_NETWORK_ISSUE_ALERT_PRESENTER_H_
