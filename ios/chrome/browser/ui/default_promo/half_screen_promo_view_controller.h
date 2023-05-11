// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_HALF_SCREEN_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_HALF_SCREEN_PROMO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

// View controller for the Half Screen Default Browser Video promo.
@interface HalfScreenPromoViewController : UIViewController

// The action handler for interactions in this view controller.
@property(nonatomic, weak) id<ConfirmationAlertActionHandler> actionHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_HALF_SCREEN_PROMO_VIEW_CONTROLLER_H_
