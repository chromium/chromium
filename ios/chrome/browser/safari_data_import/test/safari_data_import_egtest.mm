// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/features.h"
#import "ios/chrome/browser/safari_data_import/test/safari_data_import_earl_grey_ui.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

/// Tests for Safari data import.
@interface SafariDataImportTestCase : ChromeTestCase
@end

@implementation SafariDataImportTestCase

/// Helper method creating an instance of AppLaunchConfiguration that enables
/// the Safari import feature by default. No other behavior is overridden.
- (AppLaunchConfiguration)appConfigurationNoOverrideBehavior {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kImportPasswordsFromSafari);
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  return config;
}

/// App configuration for this test suite, which ensures that the Safari import
/// entry point displays on app startup in non-first-run launches. This reduces
/// the time it takes for each test case by removing the need to go through FRE
/// screens or go into Settings.
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [self appConfigurationNoOverrideBehavior];
  config.additional_args.push_back("-NextPromoForDisplayOverride");
  config.additional_args.push_back(
      "promos_manager::Promo::SafariImportRemindMeLater");
  return config;
}

/// Tests that the Safari import entry point is displayed on first run and on
/// reminder.
- (void)testFirstRunAndReminder {
  if (@available(iOS 18.2, *)) {
    [[self class] testForStartup];
    AppLaunchConfiguration firstRunConfig =
        [self appConfigurationNoOverrideBehavior];
    /// Show the First Run UI at startup.
    firstRunConfig.additional_args.push_back("-FirstRunForceEnabled");
    firstRunConfig.additional_args.push_back("true");
    firstRunConfig.relaunch_policy = ForceRelaunchByCleanShutdown;
    [[AppLaunchManager sharedManager]
        ensureAppLaunchedWithConfiguration:firstRunConfig];
    /// Go through first run screens by tapping the secondary action twice
    /// (skipping default browser settings and sign-in.)
    id<GREYMatcher> buttonMatcher =
        grey_accessibilityID(kPromoStyleSecondaryActionAccessibilityIdentifier);
    id<GREYMatcher> scrollViewMatcher =
        grey_accessibilityID(kPromoStyleScrollViewAccessibilityIdentifier);
    id<GREYAction> searchAction =
        grey_scrollInDirection(kGREYDirectionDown, 200);
    for (int i = 0; i < 2; i++) {
      GREYElementInteraction* element =
          [[EarlGrey selectElementWithMatcher:buttonMatcher]
                 usingSearchAction:searchAction
              onElementWithMatcher:scrollViewMatcher];
      [element performAction:grey_tap()];
    }
    /// Verify the visibility of the entry point, and register reminder.
    SetReminderOnSafariDataImportEntryPoint();

    /// Restart in IPH demo mode. If the reminder is successfully registered, it
    /// will be displayed on restart.
    AppLaunchConfiguration iphConfig =
        [self appConfigurationNoOverrideBehavior];
    iphConfig.iph_feature_enabled = "IPH_iOSSafariImportFeature";
    iphConfig.relaunch_policy = ForceRelaunchByCleanShutdown;
    [[AppLaunchManager sharedManager]
        ensureAppLaunchedWithConfiguration:iphConfig];
    DismissSafariDataImportEntryPoint(/*verify_visibility=*/YES);
  }
}

/// Tests that the entry point to import Safari data can be triggered through
/// Settings.
- (void)testShowEntryPointInSettings {
  if (@available(iOS 18.2, *)) {
    /// Clean restart without experimental settings.
    [[AppLaunchManager sharedManager]
        ensureAppLaunchedWithConfiguration:
            [self appConfigurationNoOverrideBehavior]];
    [ChromeEarlGreyUI openSettingsMenu];
    [ChromeEarlGreyUI
        tapSettingsMenuButton:grey_accessibilityID(
                                  kSettingsSafariDataImportSettingsCellId)];
    /// Verify visibility and that the reminder button is not displaying.
    GREYAssertTrue(IsSafariDataImportEntryPointVisible(),
                   @"Safari data import workflow is not displayed.");
    [[EarlGrey selectElementWithMatcher:
                   grey_accessibilityID(
                       kConfirmationAlertTertiaryActionAccessibilityIdentifier)]
        assertWithMatcher:grey_nil()];
    /// Also verify that swipe would not be supported.
    [[EarlGrey selectElementWithMatcher:
                   grey_accessibilityID(
                       kConfirmationAlertTitleAccessibilityIdentifier)]
        performAction:grey_swipeSlowInDirection(kGREYDirectionDown)];
    DismissSafariDataImportEntryPoint(/*verify_visibility=*/YES);
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityID(kSettingsTableViewId)]
        assertWithMatcher:grey_sufficientlyVisible()];
  }
}

/// Tests that the entry point displays in full screen in landscape mode.
- (void)testFullscreenEntryPointIPhoneLandscapeMode {
  if (@available(iOS 18.2, *)) {
    if ([ChromeEarlGrey isIPadIdiom]) {
      EARL_GREY_TEST_SKIPPED(@"Inapplicable on iPad.");
    }
    /// Verify that NTP logo is visible before rotation.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
        assertWithMatcher:grey_sufficientlyVisible()];
    /// Rotate.
    [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                  error:nil];
    /// Verify that NTP logo is mostly hidden after rotation.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
        assertWithMatcher:grey_not(grey_sufficientlyVisible())];
    /// Verify that the user is still able to proceed in landscape mode.
    StartImportOnSafariDataImportEntryPoint();
    [[EarlGrey selectElementWithMatcher:
                   grey_accessibilityID(
                       kConfirmationAlertUnderTitleViewAccessibilityIdentifier)]
        assertWithMatcher:grey_minimumVisiblePercent(0.5f)];
  }
}

/// Tests that tapping the "Cancel" button on the export screen dismisses the
/// workflow in entirety.
- (void)testDismissFromExportScreen {
  if (@available(iOS 18.2, *)) {
    StartImportOnSafariDataImportEntryPoint();
    /// Swipe should not be supported.
    [[EarlGrey selectElementWithMatcher:
                   grey_accessibilityID(
                       kConfirmationAlertUnderTitleViewAccessibilityIdentifier)]
        performAction:grey_swipeSlowInDirection(kGREYDirectionDown)];
    [[EarlGrey selectElementWithMatcher:
                   grey_accessibilityID(
                       kConfirmationAlertUnderTitleViewAccessibilityIdentifier)]
        assertWithMatcher:grey_sufficientlyVisible()];
    /// Dismiss by tapping the "Cancel" button on the top right.
    id<GREYMatcher> buttonInNavBar = grey_allOf(
        grey_kindOfClass([UIButton class]),
        grey_ancestor(grey_kindOfClass([UINavigationBar class])), nil);
    [[EarlGrey selectElementWithMatcher:buttonInNavBar]
        performAction:grey_tap()];
    /// Verify that NTP logo is interactable, which means that the entry point
    /// is dismissed.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPLogo()]
        assertWithMatcher:grey_interactable()];
    GREYAssertFalse(IsSafariDataImportEntryPointVisible(),
                    @"Safari data import workflow is not fully dismissed.");
  }
}

@end
