// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_OTP_INPUT_DIALOG_MUTATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_OTP_INPUT_DIALOG_MUTATOR_H_

#import <Foundation/Foundation.h>

@protocol OtpInputDialogMutator <NSObject>

// Invoked when the confirm button in the navigation bar is tapped by the user.
// This means a valid OTP value is typed in.
- (void)didTapConfirmButton:(NSString*)inputValue;

// Invoked when the cancel button in the navigation bar is tapped by the user.
- (void)didTapCancelButton;

// Notify the model controller when the OTP input value changes.
- (void)onOtpInputChanges:(NSString*)inputValue;

// Invoked when the new code request link is tapped by the user.
- (void)didTapNewCodeLink;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_OTP_INPUT_DIALOG_MUTATOR_H_
