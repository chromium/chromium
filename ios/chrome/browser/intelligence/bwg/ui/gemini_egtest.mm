// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_app_interface.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

// Test suite for BWG UI.
@interface GeminiTestCase : ChromeTestCase
@end

@implementation GeminiTestCase

- (void)setUp {
  [super setUp];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  [SigninEarlGreyAppInterface addFakeIdentity:fakeIdentity
                             withCapabilities:@{
                               @(kCanUseModelExecutionFeaturesName) : @YES,
                               @(kCanUseGeminiInChromeCapabilityName) : @YES
                             }];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [ChromeEarlGrey setIntegerValue:0 forUserPref:prefs::kGeminiEnabledByPolicy];
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:prefs::kAIHubEligibilityTriggered];
  [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kIOSBwgConsent];

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];
  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kPageActionMenu);

  if ([self isRunningTest:@selector(testAIHubNewBadgeAccessibility)]) {
    config.iph_feature_enabled = "IPH_iOSAIHubNewBadge";
    config.relaunch_policy = ForceRelaunchByKilling;
  }

  return config;
}

// Tests that the AI Hub entry point conveys the "New" context to accessibility.
- (void)testAIHubNewBadgeAccessibility {
  if ([ChromeEarlGrey isChromeNextEnabled]) {
    EARL_GREY_TEST_DISABLED(@"No 'new' label with Next");
  }
  NSString* baseLabel = l10n_util::GetNSString(
      IDS_IOS_BWG_PAGE_ACTION_MENU_ENTRY_POINT_ACCESSIBILITY_LABEL);
  NSString* expectedLabel =
      [NSString stringWithFormat:@"%@, %@", baseLabel,
                                 l10n_util::GetNSString(
                                     IDS_IOS_NEW_FEATURE_ACCESSIBILITY_HINT)];

  id<GREYMatcher> entrypointMatcher = grey_allOf(
      grey_accessibilityLabel(expectedLabel), grey_sufficientlyVisible(), nil);

  [[EarlGrey selectElementWithMatcher:entrypointMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
