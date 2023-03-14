// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_PASSWORD_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_PASSWORD_TEST_UTIL_H_

#import <memory>

#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"

#import "ios/chrome/browser/ui/settings/password/password_settings/scoped_password_settings_reauth_module_override.h"

@interface MockReauthenticationModule : NSObject<ReauthenticationProtocol>

// Localized string containing the reason why reauthentication is requested.
@property(nonatomic, copy) NSString* localizedReasonForAuthentication;

// Indicates whether the device is capable of reauthenticating the user.
@property(nonatomic, assign) BOOL canAttempt;

// Indicates whether (mock) authentication should succeed or not. Setting
// `shouldSucceed` to any value sets `canAttempt` to YES.
@property(nonatomic, assign) ReauthenticationResult expectedResult;

@end

namespace chrome_test_util {

// Replace the reauthentication module in
// PasswordDetailsTableViewController with a fake one to avoid being
// blocked with a reauth prompt, and return the fake reauthentication module.
// `is_add_new_password` is true if we are adding a new password (using the
// AddPasswordViewController). This used to determine the class to cast
// properly.
MockReauthenticationModule* SetUpAndReturnMockReauthenticationModule(
    bool is_add_new_password = false);

// Replaces the reauthentication module in Password Manager's password list with
// a fake one to avoid being blocked with a reauth prompt and returns the fake
// reauthentication module.
MockReauthenticationModule*
SetUpAndReturnMockReauthenticationModuleForPasswordManager();

// Replace the reauthentication module in Password Settings'
// PasswordExporter with a fake one to avoid being
// blocked with a reauth prompt, and return the fake reauthentication module.
std::unique_ptr<ScopedPasswordSettingsReauthModuleOverride>
SetUpAndReturnMockReauthenticationModuleForExportFromSettings();

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_PASSWORD_TEST_UTIL_H_
