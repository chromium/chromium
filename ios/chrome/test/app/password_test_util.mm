// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/password_test_util.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"

namespace chrome_test_util {

std::unique_ptr<ScopedPasswordSettingsReauthModuleOverride>
SetUpAndReturnMockReauthenticationModuleForPasswordManager() {
  MockReauthenticationModule* mock_reauthentication_module =
      [[MockReauthenticationModule alloc] init];
  return ScopedPasswordSettingsReauthModuleOverride::MakeAndArmForTesting(
      mock_reauthentication_module);
}

std::unique_ptr<ScopedPasswordSuggestionBottomSheetReauthModuleOverride>
SetUpAndReturnMockReauthenticationModuleForPasswordSuggestionBottomSheet() {
  MockReauthenticationModule* mock_reauthentication_module =
      [[MockReauthenticationModule alloc] init];
  return ScopedPasswordSuggestionBottomSheetReauthModuleOverride::
      MakeAndArmForTesting(mock_reauthentication_module);
}

std::unique_ptr<ScopedFormInputAccessoryReauthModuleOverride>
SetUpAndReturnMockReauthenticationModuleForFormInputAccessory() {
  MockReauthenticationModule* mock_reauthentication_module =
      [[MockReauthenticationModule alloc] init];
  return ScopedFormInputAccessoryReauthModuleOverride::MakeAndArmForTesting(
      mock_reauthentication_module);
}

}  // namespace
