// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_OTP_INPUT_DIALOG_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_OTP_INPUT_DIALOG_MEDIATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

// The delegate interface that takes calls from the mediator.
@protocol OtpInputDialogMediatorDelegate <NSObject>

// Close the dialog and terminate all related classes.
- (void)dismissDialog;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_OTP_INPUT_DIALOG_MEDIATOR_DELEGATE_H_
