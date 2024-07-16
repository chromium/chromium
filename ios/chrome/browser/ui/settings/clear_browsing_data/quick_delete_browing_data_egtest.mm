// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "components/sync/base/command_line_switches.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/features.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using chrome_test_util::ButtonWithAccessibilityLabel;

}  // namespace

// Tests the Quick Delete Browsing Data page.
@interface QuickDeleteBrowsingDataTestCase : ChromeTestCase
@end

@implementation QuickDeleteBrowsingDataTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;
  config.features_enabled.push_back(kIOSQuickDelete);
  config.additional_args.push_back(std::string("--") +
                                   syncer::kSyncShortNudgeDelayForTest);
  return config;
}

// Returns a matcher for the title of the Quick Delete bottom sheet.
- (id<GREYMatcher>)quickDeleteTitle {
  return grey_allOf(
      grey_accessibilityID(kConfirmationAlertTitleAccessibilityIdentifier),
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_TITLE)),
      nil);
}

// Returns a matcher for the Quick Delete Browsing Data button on the main page.
- (id<GREYMatcher>)quickDeleteBrowsingDataButton {
  return grey_accessibilityID(kQuickDeleteBrowsingDataButtonIdentifier);
}

// Returns a matcher for the title of the Quick Delete Browsing Data page.
- (id<GREYMatcher>)quickDeleteBrowsingDataPageTitle {
  return chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
      IDS_IOS_DELETE_BROWSING_DATA_TITLE);
}

// Returns a matcher for the confirm button on the navigation bar.
- (id<GREYMatcher>)navigationBarConfirmButton {
  return grey_accessibilityID(kQuickDeleteBrowsingDataConfirmButtonIdentifier);
}

// Opens Quick Delete browsing data page.
- (void)openQuickDeleteBrowsingDataPage {
  [ChromeEarlGreyUI openToolsMenu];

  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                   l10n_util::GetNSString(
                                       IDS_IOS_CLEAR_BROWSING_DATA_TITLE))]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:[self quickDeleteBrowsingDataButton]]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      [self quickDeleteBrowsingDataPageTitle]];
}

// Tests the cancel button dismisses the browsing data page.
- (void)testPageNavigationCancelButton {
  // Open quick delete browsing data page.
  [self openQuickDeleteBrowsingDataPage];

  // Tap cancel button.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Ensure the page is closed while quick delete bottom sheet is still open.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteBrowsingDataPageTitle]]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      assertWithMatcher:grey_notNil()];

  // TODO(crbug.com/341107834): Check prefs are not updated on cancel.
}

// Tests the confirm button dismisses the browsing data page.
- (void)testPageNavigationConfirmButton {
  // Open quick delete browsing data page.
  [self openQuickDeleteBrowsingDataPage];

  // Tap confirm button.
  [[EarlGrey selectElementWithMatcher:[self navigationBarConfirmButton]]
      performAction:grey_tap()];

  // Ensure the page is closed while quick delete bottom sheet is still open.
  [[EarlGrey selectElementWithMatcher:[self quickDeleteBrowsingDataPageTitle]]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:[self quickDeleteTitle]]
      assertWithMatcher:grey_notNil()];

  // TODO(crbug.com/341107834): Check prefs are updated on confirm.
}

@end
