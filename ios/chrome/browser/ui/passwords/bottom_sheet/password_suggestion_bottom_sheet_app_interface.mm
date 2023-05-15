// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_app_interface.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/scoped_password_suggestion_bottom_sheet_reauth_module_override.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"
#import "ios/chrome/test/app/password_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::
    SetUpAndReturnMockReauthenticationModuleForPasswordSuggestionBottomSheet;

@implementation PasswordSuggestionBottomSheetAppInterface

static std::unique_ptr<ScopedPasswordSuggestionBottomSheetReauthModuleOverride>
    _scopedReauthOverride;

+ (void)setUpMockReauthenticationModule {
  _scopedReauthOverride =
      SetUpAndReturnMockReauthenticationModuleForPasswordSuggestionBottomSheet();
}

+ (void)mockReauthenticationModuleExpectedResult:
    (ReauthenticationResult)expectedResult {
  CHECK(_scopedReauthOverride);
  MockReauthenticationModule* mockModule =
      base::mac::ObjCCastStrict<MockReauthenticationModule>(
          _scopedReauthOverride->module);
  mockModule.expectedResult = expectedResult;
}

@end
