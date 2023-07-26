// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_RECIPIENT_INFO_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_RECIPIENT_INFO_H_

#import <Foundation/Foundation.h>

// Object which is used by `FamilyPickerViewController` to show information
// about the potential password sharing recipient of the user.
@interface RecipientInfo : NSObject

// Full name of the recipient.
@property(nonatomic, copy) NSString* fullName;

// Email address of the recipient.
@property(nonatomic, copy) NSString* email;

// Whether the recipient is eligible to receive a shared password.
@property(nonatomic, assign) BOOL isEligible;

// TODO(crbug.com/1463882): Replace with constructor taking
// password_manager::RecipientInfo struct once its implementation is finalized.
- (instancetype)init;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_RECIPIENT_INFO_H_
