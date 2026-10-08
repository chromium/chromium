// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/test/credential_suggestion_bottom_sheet_app_interface.h"

#import "base/apple/foundation_util.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/common/ui/reauthentication/fake_reauthentication_module.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@implementation CredentialSuggestionBottomSheetAppInterface

+ (void)mockReauthenticationModuleExpectedResult:
    (ReauthenticationResult)expectedResult {
  FakeReauthenticationModule* fakeModule =
      base::apple::ObjCCastStrict<FakeReauthenticationModule>(
          ReauthenticationServiceFactory::GetForProfile(
              chrome_test_util::GetOriginalProfile())
              ->GetReauthModule());
  fakeModule.expectedResult = expectedResult;
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
