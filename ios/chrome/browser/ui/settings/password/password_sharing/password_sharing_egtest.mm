// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "build/branding_buildflags.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/elements/elements_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_egtest_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_constants.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
#import "components/password_manager/core/browser/password_manager_switches.h"
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace {

using base::test::ios::kWaitForActionTimeout;
using chrome_test_util::NavigationBarCancelButton;
using password_manager_test_utils::EditDoneButton;
using password_manager_test_utils::kScrollAmount;
using password_manager_test_utils::NavigationBarEditButton;
using password_manager_test_utils::OpenPasswordManager;
using password_manager_test_utils::PasswordDetailsShareButtonMatcher;
using password_manager_test_utils::PasswordDetailsTableViewMatcher;
using password_manager_test_utils::SaveExamplePasskeyToStore;
using password_manager_test_utils::SavePasswordFormToProfileStore;

// Matcher for Password Sharing First Run.
id<GREYMatcher> PasswordSharingFirstRunMatcher() {
  return grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_FIRST_RUN_TITLE));
}

// Matcher for the UITableView inside the Family Picker View.
id<GREYMatcher> FamilyPickerTableViewMatcher() {
  return grey_accessibilityID(kFamilyPickerTableViewID);
}

// Matcher for the Password Picker View.
id<GREYMatcher> PasswordPickerViewMatcher() {
  return grey_accessibilityID(kPasswordPickerViewID);
}

GREYElementInteraction* TapCredentialEntryWithDomain(NSString* domain) {
  return [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(domain),
                                          grey_accessibilityTrait(
                                              UIAccessibilityTraitButton),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(kPasswordsTableViewID)]
      performAction:grey_tap()];
}

// TODO(crbug.com/348484044): Re-enable the test suite on ipad on iOS 17.
#define DISABLE_ON_IPAD_WITH_IOS_17                            \
  if (@available(iOS 17.0, *)) {                               \
    if ([ChromeEarlGrey isIPadIdiom]) {                        \
      EARL_GREY_TEST_DISABLED(@"Disabled for iPad on iOS 17"); \
    }                                                          \
  }
}  // namespace

// Test case for the Password Sharing flow.
@interface PasswordSharingTestCase : ChromeTestCase

- (GREYElementInteraction*)saveExamplePasswordToProfileStoreAndOpenDetails;

- (GREYElementInteraction*)saveExamplePasswordsToProfileStoreAndOpenDetails;

- (GREYElementInteraction*)saveExamplePasskeyToStoreAndOpenDetails;

- (GREYElementInteraction*)saveExamplePasskeyAndPasswordToStoreAndOpenDetails;

@end

@implementation PasswordSharingTestCase

- (GREYElementInteraction*)saveExamplePasswordToProfileStoreAndOpenDetails {
  // Mock successful reauth for opening the Password Manager.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  SavePasswordFormToProfileStore();
  OpenPasswordManager();
  return TapCredentialEntryWithDomain(@"example.com");
}

- (GREYElementInteraction*)saveExamplePasswordsToProfileStoreAndOpenDetails {
  // Mock successful reauth for opening the Password Manager.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  SavePasswordFormToProfileStore(/*password=*/@"password1",
                                 /*username=*/@"username1");
  SavePasswordFormToProfileStore(/*password=*/@"password2",
                                 /*username=*/@"username2");
  OpenPasswordManager();
  return TapCredentialEntryWithDomain(@"example.com, 2 accounts");
}

- (GREYElementInteraction*)saveExamplePasskeyToStoreAndOpenDetails {
  // Mock successful reauth for opening the Password Manager.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  SaveExamplePasskeyToStore();
  OpenPasswordManager();
  return TapCredentialEntryWithDomain(@"example.com");
}

- (GREYElementInteraction*)saveExamplePasskeyAndPasswordToStoreAndOpenDetails {
  // Mock successful reauth for opening the Password Manager.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  SaveExamplePasskeyToStore();
  SavePasswordFormToProfileStore();
  OpenPasswordManager();
  return TapCredentialEntryWithDomain(@"example.com, 2 accounts");
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;
  // Make recipients fetcher return `FetchFamilyMembersRequestStatus::kSuccess`
  // by default. Individual tests can override it.
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kFamilyStatus + "=1");
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Make tests run on unbranded builds.
  config.additional_args.push_back(
      std::string("-") + password_manager::kEnableShareButtonUnbranded);
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)

  if ([self isRunningTest:@selector(testShareButtonDisabledWithJustPasskeys)] ||
      [self isRunningTest:@selector
            (testShareButtonEnabledWithMixOfPasswordsAndPasskeys)]) {
    config.features_enabled.push_back(syncer::kSyncWebauthnCredentials);
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

- (void)testShareButtonVisibility {
  DISABLE_ON_IPAD_WITH_IOS_17
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasswordToProfileStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testShareButtonVisibilityForSignedOutUser {
  DISABLE_ON_IPAD_WITH_IOS_17
  [self saveExamplePasswordToProfileStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

- (void)testShareButtonVisibilityForUserOptedInToAccountStorage {
  DISABLE_ON_IPAD_WITH_IOS_17
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fake_identity];

  [self saveExamplePasswordToProfileStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testShareButtonVisibilityWithSharingPolicyDisabled {
  DISABLE_ON_IPAD_WITH_IOS_17
  [ChromeEarlGrey
      setBoolValue:NO
       forUserPref:password_manager::prefs::kPasswordSharingEnabled];

  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasswordToProfileStoreAndOpenDetails];

  // Share button should be visible and display the policy info popup upon tap.
  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEnterpriseInfoBubbleViewId)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testShareButtonVisibilityDuringPasswordEditing {
  DISABLE_ON_IPAD_WITH_IOS_17
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasswordToProfileStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Share button should not be visible during password details editing.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Share button should be visible again after editing is confirmed.
  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // The same behaviour should be observed if editing is cancelled.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testShareButtonDisabledWithJustPasskeys {
  DISABLE_ON_IPAD_WITH_IOS_17
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasskeyToStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      assertWithMatcher:grey_not(grey_enabled())];
}

- (void)testShareButtonEnabledWithMixOfPasswordsAndPasskeys {
  DISABLE_ON_IPAD_WITH_IOS_17
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasskeyAndPasswordToStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      assertWithMatcher:grey_enabled()];
}

- (void)testFamilyPickerCancelFlow {
  DISABLE_ON_IPAD_WITH_IOS_17
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasswordToProfileStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFamilyPickerCancelButtonID)]
      performAction:grey_tap()];

  // Check that the current view is the password details view.
  [[EarlGrey selectElementWithMatcher:PasswordDetailsTableViewMatcher()]
      assertWithMatcher:grey_notNil()];
}

- (void)testPasswordPickerCancelFlow {
  DISABLE_ON_IPAD_WITH_IOS_17
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasswordsToProfileStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordPickerCancelButtonID)]
      performAction:grey_tap()];

  // Check that the current view is the password details view.
  [[EarlGrey selectElementWithMatcher:PasswordDetailsTableViewMatcher()]
      assertWithMatcher:grey_notNil()];
}

- (void)testFamilyPickerSwipeToDismissFlow {
  DISABLE_ON_IPAD_WITH_IOS_17
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasswordToProfileStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kFamilyPickerTableViewID)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the current view is the password details view.
  [[EarlGrey selectElementWithMatcher:PasswordDetailsTableViewMatcher()]
      assertWithMatcher:grey_notNil()];
}

- (void)testFamilyPromoSwipeToDismissFlow {
  DISABLE_ON_IPAD_WITH_IOS_17
  // Override family status with `FetchFamilyMembersRequestStatus::kNoFamily`.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kFamilyStatus + "=3");
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  [ChromeEarlGrey setBoolValue:YES
                   forUserPref:prefs::kPasswordSharingFlowHasBeenEntered];

  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasswordToProfileStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(kFamilyPromoViewID)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the current view is the password details view.
  [[EarlGrey selectElementWithMatcher:PasswordDetailsTableViewMatcher()]
      assertWithMatcher:grey_notNil()];
}

- (void)testPasswordPickerSwipeToDismissFlow {
  DISABLE_ON_IPAD_WITH_IOS_17
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasswordsToProfileStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordPickerViewMatcher()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the current view is the password details view.
  [[EarlGrey selectElementWithMatcher:PasswordDetailsTableViewMatcher()]
      assertWithMatcher:grey_notNil()];
}

- (void)testSharingStatusSwipeToDismissFlow {
  DISABLE_ON_IPAD_WITH_IOS_17
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasswordToProfileStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user1@gmail.com")]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kFamilyPickerShareButtonID)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSharingStatusViewID)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the current view is the password details view.
  [[EarlGrey selectElementWithMatcher:PasswordDetailsTableViewMatcher()]
      assertWithMatcher:grey_notNil()];
}

- (void)testFetchingRecipientsNoFamilyStatus {
  DISABLE_ON_IPAD_WITH_IOS_17
  // Override family status with `FetchFamilyMembersRequestStatus::kNoFamily`.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kFamilyStatus + "=3");
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  [ChromeEarlGrey setBoolValue:YES
                   forUserPref:prefs::kPasswordSharingFlowHasBeenEntered];

  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasswordToProfileStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
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
  [[EarlGrey selectElementWithMatcher:PasswordDetailsTableViewMatcher()]
      assertWithMatcher:grey_notNil()];
}

- (void)testTappingGotItInFamilyPromoInviteMembersView {
  DISABLE_ON_IPAD_WITH_IOS_17
  // Override family status with
  // `FetchFamilyMembersRequestStatus::kNoOtherFamilyMembers`.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kFamilyStatus + "=5");
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  [ChromeEarlGrey setBoolValue:YES
                   forUserPref:prefs::kPasswordSharingFlowHasBeenEntered];

  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasswordToProfileStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
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
  [[EarlGrey selectElementWithMatcher:PasswordDetailsTableViewMatcher()]
      assertWithMatcher:grey_notNil()];
}

- (void)testFetchingRecipientsError {
  DISABLE_ON_IPAD_WITH_IOS_17
  // Override family status with `FetchFamilyMembersRequestStatus::kUnknown`.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kFamilyStatus + "=0");
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
  [ChromeEarlGrey setBoolValue:YES
                   forUserPref:prefs::kPasswordSharingFlowHasBeenEntered];

  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasswordToProfileStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      performAction:grey_tap()];

  // Check that the error view was displayed and close it.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_PASSWORD_SHARING_FETCHING_RECIPIENTS_ERROR_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
      performAction:grey_tap()];

  // Check that the current view is the password details view.
  [[EarlGrey selectElementWithMatcher:PasswordDetailsTableViewMatcher()]
      assertWithMatcher:grey_notNil()];
}

- (void)testPasswordSharingSuccess {
  DISABLE_ON_IPAD_WITH_IOS_17
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasswordToProfileStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      performAction:grey_tap()];

  // Make sure that the share button is disabled before the recipient selection
  // and enabled after.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kFamilyPickerShareButtonID)]
      assertWithMatcher:grey_not(grey_enabled())];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"user1@gmail.com")]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kFamilyPickerShareButtonID)]
      assertWithMatcher:grey_enabled()];

  // Initiate sharing and wait for the animation to finish.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kFamilyPickerShareButtonID)]
      performAction:grey_tap()];
  // TODO(crbug.com/40275395): Override animation time for tests.
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
      selectElementWithMatcher:grey_accessibilityID(kSharingStatusDoneButtonID)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PasswordDetailsTableViewMatcher()]
      assertWithMatcher:grey_notNil()];
}

- (void)testNavigationBetweenPasswordAndFamilyPicker {
  DISABLE_ON_IPAD_WITH_IOS_17
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasswordsToProfileStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      performAction:grey_tap()];

  // Check that the next button is enabled by default.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordPickerNextButtonID)]
      assertWithMatcher:grey_enabled()];

  // Select second row and click "Next".
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(@"username2"),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordPickerNextButtonID)]
      performAction:grey_tap()];

  // Tap "Back" in family picker view and confirm it opens password picker.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kFamilyPickerBackButtonID)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kFamilyPickerBackButtonID)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordPickerNextButtonID)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testTappingFamilyPickerIneligibleRecipientInfoPopup {
  DISABLE_ON_IPAD_WITH_IOS_17
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasswordToProfileStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      performAction:grey_tap()];

  // Scroll down to the last recipient (the ineligible ones are on the bottom).
  [[EarlGrey selectElementWithMatcher:FamilyPickerTableViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Tap on the info button next to the ineligible recipient row.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityID([NSString
                         stringWithFormat:@"%@ %@", kFamilyPickerInfoButtonID,
                                          @"user4@gmail.com"]),
                     grey_kindOfClass([UIButton class]), nil)]
      performAction:grey_tap()];

  // Check that the info popup about ineligibility is visible.
  [[EarlGrey
      selectElementWithMatcher:
          grey_text(ParseStringWithLinks(
                        l10n_util::GetNSString(
                            IDS_IOS_PASSWORD_SHARING_FAMILY_MEMBER_INELIGIBLE))
                        .string)] assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testTappingCancelInFirstRunExperienceView {
  DISABLE_ON_IPAD_WITH_IOS_17
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:prefs::kPasswordSharingFlowHasBeenEntered];

  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasswordToProfileStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      performAction:grey_tap()];

  // Tap the cancel button.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kConfirmationAlertSecondaryActionAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Check that the current view is the password details view.
  [[EarlGrey selectElementWithMatcher:PasswordDetailsTableViewMatcher()]
      assertWithMatcher:grey_notNil()];

  // Tap the share button again and verify that the first run view is still
  // displayed since it was not acknowledged.
  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PasswordSharingFirstRunMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testTappingShareInFirstRunExperienceView {
  DISABLE_ON_IPAD_WITH_IOS_17
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:prefs::kPasswordSharingFlowHasBeenEntered];

  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasswordToProfileStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      performAction:grey_tap()];

  // Tap the share button in the first run experience view.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kConfirmationAlertPrimaryActionAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Check that the current view is the family picker view.
  [[EarlGrey selectElementWithMatcher:FamilyPickerTableViewMatcher()]
      assertWithMatcher:grey_notNil()];

  // Tap the cancel button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFamilyPickerCancelButtonID)]
      performAction:grey_tap()];

  // Tap the share button in password details view and verify that the first run
  // view will not be displayed anymore since it was acknowledged.
  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:FamilyPickerTableViewMatcher()]
      assertWithMatcher:grey_notNil()];
}

- (void)testFirstRunExperienceViewDismissedForAuthentication {
  DISABLE_ON_IPAD_WITH_IOS_17
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:prefs::kPasswordSharingFlowHasBeenEntered];

  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasswordToProfileStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordSharingFirstRunMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Background then foreground app so reauthentication UI is displayed.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Check that first run experience is gone and password details is visible.
  [[EarlGrey selectElementWithMatcher:PasswordSharingFirstRunMatcher()]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordDetailsViewControllerID)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testFamilyPickerViewDismissedForAuthentication {
  DISABLE_ON_IPAD_WITH_IOS_17
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasswordToProfileStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:FamilyPickerTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Background then foreground app so reauthentication UI is displayed.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Check that the family picker is gone and password details is visible.
  [[EarlGrey selectElementWithMatcher:FamilyPickerTableViewMatcher()]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordDetailsViewControllerID)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testPasswordPickerViewDismissedForAuthentication {
  DISABLE_ON_IPAD_WITH_IOS_17
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [self saveExamplePasswordsToProfileStoreAndOpenDetails];

  [[EarlGrey selectElementWithMatcher:PasswordDetailsShareButtonMatcher()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordPickerViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Background then foreground app so reauthentication UI is displayed.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Check that the password picker is gone and password details is visible.
  [[EarlGrey selectElementWithMatcher:PasswordPickerViewMatcher()]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordDetailsViewControllerID)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
