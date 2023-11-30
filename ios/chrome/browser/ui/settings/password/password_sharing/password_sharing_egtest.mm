// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_egtest_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"
#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_constants.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
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

using base::test::ios::kWaitForActionTimeout;
using password_manager_test_utils::kScrollAmount;
using password_manager_test_utils::OpenPasswordManager;
using password_manager_test_utils::SavePasswordForm;

constexpr char kGoogleHelpCenterURL[] = "support.google.com";

void SignInAndEnableSync() {
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fake_identity enableSync:YES];
}

// Matcher for Password Sharing First Run.
id<GREYMatcher> PasswordSharingFirstRunMatcher() {
  return grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_FIRST_RUN_TITLE));
}

}  // namespace

// Test case for the Password Sharing flow.
@interface PasswordSharingTestCase : ChromeTestCase

- (GREYElementInteraction*)saveExamplePasswordAndOpenDetails;

- (GREYElementInteraction*)saveExamplePasswordsAndOpenDetails;

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
      onElementWithMatcher:grey_accessibilityID(kPasswordsTableViewID)]
      performAction:grey_tap()];
}

- (GREYElementInteraction*)saveExamplePasswordsAndOpenDetails {
  // Mock successful reauth for opening the Password Manager.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  SavePasswordForm(/*password=*/@"password1",
                   /*username=*/@"username1");
  SavePasswordForm(/*password=*/@"password2",
                   /*username=*/@"username2");
  OpenPasswordManager();

  return [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              @"example.com, 2 accounts"),
                                          grey_accessibilityTrait(
                                              UIAccessibilityTraitButton),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(kPasswordsTableViewID)]
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
    config.features_enabled.push_back(
        password_manager::features::kPasswordManagerEnableSenderService);
  }

  if ([self isRunningTest:@selector
            (testFirstRunExperienceViewDismissedForAuthentication)]) {
    config.features_enabled.push_back(
        password_manager::features::kIOSPasswordAuthOnEntryV2);
  }

  return config;
}

- (void)setUp {
  [super setUp];

  // Make sure the following pref is in its non-default state (which should be
  // the case for all tests that do not test the first run experience flow).
  [ChromeEarlGrey setBoolValue:YES
                   forUserPref:prefs::kPasswordSharingFlowHasBeenEntered];
  // Make sure the password sharing pref is in its default state.
  [ChromeEarlGrey
      setBoolValue:YES
       forUserPref:password_manager::prefs::kPasswordSharingEnabled];
}

- (void)tearDown {
  [PasswordSettingsAppInterface removeMockReauthenticationModule];

  // Reset preference to its non-default state (which should be the case
  // for all tests that do not test the first run experience flow).
  [ChromeEarlGrey setBoolValue:YES
                   forUserPref:prefs::kPasswordSharingFlowHasBeenEntered];
  // Reset the password sharing pref to its default state.
  [ChromeEarlGrey
      setBoolValue:YES
       forUserPref:password_manager::prefs::kPasswordSharingEnabled];

  [super tearDown];
}

- (void)testShareButtonVisibilityWithSharingDisabled {
  SignInAndEnableSync();
  [self saveExamplePasswordAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

- (void)testShareButtonVisibilityWithSharingEnabled {
  SignInAndEnableSync();
  [self saveExamplePasswordAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testShareButtonVisibilityForSignedOutUser {
  [self saveExamplePasswordAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

- (void)testShareButtonVisibilityForUserOptedInToAccountStorage {
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fake_identity enableSync:NO];

  [self saveExamplePasswordAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testShareButtonVisibilityWithSharingPolicyDisabled {
  [ChromeEarlGrey
      setBoolValue:NO
       forUserPref:password_manager::prefs::kPasswordSharingEnabled];

  SignInAndEnableSync();
  [self saveExamplePasswordAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

- (void)testFamilyPickerCancelFlow {
  SignInAndEnableSync();
  [self saveExamplePasswordAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFamilyPickerCancelButtonId)]
      performAction:grey_tap()];

  // Check that the current view is the password details view.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordDetailsTableViewID)]
      assertWithMatcher:grey_notNil()];
}

- (void)testPasswordPickerCancelFlow {
  SignInAndEnableSync();
  [self saveExamplePasswordsAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordPickerCancelButtonId)]
      performAction:grey_tap()];

  // Check that the current view is the password details view.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordDetailsTableViewID)]
      assertWithMatcher:grey_notNil()];
}

- (void)testFetchingRecipientsNoFamilyStatus {
  // Override family status with `FetchFamilyMembersRequestStatus::kNoFamily`.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kFamilyStatus + "=3");
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  [ChromeEarlGrey setBoolValue:YES
                   forUserPref:prefs::kPasswordSharingFlowHasBeenEntered];

  SignInAndEnableSync();
  [self saveExamplePasswordAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      performAction:grey_tap()];

  // Check that the family promo view was displayed.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kConfirmationAlertTitleAccessibilityIdentifier),
                            grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_PASSWORD_SHARING_FAMILY_PROMO_TITLE)),
                            nil)] assertWithMatcher:grey_sufficientlyVisible()];

  // Click the "Got It" button.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kConfirmationAlertPrimaryActionAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Check that the current view is the password details view.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordDetailsTableViewID)]
      assertWithMatcher:grey_notNil()];
}

- (void)testTappingGotItInFamilyPromoInviteMembersView {
  // Override family status with
  // `FetchFamilyMembersRequestStatus::kNoOtherFamilyMembers`.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kFamilyStatus + "=5");
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  [ChromeEarlGrey setBoolValue:YES
                   forUserPref:prefs::kPasswordSharingFlowHasBeenEntered];

  SignInAndEnableSync();
  [self saveExamplePasswordAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      performAction:grey_tap()];

  // Check that the family promo view was displayed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityID(
                  kConfirmationAlertTitleAccessibilityIdentifier),
              grey_accessibilityLabel(l10n_util::GetNSString(
                  IDS_IOS_PASSWORD_SHARING_FAMILY_PROMO_INVITE_MEMBERS_TITLE)),
              nil)] assertWithMatcher:grey_sufficientlyVisible()];

  // Click the "Got It" button.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kConfirmationAlertPrimaryActionAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Check that the current view is the password details view.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordDetailsTableViewID)]
      assertWithMatcher:grey_notNil()];
}

- (void)testFetchingRecipientsError {
  // Override family status with `FetchFamilyMembersRequestStatus::kUnknown`.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kFamilyStatus + "=0");
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  [ChromeEarlGrey setBoolValue:YES
                   forUserPref:prefs::kPasswordSharingFlowHasBeenEntered];

  SignInAndEnableSync();
  [self saveExamplePasswordAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
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
                                          kPasswordDetailsTableViewID)]
      assertWithMatcher:grey_notNil()];
}

- (void)testPasswordSharingSuccess {
  SignInAndEnableSync();
  [self saveExamplePasswordAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      performAction:grey_tap()];

  // Make sure that the share button is disabled before the recipient selection
  // and enabled after.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kFamilyPickerShareButtonId)]
      assertWithMatcher:grey_not(grey_enabled())];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user1@gmail.com")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kFamilyPickerShareButtonId)]
      assertWithMatcher:grey_enabled()];

  // Initiate sharing and wait for the animation to finish.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kFamilyPickerShareButtonId)]
      performAction:grey_tap()];
  // TODO(crbug.com/1463882): Override animation time for tests.
  GREYCondition* waitForAnimationEnding = [GREYCondition
      conditionWithName:@"Wait for sharing animation to end"
                  block:^{
                    NSError* error = nil;
                    [[EarlGrey
                        selectElementWithMatcher:
                            grey_allOf(
                                grey_accessibilityLabel(l10n_util::GetNSString(
                                    IDS_IOS_PASSWORD_SHARING_SUCCESS_TITLE)),
                                grey_kindOfClassName(@"UILabel"), nil)]
                        assertWithMatcher:grey_sufficientlyVisible()
                                    error:&error];
                    return error == nil;
                  }];
  GREYAssertTrue([waitForAnimationEnding
                     waitWithTimeout:kWaitForActionTimeout.InSecondsF()],
                 @"Animation did not finish.");

  // Dismiss the success status view and check that the password details view is
  // currently displayed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSharingStatusDoneButtonId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordDetailsTableViewID)]
      assertWithMatcher:grey_notNil()];
}

- (void)testNavigationBetweenPasswordAndFamilyPicker {
  SignInAndEnableSync();
  [self saveExamplePasswordsAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      performAction:grey_tap()];

  // Check that the next button is enabled by default.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordPickerNextButtonId)]
      assertWithMatcher:grey_enabled()];

  // Select second row and click "Next".
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(@"username2"),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordPickerNextButtonId)]
      performAction:grey_tap()];

  // Tap "Back" in family picker view and confirm it opens password picker.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kFamilyPickerBackButtonId)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kFamilyPickerBackButtonId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordPickerNextButtonId)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testTappingLearnMoreInFamilyPickerInfoPopup {
  SignInAndEnableSync();
  [self saveExamplePasswordAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      performAction:grey_tap()];

  // Scroll down to the last recipient (the ineligible ones are on the bottom).
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kFamilyPickerTableViewId)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  // Tap on the info button next to the ineligible recipient row.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityID([NSString
                         stringWithFormat:@"%@ %@", kFamilyPickerInfoButtonId,
                                          @"user4@gmail.com"]),
                     grey_kindOfClass([UIButton class]), nil)]
      performAction:grey_tap()];

  // Tap the "Learn more" link in the popup.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Learn more")]
      performAction:grey_tap()];

  // Check that the help center article was opened.
  GREYAssertEqual(std::string(kGoogleHelpCenterURL),
                  [ChromeEarlGrey webStateVisibleURL].host(),
                  @"Did not navigate to the help center article.");
}

- (void)testTappingCancelInFirstRunExperienceView {
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:prefs::kPasswordSharingFlowHasBeenEntered];

  SignInAndEnableSync();
  [self saveExamplePasswordAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      performAction:grey_tap()];

  // Tap the cancel button.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kConfirmationAlertSecondaryActionAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Check that the current view is the password details view.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordDetailsTableViewID)]
      assertWithMatcher:grey_notNil()];

  // Tap the share button again and verify that the first run view is still
  // displayed since it was not acknowledged.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PasswordSharingFirstRunMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testTappingShareInFirstRunExperienceView {
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:prefs::kPasswordSharingFlowHasBeenEntered];

  SignInAndEnableSync();
  [self saveExamplePasswordAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      performAction:grey_tap()];

  // Tap the share button in the first run experience view.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kConfirmationAlertPrimaryActionAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Check that the current view is the family picker view.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kFamilyPickerTableViewId)]
      assertWithMatcher:grey_notNil()];

  // Tap the cancel button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFamilyPickerCancelButtonId)]
      performAction:grey_tap()];

  // Tap the share button in password details view and verify that the first run
  // view will not be displayed anymore since it was acknowledged.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kFamilyPickerTableViewId)]
      assertWithMatcher:grey_notNil()];
}

- (void)testTappingLearnMoreInFirstRunExperienceView {
  // TODO(crbug.com/1488977): Test fails on iPad simulator.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Failing on iPad Simulator");
  }

  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:prefs::kPasswordSharingFlowHasBeenEntered];

  SignInAndEnableSync();
  [self saveExamplePasswordAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      performAction:grey_tap()];

  // Tap the "Learn more" link in the popup.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Learn more")]
      performAction:grey_tap()];

  // Check that the help center article was opened.
  GREYAssertEqual(std::string(kGoogleHelpCenterURL),
                  [ChromeEarlGrey webStateVisibleURL].host(),
                  @"Did not navigate to the help center article.");
}

- (void)testFirstRunExperienceViewDismissedForAuthentication {
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:prefs::kPasswordSharingFlowHasBeenEntered];

  SignInAndEnableSync();
  [self saveExamplePasswordAndOpenDetails];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordShareButtonID)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordSharingFirstRunMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Background then foreground app so reauthentication UI is displayed.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Check that first run experience is gone.
  [[EarlGrey selectElementWithMatcher:PasswordSharingFirstRunMatcher()]
      assertWithMatcher:grey_nil()];
}

@end
