// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_reauth/test/reauthentication_app_interface.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/common/ui/reauthentication/mock_reauthentication_module.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@implementation ReauthenticationAppInterface

#pragma mark - Public

+ (void)mockReauthenticationModuleExpectedResult:
    (ReauthenticationResult)expectedResult {
  [self mockModule].expectedResult = expectedResult;
}

+ (void)mockReauthenticationModuleCanAttempt:(BOOL)canAttempt {
  [self mockModule].canAttempt = canAttempt;
}

+ (void)mockReauthenticationModuleShouldSkipReAuth:(BOOL)shouldSkipReAuth {
  [self mockModule].shouldSkipReAuth = shouldSkipReAuth;
}

+ (void)mockReauthenticationModuleReturnMockedResult {
  [[self mockModule] returnMockedReauthenticationResult];
}

#pragma mark - Private

// Helper for accessing the reauthentication module for the profile.
+ (MockReauthenticationModule*)mockModule {
  return base::apple::ObjCCastStrict<MockReauthenticationModule>(
      ReauthenticationServiceFactory::GetForProfile(
          chrome_test_util::GetOriginalProfile())
          ->GetReauthModule());
}

@end
