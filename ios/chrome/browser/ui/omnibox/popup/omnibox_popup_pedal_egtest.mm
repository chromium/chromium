// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_app_interface.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/features.h"
#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

NSString* kDinoPedalString = @"chrome://dino";
NSString* kDinoSearchString = @"dino game";

}  // namespace

@interface OmniboxPedalsTestCase : ChromeTestCase
@end
@implementation OmniboxPedalsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_enabled.push_back(kIOSQuickDelete);

  return config;
}

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey clearBrowsingHistory];

  [OmniboxAppInterface
      setUpFakeSuggestionsService:@"fake_suggestions_pedal.json"];
}

- (void)tearDown {
  [OmniboxAppInterface tearDownFakeSuggestionsService];
  [super tearDown];
}

// Tests that the dino pedal is present and that it opens the dino game.
- (void)testDinoPedal {
  // Focus omnibox from Web.
  [ChromeEarlGrey loadURL:GURL("about:blank")];
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:@"pedaldino"];

  // Matcher for the dino pedal and search suggestions.
  id<GREYMatcher> dinoPedal =
      chrome_test_util::OmniboxPopupRowWithString(kDinoPedalString);
  id<GREYMatcher> dinoSearch =
      chrome_test_util::OmniboxPopupRowWithString(kDinoSearchString);

  // Dino pedal and search suggestions should be visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:dinoPedal];
  [[EarlGrey selectElementWithMatcher:dinoSearch]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on dino pedal.
  [[EarlGrey selectElementWithMatcher:dinoPedal] performAction:grey_tap()];

  // The dino game should be loaded.
  [ChromeEarlGrey waitForPageToFinishLoading];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText(
                            base::SysNSStringToUTF8(kDinoPedalString))];

  [ChromeEarlGrey closeCurrentTab];
}

// Tests that open incognito pedal is present and it opens incognito tab.
- (void)testOpenNewIncognitoTabPedal {
  // Focus omnibox from Web.
  [ChromeEarlGrey loadURL:GURL("about:blank")];
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:@"pedalincognitotab"];

  NSString* incognitoPedalString =
      l10n_util::GetNSString(IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_LAUNCH_INCOGNITO);

  // Matcher for the incognito pedal suggestion.
  id<GREYMatcher> incognitoPedal =
      chrome_test_util::OmniboxPopupRowWithString(incognitoPedalString);

  // Incognito pedal should be visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:incognitoPedal];

  // Tap on incognito pedal.
  [[EarlGrey selectElementWithMatcher:incognitoPedal] performAction:grey_tap()];

  // New tab incognito page should be displayed.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::NTPIncognitoView()];

  [ChromeEarlGrey closeCurrentTab];
}

// Tests that the manage passwords pedal is present and it opens the password
// manager page.
- (void)testManagePasswordsPedal {
  // Focus omnibox from Web.
  [ChromeEarlGrey loadURL:GURL("about:blank")];
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:@"passwords"];

  NSString* managePasswordsPedalString =
      l10n_util::GetNSString(IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_MANAGE_PASSWORDS);

  // Matcher for the manage passwords pedal suggestion.
  id<GREYMatcher> managePasswordsPedal =
      chrome_test_util::OmniboxPopupRowWithString(managePasswordsPedalString);

  // Manage passwords pedal should be visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:managePasswordsPedal];

  // Mock successful reauth when opening the Password Manager.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  // Tap on Manage passwords pedal.
  [[EarlGrey selectElementWithMatcher:managePasswordsPedal]
      performAction:grey_tap()];

  // Password Manager page should be displayed.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::PasswordsTableViewMatcher()];

  // Close the password manager.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      chrome_test_util::PasswordsTableViewMatcher()];

  [ChromeEarlGrey closeCurrentTab];

  // Remove mock to keep the app in the same state as before running the test.
  [PasswordSettingsAppInterface removeMockReauthenticationModule];
}

// Tests that the clear browsing data pedal is present and it opens the clear
// browsing data page.
- (void)testClearBrowsingDataPedal {
  // Focus omnibox from Web.
  [ChromeEarlGrey loadURL:GURL("about:blank")];
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:@"pedalclearbrowsing"];

  NSString* clearBrowsingDataPedalString = l10n_util::GetNSString(
      IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_CLEAR_BROWSING_DATA);

  // Matcher for the clear browsing data pedal suggestion.
  id<GREYMatcher> clearBrowsingDataPedal =
      chrome_test_util::OmniboxPopupRowWithString(clearBrowsingDataPedalString);

  // Clear browsing data pedal should be visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:clearBrowsingDataPedal];

  // Tap on clear browsing data pedal.
  [[EarlGrey selectElementWithMatcher:clearBrowsingDataPedal]
      performAction:grey_tap()];

  id<GREYMatcher> quickDeleteTitle = grey_allOf(
      grey_accessibilityID(kConfirmationAlertTitleAccessibilityIdentifier),
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_TITLE)),
      nil);

  // Clear browsing data page should be displayed.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:quickDeleteTitle];

  // Close the Clear browsing data page.
  [[EarlGrey selectElementWithMatcher:quickDeleteTitle]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:quickDeleteTitle];

  [ChromeEarlGrey closeCurrentTab];
}

// Tests that the default browser pedal is present and it opens the set default
// browser page.
- (void)testSetDefaultBrowserPedal {
  // Focus omnibox from Web.
  [ChromeEarlGrey loadURL:GURL("about:blank")];
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:@"pedaldefaultbrowser"];

  NSString* defaultBrowserPedalString =
      l10n_util::GetNSString(IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_DEFAULT_BROWSER);

  // Matcher for the set default browser pedal suggestion.
  id<GREYMatcher> setDefaultBrowserPedal =
      chrome_test_util::OmniboxPopupRowWithString(defaultBrowserPedalString);

  // Set default browser pedal should be visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:setDefaultBrowserPedal];

  // Tap on Set default browser pedal.
  [[EarlGrey selectElementWithMatcher:setDefaultBrowserPedal]
      performAction:grey_tap()];

  // Set default browser page should be displayed.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          chrome_test_util::DefaultBrowserSettingsTableViewMatcher()];

  // Close the set default browser page.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarCancelButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:
          chrome_test_util::DefaultBrowserSettingsTableViewMatcher()];

  [ChromeEarlGrey closeCurrentTab];
}

// Tests that manage settings pedal is present and it opens the manage settings
// page.
- (void)testManageSettingsPedal {
  // Focus omnibox from Web.
  [ChromeEarlGrey loadURL:GURL("about:blank")];
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:@"pedalsettings"];

  NSString* manageSettingsPedalString = l10n_util::GetNSString(
      IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_MANAGE_CHROME_SETTINGS);

  // Matcher for the manage settings pedal suggestion.
  id<GREYMatcher> manageSettingsPedal =
      chrome_test_util::OmniboxPopupRowWithString(manageSettingsPedalString);

  // Manage settings pedal should be visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:manageSettingsPedal];

  // Tap on manage settings pedal.
  [[EarlGrey selectElementWithMatcher:manageSettingsPedal]
      performAction:grey_tap()];

  // Manage settings page should be displayed.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::SettingsCollectionView()];

  // Close the Manage settings page.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      chrome_test_util::SettingsCollectionView()];

  [ChromeEarlGrey closeCurrentTab];
}

// Tests that manage payment methods pedal is present and it opens the manage
// payment methods page.
- (void)testManagePaymentMethodsPedal {
  // Focus omnibox from Web.
  [ChromeEarlGrey loadURL:GURL("about:blank")];
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:@"pedalmanagepayment"];

  NSString* managePaymenyMethodsPedalString =
      l10n_util::GetNSString(IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_UPDATE_CREDIT_CARD);

  // Matcher for the manage payment methods pedal suggestion.
  id<GREYMatcher> managePaymentMethodsPedal =
      chrome_test_util::OmniboxPopupRowWithString(
          managePaymenyMethodsPedalString);

  // Manage payment methods pedal should be visible.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:managePaymentMethodsPedal];

  // Tap on manage payment methods pedal.
  [[EarlGrey selectElementWithMatcher:managePaymentMethodsPedal]
      performAction:grey_tap()];

  // Manage payment methods page should be displayed.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::AutofillCreditCardTableView()];

  // Close the Manage payments settings page.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      chrome_test_util::AutofillCreditCardTableView()];

  [ChromeEarlGrey closeCurrentTab];
}

// Tests that safety check pedal is present and it opens the safety check page.
- (void)testSafetyCheckPedal {
  // Focus omnibox from Web.
  [ChromeEarlGrey loadURL:GURL("about:blank")];
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:@"pedalsafetycheck"];

  NSString* safetyCheckPedalString = l10n_util::GetNSString(
      IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_RUN_CHROME_SAFETY_CHECK);

  // Matcher for safety check pedal suggestion.
  id<GREYMatcher> safetyCheckPedal =
      chrome_test_util::OmniboxPopupRowWithString(safetyCheckPedalString);

  // Safety check pedal should be visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:safetyCheckPedal];

  // Tap on safety check pedal.
  [[EarlGrey selectElementWithMatcher:safetyCheckPedal]
      performAction:grey_tap()];

  // Safety check page should be displayed.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::SafetyCheckTableViewMatcher()];

  // Close the safety check page.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      chrome_test_util::SafetyCheckTableViewMatcher()];

  [ChromeEarlGrey closeCurrentTab];
}

// Tests that visit history pedal is present and it opens the browser history
// page.
- (void)testVisitHistoryPedal {
  // Focus omnibox from Web.
  [ChromeEarlGrey loadURL:GURL("about:blank")];
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:@"history"];

  NSString* visitHistoryPedalString = l10n_util::GetNSString(
      IDS_IOS_OMNIBOX_PEDAL_SUBTITLE_VIEW_CHROME_HISTORY);

  // Matcher for visit history pedal suggestion.
  id<GREYMatcher> visitHistoryPedal =
      chrome_test_util::OmniboxPopupRowWithString(visitHistoryPedalString);

  // Visit history pedal should be visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:visitHistoryPedal];

  // Tap on visit history pedal.
  [[EarlGrey selectElementWithMatcher:visitHistoryPedal]
      performAction:grey_tap()];

  // Visit history page should be displayed.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:chrome_test_util::HistoryTableView()];

  // Close the Visit history page.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      chrome_test_util::HistoryTableView()];

  [ChromeEarlGrey closeCurrentTab];
}

// Tests that the dino pedal does not appear when the search suggestion is below
// the top 3.
- (void)testNoPedal {
  // Focus omnibox from Web.
  [ChromeEarlGrey loadURL:GURL("about:blank")];
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:@"nopedal"];

  // Matcher for the dino pedal and search suggestions.
  id<GREYMatcher> dinoPedal =
      chrome_test_util::OmniboxPopupRowWithString(kDinoPedalString);
  id<GREYMatcher> dinoSearch =
      chrome_test_util::OmniboxPopupRowWithString(kDinoSearchString);

  // The dino search suggestion should be present.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:dinoSearch];

  // The dino pedal should not appear.
  [[EarlGrey selectElementWithMatcher:dinoPedal] assertWithMatcher:grey_nil()];

  [ChromeEarlGrey closeCurrentTab];
}
@end
