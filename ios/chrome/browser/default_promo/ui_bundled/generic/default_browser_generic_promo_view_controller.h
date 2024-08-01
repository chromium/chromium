// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_GENERIC_DEFAULT_BROWSER_GENERIC_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_GENERIC_DEFAULT_BROWSER_GENERIC_PROMO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/default_promo/ui_bundled/generic/default_browser_generic_promo_consumer.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

// View controller for the Full Screen Default Browser generic promo.
@interface DefaultBrowserGenericPromoViewController
    : UIViewController <DefaultBrowserGenericPromoConsumer>

// The action handler for interactions in this view controller.
@property(nonatomic, weak) id<ConfirmationAlertActionHandler> actionHandler;

// Whether or not to show the Remind Me Later button.
@property(nonatomic, assign) BOOL hasRemindMeLater;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_GENERIC_DEFAULT_BROWSER_GENERIC_PROMO_VIEW_CONTROLLER_H_
