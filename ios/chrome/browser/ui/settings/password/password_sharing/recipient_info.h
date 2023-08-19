// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_RECIPIENT_INFO_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_RECIPIENT_INFO_H_

#import <Foundation/Foundation.h>

namespace password_manager {
struct RecipientInfo;
}  // namespace password_manager

// Object which is used by `FamilyPickerViewController` to show information
// about the potential password sharing recipient of the user.
@interface RecipientInfoForIOSDisplay : NSObject

// Full name of the recipient.
@property(nonatomic, copy) NSString* fullName;

// Email address of the recipient.
@property(nonatomic, copy) NSString* email;

// Whether the recipient is eligible to receive a shared password.
@property(nonatomic, assign) BOOL isEligible;

- (instancetype)initWithRecipientInfo:
    (const password_manager::RecipientInfo&)recipient NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_RECIPIENT_INFO_H_
