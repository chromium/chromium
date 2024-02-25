// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_WIDGET_PROMO_INSTRUCTIONS_WIDGET_PROMO_INSTRUCTIONS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_WIDGET_PROMO_INSTRUCTIONS_WIDGET_PROMO_INSTRUCTIONS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol ConfirmationAlertActionHandler;

// Screen that presents the instructions on how to install the Password Manager
// widget. Presented when the user interacts with the "Show Me How" button of
// the Password Manager widget promo that's presented in the Password Manager.
@interface WidgetPromoInstructionsViewController : UIViewController

// The action handler for interactions in this view controller.
@property(nonatomic, weak) id<ConfirmationAlertActionHandler> actionHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_WIDGET_PROMO_INSTRUCTIONS_WIDGET_PROMO_INSTRUCTIONS_VIEW_CONTROLLER_H_
