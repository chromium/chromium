// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

@protocol PasswordSuggestionBottomSheetDelegate;
@protocol PasswordSuggestionBottomSheetHandler;

// Password Bottom Sheet UI, which includes a table to display password
// suggestions, a button to use a suggestion and a button to revert to using the
// keyboard to enter a password.
@interface PasswordSuggestionBottomSheetViewController
    : ConfirmationAlertViewController <PasswordSuggestionBottomSheetConsumer>

// Initialize with the delegate used to open the password manager.
- (instancetype)initWithHandler:
    (id<PasswordSuggestionBottomSheetHandler>)handler;

// The delegate for the bottom sheet view controller.
@property(nonatomic, strong) id<PasswordSuggestionBottomSheetDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_VIEW_CONTROLLER_H_
