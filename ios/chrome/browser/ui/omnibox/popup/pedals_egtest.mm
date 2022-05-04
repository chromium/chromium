// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "base/bind.h"
#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/history/history_ui_constants.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::HistoryTableView;
using chrome_test_util::NavigationBarCancelButton;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::SettingsCollectionView;
using chrome_test_util::SettingsCreditCardMatcher;
using chrome_test_util::SettingsPasswordMatcher;
using chrome_test_util::SettingsSafetyCheckTableView;

namespace {

// Hard-coded here to avoid dependency on //content. This needs to be kept in
// sync with kChromeUIScheme in `content/public/common/url_constants.h`.
const char kChromeUIScheme[] = "chrome";

id<GREYMatcher> PopupPedalRow(NSString* text, NSString* subtitle) {
  return grey_allOf(
      chrome_test_util::OmniboxPopupRow(),
      grey_descendant(grey_allOf(grey_accessibilityLabel(text),
                                 grey_accessibilityValue(subtitle), nil)),
      nil);
}

}  //  namespace

@interface OmniboxPopupPedalsTestCase : ChromeTestCase

@end

@implementation OmniboxPopupPedalsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kIOSOmniboxUpdatedPopupUI);
  // Swap the base Google URL to prevent any search suggestions.
  // This improves the reproductability of the results.
  config.additional_args.push_back("--google-base-url=404.com");
  return config;
}

- (void)testClearBrowsingDataPedal {
  if (!base::ios::IsRunningOnIOS15OrLater()) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS 14.")
  }

  // Type the pedal hint in the omnibox to trigger it as suggestion.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"clear browsing data")];

  // Tap the pedal suggestion in the omnibox popup.
  id<GREYMatcher> pedalMatcher = PopupPedalRow(
      l10n_util::GetNSString(IDS_IOS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT),
      l10n_util::GetNSString(
          IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_CLEAR_BROWSING_DATA));

  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:pedalMatcher];
  [[EarlGrey selectElementWithMatcher:pedalMatcher] performAction:grey_tap()];

  // Check that the Clear Browsing Data dialog is presented.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kClearBrowsingDataViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Press Cancel.
  id<GREYMatcher> exitMatcher = NavigationBarDoneButton();
  [[EarlGrey selectElementWithMatcher:exitMatcher] performAction:grey_tap()];

  // Check that the CBD dialog has been dismissed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kClearBrowsingDataViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];

  // Check that the omnibox is defocused.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_notVisible()];
}

- (void)testSetChromeAsDefaultBrowserPedal {
  if (!base::ios::IsRunningOnIOS15OrLater()) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS 14.")
  }

  // Type the pedal hint in the omnibox to trigger it as suggestion.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"set chrome as default browser")];

  // Tap the pedal suggestion in the omnibox popup.
  id<GREYMatcher> pedalMatcher = PopupPedalRow(
      l10n_util::GetNSString(
          IDS_IOS_OMNIBOX_PEDAL_SET_CHROME_AS_DEFAULT_BROWSER_HINT),
      l10n_util::GetNSString(IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_DEFAULT_BROWSER));

  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:pedalMatcher];
  [[EarlGrey selectElementWithMatcher:pedalMatcher] performAction:grey_tap()];

  // Check that the Default Browser settings dialog is presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kDefaultBrowserSettingsTableViewId)]
      assertWithMatcher:grey_notNil()];

  // Press Cancel.
  id<GREYMatcher> exitMatcher = NavigationBarCancelButton();
  [[EarlGrey selectElementWithMatcher:exitMatcher] performAction:grey_tap()];

  // Check that the default browser dialog has been dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kDefaultBrowserSettingsTableViewId)]
      assertWithMatcher:grey_nil()];

  // Check that the omnibox is defocused.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_notVisible()];
}

- (void)testManagePaymentMethodsPedal {
  if (!base::ios::IsRunningOnIOS15OrLater()) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS 14.")
  }

  // Type the pedal hint in the omnibox to trigger it as suggestion.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"manage payment methods")];

  // Tap the pedal suggestion in the omnibox popup.
  id<GREYMatcher> pedalMatcher = PopupPedalRow(
      l10n_util::GetNSString(IDS_IOS_OMNIBOX_PEDAL_UPDATE_CREDIT_CARD_HINT),
      l10n_util::GetNSString(
          IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_UPDATE_CREDIT_CARD));

  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:pedalMatcher];
  [[EarlGrey selectElementWithMatcher:pedalMatcher] performAction:grey_tap()];

  // Check that the Manage Payment Methods settings dialog is presented.
  [[EarlGrey selectElementWithMatcher:SettingsCreditCardMatcher()]
      assertWithMatcher:grey_notNil()];

  // Press Cancel.
  id<GREYMatcher> exitMatcher = NavigationBarCancelButton();
  [[EarlGrey selectElementWithMatcher:exitMatcher] performAction:grey_tap()];

  // Check that the Manage Payment Methods settings dialog has been dismissed.
  [[EarlGrey selectElementWithMatcher:SettingsCreditCardMatcher()]
      assertWithMatcher:grey_nil()];

  // Check that the omnibox is defocused.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_notVisible()];
}

- (void)testLaunchIncognitoPedal {
  if (!base::ios::IsRunningOnIOS15OrLater()) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS 14.")
  }

  [ChromeEarlGrey waitForIncognitoTabCount:0];

  // Type the pedal hint in the omnibox to trigger it as suggestion.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"open incognito tab")];

  // Tap the pedal suggestion in the omnibox popup.
  id<GREYMatcher> pedalMatcher = PopupPedalRow(
      l10n_util::GetNSString(IDS_IOS_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_HINT),
      l10n_util::GetNSString(IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_LAUNCH_INCOGNITO));

  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:pedalMatcher];
  [[EarlGrey selectElementWithMatcher:pedalMatcher] performAction:grey_tap()];

  // Check that a new incognito tab is opened.
  [ChromeEarlGrey waitForIncognitoTabCount:1];

  // Check that the omnibox is defocused.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_notVisible()];
}

- (void)testRunChromeSafetyCheckPedal {
  if (!base::ios::IsRunningOnIOS15OrLater()) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS 14.")
  }

  // Type the pedal hint in the omnibox to trigger it as suggestion.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"run safety check")];

  // Tap the pedal suggestion in the omnibox popup.
  id<GREYMatcher> pedalMatcher = PopupPedalRow(
      l10n_util::GetNSString(
          IDS_IOS_OMNIBOX_PEDAL_RUN_CHROME_SAFETY_CHECK_HINT),
      l10n_util::GetNSString(
          IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_RUN_CHROME_SAFETY_CHECK));

  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:pedalMatcher];
  [[EarlGrey selectElementWithMatcher:pedalMatcher] performAction:grey_tap()];

  // Check that the Safety Check settings dialog is presented.
  [[EarlGrey selectElementWithMatcher:SettingsSafetyCheckTableView()]
      assertWithMatcher:grey_notNil()];

  // Press Done.
  id<GREYMatcher> exitMatcher = NavigationBarDoneButton();
  [[EarlGrey selectElementWithMatcher:exitMatcher] performAction:grey_tap()];

  // Check that the Safety Check settings dialog has been dismissed.
  [[EarlGrey selectElementWithMatcher:SettingsSafetyCheckTableView()]
      assertWithMatcher:grey_nil()];

  // Check that the omnibox is defocused.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_notVisible()];
}

- (void)testManageChromeSettingsPedal {
  if (!base::ios::IsRunningOnIOS15OrLater()) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS 14.")
  }

  // Type the pedal hint in the omnibox to trigger it as suggestion.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"manage settings")];

  // Tap the pedal suggestion in the omnibox popup.
  id<GREYMatcher> pedalMatcher = PopupPedalRow(
      l10n_util::GetNSString(IDS_IOS_OMNIBOX_PEDAL_MANAGE_CHROME_SETTINGS_HINT),
      l10n_util::GetNSString(
          IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_MANAGE_CHROME_SETTINGS));

  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:pedalMatcher];
  [[EarlGrey selectElementWithMatcher:pedalMatcher] performAction:grey_tap()];

  // Check that the settings dialog is presented.
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_notNil()];

  // Press Done.
  id<GREYMatcher> exitMatcher = NavigationBarDoneButton();
  [[EarlGrey selectElementWithMatcher:exitMatcher] performAction:grey_tap()];

  // Check that the settings dialog has been dismissed.
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_nil()];

  // Check that the omnibox is defocused.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_notVisible()];
}

- (void)testViewChromeHistoryPedal {
  if (!base::ios::IsRunningOnIOS15OrLater()) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS 14.")
  }

  // Type the pedal hint in the omnibox to trigger it as suggestion.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"view chrome history")];

  // Tap the pedal suggestion in the omnibox popup.
  id<GREYMatcher> pedalMatcher = PopupPedalRow(
      l10n_util::GetNSString(IDS_IOS_OMNIBOX_PEDAL_VIEW_CHROME_HISTORY_HINT),
      l10n_util::GetNSString(
          IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_VIEW_CHROME_HISTORY));

  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:pedalMatcher];
  [[EarlGrey selectElementWithMatcher:pedalMatcher] performAction:grey_tap()];

  // Check that the History dialog is presented.
  [[EarlGrey selectElementWithMatcher:HistoryTableView()]
      assertWithMatcher:grey_notNil()];

  // Press Done.
  id<GREYMatcher> exitMatcher = NavigationBarDoneButton();
  [[EarlGrey selectElementWithMatcher:exitMatcher] performAction:grey_tap()];

  // Check that the History dialog has been dismissed.
  [[EarlGrey selectElementWithMatcher:HistoryTableView()]
      assertWithMatcher:grey_nil()];

  // Check that the omnibox is defocused.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_notVisible()];
}

- (void)testPlayChromeDinoGamePedal {
  if (!base::ios::IsRunningOnIOS15OrLater()) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS 14.")
  }

  // Type the pedal hint in the omnibox to trigger it as suggestion.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"play dino game")];

  NSString* urlStr = [NSString
      stringWithFormat:@"%s://%s", kChromeUIScheme, kChromeUIDinoHost];
  GURL url(base::SysNSStringToUTF8(urlStr) + "/");

  // Tap the pedal suggestion in the omnibox popup.
  id<GREYMatcher> pedalMatcher = PopupPedalRow(
      l10n_util::GetNSString(IDS_IOS_OMNIBOX_PEDAL_PLAY_CHROME_DINO_GAME_HINT),
      urlStr);

  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:pedalMatcher];
  [[EarlGrey selectElementWithMatcher:pedalMatcher] performAction:grey_tap()];

  // Check that the dino game page is presented.
  GREYAssertEqual(url, [ChromeEarlGrey webStateVisibleURL],
                  @"Did not navigate to the dino game URL.");

  // Check that the omnibox is defocused.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_notVisible()];
}

@end
