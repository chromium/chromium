// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_app_interface.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_constants.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_earl_grey.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_test_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

/// Returns the popup row containing the `url` as suggestion.
id<GREYMatcher> PopupRowWithUrl(GURL url) {
  NSString* urlString = base::SysUTF8ToNSString(url.GetContent());
  id<GREYMatcher> URLMatcher = grey_allOf(
      grey_descendant(
          chrome_test_util::StaticTextWithAccessibilityLabel(urlString)),
      grey_sufficientlyVisible(), nil);
  return grey_allOf(chrome_test_util::OmniboxPopupRow(), URLMatcher, nil);
}

/// Returns the switch to open tab element for the `url`.
id<GREYMatcher> SwitchTabElementForUrl(const GURL& url) {
  return grey_allOf(
      grey_ancestor(PopupRowWithUrl(url)),
      grey_accessibilityID(kOmniboxPopupRowSwitchTabAccessibilityIdentifier),
      grey_interactable(), nil);
}

void TapSwitchToTabButton(const GURL& url) {
  [[EarlGrey selectElementWithMatcher:grey_allOf(SwitchTabElementForUrl(url),
                                                 grey_interactable(), nil)]
      performAction:grey_tap()];
}

id<GREYMatcher> OmniboxWithLeadingImageElement(
    NSString* const leadingImageIdentifier) {
  return grey_allOf(
      grey_ancestor(grey_kindOfClassName(@"OmniboxContainerView")),
      grey_accessibilityID(leadingImageIdentifier), grey_interactable(), nil);
}

void ScrollToSwitchToTabElement(const GURL& url) {
  [[[EarlGrey selectElementWithMatcher:grey_allOf(SwitchTabElementForUrl(url),
                                                  grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:chrome_test_util::OmniboxPopupList()]
      assertWithMatcher:grey_interactable()];
}

// Web page 1.
const char kPage1[] = "This is the first page";
const char kPage1Title[] = "Title 1";
const char kPage1URL[] = "/page1.html";

// Web page 2.
const char kPage2[] = "This is the second page";
const char kPage2Title[] = "Title 2";
const char kPage2URL[] = "/page2.html";

// Web page 2.
const char kPage3[] = "This is the third page";
const char kPage3Title[] = "Title 3";
const char kPage3URL[] = "/page3.html";

/// Provides responses for the different pages.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  if (request.relative_url == kPage1URL) {
    http_response->set_content(
        "<html><head><title>" + std::string(kPage1Title) +
        "</title></head><body>" + std::string(kPage1) + "</body></html>");
    return std::move(http_response);
  }

  if (request.relative_url == kPage2URL) {
    http_response->set_content(
        "<html><head><title>" + std::string(kPage2Title) +
        "</title></head><body>" + std::string(kPage2) + "</body></html>");
    return std::move(http_response);
  }

  if (request.relative_url == kPage3URL) {
    http_response->set_content(
        "<html><head><title>" + std::string(kPage3Title) +
        "</title></head><body>" + std::string(kPage3) + "</body></html>");
    return std::move(http_response);
  }

  return nil;
}

}  //  namespace

@interface OmniboxPopupTestCase : ChromeTestCase
@end

@implementation OmniboxPopupTestCase {
  GURL _URL1;
  GURL _URL2;
  GURL _URL3;
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  // Disable AutocompleteProvider types: TYPE_SEARCH and TYPE_ON_DEVICE_HEAD.
  omnibox::DisableAutocompleteProviders(config, 1056);

  return config;
}

- (void)setUp {
  [super setUp];

  // Start a server to be able to navigate to a web page.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  _URL1 = self.testServer->GetURL(kPage1URL);
  _URL2 = self.testServer->GetURL(kPage2URL);
  _URL3 = self.testServer->GetURL(kPage3URL);

  [ChromeEarlGrey clearBrowsingHistory];
}

// Tests that tapping the switch to open tab button, switch to the open tab,
// doesn't close the tab.
- (void)testSwitchToOpenTab {
  // Open the first page.
  GURL firstPageURL = self.testServer->GetURL(kPage1URL);
  [ChromeEarlGrey loadURL:firstPageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Open the second page in another tab.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPage2URL)];
  [ChromeEarlGrey waitForWebStateContainingText:kPage2];

  // Type the URL of the first page in the omnibox to trigger it as suggestion.
  [ChromeEarlGreyUI
      focusOmniboxAndReplaceText:base::SysUTF8ToNSString(kPage1URL)];

  // Switch to the first tab, scrolling the popup if necessary.
  ScrollToSwitchToTabElement(firstPageURL);
  TapSwitchToTabButton(firstPageURL);

  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Check that both tabs are opened (and that we switched tab and not just
  // navigated.
  [ChromeEarlGreyUI openTabGrid];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(
                         base::SysUTF8ToNSString(kPage2Title)),
                     grey_ancestor(chrome_test_util::TabGridCellAtIndex(1)),
                     nil)] assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the switch to open tab button isn't displayed for the current tab.
- (void)testNotSwitchButtonOnCurrentTab {
  // Open the first page.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPage1URL)];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Open the second page in another tab.
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kPage2];

  // Type the URL of the first page in the omnibox to trigger it as suggestion.
  [ChromeEarlGreyUI
      focusOmniboxAndReplaceText:base::SysUTF8ToNSString(kPage2URL)];

  // Check that we have the suggestion for the second page, but not the switch
  // as it is the current page.

  [[EarlGrey selectElementWithMatcher:PopupRowWithUrl(_URL2)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:SwitchTabElementForUrl(_URL2)]
      assertWithMatcher:grey_not(grey_interactable())];
}

// Test that swiping left on a historical suggestion and tapping
// the delete button , removes the suggestions.
- (void)testDeleteHistoricalSuggestion {
  [self populateHistory];
  NSString* omniboxInput = [NSString
      stringWithFormat:@"%@:%@", base::SysUTF8ToNSString(_URL3.host()),
                       base::SysUTF8ToNSString(_URL3.port())];

  [ChromeEarlGreyUI focusOmniboxAndReplaceText:omniboxInput];

  // Swipe one of the historical suggestions, to the left.
  if ([ChromeEarlGrey isIPadIdiom]) {
    [[EarlGrey selectElementWithMatcher:PopupRowWithUrl(_URL1)]
        performAction:GREYSwipeSlowInDirectionWithStartPoint(kGREYDirectionLeft,
                                                             0.09, 0.5)];
  } else {
    [[EarlGrey selectElementWithMatcher:PopupRowWithUrl(_URL1)]
        performAction:grey_swipeSlowInDirection(kGREYDirectionLeft)];
  }

  // Delete button is displayed.
  [[EarlGrey selectElementWithMatcher:grey_kindOfClassName(
                                          @"UISwipeActionStandardButton")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on the delete button.
  [[EarlGrey selectElementWithMatcher:grey_kindOfClassName(
                                          @"UISwipeActionStandardButton")]
      performAction:grey_tap()];

  // Historical suggestion with URL1 is now deleted.
  [[EarlGrey selectElementWithMatcher:PopupRowWithUrl(_URL1)]
      assertWithMatcher:grey_nil()];
}

// Tests that the incognito tabs aren't displayed as "opened" tab in the
// non-incognito suggestions and vice-versa.
- (void)testIncognitoSeparation {
  [self populateHistory];
  [[self class] closeAllTabs];

  // Load page 1 in non-incognito and page 2 in incognito.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kPage2];

  // Open page 3 in non-incognito.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kPage3];

  NSString* omniboxInput = [NSString
      stringWithFormat:@"%@:%@", base::SysUTF8ToNSString(_URL3.host()),
                       base::SysUTF8ToNSString(_URL3.port())];
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:omniboxInput];

  // Check that we have the switch button for the first page.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_ancestor(PopupRowWithUrl(_URL1)),
                     grey_accessibilityID(
                         kOmniboxPopupRowSwitchTabAccessibilityIdentifier),
                     nil)] assertWithMatcher:grey_sufficientlyVisible()];

  // Check that we have the suggestion for the second page, but not the switch.
  [[EarlGrey selectElementWithMatcher:PopupRowWithUrl(_URL2)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:SwitchTabElementForUrl(_URL2)]
      assertWithMatcher:grey_nil()];

  // Open page 3 in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kPage3];

  [ChromeEarlGreyUI
      focusOmniboxAndReplaceText:base::SysUTF8ToNSString(_URL3.host())];

  // Check that we have the switch button for the second page.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_ancestor(PopupRowWithUrl(_URL2)),
                     grey_accessibilityID(
                         kOmniboxPopupRowSwitchTabAccessibilityIdentifier),
                     nil)] assertWithMatcher:grey_sufficientlyVisible()];

  // Check that we have the suggestion for the first page, but not the switch.
  [[EarlGrey selectElementWithMatcher:PopupRowWithUrl(_URL1)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:SwitchTabElementForUrl(_URL1)]
      assertWithMatcher:grey_nil()];
}

- (void)testCloseNTPWhenSwitching {
  // Open the first page.
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Open a new tab and switch to the first tab.
  [ChromeEarlGrey openNewTab];
  NSString* omniboxInput = [NSString
      stringWithFormat:@"%@:%@", base::SysUTF8ToNSString(_URL1.host()),
                       base::SysUTF8ToNSString(_URL1.port())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(omniboxInput)];

  TapSwitchToTabButton(_URL1);
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Check that the other tab is closed.
  [ChromeEarlGrey waitForMainTabCount:1];
}

- (void)testDontCloseNTPWhenSwitchingWithForwardHistory {
  // Open the first page.
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Open a new tab, navigate to a page and go back to have forward history.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];
  [ChromeEarlGrey goBack];

  // Navigate to the other tab.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(base::SysUTF8ToNSString(_URL1.host()))];

  // Omnibox can reorder itself in multiple animations, so add an extra wait
  // here.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:SwitchTabElementForUrl(
                                                       _URL1)];
  [[EarlGrey selectElementWithMatcher:SwitchTabElementForUrl(_URL1)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Check that the other tab is not closed.
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that switching to closed tab opens the tab in foreground, except if it
// is from NTP without history.
- (void)testSwitchToClosedTab {
  // Open the first page.
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Open a new tab and load another URL.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPage2URL)];
  [ChromeEarlGrey waitForWebStateContainingText:kPage2];

  // Start typing url of the first page.
  [ChromeEarlGreyUI
      focusOmniboxAndReplaceText:base::SysUTF8ToNSString(kPage1URL)];

  // Make sure that the "Switch to Open Tab" element is visible, scrolling the
  // popup if necessary.
  ScrollToSwitchToTabElement(_URL1);

  // Close the first page.
  [ChromeEarlGrey closeTabAtIndex:0];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Try to switch to the first tab.
  TapSwitchToTabButton(_URL1);
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that the URL has been opened in a new foreground tab.
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that having multiple suggestions with corresponding opened tabs display
// multiple buttons.

- (void)testMultiplePageOpened {
  // Open the first page.
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Open the second page in a new tab.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kPage2];

  // Start typing url of the two opened pages in a new tab.
  [ChromeEarlGrey openNewTab];
  NSString* omniboxInput = [NSString
      stringWithFormat:@"%@:%@", base::SysUTF8ToNSString(_URL1.host()),
                       base::SysUTF8ToNSString(_URL1.port())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(omniboxInput)];

  // Check that both elements are displayed.
  // Omnibox can reorder itself in multiple animations, so add an extra wait
  // here.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:SwitchTabElementForUrl(
                                                       _URL1)];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:SwitchTabElementForUrl(
                                                       _URL2)];
}

// Tests that selecting a suggestion in the omnibox and successfully navigating
// to it adds an entry in the shortcuts database.
- (void)testShortcutsDatabasePopulation {
  [ChromeEarlGrey clearBrowsingHistory];
  // Ensure the database is initialized and empty.
  [OmniboxEarlGrey waitForShortcutsBackendInitialization];
  [OmniboxEarlGrey waitForNumberOfShortcutsInDatabase:0];

  [self populateHistory];
  NSString* omniboxInput = [NSString
      stringWithFormat:@"%@:%@", base::SysUTF8ToNSString(_URL3.host()),
                       base::SysUTF8ToNSString(_URL3.port())];

  [ChromeEarlGreyUI focusOmniboxAndReplaceText:omniboxInput];

  [[EarlGrey selectElementWithMatcher:PopupRowWithUrl(_URL1)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Verify that the shortcut database has been populated.
  [OmniboxEarlGrey waitForNumberOfShortcutsInDatabase:1];
}

#pragma mark - Helpers

// Populate history by visiting the 3 different pages.
- (void)populateHistory {
  // Add all the pages to the history.
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];
  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kPage2];
  [ChromeEarlGrey loadURL:_URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kPage3];
}

@end

@interface OmniboxPopupWithFakeSuggestionTestCase : ChromeTestCase
@end

@implementation OmniboxPopupWithFakeSuggestionTestCase

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

- (void)testTapAppendArrowButton {
  [ChromeEarlGrey loadURL:GURL("about:blank")];

  // Clears the url and replace it with local url host.
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:@"abc"];

  // Wait for the suggestions to show.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::OmniboxPopupRowWithString(@"abcdef")];

  id<GREYMatcher> appendArrowButtonMatcher = grey_allOf(
      grey_ancestor(chrome_test_util::OmniboxPopupRowWithString(@"abcdef")),
      grey_accessibilityID(kOmniboxPopupRowAppendAccessibilityIdentifier), nil);

  // Wait for the append button to show.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:appendArrowButtonMatcher];

  // Tap on the append arrow button.
  [[EarlGrey selectElementWithMatcher:grey_allOf(appendArrowButtonMatcher,
                                                 grey_interactable(), nil)]
      performAction:grey_tap()];

  // Omnibox should now contain the suggestion row string 'abcdef '.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::OmniboxContainingText("abcdef ")];

  // Wait for the new suggestions to show.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          chrome_test_util::OmniboxPopupRowWithString(@"abcdefghi")];
}

// Test when the popup is scrolled, the keyboard is dismissed
// but the omnibox is still expanded and the suggestions are visible.
- (void)testScrollingDismissesKeyboard {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(@"abc")];

  // Matcher for a URL-what-you-typed suggestion.
  id<GREYMatcher> textMatcher = grey_descendant(
      chrome_test_util::StaticTextWithAccessibilityLabel(@"abc"));
  id<GREYMatcher> row =
      grey_allOf(chrome_test_util::OmniboxPopupRow(), textMatcher,
                 grey_sufficientlyVisible(), nil);

  // Omnibox can reorder itself in multiple animations, so add an extra wait
  // here.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:row];
  [ChromeEarlGrey waitForKeyboardToAppear];

  // Scroll the popup. This swipes from the point located at 50% of the width of
  // the frame horizontally and most importantly 10% of the height of the frame
  // vertically. This is necessary if the center of the list's accessibility
  // frame is not visible, as it is the default start point.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxPopupList()]
      performAction:grey_swipeFastInDirectionWithStartPoint(kGREYDirectionDown,
                                                            0.5, 0.1)];

  [[EarlGrey selectElementWithMatcher:row]
      assertWithMatcher:grey_sufficientlyVisible()];

  // The keyboard should be dismissed.
  [ChromeEarlGrey waitForKeyboardToDisappear];
}

@end

@interface HardwareKeyboardInteractionTestCase : ChromeTestCase
@end

@implementation HardwareKeyboardInteractionTestCase

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey clearBrowsingHistory];

  [OmniboxAppInterface
      setUpFakeSuggestionsService:@"fake_suggestions_pedal.json"];
}

- (void)tearDown {
  [OmniboxAppInterface tearDownFakeSuggestionsService];
  [super tearDown];
  // HW keyboard simulation does mess up the SW keyboard simulator state.
  // Relaunching resets the state.
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

// Tests up down interaction in omnibox popup using a hardware keyboard.
- (void)testUpDownArrowAutocomplete {
  // Focus omnibox from Web.
  [ChromeEarlGrey loadURL:GURL("about:blank")];
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:@"testupdown"];

  // Matcher for the first autocomplete suggestions.
  id<GREYMatcher> testupDownAutocomplete1 =
      chrome_test_util::OmniboxPopupRowWithString(@"testupdownautocomplete1");

  // Wait for the suggestions to show.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:testupDownAutocomplete1];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText("testupdown")];

  // The omnibox popup may update multiple times.  Don't downArrow until this
  // is done.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));

  // Go down to testautocomplete1 popup row.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"downArrow" flags:0];
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"downArrow" flags:0];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          chrome_test_util::OmniboxContainingText("testupdownautocomplete1")];

  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"upArrow" flags:0];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      chrome_test_util::OmniboxContainingText("testupdown")];
}

// Tests that leading image in omnibox changes based on the suggestion
// highlighted.
// TODO(crbug.com/40917341): Test is flaky on both device and simulator.
- (void)DISABLED_testOmniboxLeadingImage {
  // Start a server to be able to navigate to a web page.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  GURL _URL1 = self.testServer->GetURL(kPage1URL);

  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];

  // Focus omnibox from Web.
  [ChromeEarlGreyUI focusOmnibox];

  // Typing the title of page1.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(base::SysUTF8ToNSString(kPage1Title))];

  // Wait for suggestions to show.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:PopupRowWithUrl(_URL1)];

  // The omnibox popup may update multiple times.  Don't downArrow until this
  // is done.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"downArrow" flags:0];

  // We expect to have the default leading image.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_allOf(OmniboxWithLeadingImageElement(
                         kOmniboxLeadingImageDefaultAccessibilityIdentifier),
                     nil)];

  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"downArrow" flags:0];

  // The popup row is a url suggestion so we expect to have the leading
  // suggestion image .
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_allOf(
              OmniboxWithLeadingImageElement(
                  kOmniboxLeadingImageSuggestionImageAccessibilityIdentifier),
              nil)];
}

@end
