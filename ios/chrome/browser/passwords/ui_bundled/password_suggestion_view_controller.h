// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_SUGGESTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_SUGGESTION_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

// A view controller to show strong, auto-generated, suggested passwords from
// Google Password Manager.
@interface PasswordSuggestionViewController : ConfirmationAlertViewController

// Initializes this alert with password suggestion and current user email.
- (instancetype)initWithPasswordSuggestion:(NSString*)passwordSuggestion
                                 userEmail:(NSString*)userEmail
                                 proactive:(BOOL)proactivePasswordGeneration
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_SUGGESTION_VIEW_CONTROLLER_H_
