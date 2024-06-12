// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

#import "ios/chrome/browser/passwords/ui_bundled/password_breach_consumer.h"

@interface PasswordBreachViewController
    : ConfirmationAlertViewController <PasswordBreachConsumer>

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_VIEW_CONTROLLER_H_
