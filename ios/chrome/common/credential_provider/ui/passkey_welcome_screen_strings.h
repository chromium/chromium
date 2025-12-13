// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_UI_PASSKEY_WELCOME_SCREEN_STRINGS_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_UI_PASSKEY_WELCOME_SCREEN_STRINGS_H_

#import <Foundation/Foundation.h>

// Possible purposes for showing the passkey welcome screen.
enum class PasskeyWelcomeScreenPurpose {
  kEnroll,
  kFixDegradedRecoverability,
  kReauthenticate,
};

// Contains all the strings that need to be displayed in passkey welcome screen
// for a specific `PasskeyWelcomeScreenPurpose`. Those strings cannot be
// directly initialized in the view controller, because it needs to be displayed
// in both Chromium and Credential Provider Extension, which have different
// logic for string localization.
@interface PasskeyWelcomeScreenStrings : NSObject

@property(nonatomic, readonly) NSString* title;
@property(nonatomic, readonly) NSString* subtitle;
@property(nonatomic, readonly) NSString* footer;
@property(nonatomic, readonly) NSString* primaryButton;
@property(nonatomic, readonly) NSString* secondaryButton;
@property(nonatomic, readonly) NSArray<NSString*>* instructions;

- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
                       footer:(NSString*)footer
                primaryButton:(NSString*)primaryButton
              secondaryButton:(NSString*)secondaryButton
                 instructions:(NSArray<NSString*>*)instructions
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_UI_PASSKEY_WELCOME_SCREEN_STRINGS_H_
