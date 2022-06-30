// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/signin/ios/browser/features.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/first_run/first_run_app_interface.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/ui/first_run/fre_field_trial.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns matcher for the secondary action button.
id<GREYMatcher> PromoStyleSecondaryActionButtonMatcher() {
  return grey_accessibilityID(
      kPromoStyleSecondaryActionAccessibilityIdentifier);
}

// Returns matcher for UMA manage link.
id<GREYMatcher> ManageUMALinkMatcher() {
  return grey_accessibilityLabel(@"Manage");
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

#pragma mark Tests

// Tests FRE with UMA default value and without sign-in.
- (void)testWithUMACheckedAndNoSignin {
  [self verifyWelcomeScreenIsDisplayed];
  // Skip sign-in.
  id<GREYMatcher> secondaryButtonMatcher =
      PromoStyleSecondaryActionButtonMatcher();
  [self scrollToElementAndAssertVisibility:secondaryButtonMatcher];
  [[EarlGrey selectElementWithMatcher:PromoStyleSecondaryActionButtonMatcher()]
      performAction:grey_tap()];
  // Check that UMA is ON.
  [self checkUMACheckboxValue:YES];
  // Check signed out.
  [SigninEarlGrey verifySignedOut];
}

// Tests FRE with UMA off and without sign-in.
- (void)testWithUMAUncheckedAndNoSignin {
  [self verifyWelcomeScreenIsDisplayed];
  // Scroll to and open the UMA dialog.
  id<GREYMatcher> manageUMALinkMatcher = ManageUMALinkMatcher();
  [self scrollToElementAndAssertVisibility:manageUMALinkMatcher];
  [[EarlGrey selectElementWithMatcher:manageUMALinkMatcher]
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
  id<GREYMatcher> secondaryButtonMatcher =
      PromoStyleSecondaryActionButtonMatcher();
  [self scrollToElementAndAssertVisibility:secondaryButtonMatcher];
  [[EarlGrey selectElementWithMatcher:PromoStyleSecondaryActionButtonMatcher()]
      performAction:grey_tap()];
  GREYAssertFalse(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly true by default.");
  // Check that UMA is OFF.
  [self checkUMACheckboxValue:NO];
  // Check signed out.
  [SigninEarlGrey verifySignedOut];
}

#pragma mark Helper

// Checks that the sign-in screen is displayed.
- (void)verifyWelcomeScreenIsDisplayed {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSignInScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Opens the Google services settings.
- (void)checkUMACheckboxValue:(BOOL)UMACheckboxValue {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::GoogleServicesSettingsButton()];
  id<GREYMatcher> scrollViewMatcher =
      grey_accessibilityID(kGoogleServicesSettingsViewIdentifier);
  [[EarlGrey selectElementWithMatcher:scrollViewMatcher]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                   kImproveChromeItemAccessibilityIdentifier,
                                   /*is_toggled_on=*/UMACheckboxValue,
                                   /*enabled=*/YES)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Scrolls down to |elementMatcher| in the scrollable content of the first run
// screen.
- (void)scrollToElementAndAssertVisibility:(id<GREYMatcher>)elementMatcher {
  id<GREYMatcher> scrollView =
      grey_accessibilityID(kPromoStyleScrollViewAccessibilityIdentifier);
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(elementMatcher,
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:scrollView] assertWithMatcher:grey_notNil()];
}

@end
