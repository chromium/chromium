// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_app_interface.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/scoped_form_input_accessory_reauth_module_override.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"
#import "ios/chrome/test/app/password_test_util.h"

using chrome_test_util::
    SetUpAndReturnMockReauthenticationModuleForFormInputAccessory;

@interface FormInputAccessoryAppInterface () {
  std::unique_ptr<ScopedFormInputAccessoryReauthModuleOverride>
      _scopedReauthOverride;
}

@end

@implementation FormInputAccessoryAppInterface

+ (instancetype)sharedInstance {
  static FormInputAccessoryAppInterface* instance;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    instance = [[FormInputAccessoryAppInterface alloc] init];
  });
  return instance;
}

+ (std::unique_ptr<ScopedFormInputAccessoryReauthModuleOverride>&)
    scopedReauthOverride {
  return [FormInputAccessoryAppInterface sharedInstance]->_scopedReauthOverride;
}

+ (void)setUpMockReauthenticationModule {
  [FormInputAccessoryAppInterface scopedReauthOverride] =
      SetUpAndReturnMockReauthenticationModuleForFormInputAccessory();
}

+ (void)mockReauthenticationModuleExpectedResult:
    (ReauthenticationResult)expectedResult {
  std::unique_ptr<ScopedFormInputAccessoryReauthModuleOverride>&
      scopedReauthOverride =
          [FormInputAccessoryAppInterface scopedReauthOverride];
  CHECK(scopedReauthOverride);
  MockReauthenticationModule* mockModule =
      base::apple::ObjCCastStrict<MockReauthenticationModule>(
          scopedReauthOverride->module);
  mockModule.expectedResult = expectedResult;
}

+ (void)removeMockReauthenticationModule {
  [FormInputAccessoryAppInterface scopedReauthOverride] = nullptr;
}

@end
