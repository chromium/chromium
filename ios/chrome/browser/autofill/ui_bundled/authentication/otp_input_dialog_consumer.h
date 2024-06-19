// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_OTP_INPUT_DIALOG_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_OTP_INPUT_DIALOG_CONSUMER_H_

#import <Foundation/Foundation.h>

@class OtpInputDialogContent;

// The consumer interface that takes data from the mediator.
@protocol OtpInputDialogConsumer <NSObject>

// Set the Autofill OTP input dialog content data. This should be called only
// once before the view is loaded.
- (void)setContent:(OtpInputDialogContent*)content;

// Updates the confirm button's enabling state.
- (void)setConfirmButtonEnabled:(BOOL)enabled;

// Update the dialog to show pending state.
- (void)showPendingState;

// Update the dialog to show the invalid state. This invalid state is shown if
// the user submits an incorrect or expired OTP.
- (void)showInvalidState:(NSString*)invalidLabelText;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_OTP_INPUT_DIALOG_CONSUMER_H_
