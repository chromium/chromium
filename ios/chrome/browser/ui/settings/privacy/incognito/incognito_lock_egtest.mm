// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/features.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::SettingsMenuPrivacyButton;

// Returns a matcher for the do not hide cell.
id<GREYMatcher> doNotHideCellMatcher() {
  return grey_allOf(grey_accessibilityID(kSettingsIncognitoLockDoNotHideCellId),
                    grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the hide with soft lock cell.
id<GREYMatcher> hideWithSoftLockCellMatcher() {
  return grey_allOf(
      grey_accessibilityID(kSettingsIncognitoLockHideWithSoftLockCellId),
      grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the hide with reauth cell.
id<GREYMatcher> hideWithReauthCellMatcher() {
  return grey_allOf(
      grey_accessibilityID(kSettingsIncognitoLockHideWithReauthCellId),
      grey_sufficientlyVisible(), nil);
}

// Returns matcher for an element with or without the
// UIAccessibilityTraitSelected accessibility trait depending on `selected`.
id<GREYMatcher> elementIsSelectedMatcher(bool selected) {
  return selected
             ? grey_accessibilityTrait(UIAccessibilityTraitSelected)
             : grey_not(grey_accessibilityTrait(UIAccessibilityTraitSelected));
}

// Asserts if the IOS.IncognitoLockSettingInteraction histogram for bucket of
// `action` was logged once.
void ExpectIncognitoLockSettingInteractionHistogram(
    IncognitoLockSettingInteraction action) {
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:static_cast<int>(action)
          forHistogram:base::SysUTF8ToNSString(
                           kIncognitoLockSettingInteractionHistogram)],
      @"IOS.IncognitoLockSettingInteraction histogram for action %d was not "
      @"logged.",
      static_cast<int>(action));
}

}  // namespace

// Test Incognito lock settings page.
@interface IncognitoLockSettingTestCase : ChromeTestCase

@end

@implementation IncognitoLockSettingTestCase

- (void)setUp {
  [super setUp];
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];
}

- (void)tearDownHelper {
  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");
  [super tearDownHelper];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kIOSSoftLock);
  return config;
}

// Tests that the pref for the Incognito lock setting is changed on selection.
- (void)testChangeIncognitoLockSettingSelection {
  [self openIncognitoLockSettings];
  // Check default pref is hide with Soft Lock.
  GREYAssertTrue(
      [ChromeEarlGrey localStateBooleanPref:prefs::kIncognitoSoftLockSetting],
      @"Soft lock pref value is not true by default");
  GREYAssertFalse(
      [ChromeEarlGrey
          localStateBooleanPref:prefs::kIncognitoAuthenticationSetting],
      @"Biometric lock pref value is not false by default");

  // Select Do Not Hide option.
  [[EarlGrey selectElementWithMatcher:doNotHideCellMatcher()]
      performAction:grey_tap()];

  // Validate the local prefs have been updated.
  GREYAssertFalse(
      [ChromeEarlGrey localStateBooleanPref:prefs::kIncognitoSoftLockSetting],
      @"Failed to disable incognito lock with soft lock pref");
  GREYAssertFalse(
      [ChromeEarlGrey
          localStateBooleanPref:prefs::kIncognitoAuthenticationSetting],
      @"Failed to disable incognito lock with reauth pref");

  // Validate checkmark UI selection is updated.
  [[EarlGrey selectElementWithMatcher:doNotHideCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:hideWithSoftLockCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:hideWithReauthCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(false)];

  // Ensure interaction metric is correctly logged.
  ExpectIncognitoLockSettingInteractionHistogram(
      IncognitoLockSettingInteraction::kDoNotHideSelected);

  // Select Hide with Soft Lock option.
  [[EarlGrey selectElementWithMatcher:hideWithSoftLockCellMatcher()]
      performAction:grey_tap()];
  // Validate the local prefs have been updated.
  GREYAssertTrue(
      [ChromeEarlGrey localStateBooleanPref:prefs::kIncognitoSoftLockSetting],
      @"Failed to enable incognito soft lock with soft lock pref");
  GREYAssertFalse(
      [ChromeEarlGrey
          localStateBooleanPref:prefs::kIncognitoAuthenticationSetting],
      @"Failed to enable incognito soft lock with reauth pref");

  // Validate checkmark UI selection is updated.
  [[EarlGrey selectElementWithMatcher:doNotHideCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(false)];
  [[EarlGrey selectElementWithMatcher:hideWithSoftLockCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(true)];
  [[EarlGrey selectElementWithMatcher:hideWithReauthCellMatcher()]
      assertWithMatcher:elementIsSelectedMatcher(false)];

  // Ensure interaction metric is correctly logged.
  ExpectIncognitoLockSettingInteractionHistogram(
      IncognitoLockSettingInteraction::kHideWithSoftLockSelected);
}

#pragma mark - Helpers

- (void)openIncognitoLockSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [ChromeEarlGreyUI
      tapPrivacyMenuButton:ButtonWithAccessibilityLabelId(
                               IDS_IOS_INCOGNITO_LOCK_SETTING_NAME)];
}

@end
