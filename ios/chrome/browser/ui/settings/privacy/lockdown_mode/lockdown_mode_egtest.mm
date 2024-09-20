// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_constants.h"
#import "ios/chrome/browser/web/model/features.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::SettingsMenuPrivacyButton;
using chrome_test_util::SettingsPrivacyTableView;
using chrome_test_util::SyncSwitchCell;
using chrome_test_util::TableViewSwitchCell;

namespace {
NSString* const kLockdownModeTableViewId = @"kLockdownModeTableViewId";
NSString* const kLockdownModeCellId = @"kLockdownModeCellId";

// Waits until the pop up is shown.
[[nodiscard]] bool WaitForPopupDisplay(NSString* popUpMessage) {
  return base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        NSError* error = nil;
        [[EarlGrey selectElementWithMatcher:grey_text(popUpMessage)]
            assertWithMatcher:grey_notNil()
                        error:&error];
        return (error == nil);
      });
}

}  // namespace

// Integration tests using the Lockdown Mode settings screen.
@interface LockdownModeTestCase : ChromeTestCase
@end

@implementation LockdownModeTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // TODO (crbug.com/1285974) Remove when bug is resolved.
  config.features_disabled.push_back(kNewOverflowMenu);
  return config;
}

- (void)setUp {
  [super setUp];
  // Ensure that Browser Lockdown Mode opt-out starts in its default (opted-out)
  // state.
  [ChromeEarlGrey setBoolValue:NO
             forLocalStatePref:prefs::kBrowserLockdownModeEnabled];
  // Ensure that OS Lockdown Mode opt-in starts in its default (opted-out)
  // state.
  [ChromeEarlGrey setBoolValue:NO
             forLocalStatePref:prefs::kOSLockdownModeEnabled];
}

- (void)tearDown {
  // Reset preferences back to default values.
  [ChromeEarlGrey setBoolValue:NO
             forLocalStatePref:prefs::kBrowserLockdownModeEnabled];
  [ChromeEarlGrey setBoolValue:NO
             forLocalStatePref:prefs::kOSLockdownModeEnabled];
  [super tearDown];
}

// Tests the lockdown mode settings when OS lockdown mode is disabled. The
// settings page should show a toggle button, press it, and check that row says
// "Off" in the previous screen.
- (void)testOSLockdownModeDisabled {
  [self openLockdownModeSettings];

  // Check that lockdown mode row shows an "off" text label.
  [[EarlGrey
      selectElementWithMatcher:TableViewSwitchCell(kLockdownModeCellId, NO)]
      assertWithMatcher:grey_notNil()];

  [self tapLockdownModeToggleButton:NO withNewValue:YES];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];

  // Check that lockdown mode row shows an "on" text label.
  [self checkPrivacyLockdownModeItemStatus:YES];
  GREYAssertTrue(
      [ChromeEarlGrey localStateBooleanPref:prefs::kBrowserLockdownModeEnabled],
      @"Lockdown mode should be on.");
}

// Tests the lockdown mode settings when OS lockdown mode is enabled. The
// settings page should show a row with an "i" button.
- (void)testOSLockdownModeEnabled {
  [ChromeEarlGrey setBoolValue:NO
             forLocalStatePref:prefs::kBrowserLockdownModeEnabled];
  [ChromeEarlGrey setBoolValue:YES
             forLocalStatePref:prefs::kOSLockdownModeEnabled];
  [self openLockdownModeSettings];

  // Check that lockdown mode row shows an "on" text label.
  id<GREYMatcher> lockdownModeInfoButtonItem = grey_allOf(
      grey_accessibilityID(kLockdownModeCellId),
      grey_accessibilityValue(l10n_util::GetNSString(IDS_IOS_SETTING_ON)),
      grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:lockdownModeInfoButtonItem]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:lockdownModeInfoButtonItem]
      performAction:grey_tap()];
  GREYAssert(WaitForPopupDisplay(l10n_util::GetNSString(
                 IDS_IOS_LOCKDOWN_MODE_INFO_BUTTON_TITLE)),
             @"Lockdown mode 'i' button wasn't tapped");

  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    [[EarlGrey selectElementWithMatcher:
                   grey_text(l10n_util::GetNSString(
                       IDS_IOS_LOCKDOWN_MODE_INFO_BUTTON_SUMMARY_FOR_IPAD))]
        assertWithMatcher:grey_notNil()];
  } else {
    [[EarlGrey selectElementWithMatcher:
                   grey_text(l10n_util::GetNSString(
                       IDS_IOS_LOCKDOWN_MODE_INFO_BUTTON_SUMMARY_FOR_IPHONE))]
        assertWithMatcher:grey_notNil()];
  }
}

#pragma mark - Helpers

// Opens lockdown mode settings.
- (void)openLockdownModeSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [ChromeEarlGreyUI tapPrivacyMenuButton:ButtonWithAccessibilityLabelId(
                                             IDS_IOS_LOCKDOWN_MODE_TITLE)];
}

// Taps lockdown mode toggle button with the expected `currentToggleStatus`.
- (void)tapLockdownModeToggleButton:(BOOL)currentToggleStatus
                       withNewValue:(BOOL)newToggleStatus {
  // Needs to scroll slowly to make sure to not miss a cell if it is not
  // currently on the screen. It should not be bigger than the visible part
  // of the collection view.
  const CGFloat kPixelsToScroll = 300;
  id<GREYAction> searchAction =
      grey_scrollInDirection(kGREYDirectionDown, kPixelsToScroll);
  id<GREYMatcher> lockdownSwitchButton = chrome_test_util::TableViewSwitchCell(
      kLockdownModeCellId,
      /*is_toggled_on=*/currentToggleStatus,
      /*enabled=*/YES);
  [[[EarlGrey selectElementWithMatcher:lockdownSwitchButton]
         usingSearchAction:searchAction
      onElementWithMatcher:grey_accessibilityID(kLockdownModeTableViewId)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(newToggleStatus)];
}

// Checks if the lockdown mode item in the privacy settings page shows on or
// off. Scroll action is needed for smaller devices where the lockdown mode item
// may not be shown initially.
- (void)checkPrivacyLockdownModeItemStatus:(BOOL)status {
  const CGFloat kPixelsToScroll = 300;
  id<GREYAction> searchAction =
      grey_scrollInDirection(kGREYDirectionDown, kPixelsToScroll);
  NSString* statusLabel = status ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                                 : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  id<GREYMatcher> lockdownModeTableViewDetailItem = grey_allOf(
      grey_accessibilityID(kPrivacyLockdownModeCellId),
      grey_accessibilityValue(statusLabel), grey_sufficientlyVisible(), nil);
  [[[EarlGrey selectElementWithMatcher:lockdownModeTableViewDetailItem]
         usingSearchAction:searchAction
      onElementWithMatcher:SettingsPrivacyTableView()]
      assertWithMatcher:grey_notNil()];
}

@end
