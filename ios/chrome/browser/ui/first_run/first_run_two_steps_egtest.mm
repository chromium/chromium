// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/signin/ios/browser/features.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/ui/first_run/fre_field_trial.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test first run stages
@interface FirstRunTwoStepsTestCase : ChromeTestCase

@end

@implementation FirstRunTwoStepsTestCase

- (void)setUp {
  [[self class] testForStartup];
  [super setUp];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kSignInAtStartup);
  // Enable 2 steps MICe FRe.
  config.additional_args.push_back(
      "--enable-features=" +
      std::string(signin::kNewMobileIdentityConsistencyFRE.name) + "<" +
      std::string(signin::kNewMobileIdentityConsistencyFRE.name));
  config.additional_args.push_back(
      "--force-fieldtrials=" +
      std::string(signin::kNewMobileIdentityConsistencyFRE.name) + "/Test");
  config.additional_args.push_back(
      "--force-fieldtrial-params=" +
      std::string(signin::kNewMobileIdentityConsistencyFRE.name) +
      ".Test:" + std::string(kNewMobileIdentityConsistencyFREParam) + "/" +
      kNewMobileIdentityConsistencyFREParamTwoSteps);
  // Disable default browser promo.
  config.features_disabled.push_back(kEnableFREDefaultBrowserPromoScreen);
  // Show the First Run UI at startup.
  config.additional_args.push_back("-FirstRunForceEnabled");
  config.additional_args.push_back("true");
  // Relaunch app at each test to rewind the startup state.
  config.relaunch_policy = ForceRelaunchByKilling;

  return config;
}

#pragma mark Tests

// First test.
// TODO(crbug.com/1290848): Need to create EGTests for 2 steps FRE.
- (void)testWelcomeScreenUI {
  [self verifyWelcomeScreenIsDisplayed];
}

#pragma mark Helper

// Checks that the sign-in screen is displayed.
- (void)verifyWelcomeScreenIsDisplayed {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSignInScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

@end
