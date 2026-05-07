// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/eg_tests/composebox_app_interface.h"

#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/aim/model/mock_ios_chrome_aim_eligibility_service.h"
#import "ios/chrome/browser/composebox/model/ios_contextual_search_service_factory.h"
#import "ios/chrome/browser/composebox/model/mock_ios_contextual_search_service.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_input_state.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "testing/gmock/include/gmock/gmock.h"

@implementation ComposeboxAppInterface

+ (void)setFuseboxEligible:(BOOL)eligible {
  ON_CALL(*[self mockService], IsFuseboxEligible)
      .WillByDefault(testing::Return(eligible));
}

+ (void)setCreateImagesEligible:(BOOL)eligible {
  ON_CALL(*[self mockService], IsCreateImagesEligible)
      .WillByDefault(testing::Return(eligible));
}

+ (void)setAimLocallyEligible:(BOOL)eligible {
  ON_CALL(*[self mockService], IsAimLocallyEligible)
      .WillByDefault(testing::Return(eligible));
}

+ (void)setServerEligibilityEnabled:(BOOL)enabled {
  ON_CALL(*[self mockService], IsServerEligibilityEnabled)
      .WillByDefault(testing::Return(enabled));
}

+ (void)setTabUploadAutoSucceed:(BOOL)autoSucceed {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  contextual_search::ContextualSearchService* service =
      ContextualSearchServiceFactory::GetForProfile(profile);
  static_cast<MockIOSContextualSearchService*>(service)
      ->SetTabUploadAutoSucceed(autoSucceed);
}

+ (void)enableAllTools {
  [ComposeboxUIInputState setAllToolsEnabled:YES];
}

+ (void)setAllToolsEnabled:(BOOL)enabled {
  [ComposeboxUIInputState setAllToolsEnabled:enabled];
}

+ (BOOL)isServerSideStateEnabled {
  return EnableComposeboxServerSideState();
}

#pragma mark - Private

+ (MockIOSChromeAimEligibilityService*)mockService {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  AimEligibilityService* service =
      IOSChromeAimEligibilityServiceFactory::GetForProfile(profile);

  return static_cast<MockIOSChromeAimEligibilityService*>(service);
}

@end
