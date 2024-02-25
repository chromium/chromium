// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOCKING_PROMO_UI_DOCKING_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_DOCKING_PROMO_UI_DOCKING_PROMO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/docking_promo/ui/docking_promo_consumer.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

// Container view controller for the Docking Promo.
@interface DockingPromoViewController : UIViewController <DockingPromoConsumer>

// The action handler for interactions in this view controller.
@property(nonatomic, weak) id<ConfirmationAlertActionHandler> actionHandler;

@end

#endif  // IOS_CHROME_BROWSER_DOCKING_PROMO_UI_DOCKING_PROMO_VIEW_CONTROLLER_H_
