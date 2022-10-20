// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/signin/ios/browser/features.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/first_run/field_trial_constants.h"
#import "ios/chrome/browser/ui/first_run/first_run_app_interface.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
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

// Returns a matcher for the welcome screen accept button.
id<GREYMatcher> GetAcceptButton() {
  return grey_allOf(grey_text(l10n_util::GetNSString(
                        IDS_IOS_FIRST_RUN_WELCOME_SCREEN_ACCEPT_BUTTON)),
                    grey_sufficientlyVisible(), nil);
}

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

// Checks that the welcome screen is displayed.
void VerifyWelcomeScreenIsDisplayed() {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunWelcomeScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Returns GREYElementInteraction for `matcher`, using `scrollViewMatcher` to
// scroll.
GREYElementInteraction* ElementInteractionWithGreyMatcher(
    id<GREYMatcher> matcher,
    NSString* scrollViewIdentifier) {
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

}  // namespace

// Test first run with UMA dialog MICe FRE. Those tests are only related to the
// new UMA dialog. The tests for the rest of the features are in
// FirstRunTestCase.
@interface FirstRunUMADialogTestCase : ChromeTestCase

@end

@implementation FirstRunUMADialogTestCase

- (void)setUp {
  [[self class] testForStartup];
  [super setUp];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kSignInAtStartup);
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
  // Verify FRE.
  VerifyWelcomeScreenIsDisplayed();
  // Accept welcome screen.
  [ElementInteractionWithGreyMatcher(
      GetAcceptButton(), kPromoStyleScrollViewAccessibilityIdentifier)
      performAction:grey_tap()];
  // Skip sign-in.
  [ElementInteractionWithGreyMatcher(
      PromoStyleSecondaryActionButtonMatcher(),
      kPromoStyleScrollViewAccessibilityIdentifier) performAction:grey_tap()];
  // Check that UMA is on.
  GREYAssertTrue(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly false by default.");
  // Check signed out.
  [SigninEarlGrey verifySignedOut];
}

// Tests FRE with UMA off and without sign-in.
- (void)testWithUMAUncheckedAndNoSignin {
  // Verify FRE.
  VerifyWelcomeScreenIsDisplayed();
  // Scroll down and open the UMA dialog.
  [ElementInteractionWithGreyMatcher(
      grey_allOf(ManageUMALinkMatcher(), grey_sufficientlyVisible(), nil),
      kPromoStyleScrollViewAccessibilityIdentifier) performAction:grey_tap()];
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
  // Accept welcome screen.
  [ElementInteractionWithGreyMatcher(
      GetAcceptButton(), kPromoStyleScrollViewAccessibilityIdentifier)
      performAction:grey_tap()];
  // Skip sign-in.
  [ElementInteractionWithGreyMatcher(
      PromoStyleSecondaryActionButtonMatcher(),
      kPromoStyleScrollViewAccessibilityIdentifier) performAction:grey_tap()];
  // Check that UMA is off.
  GREYAssertFalse(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly true by default.");
  // Check signed out.
  [SigninEarlGrey verifySignedOut];
}

// Tests FRE with UMA off, reopen UMA dialog and close the FRE without sign-in.
- (void)testUMAUncheckedWhenOpenedSecondTime {
  // Verify FRE.
  VerifyWelcomeScreenIsDisplayed();
  // Scroll down and open the UMA dialog.
  id<GREYMatcher> manageUMALinkMatcher =
      grey_allOf(ManageUMALinkMatcher(), grey_sufficientlyVisible(), nil);
  [ElementInteractionWithGreyMatcher(
      manageUMALinkMatcher, kPromoStyleScrollViewAccessibilityIdentifier)
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
  [ElementInteractionWithGreyMatcher(
      manageUMALinkMatcher, kPromoStyleScrollViewAccessibilityIdentifier)
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
  // Accept welcome screen.
  [ElementInteractionWithGreyMatcher(
      GetAcceptButton(), kPromoStyleScrollViewAccessibilityIdentifier)
      performAction:grey_tap()];
  // Skip sign-in.
  [ElementInteractionWithGreyMatcher(
      PromoStyleSecondaryActionButtonMatcher(),
      kPromoStyleScrollViewAccessibilityIdentifier) performAction:grey_tap()];
  // Check that UMA is off.
  GREYAssertFalse(
      [FirstRunAppInterface isUMACollectionEnabled],
      @"kMetricsReportingEnabled pref was unexpectedly true by default.");
  // Check signed out.
  [SigninEarlGrey verifySignedOut];
}

// Tests to turn off UMA, and open the UMA dialog to turn it back on.
- (void)testUMAUncheckedAndCheckItAgain {
  // Verify FRE.
  VerifyWelcomeScreenIsDisplayed();
  // Scroll down and open the UMA dialog.
  id<GREYMatcher> manageUMALinkMatcher =
      grey_allOf(ManageUMALinkMatcher(), grey_sufficientlyVisible(), nil);
  [ElementInteractionWithGreyMatcher(
      manageUMALinkMatcher, kPromoStyleScrollViewAccessibilityIdentifier)
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
  [ElementInteractionWithGreyMatcher(
      manageUMALinkMatcher, kPromoStyleScrollViewAccessibilityIdentifier)
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
  // Accept welcome screen.
  [ElementInteractionWithGreyMatcher(
      GetAcceptButton(), kPromoStyleScrollViewAccessibilityIdentifier)
      performAction:grey_tap()];
  // Skip sign-in.
  [ElementInteractionWithGreyMatcher(
      PromoStyleSecondaryActionButtonMatcher(),
      kPromoStyleScrollViewAccessibilityIdentifier) performAction:grey_tap()];
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
  // Verify FRE.
  VerifyWelcomeScreenIsDisplayed();
  // Scroll down and open the UMA dialog.
  [ElementInteractionWithGreyMatcher(
      grey_allOf(ManageUMALinkMatcher(), grey_sufficientlyVisible(), nil),
      kPromoStyleScrollViewAccessibilityIdentifier) performAction:grey_tap()];
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
  // Accept welcome screen.
  [ElementInteractionWithGreyMatcher(
      GetAcceptButton(), kPromoStyleScrollViewAccessibilityIdentifier)
      performAction:grey_tap()];
  // Accept sign-in.
  [ElementInteractionWithGreyMatcher(
      PromoStylePrimaryActionButtonMatcher(),
      kPromoStyleScrollViewAccessibilityIdentifier) performAction:grey_tap()];
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
  // Verify FRE.
  VerifyWelcomeScreenIsDisplayed();
  // Accept welcome screen.
  [ElementInteractionWithGreyMatcher(
      GetAcceptButton(), kPromoStyleScrollViewAccessibilityIdentifier)
      performAction:grey_tap()];
  // Accept sign-in.
  [ElementInteractionWithGreyMatcher(
      PromoStylePrimaryActionButtonMatcher(),
      kPromoStyleScrollViewAccessibilityIdentifier) performAction:grey_tap()];
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

@end
