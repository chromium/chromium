// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_OTP_INPUT_DIALOG_CONTENT_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_OTP_INPUT_DIALOG_CONTENT_H_

#import <Foundation/Foundation.h>

// A list of content shown in the OTP input dialog.
@interface OtpInputDialogContent : NSObject

// The title of the dialog.
@property(nonatomic, strong) NSString* windowTitle;

// The placeholder text to show in the empty textfield.
@property(nonatomic, strong) NSString* textFieldPlaceholder;

// The text label for the confirm button.
@property(nonatomic, strong) NSString* confirmButtonLabel;

// TODO(b/324611600): Add footer link text message.

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_OTP_INPUT_DIALOG_CONTENT_H_
