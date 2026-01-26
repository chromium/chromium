// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/test/credential_suggestion_bottom_sheet_app_interface.h"

#import "base/apple/foundation_util.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/passwords/bottom_sheet/public/scoped_credential_suggestion_bottom_sheet_reauth_module_override.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/common/ui/reauthentication/mock_reauthentication_module.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/password_test_util.h"

using chrome_test_util::
    SetUpAndReturnMockReauthenticationModuleForCredentialSuggestionBottomSheet;

@implementation CredentialSuggestionBottomSheetAppInterface {
  std::unique_ptr<ScopedCredentialSuggestionBottomSheetReauthModuleOverride>
      _scopedReauthOverride;
}

+ (instancetype)sharedInstance {
  static CredentialSuggestionBottomSheetAppInterface* sharedInstance = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    sharedInstance = [[CredentialSuggestionBottomSheetAppInterface alloc] init];
  });
  return sharedInstance;
}

+ (void)setUpMockReauthenticationModule {
  CredentialSuggestionBottomSheetAppInterface* shared =
      [CredentialSuggestionBottomSheetAppInterface sharedInstance];
  shared->_scopedReauthOverride =
      SetUpAndReturnMockReauthenticationModuleForCredentialSuggestionBottomSheet();
}

+ (void)mockReauthenticationModuleExpectedResult:
    (ReauthenticationResult)expectedResult {
  CredentialSuggestionBottomSheetAppInterface* shared =
      [CredentialSuggestionBottomSheetAppInterface sharedInstance];
  CHECK(shared->_scopedReauthOverride);
  MockReauthenticationModule* mockModule =
      base::apple::ObjCCastStrict<MockReauthenticationModule>(
          shared->_scopedReauthOverride->module);
  mockModule.expectedResult = expectedResult;
}

+ (void)removeMockReauthenticationModule {
  CredentialSuggestionBottomSheetAppInterface* shared =
      [CredentialSuggestionBottomSheetAppInterface sharedInstance];
  shared->_scopedReauthOverride = nullptr;
}

+ (void)setDismissCount:(int)dismissCount {
  chrome_test_util::GetOriginalProfile()->GetPrefs()->SetInteger(
      prefs::kIosPasswordBottomSheetDismissCount, dismissCount);
}

+ (void)disableBottomSheet {
  chrome_test_util::GetOriginalProfile()->GetPrefs()->SetInteger(
      prefs::kIosPasswordBottomSheetDismissCount,
      AutofillBottomSheetTabHelper::kCredentialBottomSheetMaxDismissCount);
}

@end
