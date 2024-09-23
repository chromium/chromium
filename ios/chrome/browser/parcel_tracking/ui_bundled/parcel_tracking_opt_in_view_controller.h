// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PARCEL_TRACKING_UI_BUNDLED_PARCEL_TRACKING_OPT_IN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PARCEL_TRACKING_UI_BUNDLED_PARCEL_TRACKING_OPT_IN_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

@protocol ParcelTrackingOptInViewControllerDelegate;

//  View controller for the parcel tracking opt-in prompt.
@interface ParcelTrackingOptInViewController
    : ConfirmationAlertViewController <ConfirmationAlertActionHandler>

// The delegate for interactions in this View Controller.
@property(nonatomic, weak) id<ParcelTrackingOptInViewControllerDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_PARCEL_TRACKING_UI_BUNDLED_PARCEL_TRACKING_OPT_IN_VIEW_CONTROLLER_H_
