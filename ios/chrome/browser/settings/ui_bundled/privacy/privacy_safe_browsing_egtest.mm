// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/authentication/test/signin_matchers.h"
#import "ios/chrome/browser/popup_menu/ui_bundled/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/privacy_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/safe_browsing/safe_browsing_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuPrivacyButton;
using chrome_test_util::StaticTextWithAccessibilityLabelId;
using chrome_test_util::WindowWithNumber;
using l10n_util::GetNSString;

namespace {

// Waits until the warning alert is shown.
[[nodiscard]] bool WaitForWarningAlert(NSString* alertMessage) {
  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        NSError* error = nil;
        [[EarlGrey selectElementWithMatcher:grey_text(alertMessage)]
            assertWithMatcher:grey_notNil()
                        error:&error];
        return (error == nil);
      });
}

// Returns GREYElementInteraction for `matcher`, using `scrollViewMatcher` to
// scroll.
GREYElementInteraction* ElementInteractionWithGreyMatcher(
    id<GREYMatcher> matcher,
    id<GREYMatcher> scrollViewMatcher) {
  // Needs to scroll slowly to make sure to not miss a cell if it is not
  // currently on the screen. It should not be bigger than the visible part
  // of the collection view.
  const CGFloat kPixelsToScroll = 300;
  id<GREYAction> searchAction =
      grey_scrollInDirection(kGREYDirectionDown, kPixelsToScroll);
  return [[EarlGrey selectElementWithMatcher:matcher]
         usingSearchAction:searchAction
      onElementWithMatcher:scrollViewMatcher];
}

// Opens privacy safe browsing settings.
void OpenPrivacySafeBrowsingSettings() {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [ChromeEarlGreyUI
      tapPrivacyMenuButton:ButtonWithAccessibilityLabelId(
                               IDS_IOS_PRIVACY_SAFE_BROWSING_TITLE)];
}

// Open privacy safe browsing settings in the window with the given number.
void OpenPrivacySafeBrowsingSettingsInWindowWithNumber(int windowNumber) {
  [ChromeEarlGrey openSettingsInWindowWithNumber:windowNumber];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [ChromeEarlGreyUI
      tapPrivacyMenuButton:ButtonWithAccessibilityLabelId(
                               IDS_IOS_PRIVACY_SAFE_BROWSING_TITLE)];
}

// Opens "i" button for a specific cell identifier.
void PressInfoButtonForCell(NSString* cellId) {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_ancestor(grey_accessibilityID(cellId)),
                                   grey_accessibilityID(
                                       kTableViewCellInfoButtonViewId),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
}

}  // namespace

// Integration tests using the Privacy Safe Browsing settings screen.
@interface PrivacySafeBrowsingTestCase : ChromeTestCase {
  // The default value for SafeBrowsingEnabled pref.
  BOOL _safeBrowsingEnabledPrefDefault;
}
@end

@implementation PrivacySafeBrowsingTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // TODO: crbug.com/336547987 - Remove when this is fully deployed.
  config.features_enabled.push_back(
      safe_browsing::kExtendedReportingRemovePrefDependencyIos);
  // TODO: crbug.com/444244681 - Remove this and tests when fully deployed.
  config.features_enabled.push_back(
      safe_browsing::kMovePasswordLeakDetectionToggleIos);
  return config;
}

- (void)setUp {
  [super setUp];
  // Ensure that Safe Browsing opt-out starts in its default (opted-in) state.
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kSafeBrowsingEnabled];
  // Ensure that Enhanced Safe Browsing opt-in starts in its default (opted-out)
  // state.
  [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kSafeBrowsingEnhanced];
}

- (void)tearDownHelper {
  // Reset preferences back to default values.
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kSafeBrowsingEnabled];
  [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kSafeBrowsingEnhanced];
  [super tearDownHelper];
}

- (void)testOpenPrivacySafeBrowsingSettings {
  OpenPrivacySafeBrowsingSettings();
}

- (void)testEachSafeBrowsingOption {
  OpenPrivacySafeBrowsingSettings();

  // Presses each of the Safe Browsing options.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kSettingsSafeBrowsingEnhancedProtectionCellId)]
      performAction:grey_tap()];
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnhanced],
                 @"Failed to toggle-on Enhanced Safe Browsing");

  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kSettingsSafeBrowsingStandardProtectionCellId),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  GREYAssertFalse([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnhanced],
                  @"Failed to toggle-off Enhanced Safe Browsing");
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnabled],
                 @"Failed to toggle-on Standard Safe Browsing");

  // Taps "No Protection" and then the Cancel button on pop-up.
  [ChromeEarlGreyUI tapPrivacySafeBrowsingMenuButton:
                        grey_allOf(grey_accessibilityID(
                                       kSettingsSafeBrowsingNoProtectionCellId),
                                   grey_sufficientlyVisible(), nil)];
  GREYAssert(
      WaitForWarningAlert(l10n_util::GetNSString(
          IDS_IOS_SAFE_BROWSING_NO_PROTECTION_CONFIRMATION_DIALOG_CONFIRM)),
      @"The No Protection pop-up did not show up");
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ActionSheetItemWithAccessibilityLabelId(
                     IDS_CANCEL)] performAction:grey_tap()];
  GREYAssertFalse([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnhanced],
                  @"Failed to keep Enhanced Safe Browsing off");
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnabled],
                 @"Failed to keep Standard Safe Browsing on");

  [self turnOffSafeBrowsing];
}

- (void)testPrivacySafeBrowsingDoneButton {
  OpenPrivacySafeBrowsingSettings();
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

- (void)testPrivacySafeBrowsingSwipeDown {
  OpenPrivacySafeBrowsingSettings();

  // Check that Privacy Safe Browsing TableView is presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPrivacySafeBrowsingTableViewId)]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPrivacySafeBrowsingTableViewId)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that Settings has been dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPrivacySafeBrowsingTableViewId)]
      assertWithMatcher:grey_nil()];
}

// Tests UI and preference value updates between multiple windows.
- (void)testPrivacySafeBrowsingMultiWindow {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }
  if (@available(iOS 19.0, *)) {
    // TODO(crbug.com/427699033): Re-enable test on iOS 26.
    // Fails to interact with second window.
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 26.");
  }

  OpenPrivacySafeBrowsingSettingsInWindowWithNumber(0);

  // Open privacy safe browsing settings on second window and select enhanced
  // protection.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  OpenPrivacySafeBrowsingSettingsInWindowWithNumber(1);
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kSettingsSafeBrowsingEnhancedProtectionCellId)]
      performAction:grey_tap()];

  // Check that the second window updated the first window correctly by tapping
  // the same option. If updated correctly, tapping the enhanced protection
  // option again should show the enhanced protection table view.
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kSettingsSafeBrowsingEnhancedProtectionCellId),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kSafeBrowsingEnhancedProtectionScrollViewId)]
      assertWithMatcher:grey_notNil()];
}

// Tests that Enhanced Protection page can be navigated to and populated
// correctly.
- (void)testEnhancedProtectionSettingsPage {
  OpenPrivacySafeBrowsingSettings();
  PressInfoButtonForCell(kSettingsSafeBrowsingEnhancedProtectionCellId);

  // First section.
  [ElementInteractionWithGreyMatcher(
      StaticTextWithAccessibilityLabelId(
          IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_WHEN_ON_HEADER),
      grey_accessibilityID(kSafeBrowsingEnhancedProtectionScrollViewId))
      assertWithMatcher:grey_notNil()];

  [ElementInteractionWithGreyMatcher(
      StaticTextWithAccessibilityLabelId(
          IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_DATA_ICON_DESCRIPTION),
      grey_accessibilityID(kSafeBrowsingEnhancedProtectionScrollViewId))
      assertWithMatcher:grey_notNil()];
  [ElementInteractionWithGreyMatcher(
      StaticTextWithAccessibilityLabelId(
          IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_DOWNLOAD_ICON_DESCRIPTION),
      grey_accessibilityID(kSafeBrowsingEnhancedProtectionScrollViewId))
      assertWithMatcher:grey_notNil()];
  [ElementInteractionWithGreyMatcher(
      StaticTextWithAccessibilityLabelId(
          IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_G_ICON_DESCRIPTION),
      grey_accessibilityID(kSafeBrowsingEnhancedProtectionScrollViewId))
      assertWithMatcher:grey_notNil()];
  [ElementInteractionWithGreyMatcher(
      StaticTextWithAccessibilityLabelId(
          IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_GLOBE_ICON_DESCRIPTION),
      grey_accessibilityID(kSafeBrowsingEnhancedProtectionScrollViewId))
      assertWithMatcher:grey_notNil()];
  [ElementInteractionWithGreyMatcher(
      StaticTextWithAccessibilityLabelId(
          IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_KEY_ICON_DESCRIPTION),
      grey_accessibilityID(kSafeBrowsingEnhancedProtectionScrollViewId))
      assertWithMatcher:grey_notNil()];

  // Second section.
  [ElementInteractionWithGreyMatcher(
      StaticTextWithAccessibilityLabelId(
          IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_THINGS_TO_CONSIDER_HEADER),
      grey_accessibilityID(kSafeBrowsingEnhancedProtectionScrollViewId))
      assertWithMatcher:grey_notNil()];

  [ElementInteractionWithGreyMatcher(
      StaticTextWithAccessibilityLabelId(
          IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_LINK_ICON_DESCRIPTION),
      grey_accessibilityID(kSafeBrowsingEnhancedProtectionScrollViewId))
      assertWithMatcher:grey_notNil()];

  [ElementInteractionWithGreyMatcher(
      StaticTextWithAccessibilityLabelId(
          IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_ACCOUNT_ICON_DESCRIPTION),
      grey_accessibilityID(kSafeBrowsingEnhancedProtectionScrollViewId))
      assertWithMatcher:grey_notNil()];
  [ElementInteractionWithGreyMatcher(
      StaticTextWithAccessibilityLabelId(
          IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_PERFORMANCE_ICON_DESCRIPTION),
      grey_accessibilityID(kSafeBrowsingEnhancedProtectionScrollViewId))
      assertWithMatcher:grey_notNil()];

  // Footer.
  NSString* footerString =
      ParseStringWithLinks(
          l10n_util::GetNSString(
              IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_FOOTER))
          .string;
  [ElementInteractionWithGreyMatcher(
      grey_text(footerString),
      grey_accessibilityID(kSafeBrowsingEnhancedProtectionScrollViewId))
      assertWithMatcher:grey_notNil()];
}

#pragma mark - Helpers

// Taps "No Protection" and then the "Turn Off" Button on pop-up.
- (void)turnOffSafeBrowsing {
  [ChromeEarlGreyUI tapPrivacySafeBrowsingMenuButton:
                        grey_allOf(grey_accessibilityID(
                                       kSettingsSafeBrowsingNoProtectionCellId),
                                   grey_sufficientlyVisible(), nil)];
  GREYAssert(
      WaitForWarningAlert(l10n_util::GetNSString(
          IDS_IOS_SAFE_BROWSING_NO_PROTECTION_CONFIRMATION_DIALOG_CONFIRM)),
      @"The No Protection pop-up did not show up");
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ActionSheetItemWithAccessibilityLabelId(
              IDS_IOS_SAFE_BROWSING_NO_PROTECTION_CONFIRMATION_DIALOG_CONFIRM)]
      performAction:grey_tap()];
  GREYAssertFalse([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnabled],
                  @"Failed to toggle-off Standard Safe Browsing");
}

@end

@interface SafeBrowsingExtendedReportingDeprecationDisabled : ChromeTestCase
@end
@implementation SafeBrowsingExtendedReportingDeprecationDisabled
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_disabled.push_back(
      safe_browsing::kExtendedReportingRemovePrefDependencyIos);
  return config;
}

- (void)testSBERCellIsPresent {
  OpenPrivacySafeBrowsingSettings();
  PressInfoButtonForCell(kSettingsSafeBrowsingStandardProtectionCellId);
  [ElementInteractionWithGreyMatcher(
      grey_accessibilityID(kSafeBrowsingExtendedReportingCellId),
      grey_accessibilityID(kSafeBrowsingStandardProtectionTableViewId))
      assertWithMatcher:grey_notNil()];
}
@end

@interface SafeBrowsingExtendedReportingDeprecationEnabled : ChromeTestCase
@end
@implementation SafeBrowsingExtendedReportingDeprecationEnabled
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // TODO: crbug.com/444243524 - Remove when this is fully deployed.
  config.features_enabled.push_back(
      safe_browsing::kExtendedReportingRemovePrefDependencyIos);
  // TODO: crbug.com/444244681 - Remove when this is fully deployed.
  config.features_enabled.push_back(
      safe_browsing::kMovePasswordLeakDetectionToggleIos);
  return config;
}

- (void)testSBERCellIsRemoved {
  OpenPrivacySafeBrowsingSettings();
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_ancestor(grey_accessibilityID(
                         kSettingsSafeBrowsingStandardProtectionCellId)),
                     grey_accessibilityID(kTableViewCellInfoButtonViewId), nil)]
      assertWithMatcher:grey_notVisible()];
}
@end

@interface SafeBrowsingPasswordLeakCheckToggleMoveDisabled : ChromeTestCase
@end
@implementation SafeBrowsingPasswordLeakCheckToggleMoveDisabled
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // TODO: crbug.com/444243524 - Remove after the SBER deprecation rolls out.
  config.features_enabled.push_back(
      safe_browsing::kExtendedReportingRemovePrefDependencyIos);
  // TODO: crbug.com/444244681 - Remove when this is fully deployed.
  config.features_disabled.push_back(
      safe_browsing::kMovePasswordLeakDetectionToggleIos);
  return config;
}

// Tests that the Password Leak detection toggle doesn't under Standard
// Protection if the the feature is enabled.
- (void)testPasswordLeakCheckToggle_PresentWhenFeatureFlagDisabled {
  // Ensure that Safe Browsing and password leak detection opt-outs start in
  // their default (opted-in) state.
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kSafeBrowsingEnabled];
  [ChromeEarlGrey
      setBoolValue:YES
       forUserPref:password_manager::prefs::kPasswordLeakDetectionEnabled];

  // Sign in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // Open Privacy Safe Browsing settings.
  OpenPrivacySafeBrowsingSettings();

  // Open Standard Protection menu.
  PressInfoButtonForCell(kSettingsSafeBrowsingStandardProtectionCellId);
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kSafeBrowsingStandardProtectionTableViewId)]
      assertWithMatcher:grey_notNil()];

  // Check that the password leak check toggle is both toggled on and enabled.
  [ElementInteractionWithGreyMatcher(
      chrome_test_util::TableViewSwitchCell(
          kSafeBrowsingStandardProtectionPasswordLeakCellId,
          /*is_toggled_on=*/YES,
          /*enabled=*/YES),
      grey_accessibilityID(kSafeBrowsingStandardProtectionTableViewId))
      assertWithMatcher:grey_notNil()];
}
@end

@interface SafeBrowsingPasswordLeakCheckToggleMoveEnabled : ChromeTestCase
@end
@implementation SafeBrowsingPasswordLeakCheckToggleMoveEnabled
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // TODO: crbug.com/444243524 - Remove when this is fully deployed.
  config.features_enabled.push_back(
      safe_browsing::kExtendedReportingRemovePrefDependencyIos);
  // TODO: crbug.com/444244681 - Remove when this is fully deployed.
  config.features_enabled.push_back(
      safe_browsing::kMovePasswordLeakDetectionToggleIos);
  return config;
}

- (void)testPasswordLeakCheckToggle_MissingWhenFeatureFlagEnabled {
  // Ensure that Safe Browsing and password leak detection opt-outs start in
  // their default (opted-in) state.
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kSafeBrowsingEnabled];
  [ChromeEarlGrey
      setBoolValue:YES
       forUserPref:password_manager::prefs::kPasswordLeakDetectionEnabled];

  // Sign in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // Open Privacy Safe Browsing settings.
  OpenPrivacySafeBrowsingSettings();

  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_ancestor(grey_accessibilityID(
                         kSettingsSafeBrowsingStandardProtectionCellId)),
                     grey_accessibilityID(kTableViewCellInfoButtonViewId), nil)]
      assertWithMatcher:grey_notVisible()];
}
@end
