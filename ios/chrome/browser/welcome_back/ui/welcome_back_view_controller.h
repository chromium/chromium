// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WELCOME_BACK_UI_WELCOME_BACK_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_WELCOME_BACK_UI_WELCOME_BACK_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/welcome_back/ui/welcome_back_screen_consumer.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

@protocol WelcomeBackActionHandler;

// View controller that presents a half-sheet UI displaying the Welcome Back
// promo.
@interface WelcomeBackViewController
    : ConfirmationAlertViewController <WelcomeBackScreenConsumer>

@property(nonatomic, weak) id<WelcomeBackActionHandler>
    welcomeBackActionHandler;

@end

#endif  // IOS_CHROME_BROWSER_WELCOME_BACK_UI_WELCOME_BACK_VIEW_CONTROLLER_H_
