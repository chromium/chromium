// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/i18n/number_formatting.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "build/branding_buildflags.h"
#import "components/policy/policy_constants.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "components/signin/ios/browser/features.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/unified_consent/pref_names.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/policy/model/policy_earl_grey_utils.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/elements_constants.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_app_interface.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_test_case_base.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/settings_app_interface.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
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
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

namespace {

// Returns matcher for UMA manage link.
id<GREYMatcher> ManageUMALinkMatcher() {
  return grey_allOf(grey_accessibilityLabel(@"Manage"),
                    grey_sufficientlyVisible(), nil);
}

}  // namespace

// Test first run stages without search engine choice
@interface FirstRunWithoutSearchEngineChoiceTestCase : FirstRunTestCaseBase

@end

@implementation FirstRunWithoutSearchEngineChoiceTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  if ([self isRunningTest:@selector
            (testHistorySyncShownWithEquallyWeightedButtons)]) {
    config.features_enabled.push_back(switches::kAlwaysLoadDeviceAccounts);
  }

  return config;
}

#pragma mark - Tests

// Tests that the sentinel is written at the end of the first run.
- (void)testFRESentinel {
  [ChromeEarlGrey removeFirstRunSentinel];
  GREYAssertFalse([ChromeEarlGrey hasFirstRunSentinel],
                  @"First Run Sentinel not removed");
  [ChromeEarlGreyUI waitForAppToIdle];
  // Skip sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoSecondaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoSecondaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Tests that the sentinel file has been created.
  GREYAssertTrue([ChromeEarlGrey hasFirstRunSentinel],
                 @"First Run Sentinel not created");
}

// Tests FRE with UMA default value and without sign-in.
- (void)testWithUMACheckedAndNoSignin {
  // Verify 2 steps FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Skip sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoSecondaryButtonMatcher()
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
  // Verify 2 steps FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Scroll down and open the UMA dialog.
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               ManageUMALinkMatcher(),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
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
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
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
  // Verify 2 steps FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Scroll down and open the UMA dialog.
  id<GREYMatcher> manageUMALinkMatcher =
      grey_allOf(ManageUMALinkMatcher(), grey_sufficientlyVisible(), nil);
  [[self elementInteractionWithGreyMatcher:manageUMALinkMatcher
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];

  // This wait is required because, on devices, EG-test may tap on the button
  // while it is sliding up, which cause the tap to misses the button.
  // Turn off UMA.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_allOf(chrome_test_util::TableViewSwitchCell(
                                     kImproveChromeItemAccessibilityIdentifier,
                                     /*is_toggled_on=*/YES,
                                     /*enabled=*/YES),
                                 grey_sufficientlyVisible(), nil)];

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
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_allOf(chrome_test_util::TableViewSwitchCell(
                                     kImproveChromeItemAccessibilityIdentifier,
                                     /*is_toggled_on=*/NO,
                                     /*enabled=*/YES),
                                 grey_sufficientlyVisible(), nil)];

  // Close UMA dialog.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  // Skip sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
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
// TODO(crbug.com/40073685): Test fails on official builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#define MAYBE_testUMAUncheckedAndCheckItAgain \
  DISABLED_testUMAUncheckedAndCheckItAgain
#else
#define MAYBE_testUMAUncheckedAndCheckItAgain testUMAUncheckedAndCheckItAgain
#endif
- (void)MAYBE_testUMAUncheckedAndCheckItAgain {
  // Verify 2 steps FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Scroll down and open the UMA dialog.
  id<GREYMatcher> manageUMALinkMatcher =
      grey_allOf(ManageUMALinkMatcher(), grey_sufficientlyVisible(), nil);
  [[self elementInteractionWithGreyMatcher:manageUMALinkMatcher
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
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
  [ChromeEarlGreyUI waitForAppToIdle];
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
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
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
  // Verify 2 steps FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Scroll down and open the UMA dialog.
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               ManageUMALinkMatcher(),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
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
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Accept sync.
  [self acceptSyncOrHistory];
  // Check that UMA is off.
  GREYAssertFalse(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly true by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  // Check sync is on.
  [[self class] dismissDefaultBrowser];
  [ChromeEarlGreyUI openSettingsMenu];
  [self verifySyncOrHistoryEnabled:YES];
}

// Tests FRE with UMA default value and with sign-in.
- (void)testWithUMACheckedAndSignin {
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 steps FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Accept sync.
  [self acceptSyncOrHistory];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  // Check sync is on.
  [[self class] dismissDefaultBrowser];
  [ChromeEarlGreyUI openSettingsMenu];
  [self verifySyncOrHistoryEnabled:YES];
}

// Tests FRE with UMA default value, with sign-in and no sync.
- (void)testWithUMACheckedAndSigninAndNoSync {
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Verify 2 steps FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Refuse sync.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
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
  [[self class] dismissDefaultBrowser];
  [ChromeEarlGreyUI openSettingsMenu];
  [self verifySyncOrHistoryEnabled:NO];
}

#pragma mark - Enterprise

// Tests FRE with disabled sign-in policy.
- (void)testSignInDisabledByPolicy {
  // Configure the policy to disable SignIn.
  [self relaunchAppWithBrowserSigninMode:BrowserSigninMode::kDisabled];
  // Verify 2 steps FRE with disabled sign-in policy.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentSigninDisabledByPolicy];
  // Accept FRE.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
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
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  GREYAssertTrue([SigninEarlGrey isIdentityAdded:fakeIdentity],
                 @"Identity not added by kSignInAtStartup flag, in "
                 @"`relaunchAppWithBrowserSigninMode:`, during the relaunch.");
  // Verify 2 steps FRE with forced sign-in policy.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentSigninForcedByPolicy];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Accept sync.
  [self acceptSyncOrHistory];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  // Check sync is on.
  [[self class] dismissDefaultBrowser];
  [ChromeEarlGreyUI openSettingsMenu];
  [self verifySyncOrHistoryEnabled:YES];
  // Close settings.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests forced sign-in policy, and refuse sync.
- (void)testForceSigninByPolicyWithoutSync {
  // Configure the policy to force sign-in.
  [self relaunchAppWithBrowserSigninMode:BrowserSigninMode::kForced];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  GREYAssertTrue([SigninEarlGrey isIdentityAdded:fakeIdentity],
                 @"Identity not added by kSignInAtStartup flag, in "
                 @"`relaunchAppWithBrowserSigninMode:`, during the relaunch.");
  // Verify 2 steps FRE with forced sign-in policy.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentSigninForcedByPolicy];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Refuse sync.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
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
  [[self class] dismissDefaultBrowser];
  [ChromeEarlGreyUI openSettingsMenu];
  [self verifySyncOrHistoryEnabled:NO];
  // Close settings.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests sign-in with sync disabled policy.
- (void)testSyncDisabledByPolicy {
  [self relaunchAppWithPolicyKey:policy::key::kSyncDisabled
                  xmlPolicyValue:"<true/>"];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  GREYAssertTrue(
      [SigninEarlGrey isIdentityAdded:fakeIdentity],
      @"Identity not added by kSignInAtStartup flag, in "
      @"`relaunchAppWithPolicyKey:xmlPolicyValue:`, during the relaunch.");
  // Verify 2 steps FRE with forced sign-in policy.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentSigninWithSyncDisabledPolicy];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
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
  [[self class] dismissDefaultBrowser];
  [ChromeEarlGreyUI openSettingsMenu];
  [self verifySyncOrHistoryEnabled:NO];
}

// Tests enterprise policy wording on FRE when incognito policy is set.
- (void)testIncognitoPolicy {
  // Configure the policy to force sign-in.
  [self relaunchAppWithPolicyKey:policy::key::kIncognitoModeAvailability
                  xmlPolicyValue:"<integer>1</integer>"];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  GREYAssertTrue(
      [SigninEarlGrey isIdentityAdded:fakeIdentity],
      @"Identity not added by kSignInAtStartup flag, in "
      @"`relaunchAppWithPolicyKey:xmlPolicyValue:`, during the relaunch.");
  // Verify 2 steps FRE with forced sign-in policy.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentSigninWithPolicy];
  // Refuse sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
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
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  GREYAssertTrue(
      [SigninEarlGrey isIdentityAdded:fakeIdentity],
      @"Identity not added by kSignInAtStartup flag, in "
      @"`relaunchAppWithPolicyKey:xmlPolicyValue:`, during the relaunch.");
  // Verify 2 steps FRE with no UMA footer.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentSigninWithUMAReportingDisabledPolicy];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Accept sync.
  [self acceptSyncOrHistory];
  // Check that UMA is off.
  GREYAssertFalse(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly true by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

#pragma mark - Supervised User

// TODO(crbug.com/40070867): This test is failing.
// Tests FRE with UMA default value and with sign-in for a supervised user.
- (void)DISABLED_testWithUMACheckedAndSigninSupervised {
  // Add a fake supervised identity to the device.
  FakeSystemIdentity* fakeSupervisedIdentity =
      [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeSupervisedIdentity
                 withCapabilities:@{
                   @(kIsSubjectToParentalControlsCapabilityName) : @YES,
                 }];

  // Verify 2 steps FRE.
  [self verifyEnterpriseWelcomeScreenIsDisplayedWithFRESigninIntent:
            FRESigninIntentRegular];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Accept sync.
  [self acceptSyncOrHistory];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeSupervisedIdentity];
  // Check sync is on.
  [[self class] dismissDefaultBrowser];
  [ChromeEarlGreyUI openSettingsMenu];
  [self verifySyncOrHistoryEnabled:YES];
}

// Tests that the History Sync Opt-In screen will have equally weighted button
// for users with minor mode restrictions.
- (void)testHistorySyncShownWithEquallyWeightedButtons {
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey
       addFakeIdentity:fakeIdentity
      withCapabilities:@{
        @(kCanShowHistorySyncOptInsWithoutMinorModeRestrictionsCapabilityName) :
            @NO,
      }];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [SigninEarlGrey verifyPrimaryAccountWithEmail:fakeIdentity.userEmail
                                        consent:signin::ConsentLevel::kSignin];
  // Verify that the History Sync Opt-In screen is shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Verify that the primary and secondary buttons have the same foreground and
  // background colors.
  NSString* foregroundColorName = kBlueColor;
  NSString* backgroundColorName = kBlueHaloColor;
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              chrome_test_util::ButtonWithForegroundColor(foregroundColorName),
              chrome_test_util::ButtonWithBackgroundColor(backgroundColorName),
              chrome_test_util::SigninScreenPromoPrimaryButtonMatcher(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              chrome_test_util::ButtonWithForegroundColor(foregroundColorName),
              chrome_test_util::ButtonWithBackgroundColor(backgroundColorName),
              chrome_test_util::SigninScreenPromoSecondaryButtonMatcher(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Accept History Sync.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Verify that latency metrics are recorded.
  GREYAssertNil([MetricsAppInterface
                    expectUniqueSampleWithCount:1
                                      forBucket:true
                                   forHistogram:@"Signin.AccountCapabilities."
                                                @"ImmediatelyAvailable"],
                @"Incorrect immediate availability histogram");
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:1
              forHistogram:@"Signin.AccountCapabilities.UserVisibleLatency"],
      @"Failed to record user visibile latency histogram");
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:0
              forHistogram:@"Signin.AccountCapabilities.FetchLatency"],
      @"Failed to record fetch latency histogram.");
  // Verify that History Sync buttons metrics are recorded.
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:
                                static_cast<int>(
                                    signin_metrics::SyncButtonsType::
                                        kHistorySyncEqualWeightedFromCapability)
                         forHistogram:@"Signin.SyncButtons.Shown"],
      @"Failed to record History Sync button type histogram.");
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:static_cast<int>(
                                          signin_metrics::SyncButtonClicked::
                                              kHistorySyncOptInEqualWeighted)
                         forHistogram:@"Signin.SyncButtons.Clicked"],
      @"Failed to record History Sync buttons clicked histogram.");
}

// Tests that the History Sync Opt-In screen will not have equally weighted
// button for users without minor mode restrictions.
- (void)testHistorySyncShownWithoutMinorModeRestrictions {
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey
       addFakeIdentity:fakeIdentity
      withCapabilities:@{
        @(kCanShowHistorySyncOptInsWithoutMinorModeRestrictionsCapabilityName) :
            @YES,
      }];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [SigninEarlGrey verifyPrimaryAccountWithEmail:fakeIdentity.userEmail
                                        consent:signin::ConsentLevel::kSignin];
  // Verify that the History Sync Opt-In screen is shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Verify that buttons have the expected colors.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::ButtonWithForegroundColor(
                         kSolidButtonTextColor),
                     chrome_test_util::ButtonWithBackgroundColor(kBlueColor),
                     chrome_test_util::SigninScreenPromoPrimaryButtonMatcher(),
                     nil)] assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              chrome_test_util::ButtonWithForegroundColor(kBlueColor),
              chrome_test_util::SigninScreenPromoSecondaryButtonMatcher(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Decline History Sync.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoSecondaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Verify that History Sync buttons metrics are recorded.
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:static_cast<int>(
                                          signin_metrics::SyncButtonsType::
                                              kHistorySyncNotEqualWeighted)
                         forHistogram:@"Signin.SyncButtons.Shown"],
      @"Failed to record History Sync button type histogram.");
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:
                                static_cast<int>(
                                    signin_metrics::SyncButtonClicked::
                                        kHistorySyncCancelNotEqualWeighted)
                         forHistogram:@"Signin.SyncButtons.Clicked"],
      @"Failed to record History Sync buttons clicked histogram.");
}

// Tests that the History Sync Opt-In screen will have equally weighted button
// for users with unknown minor mode restrictions status.
- (void)
    testHistorySyncShownWithEquallyWeightedButtonsOnCapabilitiesFetchTimeout {
  // Add identity without setting capabilities.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity withUnknownCapabilities:YES];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [SigninEarlGrey verifyPrimaryAccountWithEmail:fakeIdentity.userEmail
                                        consent:signin::ConsentLevel::kSignin];
  // Wait for the History Sync Opt-In screen.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kHistorySyncViewAccessibilityIdentifier)];
  // Verify that the title and subtitle are present.
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_HISTORY_SYNC_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_HISTORY_SYNC_SUBTITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Verify that the primary and secondary buttons have the same foreground and
  // background colors.
  NSString* foregroundColorName = kBlueColor;
  NSString* backgroundColorName = kBlueHaloColor;
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              chrome_test_util::ButtonWithForegroundColor(foregroundColorName),
              chrome_test_util::ButtonWithBackgroundColor(backgroundColorName),
              chrome_test_util::SigninScreenPromoPrimaryButtonMatcher(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              chrome_test_util::ButtonWithForegroundColor(foregroundColorName),
              chrome_test_util::ButtonWithBackgroundColor(backgroundColorName),
              chrome_test_util::SigninScreenPromoSecondaryButtonMatcher(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Accept History Sync.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Verify that latency metrics are recorded later for when the capability is
  // not immediately available.
  GREYAssertNil([MetricsAppInterface
                    expectUniqueSampleWithCount:1
                                      forBucket:false
                                   forHistogram:@"Signin.AccountCapabilities."
                                                @"ImmediatelyAvailable"],
                @"Incorrect immediate availability histogram");
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:1
              forHistogram:@"Signin.AccountCapabilities.UserVisibleLatency"],
      @"Failed to record user visibile latency histogram");
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:1
              forHistogram:@"Signin.AccountCapabilities.FetchLatency"],
      @"Fetch latency should not be recorded on immediate availability.");
  // Verify that History Sync buttons metrics are recorded.
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:
                                static_cast<int>(
                                    signin_metrics::SyncButtonsType::
                                        kHistorySyncEqualWeightedFromDeadline)
                         forHistogram:@"Signin.SyncButtons.Shown"],
      @"Failed to record History Sync button type histogram.");
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:static_cast<int>(
                                          signin_metrics::SyncButtonClicked::
                                              kHistorySyncOptInEqualWeighted)
                         forHistogram:@"Signin.SyncButtons.Clicked"],
      @"Failed to record History Sync buttons clicked histogram.");
}

// Tests that the History Sync Opt-In screen for users with unknown minor mode
// restrictions status, if declined, will have metrics correctly recorded.
- (void)
    testHistorySyncShownWithEquallyWeightedButtonsOnCapabilitiesFetchTimeoutThenDeclined {
  // Add identity without setting capabilities.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity withUnknownCapabilities:YES];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [SigninEarlGrey verifyPrimaryAccountWithEmail:fakeIdentity.userEmail
                                        consent:signin::ConsentLevel::kSignin];
  // Wait for the History Sync Opt-In screen.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kHistorySyncViewAccessibilityIdentifier)];
  // Decline History Sync.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoSecondaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Verify that latency metrics are recorded later for when the capability is
  // not immediately available.
  GREYAssertNil([MetricsAppInterface
                    expectUniqueSampleWithCount:1
                                      forBucket:false
                                   forHistogram:@"Signin.AccountCapabilities."
                                                @"ImmediatelyAvailable"],
                @"Incorrect immediate availability histogram");
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:1
              forHistogram:@"Signin.AccountCapabilities.UserVisibleLatency"],
      @"Failed to record user visibile latency histogram");
  GREYAssertNil(
      [MetricsAppInterface
          expectTotalCount:1
              forHistogram:@"Signin.AccountCapabilities.FetchLatency"],
      @"Fetch latency should not be recorded on immediate availability.");
  // Verify that History Sync buttons metrics are recorded.
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:
                                static_cast<int>(
                                    signin_metrics::SyncButtonsType::
                                        kHistorySyncEqualWeightedFromDeadline)
                         forHistogram:@"Signin.SyncButtons.Shown"],
      @"Failed to record History Sync button type histogram.");
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:static_cast<int>(
                                          signin_metrics::SyncButtonClicked::
                                              kHistorySyncCancelEqualWeighted)
                         forHistogram:@"Signin.SyncButtons.Clicked"],
      @"Failed to record History Sync buttons clicked histogram.");
}

#pragma mark - Sync UI Disabled

// Tests sign-in with FRE when there's no account on the device.
// See https://crbug.com/1471972.
- (void)testSignInWithNoAccount {
  // Add account.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentityForSSOAuthAddAccountFlow:fakeIdentity];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(
                                       kFakeAuthAddAccountButtonIdentifier),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Verify that the primary button text is correct.
  NSString* continueAsText = l10n_util::GetNSStringF(
      IDS_IOS_FIRST_RUN_SIGNIN_CONTINUE_AS,
      base::SysNSStringToUTF16(fakeIdentity.userGivenName));
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::SigninScreenPromoPrimaryButtonMatcher(),
                     grey_descendant(grey_text(continueAsText)), nil)]
      assertWithMatcher:grey_notNil()];
  // Sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Verify that the History Sync Opt-In screen is shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Check signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests if the user skip the Sign-in step, the History Sync Opt-in screen is
// skipped and the default browser screen is shown.
- (void)testHistorySyncSkipIfNoSignIn {
  // Skip sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedOut];
  // Verify that the History Sync Opt-In screen is hidden.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
  // Verify that the default browser screen is shown.
  [self verifyDefaultBrowserIsDisplayed];
}

// Tests if the user signs in with the first screen, the History Sync Opt-In
// screen is shown next.
- (void)testHistorySyncShownAfterSignIn {
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [SigninEarlGrey verifyPrimaryAccountWithEmail:fakeIdentity.userEmail
                                        consent:signin::ConsentLevel::kSignin];
  // Verify that the History Sync Opt-In screen is shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Verify that the footer is shown without the user's email.
  NSString* disclaimerText =
      l10n_util::GetNSString(IDS_IOS_HISTORY_SYNC_FOOTER_WITHOUT_EMAIL);
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               grey_text(disclaimerText),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      assertWithMatcher:grey_notNil()];
  // Verify that buttons have the expected colors.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::ButtonWithForegroundColor(
                         kSolidButtonTextColor),
                     chrome_test_util::ButtonWithBackgroundColor(kBlueColor),
                     chrome_test_util::SigninScreenPromoPrimaryButtonMatcher(),
                     nil)] assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              chrome_test_util::ButtonWithForegroundColor(kBlueColor),
              chrome_test_util::SigninScreenPromoSecondaryButtonMatcher(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Accept History Sync.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Verify that History Sync buttons metrics are recorded.
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:static_cast<int>(
                                          signin_metrics::SyncButtonsType::
                                              kHistorySyncNotEqualWeighted)
                         forHistogram:@"Signin.SyncButtons.Shown"],
      @"Failed to record History Sync button type histogram.");
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:static_cast<int>(
                                          signin_metrics::SyncButtonClicked::
                                              kHistorySyncOptInNotEqualWeighted)
                         forHistogram:@"Signin.SyncButtons.Clicked"],
      @"Failed to record History Sync buttons clicked histogram.");
}

// Tests that the correct subtitle is shown in the FRE sign-in screen if the
// History Sync Opt-In feature is enabled.
- (void)testSignInSubtitleIfHistorySyncOptInEnabled {
  // Verify that the first run screen is present.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSignInScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
  // Validate the subtitle text.
  NSString* subtitle =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_BENEFITS_SUBTITLE_SHORT);
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               grey_text(subtitle),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      assertWithMatcher:grey_notNil()];
}

// Tests that the standard subtitle is shown in the FRE sign-in screen, and that
// History Sync Opt-In screen is skipped, if the sync is disabled by policy, and
// History Sync Opt-In feature is enabled.
- (void)testHistorySyncSkipIfSyncDisabled {
  [self relaunchAppWithPolicyKey:policy::key::kSyncDisabled
                  xmlPolicyValue:"<true/>"];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  GREYAssertTrue(
      [SigninEarlGrey isIdentityAdded:fakeIdentity],
      @"Identity not added by kSignInAtStartup flag, in "
      @"`relaunchAppWithPolicyKey:xmlPolicyValue:`, during the relaunch.");
  // Verify that the first run screen is present.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSignInScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
  // Verify the subtitle text is the standard one.
  NSString* subtitle =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE_SHORT);
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               grey_text(subtitle),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      assertWithMatcher:grey_notNil()];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [SigninEarlGrey verifyPrimaryAccountWithEmail:fakeIdentity.userEmail
                                        consent:signin::ConsentLevel::kSignin];
  // Verify that the History Sync Opt-In screen is hidden.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
  // Verify that the default browser screen is shown.
  [self verifyDefaultBrowserIsDisplayed];
}

// Tests that the standard subtitle is shown in the FRE sign-in screen, and that
// History Sync Opt-In screen is skipped, in case the tabs sync is disabled by
// policy, and History Sync Opt-In feature is enabled.
- (void)testHistorySyncSkipIfTabsSyncDisabled {
  [self relaunchAppWithPolicyKey:policy::key::kSyncTypesListDisabled
                  xmlPolicyValue:"<array><string>tabs</string></array>"];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  GREYAssertTrue(
      [SigninEarlGrey isIdentityAdded:fakeIdentity],
      @"Identity not added by kSignInAtStartup flag, in "
      @"`relaunchAppWithPolicyKey:xmlPolicyValue:`, during the relaunch.");
  // Verify that the first run screen is present.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSignInScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
  // Verify the subtitle text is the standard one.
  NSString* subtitle =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE_SHORT);
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               grey_text(subtitle),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      assertWithMatcher:grey_notNil()];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [SigninEarlGrey verifyPrimaryAccountWithEmail:fakeIdentity.userEmail
                                        consent:signin::ConsentLevel::kSignin];
  // Verify that the History Sync Opt-In screen is hidden.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
  // Verify that the default browser screen is shown.
  [self verifyDefaultBrowserIsDisplayed];
}

// Tests that the standard subtitle is shown in the FRE sign-in screen, and
// that History Sync Opt-In screen is shown, in case only the bookmarks sync is
// disabled by policy, and History Sync Opt-In feature is enabled.
- (void)testHistorySyncShownIfBookmarksSyncDisabled {
  [self relaunchAppWithPolicyKey:policy::key::kSyncTypesListDisabled
                  xmlPolicyValue:"<array><string>bookmarks</string></array>"];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  GREYAssertTrue(
      [SigninEarlGrey isIdentityAdded:fakeIdentity],
      @"Identity not added by kSignInAtStartup flag, in "
      @"`relaunchAppWithPolicyKey:xmlPolicyValue:`, during the relaunch.");
  // Verify that the first run screen is present.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSignInScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
  // Verify the subtitle text is the standard one.
  NSString* subtitle =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE_SHORT);
  [[self elementInteractionWithGreyMatcher:grey_allOf(
                                               grey_text(subtitle),
                                               grey_sufficientlyVisible(), nil)
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      assertWithMatcher:grey_notNil()];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  [SigninEarlGrey verifyPrimaryAccountWithEmail:fakeIdentity.userEmail
                                        consent:signin::ConsentLevel::kSignin];
  // Verify that the History Sync Opt-In screen is shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that accepting History Sync enables the history sync, grants the
// history sync and MSBB consent.
- (void)testHistorySyncConsentGrantedAfterConfirm {
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Accept History Sync.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Verify that the default browser screen is shown.
  [self verifyDefaultBrowserIsDisplayed];
  // Verify that the history sync is enabled.
  GREYAssertTrue(
      [SigninEarlGrey
          isSelectedTypeEnabled:syncer::UserSelectableType::kHistory],
      @"History sync should be enabled.");
  GREYAssertTrue(
      [SigninEarlGrey isSelectedTypeEnabled:syncer::UserSelectableType::kTabs],
      @"Tabs sync should be enabled.");
  // TODO(crbug.com/40068130): Verify that sync consent is granted.
  // Verify that MSBB consent is granted.
  GREYAssertTrue(
      [ChromeEarlGrey
          userBooleanPref:unified_consent::prefs::
                              kUrlKeyedAnonymizedDataCollectionEnabled],
      @"MSBB consent was not granted.");
  // Verify that the identity is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that refusing History Sync keep the history syncdisabled, and does not
// grant the history sync and MSBB consent.
- (void)testHistorySyncConsentNotGrantedAfterReject {
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Refuse History Sync.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::PromoStyleSecondaryActionButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Verify that the default browser screen is shown.
  [self verifyDefaultBrowserIsDisplayed];
  // Verify that the history sync is disabled.
  GREYAssertFalse(
      [SigninEarlGrey
          isSelectedTypeEnabled:syncer::UserSelectableType::kHistory],
      @"History sync should be disabled.");
  GREYAssertFalse(
      [SigninEarlGrey isSelectedTypeEnabled:syncer::UserSelectableType::kTabs],
      @"Tabs sync should be disabled.");
  // TODO(crbug.com/40068130): Verify that sync consent is not granted.
  // Verify that MSBB consent is not granted.
  GREYAssertFalse(
      [ChromeEarlGrey
          userBooleanPref:unified_consent::prefs::
                              kUrlKeyedAnonymizedDataCollectionEnabled],
      @"MSBB consent should not be granted.");
  // Verify that the identity is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that the History Sync Opt-In screen contains the avatar of the
// signed-in user, and the correct background image for the avatar.
// TODO(crbug.com/365786558): Test flaky on simulator
- (void)DISABLED_testHistorySyncLayout {
  // Add identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Accept sign-in.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()
                      scrollViewIdentifier:
                          kPromoStyleScrollViewAccessibilityIdentifier]
      performAction:grey_tap()];
  // Verify that the History Sync Opt-In screen is shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kHistorySyncViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Verify that the user's avatar is shown.
  NSString* avatarLabel =
      [NSString stringWithFormat:@"%@ %@", fakeIdentity.userFullName,
                                 fakeIdentity.userEmail];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(avatarLabel)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Verify that the avatar background is shown.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_accessibilityID(
                  kPromoStyleHeaderViewBackgroundAccessibilityIdentifier),
              chrome_test_util::ImageViewWithImageNamed(
                  IsNewSyncOptInIllustration()
                      ? @"sync_opt_in_background"
                      : @"history_sync_opt_in_background"),
              grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

@end
