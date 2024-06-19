// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_PASSWORD_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_PASSWORD_TEST_UTIL_H_

#import <memory>

#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/scoped_form_input_accessory_reauth_module_override.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/scoped_password_suggestion_bottom_sheet_reauth_module_override.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/scoped_password_settings_reauth_module_override.h"

@class MockReauthenticationModule;

namespace chrome_test_util {

// Replaces the reauthentication module in Password Manager with a fake one to
// avoid being blocked with a reauth prompt and returns the fake
// reauthentication module.
std::unique_ptr<ScopedPasswordSettingsReauthModuleOverride>
SetUpAndReturnMockReauthenticationModuleForPasswordManager();

// Replace the reauthentication module in Password Suggestion Bottom Sheet with
// a fake one to avoid being blocked with a reauth prompt, and return the fake
// reauthentication module.
std::unique_ptr<ScopedPasswordSuggestionBottomSheetReauthModuleOverride>
SetUpAndReturnMockReauthenticationModuleForPasswordSuggestionBottomSheet();

// Replace the reauthentication module in Form Input Accessory with
// a fake one to avoid being blocked with a reauth prompt, and return the fake
// reauthentication module.
std::unique_ptr<ScopedFormInputAccessoryReauthModuleOverride>
SetUpAndReturnMockReauthenticationModuleForFormInputAccessory();

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_PASSWORD_TEST_UTIL_H_
