// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_NETWORK_ISSUE_ALERT_PRESENTER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_NETWORK_ISSUE_ALERT_PRESENTER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_error_handler.h"

@protocol LensOverlayNetworkIssuePresenterDelegate;

// Utility class for presenting the connectivity alert in Lens Overlay.
@interface LensOverlayNetworkIssuePresenter : NSObject <LensOverlayErrorHandler>

- (instancetype)initWithBaseViewController:
    (UIViewController*)baseViewController;

- (instancetype)init NS_UNAVAILABLE;

// Delegate for events of this presenter.
@property(nonatomic, weak) id<LensOverlayNetworkIssuePresenterDelegate>
    delegate;

@end

// Delegate for events related to issues in connectivity.
// A set of methods to react to events related to issues in connectivity.
@protocol LensOverlayNetworkIssuePresenterDelegate <NSObject>

/// Informs the delegate that an alert will be shown.
- (void)lensOverlayNetworkIssuePresenterWillShowAlert:
    (LensOverlayNetworkIssuePresenter*)presenter;

/// The user acknowledged the connectivity issue.
- (void)lensOverlayNetworkIssuePresenterDidAcknowledgeAlert:
    (LensOverlayNetworkIssuePresenter*)presenter;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_NETWORK_ISSUE_ALERT_PRESENTER_H_
