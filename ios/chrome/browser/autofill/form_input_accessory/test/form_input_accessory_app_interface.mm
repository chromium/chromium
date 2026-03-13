// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_input_accessory/test/form_input_accessory_app_interface.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service_factory.h"
#import "ios/chrome/common/ui/reauthentication/mock_reauthentication_module.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@implementation FormInputAccessoryAppInterface

+ (void)mockReauthenticationModuleExpectedResult:
    (ReauthenticationResult)expectedResult {
  MockReauthenticationModule* mockReauthModule =
      ReauthenticationServiceFactory::GetForProfile(
          chrome_test_util::GetOriginalProfile())
          ->GetReauthModule();
  [mockReauthModule setExpectedResult:expectedResult];
}

@end
