// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_VIDEO_DEFAULT_BROWSER_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_VIDEO_DEFAULT_BROWSER_PROMO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

// View controller for the Full Screen Default Browser Video promo.
@interface VideoDefaultBrowserPromoViewController : UIViewController

// The action handler for interactions in this view controller.
@property(nonatomic, weak) id<ConfirmationAlertActionHandler> actionHandler;

// Whether or not to show the Remind Me Later button.
@property(nonatomic, assign) BOOL showRemindMeLater;

@end

#endif  // IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_VIDEO_DEFAULT_BROWSER_PROMO_VIEW_CONTROLLER_H_
