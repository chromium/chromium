// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/i18n/number_formatting.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "components/signin/ios/browser/features.h"
#import "ios/chrome/browser/policy/policy_earl_grey_utils.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/first_run/field_trial_constants.h"
#import "ios/chrome/browser/ui/first_run/first_run_app_interface.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_google_chrome_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Type of FRE sign-in screen intent.
typedef NS_ENUM(NSUInteger, FRESigninIntent) {
  // FRE without enterprise policy.
  FRESigninIntentRegular,
  // FRE without forced sign-in policy.
  FRESigninIntentSigninForcedByPolicy,
  // FRE without disabled sign-in policy.
  FRESigninIntentSigninDisabledByPolicy,
  // FRE with an enterprise policy.
  FRESigninIntentSigninWithPolicy,
  // FRE with no UMA link in the first screen.
  FRESigninIntentSigninWithUMAReportingDisabledPolicy,
};

NSString* const kSyncPassphrase = @"hello";

// Returns matcher for the primary action button.
id<GREYMatcher> PromoStylePrimaryActionButtonMatcher() {
  return grey_accessibilityID(kPromoStylePrimaryActionAccessibilityIdentifier);
}

// Returns matcher for the secondary action button.
id<GREYMatcher> PromoStyleSecondaryActionButtonMatcher() {
  return grey_accessibilityID(
      kPromoStyleSecondaryActionAccessibilityIdentifier);
}

// Returns matcher for UMA manage link.
id<GREYMatcher> ManageUMALinkMatcher() {
  return grey_accessibilityLabel(@"Manage");
}

// Returns matcher for the button to open the Sync settings.
id<GREYMatcher> GetSyncSettings() {
  id<GREYMatcher> disclaimer =
      grey_accessibilityID(kPromoStyleDisclaimerViewAccessibilityIdentifier);
  return grey_allOf(grey_accessibilityLabel(@"settings"),
                    grey_ancestor(disclaimer), nil);
}

}  // namespace

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

#pragma mark - Tests

// Tests FRE with UMA default value and without sign-in.
- (void)testWithUMACheckedAndNoSignin {
  // Verify 2 step FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Skip sign-in.
  [[self
      elementInteractionWithGreyMatcher:PromoStyleSecondaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed out.
  [SigninEarlGrey verifySignedOut];
}

// Tests FRE with UMA off and without sign-in.
- (void)testWithUMAUncheckedAndNoSignin {
  // Verify 2 step FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Scroll down and open the UMA dialog.
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               ManageUMALinkMatcher(),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Turn off UMA.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kImproveChromeItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/YES,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  // Close UMA dialog.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  // Skip sign-in.
  [[self
      elementInteractionWithGreyMatcher:PromoStyleSecondaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is off.
  GREYAssertFalse(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly true by default.");
  // Check signed out.
  [SigninEarlGrey verifySignedOut];
}

// Tests FRE with UMA off, reopen UMA dialog and close the FRE without sign-in.
- (void)testUMAUncheckedWhenOpenedSecondTime {
  // Verify 2 step FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Scroll down and open the UMA dialog.
  id<GREYMatcher> manageUMALinkMatcher =
      grey_allOf(ManageUMALinkMatcher(), grey_sufficientlyVisible(), nil);
  [[self elementInteractionWithGreyMatcher:manageUMALinkMatcher
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Turn off UMA.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kImproveChromeItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/YES,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  // Close UMA dialog.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  // Open UMA dialog again.
  [[self elementInteractionWithGreyMatcher:manageUMALinkMatcher
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check UMA off.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kImproveChromeItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/NO,
                                   /*enabled=*/YES)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Close UMA dialog.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  // Skip sign-in.
  [[self
      elementInteractionWithGreyMatcher:PromoStyleSecondaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is off.
  GREYAssertFalse(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly true by default.");
  // Check signed out.
  [SigninEarlGrey verifySignedOut];
}

// Tests to turn off UMA, and open the UMA dialog to turn it back on.
- (void)testUMAUncheckedAndCheckItAgain {
  // Verify 2 step FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Scroll down and open the UMA dialog.
  id<GREYMatcher> manageUMALinkMatcher =
      grey_allOf(ManageUMALinkMatcher(), grey_sufficientlyVisible(), nil);
  [[self elementInteractionWithGreyMatcher:manageUMALinkMatcher
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Turn off UMA.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kImproveChromeItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/YES,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  // Close UMA dialog.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  // Open UMA dialog again.
  [[self elementInteractionWithGreyMatcher:manageUMALinkMatcher
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Turn UMA back on.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kImproveChromeItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/NO,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];
  // Close UMA dialog.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  // Skip sign-in.
  [[self
      elementInteractionWithGreyMatcher:PromoStyleSecondaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed out.
  [SigninEarlGrey verifySignedOut];
}

// Tests FRE with UMA off and without sign-in.
- (void)testWithUMAUncheckedAndSignin {
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Scroll down and open the UMA dialog.
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               ManageUMALinkMatcher(),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Turn off UMA.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kImproveChromeItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/YES,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  // Close UMA dialog.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  // Accept sign-in.
  [[self
      elementInteractionWithGreyMatcher:PromoStylePrimaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Accept sync.
  [[self
      elementInteractionWithGreyMatcher:PromoStylePrimaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is off.
  GREYAssertFalse(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly true by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  // Check sync is on.
  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySyncUIEnabled:YES];
}

// Tests FRE with UMA default value and with sign-in.
- (void)testWithUMACheckedAndSignin {
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Accept sign-in.
  [[self
      elementInteractionWithGreyMatcher:PromoStylePrimaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Accept sync.
  [[self
      elementInteractionWithGreyMatcher:PromoStylePrimaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  // Check sync is on.
  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySyncUIEnabled:YES];
}

// Tests FRE with UMA default value, with sign-in and no sync.
- (void)testWithUMACheckedAndSigninAndNoSync {
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Accept sign-in.
  [[self
      elementInteractionWithGreyMatcher:PromoStylePrimaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Refuse sync.
  [[self
      elementInteractionWithGreyMatcher:PromoStyleSecondaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  // Check sync is off.
  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySyncUIEnabled:NO];
}

// Tests accepting sync with 2 datatype disabled.
- (void)testAdvancedSettingsAndDisableTwoDataTypes {
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Accept sign-in.
  [[self
      elementInteractionWithGreyMatcher:PromoStylePrimaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Open advanced sync settings.
  [[EarlGrey selectElementWithMatcher:GetSyncSettings()]
      performAction:grey_tap()];
  // Turn off "Sync Everything".
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kSyncEverythingItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/YES,
                                   /*enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  // Turn off "Address and more".
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSyncAutofillIdentifier,
                                          /*is_toggled_on=*/YES,
                                          /*enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  // Turn off "Bookmarks".
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSyncBookmarksIdentifier,
                                          /*is_toggled_on=*/YES,
                                          /*enabled=*/YES)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  // Close the advanced sync settings.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AdvancedSyncSettingsDoneButtonMatcher()]
      performAction:grey_tap()];
  // Check sync did not start yet.
  GREYAssertFalse([FirstRunAppInterface isSyncFirstSetupComplete],
                  @"Sync shouldn't start when discarding advanced settings.");
  // Accept sync.
  [[self
      elementInteractionWithGreyMatcher:PromoStylePrimaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  // Check sync is on.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::ManageSyncSettingsButton()];
  // Check "Sync Everything" is off.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kSyncEverythingItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/NO,
                                   /*enabled=*/YES)]
      assertWithMatcher:grey_notNil()];
  // Check "Address and more" is off.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSyncAutofillIdentifier,
                                          /*is_toggled_on=*/NO,
                                          /*enabled=*/YES)]
      assertWithMatcher:grey_notNil()];
  // Check "Bookmarks" is off.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSyncBookmarksIdentifier,
                                          /*is_toggled_on=*/NO,
                                          /*enabled=*/YES)]
      assertWithMatcher:grey_notNil()];
}

// Tests sign-in in FRE with an identity that needs a sync passphrase.
- (void)testAdvancedSettingsWithSyncPassphrase {
  [ChromeEarlGrey addBookmarkWithSyncPassphrase:kSyncPassphrase];
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Accept sign-in.
  [[self
      elementInteractionWithGreyMatcher:PromoStylePrimaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Open advanced sync settings.
  [[EarlGrey selectElementWithMatcher:GetSyncSettings()]
      performAction:grey_tap()];
  // Select Encryption item.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::ButtonWithAccessibilityLabelId(
                 IDS_IOS_MANAGE_SYNC_ENCRYPTION)
                      scrollViewIdentifier:
                          kManageSyncTableViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [SigninEarlGreyUI submitSyncPassphrase:kSyncPassphrase];
  // Close the advanced sync settings.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AdvancedSyncSettingsDoneButtonMatcher()]
      performAction:grey_tap()];
  // Accept sync.
  [[self
      elementInteractionWithGreyMatcher:PromoStylePrimaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check sync is on.
  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySyncUIEnabled:YES];
}

#pragma mark - Enterprise

// Tests FRE with disabled sign-in policy.
- (void)testSignInDisabledByPolicy {
  // Configure the policy to disable SignIn.
  [self relaunchAppWithBrowserSigninMode:BrowserSigninMode::kDisabled];
  // Verify 2 step FRE with disabled sign-in policy.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentSigninDisabledByPolicy];
  // Accept FRE.
  [[self
      elementInteractionWithGreyMatcher:PromoStylePrimaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed out.
  [SigninEarlGrey verifySignedOut];
}

// Tests forced sign-in policy, and accept sync.
- (void)testForceSigninByPolicy {
  // Configure the policy to force sign-in.
  [self relaunchAppWithBrowserSigninMode:BrowserSigninMode::kForced];
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE with forced sign-in policy.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentSigninForcedByPolicy];
  // Accept sign-in.
  [[self
      elementInteractionWithGreyMatcher:PromoStylePrimaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Accept sync.
  [[self
      elementInteractionWithGreyMatcher:PromoStylePrimaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  // Check sync is on.
  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySyncUIEnabled:YES];
  // Close settings.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests forced sign-in policy, and refuse sync.
- (void)testForceSigninByPolicyWithoutSync {
  // Configure the policy to force sign-in.
  [self relaunchAppWithBrowserSigninMode:BrowserSigninMode::kForced];
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE with forced sign-in policy.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentSigninForcedByPolicy];
  // Accept sign-in.
  [[self
      elementInteractionWithGreyMatcher:PromoStylePrimaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Refuse sync.
  [[self
      elementInteractionWithGreyMatcher:PromoStyleSecondaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  // Check sync is on.
  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySyncUIEnabled:NO];
  // Close settings.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests sign-in with sync disabled policy.
- (void)testSyncDisabledByPolicy {
  [self relaunchAppWithPolicyKey:policy::key::kSyncDisabled
                  xmlPolicyValue:"<true/>"];
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE with forced sign-in policy.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentSigninWithPolicy];
  // Accept sign-in.
  [[self
      elementInteractionWithGreyMatcher:PromoStylePrimaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  // Check sync is on.
  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySyncUIEnabled:NO];
}

// Tests sign-in and no sync with forced policy.
- (void)testSigninWithOnlyBookmarkSyncDataTypeEnabled {
  // Configure the policy to force sign-in.
  [self relaunchAppWithPolicyKey:policy::key::kSyncTypesListDisabled
                  xmlPolicyValue:"<array><string>bookmarks</string></array>"];
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE with forced sign-in policy.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentSigninWithPolicy];
  // Accept sign-in.
  [[self
      elementInteractionWithGreyMatcher:PromoStylePrimaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Open advanced sync settings.
  [[EarlGrey selectElementWithMatcher:GetSyncSettings()]
      performAction:grey_tap()];
  // Check "Sync Everything" is off
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kSyncEverythingItemAccessibilityIdentifier),
                            grey_descendant(grey_text(
                                l10n_util::GetNSString(IDS_IOS_SETTING_OFF))),
                            nil)] assertWithMatcher:grey_notNil()];
  // Check "Bookmarks" is off
  [[EarlGrey selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                     kSyncBookmarksIdentifier),
                                                 grey_descendant(grey_text(
                                                     l10n_util::GetNSString(
                                                         IDS_IOS_SETTING_OFF))),
                                                 nil)]
      assertWithMatcher:grey_notNil()];
  // Close the advanced sync settings.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::AdvancedSyncSettingsDoneButtonMatcher()]
      performAction:grey_tap()];
  // Accept sync.
  [[self
      elementInteractionWithGreyMatcher:PromoStylePrimaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  // Check sync is on.
  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySyncUIEnabled:YES];
}

// Tests enterprise policy wording on FRE when incognito policy is set.
- (void)testIncognitoPolicy {
  // Configure the policy to force sign-in.
  [self relaunchAppWithPolicyKey:policy::key::kIncognitoModeAvailability
                  xmlPolicyValue:"<integer>1</integer>"];
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE with forced sign-in policy.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentSigninWithPolicy];
  // Refuse sign-in.
  [[self
      elementInteractionWithGreyMatcher:PromoStyleSecondaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed out.
  [SigninEarlGrey verifySignedOut];
}

// Tests that the UMA link does not appear in  FRE when UMA is disabled by
// enterprise policy.
- (void)testUMADisabledByPolicy {
  // Configure the policy to disable UMA.
  [self relaunchAppWithPolicyKey:policy::key::kMetricsReportingEnabled
                  xmlPolicyValue:"<false/>"];
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 step FRE with no UMA footer.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentSigninWithUMAReportingDisabledPolicy];
  // Accept sign-in.
  [[self
      elementInteractionWithGreyMatcher:PromoStylePrimaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Accept sync.
  [[self
      elementInteractionWithGreyMatcher:PromoStylePrimaryActionButtonMatcher()
                   scrollViewIdentifier:
                       kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Check that UMA is off.
  GREYAssertFalse(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly true by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

#pragma mark - Helper

- (void)relaunchAppWithBrowserSigninMode:(BrowserSigninMode)mode {
  std::string xmlPolicyValue("<integer>");
  xmlPolicyValue += std::to_string(static_cast<int>(mode));
  xmlPolicyValue += "</integer>";
  [self relaunchAppWithPolicyKey:policy::key::kBrowserSignin
                  xmlPolicyValue:xmlPolicyValue];
}

// Sets policy value and relaunches the app.
- (void)relaunchAppWithPolicyKey:(std::string)policyKey
                  xmlPolicyValue:(std::string)xmlPolicyValue {
  std::string policyData = std::string("<dict><key>") + policyKey + "</key>" +
                           xmlPolicyValue + "</dict>";
  // Configure the policy to force sign-in.
  AppLaunchConfiguration config = self.appConfigurationForTestCase;
  config.additional_args.push_back(
      "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
  config.additional_args.push_back(policyData);
  // Relaunch the app to take the configuration into account.
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

// Checks that the sign-in screen for enterprise is displayed.
- (void)verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
    (FRESigninIntent)FRESigninIntent {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSignInScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
  NSString* title = nil;
  NSString* subtitle = nil;
  NSArray* disclaimerStrings = nil;
  switch (FRESigninIntent) {
    case FRESigninIntentRegular:
      title = l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_TITLE);
      subtitle =
          l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE_SHORT);
      disclaimerStrings = @[
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TERMS_OF_SERVICE),
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_METRIC_REPORTING),
      ];
      break;
    case FRESigninIntentSigninForcedByPolicy:
      title =
          l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_TITLE_SIGNIN_FORCED);
      subtitle = l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE_SIGNIN_FORCED);
      disclaimerStrings = @[
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_BROWSER_MANAGED),
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TERMS_OF_SERVICE),
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_METRIC_REPORTING),
      ];
      break;
    case FRESigninIntentSigninDisabledByPolicy:
      if ([ChromeEarlGrey isIPadIdiom]) {
        title =
            l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_IPAD);
      } else {
        title = l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_IPHONE);
      }
      subtitle =
          l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_SUBTITLE);
      disclaimerStrings = @[
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_BROWSER_MANAGED),
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TERMS_OF_SERVICE),
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_METRIC_REPORTING),
      ];
      break;
    case FRESigninIntentSigninWithPolicy:
      title = l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_TITLE);
      subtitle =
          l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE_SHORT);
      disclaimerStrings = @[
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_BROWSER_MANAGED),
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TERMS_OF_SERVICE),
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_METRIC_REPORTING),
      ];
      break;
    case FRESigninIntentSigninWithUMAReportingDisabledPolicy:
      title = l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_TITLE);
      subtitle =
          l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE_SHORT);
      disclaimerStrings = @[
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_BROWSER_MANAGED),
        l10n_util::GetNSString(
            IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TERMS_OF_SERVICE),
      ];
      break;
  }
  // Validate the Title text.
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               grey_text(title),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      assertWithMatcher:grey_notNil()];
  // Validate the Subtitle text.
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               grey_text(subtitle),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      assertWithMatcher:grey_notNil()];
  // Validate the Managed text.
  [self verifyDisclaimerFooterWithStrings:disclaimerStrings];
}

// Checks the disclaimer footer with the list of strings. `strings` can contain
// "BEGIN_LINK" and "END_LINK" for URL tags.
- (void)verifyDisclaimerFooterWithStrings:(NSArray*)strings {
  NSString* disclaimerText = [strings componentsJoinedByString:@" "];
  // Remove URL tags.
  disclaimerText =
      [disclaimerText stringByReplacingOccurrencesOfString:@"BEGIN_LINK"
                                                withString:@""];
  disclaimerText =
      [disclaimerText stringByReplacingOccurrencesOfString:@"END_LINK"
                                                withString:@""];
  // Check the footer.
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               grey_text(disclaimerText),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      assertWithMatcher:grey_notNil()];
}

// Returns GREYElementInteraction for `matcher`, using `scrollViewMatcher` to
// scroll.
- (GREYElementInteraction*)
    elementInteractionWithGreyMatcher:(id<GREYMatcher>)matcher
                 scrollViewIdentifier:(NSString*)scrollViewIdentifier {
  id<GREYMatcher> scrollViewMatcher =
      grey_accessibilityID(scrollViewIdentifier);
  // Needs to scroll slowly to make sure to not miss a cell if it is not
  // currently on the screen. It should not be bigger than the visible part
  // of the collection view.
  id<GREYAction> searchAction = grey_scrollInDirection(kGREYDirectionDown, 200);
  return [[EarlGrey selectElementWithMatcher:matcher]
         usingSearchAction:searchAction
      onElementWithMatcher:scrollViewMatcher];
}

@end
