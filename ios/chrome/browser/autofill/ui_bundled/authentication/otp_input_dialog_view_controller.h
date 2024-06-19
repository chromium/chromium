// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_OTP_INPUT_DIALOG_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_OTP_INPUT_DIALOG_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_consumer.h"

@protocol OtpInputDialogMutator;

// Controller for the UI that allows the user to enter an OTP for card
// verification purposes.
@interface OtpInputDialogViewController
    : ChromeTableViewController <OtpInputDialogConsumer>

// The delegate for user actions.
@property(nonatomic, weak) id<OtpInputDialogMutator> mutator;

- (instancetype)init;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_OTP_INPUT_DIALOG_VIEW_CONTROLLER_H_
