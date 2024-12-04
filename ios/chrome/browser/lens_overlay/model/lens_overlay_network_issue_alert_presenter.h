// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_NETWORK_ISSUE_ALERT_PRESENTER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_NETWORK_ISSUE_ALERT_PRESENTER_H_

#import <UIKit/UIKit.h>

@protocol LensOverlayNetworkIssueDelegate;

// Utility class for presenting the connectivity alert in Lens Overlay.
@interface LensOverlayNetworkIssueAlertPresenter : NSObject

- (instancetype)initWithBaseViewController:
    (UIViewController*)baseViewController;

// Delegate for events of this presenter.
@property(nonatomic, weak) id<LensOverlayNetworkIssueDelegate> delegate;

// Presents the network issue alert over the given view controller.
- (void)showAlert;

@end

// Delegate for events related to issues in connectivity.
@protocol LensOverlayNetworkIssueDelegate <NSObject>

// The user acknowledged the connectivity issue.
- (void)onNetworkIssueAlertAcknowledged;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_NETWORK_ISSUE_ALERT_PRESENTER_H_
