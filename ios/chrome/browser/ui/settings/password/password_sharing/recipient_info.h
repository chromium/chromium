// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_RECIPIENT_INFO_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_RECIPIENT_INFO_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

namespace password_manager {
struct PublicKey;
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

// Recipient's identifier (obfuscated Gaia ID).
@property(nonatomic, readonly) NSString* userID;

// Public key of the user including its version.
@property(nonatomic, readonly) password_manager::PublicKey publicKey;

// URL to the profile picture of the recipient for display in the UI.
@property(nonatomic, copy) NSString* profileImageURL;

// Circular profile icon of the recipient. Initialized with default user icon
// placeholder.
@property(nonatomic, copy) UIImage* profileImage;

// Whether the `profileImage` has been already fetched from `profileImageURL`.
@property(nonatomic, assign, getter=isImageFetched) BOOL imageFetched;

- (instancetype)initWithRecipientInfo:
    (const password_manager::RecipientInfo&)recipient NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_RECIPIENT_INFO_H_
