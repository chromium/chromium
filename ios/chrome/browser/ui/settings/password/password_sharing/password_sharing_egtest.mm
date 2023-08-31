// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_egtest_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

using chrome_test_util::ButtonWithAccessibilityLabel;
using password_manager_test_utils::kScrollAmount;
using password_manager_test_utils::OpenPasswordManager;
using password_manager_test_utils::SavePasswordForm;

void SignInAndEnableSync() {
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fake_identity enableSync:YES];
}

}  // namespace

// Test case for the Password Sharing flow.
@interface PasswordSharingTestCase : ChromeTestCase

- (GREYElementInteraction*)saveExamplePasswordAndOpenDetails;

@end

@implementation PasswordSharingTestCase

- (GREYElementInteraction*)saveExamplePasswordAndOpenDetails {
  // Mock successful reauth for opening the Password Manager.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  SavePasswordForm();
  OpenPasswordManager();

  return [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(@"example.com"),
                                          grey_accessibilityTrait(
                                              UIAccessibilityTraitButton),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(kPasswordsTableViewId)]
      performAction:grey_tap()];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;
  // Make recipients fetcher return `FetchFamilyMembersRequestStatus::kSuccess`
  // by default. Individual tests can override it.
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kFamilyStatus + "=1");

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

- (void)tearDown {
  [PasswordSettingsAppInterface removeMockReauthenticationModule];
  [super tearDown];
}

- (void)testShareButtonVisibilityWithSharingDisabled {
  SignInAndEnableSync();
  [self saveExamplePasswordAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonId)]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

- (void)testShareButtonVisibilityWithSharingEnabled {
  SignInAndEnableSync();
  [self saveExamplePasswordAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonId)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testFamilyPickerCancelFlow {
  SignInAndEnableSync();
  [self saveExamplePasswordAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonId)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonId)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFamilyPickerCancelButtonId)]
      performAction:grey_tap()];

  // Check that the current view is the password details view.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordDetailsTableViewId)]
      assertWithMatcher:grey_notNil()];
}

- (void)testFetchingRecipientsNoFamilyStatus {
  // Override family status with `FetchFamilyMembersRequestStatus::kNoFamily`.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kFamilyStatus + "=3");
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  SignInAndEnableSync();
  [self saveExamplePasswordAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonId)]
      performAction:grey_tap()];

  // Check that the family promo view was displayed.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_PASSWORD_SHARING_FAMILY_PROMO_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_PASSWORD_SHARING_FAMILY_PROMO_BUTTON))]
      performAction:grey_tap()];

  // Check that the current view is the password details view.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordDetailsTableViewId)]
      assertWithMatcher:grey_notNil()];
}

- (void)testFetchingRecipientsError {
  // Override family status with `FetchFamilyMembersRequestStatus::kUnknown`.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kFamilyStatus + "=0");
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  SignInAndEnableSync();
  [self saveExamplePasswordAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonId)]
      performAction:grey_tap()];

  // Check that the error view was displayed and close it.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_PASSWORD_SHARING_FETCHING_RECIPIENTS_ERROR_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
      performAction:grey_tap()];

  // Check that the current view is the password details view.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordDetailsTableViewId)]
      assertWithMatcher:grey_notNil()];
}

@end
