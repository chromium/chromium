// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/public/utils.h"
#import "ios/chrome/browser/safari_data_import/test/safari_data_import_earl_grey_ui.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

/// Tests for Safari data import.
@interface SafariDataImportTestCase : ChromeTestCase
@end

@implementation SafariDataImportTestCase

/// Tests that the Safari import entry point is dieplayed on first run and on
/// reminder.
- (void)testFirstRunAndReminder {
  [[self class] testForStartup];
  AppLaunchConfiguration firstRunConfig = [self appConfigurationForTestCase];
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
  id<GREYAction> searchAction = grey_scrollInDirection(kGREYDirectionDown, 200);
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
  AppLaunchConfiguration iphConfig = [self appConfigurationForTestCase];
  iphConfig.iph_feature_enabled = "IPH_iOSSafariImportFeature";
  iphConfig.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithConfiguration:iphConfig];
  DismissSafariDataImportEntryPoint(/*verify_visibility=*/YES);
}

/// Tests that the entry point to import Safari data can be triggered through
/// Settings.
- (void)testShowEntryPointInSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [[[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kSettingsSafariDataImportSettingsCellId)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 150)
      onElementWithMatcher:grey_allOf(
                               grey_accessibilityID(kSettingsTableViewId),
                               grey_interactable(), nil)]
      performAction:grey_tap()];
  DismissSafariDataImportEntryPoint(/*verify_visibility=*/YES);
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSettingsTableViewId)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
