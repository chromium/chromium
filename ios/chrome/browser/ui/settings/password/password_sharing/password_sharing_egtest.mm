// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_egtest_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

using password_manager_test_utils::kScrollAmount;
using password_manager_test_utils::OpenPasswordManager;
using password_manager_test_utils::SavePasswordForm;

// Test case for the Password Sharing flow.
@interface PasswordSharingTestCase : ChromeTestCase

- (GREYElementInteraction*)interactionForSinglePasswordEntryWithDomain:
    (NSString*)domain;

@end

@implementation PasswordSharingTestCase

- (GREYElementInteraction*)interactionForSinglePasswordEntryWithDomain:
    (NSString*)domain {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(domain),
                                          grey_accessibilityTrait(
                                              UIAccessibilityTraitButton),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(kPasswordsTableViewId)];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;

  if ([self isRunningTest:@selector
            (testShareButtonVisibilityWithSharingDisabled)]) {
    config.features_disabled.push_back(
        password_manager::features::kSendPasswords);
  } else {
    config.features_enabled.push_back(
        password_manager::features::kSendPasswords);
  }

  return config;
}

- (void)setUp {
  [super setUp];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:YES];

  // Mock successful reauth for opening the Password Manager.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
}

- (void)tearDown {
  [PasswordSettingsAppInterface removeMockReauthenticationModule];
  [super tearDown];
}

- (void)testShareButtonVisibilityWithSharingDisabled {
  SavePasswordForm();

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonId)]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

- (void)testShareButtonVisibilityWithSharingEnabled {
  SavePasswordForm();

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonId)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
