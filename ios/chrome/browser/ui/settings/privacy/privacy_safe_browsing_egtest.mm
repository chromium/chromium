// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_constants.h"
#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_constants.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
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
  // TODO (crbug.com/1285974) Remove when bug is resolved.
  config.features_disabled.push_back(kNewOverflowMenu);
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

- (void)tearDown {
  // Reset preferences back to default values.
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kSafeBrowsingEnabled];
  [ChromeEarlGrey setBoolValue:NO forUserPref:prefs::kSafeBrowsingEnhanced];
  [super tearDown];
}

- (void)testOpenPrivacySafeBrowsingSettings {
  [self openPrivacySafeBrowsingSettings];
}

- (void)testEachSafeBrowsingOption {
  [self openPrivacySafeBrowsingSettings];

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
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(IDS_CANCEL)]
      performAction:grey_tap()];
  GREYAssertFalse([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnhanced],
                  @"Failed to keep Enhanced Safe Browsing off");
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnabled],
                 @"Failed to keep Standard Safe Browsing on");

  [self turnOffSafeBrowsing];
}

- (void)testPrivacySafeBrowsingDoneButton {
  [self openPrivacySafeBrowsingSettings];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

- (void)testPrivacySafeBrowsingSwipeDown {
  [self openPrivacySafeBrowsingSettings];

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
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");

  [self openPrivacySafeBrowsingSettings];

  // Open privacy safe browsing settings on second window and select enhanced
  // protection.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];
  [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
  [self openPrivacySafeBrowsingSettings];
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
                                   kSafeBrowsingEnhancedProtectionTableViewId)]
      assertWithMatcher:grey_notNil()];
}

// Tests that Enhanced Protection page can be navigated to and populated
// correctly.
- (void)testEnhancedProtectionSettingsPage {
  [self openPrivacySafeBrowsingSettings];
  [self pressInfoButtonForCell:kSettingsSafeBrowsingEnhancedProtectionCellId];

  // Check that headers and footer exist.
  [[self elementInteractionWithGreyMatcher:
             grey_accessibilityID(
                 kSafeBrowsingEnhancedProtectionTableViewFirstHeaderId)
                         scrollViewMatcher:
                             grey_accessibilityID(
                                 kSafeBrowsingEnhancedProtectionTableViewId)]
      assertWithMatcher:grey_notNil()];
  [[self elementInteractionWithGreyMatcher:
             grey_accessibilityID(
                 kSafeBrowsingEnhancedProtectionTableViewSecondHeaderId)
                         scrollViewMatcher:
                             grey_accessibilityID(
                                 kSafeBrowsingEnhancedProtectionTableViewId)]
      assertWithMatcher:grey_notNil()];

  // Check that rows exist
  [[self elementInteractionWithGreyMatcher:
             grey_accessibilityID(kSafeBrowsingEnhancedProtectionDataCellId)
                         scrollViewMatcher:
                             grey_accessibilityID(
                                 kSafeBrowsingEnhancedProtectionTableViewId)]
      assertWithMatcher:grey_notNil()];
  [[self elementInteractionWithGreyMatcher:
             grey_accessibilityID(kSafeBrowsingEnhancedProtectionDownloadCellId)
                         scrollViewMatcher:
                             grey_accessibilityID(
                                 kSafeBrowsingEnhancedProtectionTableViewId)]
      assertWithMatcher:grey_notNil()];
  [[self elementInteractionWithGreyMatcher:
             grey_accessibilityID(kSafeBrowsingEnhancedProtectionLinkCellId)
                         scrollViewMatcher:
                             grey_accessibilityID(
                                 kSafeBrowsingEnhancedProtectionTableViewId)]
      assertWithMatcher:grey_notNil()];

  [[self elementInteractionWithGreyMatcher:
             grey_accessibilityID(kSafeBrowsingEnhancedProtectionAccountCellId)
                         scrollViewMatcher:
                             grey_accessibilityID(
                                 kSafeBrowsingEnhancedProtectionTableViewId)]
      assertWithMatcher:grey_notNil()];
  [[self
      elementInteractionWithGreyMatcher:
          grey_accessibilityID(kSafeBrowsingEnhancedProtectionTableViewFooterId)
                      scrollViewMatcher:
                          grey_accessibilityID(
                              kSafeBrowsingEnhancedProtectionTableViewId)]
      assertWithMatcher:grey_notNil()];

  [[self elementInteractionWithGreyMatcher:
             grey_accessibilityID(kSafeBrowsingEnhancedProtectionGIconCellId)
                         scrollViewMatcher:
                             grey_accessibilityID(
                                 kSafeBrowsingEnhancedProtectionTableViewId)]
      assertWithMatcher:grey_notNil()];
  [[self elementInteractionWithGreyMatcher:
             grey_accessibilityID(kSafeBrowsingEnhancedProtectionGlobeCellId)
                         scrollViewMatcher:
                             grey_accessibilityID(
                                 kSafeBrowsingEnhancedProtectionTableViewId)]
      assertWithMatcher:grey_notNil()];
  [[self elementInteractionWithGreyMatcher:
             grey_accessibilityID(kSafeBrowsingEnhancedProtectionKeyCellId)
                         scrollViewMatcher:
                             grey_accessibilityID(
                                 kSafeBrowsingEnhancedProtectionTableViewId)]
      assertWithMatcher:grey_notNil()];
}

// Tests that Standard Protection page can be navigated to and populated
// correctly.
- (void)testStandardProtectionSettingsPage {
  [self openPrivacySafeBrowsingSettings];
  [self pressInfoButtonForCell:kSettingsSafeBrowsingStandardProtectionCellId];
  [[self elementInteractionWithGreyMatcher:
             grey_accessibilityID(
                 kSafeBrowsingStandardProtectionPasswordLeakCellId)
                         scrollViewMatcher:
                             grey_accessibilityID(
                                 kSafeBrowsingStandardProtectionTableViewId)]
      assertWithMatcher:grey_notNil()];
  [[self elementInteractionWithGreyMatcher:
             grey_accessibilityID(kSafeBrowsingExtendedReportingCellId)
                         scrollViewMatcher:
                             grey_accessibilityID(
                                 kSafeBrowsingStandardProtectionTableViewId)]
      assertWithMatcher:grey_notNil()];
}

// Tests that password leak detection can only be toggled if Safe Browsing is
// enabled for signed in user.
- (void)testTogglePasswordLeakCheckForSignedInUser {
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
  [self openPrivacySafeBrowsingSettings];

  // Check that Safe Browsing is enabled, and toggle it off.
  GREYAssertFalse([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnhanced],
                  @"Failed to keep Enhanced Safe Browsing off");
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnabled],
                 @"Failed to keep Standard Safe Browsing on");
  [self turnOffSafeBrowsing];

  // Open Standard Protection menu.
  [self pressInfoButtonForCell:kSettingsSafeBrowsingStandardProtectionCellId];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kSafeBrowsingStandardProtectionTableViewId)]
      assertWithMatcher:grey_notNil()];

  // Check that the password leak check toggle is both toggled off and disabled.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::TableViewSwitchCell(
                 kSafeBrowsingStandardProtectionPasswordLeakCellId,
                 /*is_toggled_on=*/NO,
                 /*enabled=*/NO)
                         scrollViewMatcher:
                             grey_accessibilityID(
                                 kSafeBrowsingStandardProtectionTableViewId)]
      assertWithMatcher:grey_notNil()];

  // Toggle Safe Browsing on.
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kSafeBrowsingEnabled];

  // Check that the password leak check toggle is enabled, and toggle it off.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::TableViewSwitchCell(
                 kSafeBrowsingStandardProtectionPasswordLeakCellId,
                 /*is_toggled_on=*/YES,
                 /*enabled=*/YES)
                         scrollViewMatcher:
                             grey_accessibilityID(
                                 kSafeBrowsingStandardProtectionTableViewId)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  // Check the underlying pref value.
  GREYAssertFalse(
      [ChromeEarlGrey userBooleanPref:password_manager::prefs::
                                          kPasswordLeakDetectionEnabled],
      @"Failed to toggle-off password leak checks");

  // Toggle password leak check detection back on.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::TableViewSwitchCell(
                 kSafeBrowsingStandardProtectionPasswordLeakCellId,
                 /*is_toggled_on=*/NO,
                 /*enabled=*/YES)
                         scrollViewMatcher:
                             grey_accessibilityID(
                                 kSafeBrowsingStandardProtectionTableViewId)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];

  // Check the underlying pref value.
  GREYAssertTrue(
      [ChromeEarlGrey userBooleanPref:password_manager::prefs::
                                          kPasswordLeakDetectionEnabled],
      @"Failed to toggle-on password leak checks");
}

// Tests that password leak detection can only be toggled if Safe Browsing is
// enabled for signed out user.
- (void)testTogglePasswordLeakCheckForSignedOutUser {
  // Ensure that Safe Browsing and password leak detection opt-outs start in
  // their default (opted-in) state.
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kSafeBrowsingEnabled];
  [ChromeEarlGrey
      setBoolValue:YES
       forUserPref:password_manager::prefs::kPasswordLeakDetectionEnabled];

  // Open Privacy Safe Browsing settings.
  [self openPrivacySafeBrowsingSettings];

  // Check that Safe Browsing is enabled, and toggle it off.
  GREYAssertFalse([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnhanced],
                  @"Failed to keep Enhanced Safe Browsing off");
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnabled],
                 @"Failed to keep Standard Safe Browsing on");
  [self turnOffSafeBrowsing];

  // Enter Standard Protection settings page.
  [self pressInfoButtonForCell:kSettingsSafeBrowsingStandardProtectionCellId];

  // Check that the password leak check toggle is both toggled off and disabled.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::TableViewSwitchCell(
                 kSafeBrowsingStandardProtectionPasswordLeakCellId,
                 /*is_toggled_on=*/NO,
                 /*enabled=*/NO)
                         scrollViewMatcher:
                             grey_accessibilityID(
                                 kSafeBrowsingStandardProtectionTableViewId)]
      assertWithMatcher:grey_notNil()];

  // Toggle Safe Browsing on.
  [ChromeEarlGrey setBoolValue:YES forUserPref:prefs::kSafeBrowsingEnabled];

  // Check that the password leak check toggle is enabled, and toggle it off.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::TableViewSwitchCell(
                 kSafeBrowsingStandardProtectionPasswordLeakCellId,
                 /*is_toggled_on=*/YES,
                 /*enabled=*/YES)
                         scrollViewMatcher:
                             grey_accessibilityID(
                                 kSafeBrowsingStandardProtectionTableViewId)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  // Check the underlying pref value.
  GREYAssertFalse(
      [ChromeEarlGrey userBooleanPref:password_manager::prefs::
                                          kPasswordLeakDetectionEnabled],
      @"Failed to toggle-off password leak checks");

  // Toggle password leak check detection back on.
  [[self elementInteractionWithGreyMatcher:
             chrome_test_util::TableViewSwitchCell(
                 kSafeBrowsingStandardProtectionPasswordLeakCellId,
                 /*is_toggled_on=*/NO,
                 /*enabled=*/YES)
                         scrollViewMatcher:
                             grey_accessibilityID(
                                 kSafeBrowsingStandardProtectionTableViewId)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(YES)];

  // Check the underlying pref value.
  GREYAssertTrue(
      [ChromeEarlGrey userBooleanPref:password_manager::prefs::
                                          kPasswordLeakDetectionEnabled],
      @"Failed to toggle-on password leak checks");
}

#pragma mark - Helpers

// Opens privacy safe browsing settings.
- (void)openPrivacySafeBrowsingSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [ChromeEarlGreyUI
      tapPrivacyMenuButton:ButtonWithAccessibilityLabelId(
                               IDS_IOS_PRIVACY_SAFE_BROWSING_TITLE)];
}

// Opens "i" button for a specific cell identifier.
- (void)pressInfoButtonForCell:(NSString*)cellId {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_ancestor(grey_accessibilityID(cellId)),
                                   grey_accessibilityID(
                                       kTableViewCellInfoButtonViewId),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
}

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
          ButtonWithAccessibilityLabelId(
              IDS_IOS_SAFE_BROWSING_NO_PROTECTION_CONFIRMATION_DIALOG_CONFIRM)]
      performAction:grey_tap()];
  GREYAssertFalse([ChromeEarlGrey userBooleanPref:prefs::kSafeBrowsingEnabled],
                  @"Failed to toggle-off Standard Safe Browsing");
}

// Returns GREYElementInteraction for `matcher`, using `scrollViewMatcher` to
// scroll.
- (GREYElementInteraction*)
    elementInteractionWithGreyMatcher:(id<GREYMatcher>)matcher
                    scrollViewMatcher:(id<GREYMatcher>)scrollViewMatcher {
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

@end
