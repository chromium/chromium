// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>
#import "ios/testing/earl_grey/earl_grey_test.h"

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/memory/ptr_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "build/branding_buildflags.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/common/user_agent.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using chrome_test_util::OpenLinkInNewTabButton;

namespace {

const char kPageURL[] = "/test-page.html";
const char kUserAgentPageURL[] = "/user-agent-test-page.html";
const char kPageTitle[] = "Page title!";
const char kPageLoadedString[] = "Page loaded!";

const char kMobileSiteLabel[] = "Mobile";

const char kDesktopSiteLabel[] = "Desktop";

// Provides responses for redirect and changed window location URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    int* counter,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  if (request.relative_url == kPageURL) {
    http_response->set_content("<html><head><title>" + std::string(kPageTitle) +
                               "</title></head><body>" +
                               std::string(kPageLoadedString) +
                               "</body></html>");
    (*counter)++;
    return std::move(http_response);
  } else if (request.relative_url == kUserAgentPageURL) {
    std::string desktop_product =
        "CriOS/" + version_info::GetMajorVersionNumber();
    std::string desktop_user_agent =
        web::BuildDesktopUserAgent(desktop_product);
    std::string response_body;
    auto user_agent = request.headers.find("User-Agent");
    if (user_agent != request.headers.end() &&
        user_agent->second == desktop_user_agent) {
      response_body = std::string(kDesktopSiteLabel);
    } else {
      response_body = std::string(kMobileSiteLabel);
    }
    http_response->set_content("<html><head></head><body>" +
                               std::string(response_body) + "</body></html>");
    return std::move(http_response);
  }
  return nullptr;
}

// Selects the button to request desktop site by scrolling the collection.
// 200 is a reasonable scroll displacement that works for all UI elements, while
// not being too slow.
GREYElementInteraction* RequestDesktopButton() {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kToolsMenuRequestDesktopId),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(
                               kPopupMenuToolsMenuTableViewId)];
}

// Returns the XCUIElement to interact with a tab strip tab.
XCUIElement* LegacyTabStripTabWithIdentifier(NSString* tab_identifier) {
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElementQuery* tab_strip =
      [[app descendantsMatchingType:XCUIElementTypeScrollView]
          matchingIdentifier:@"kRegularTabStripId"];
  return [[[tab_strip descendantsMatchingType:XCUIElementTypeStaticText]
      matchingIdentifier:tab_identifier] elementBoundByIndex:0];
}

// Moves a tab in the tab strip from its position to the position of another
// existing tab.
void LegacyLongPressAndDragTabInTabStrip(NSString* moving_tab_identifier,
                                         NSString* destination_tab_identifier) {
  XCUIElement* moving_tab =
      LegacyTabStripTabWithIdentifier(moving_tab_identifier);
  XCUIElement* destination_tab =
      LegacyTabStripTabWithIdentifier(destination_tab_identifier);

  XCUICoordinate* start_point =
      [moving_tab coordinateWithNormalizedOffset:CGVectorMake(0.5, 0.5)];
  XCUICoordinate* end_point =
      [destination_tab coordinateWithNormalizedOffset:CGVectorMake(0.5, 0.5)];

  [start_point pressForDuration:1.5
           thenDragToCoordinate:end_point
                   withVelocity:XCUIGestureVelocityDefault
            thenHoldForDuration:1.0];
}

}  // namespace

// Test case for the prerender.
@interface PrerenderTestCase : ChromeTestCase {
  int _visitCounter;
}

@end

@implementation PrerenderTestCase

// These tests fail very often on Official builds because of flakiness in the
// prerender. Only build these tests in non-Official builds.
#if defined(OFFICIAL_BUILD)
#define MAYBE_testLegacyOpenTabInTabStripBeforePrerenderedTab \
  DISABLED_testLegacyOpenTabInTabStripBeforePrerenderedTab
#define MAYBE_testOpenTabInTabStripBeforePrerenderedTab \
  DISABLED_testOpenTabInTabStripBeforePrerenderedTab
#else
#define MAYBE_testLegacyOpenTabInTabStripBeforePrerenderedTab \
  testLegacyOpenTabInTabStripBeforePrerenderedTab
#define MAYBE_testOpenTabInTabStripBeforePrerenderedTab \
  testOpenTabInTabStripBeforePrerenderedTab
#endif  // defined(OFFICIAL_BUILD)

#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testMovePrerenderedTabInTabStrip testMovePrerenderedTabInTabStrip
#else
#define MAYBE_testMovePrerenderedTabInTabStrip \
  DISABLED_testMovePrerenderedTabInTabStrip
#endif  // TARGET_IPHONE_SIMULATOR

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if ([self isRunningTest:@selector
            (MAYBE_testLegacyOpenTabInTabStripBeforePrerenderedTab)] ||
      [self isRunningTest:@selector(MAYBE_testMovePrerenderedTabInTabStrip)]) {
    config.features_disabled.push_back(kModernTabStrip);
    config.features_disabled.push_back(kTabGroupsIPad);
  } else {
    config.features_enabled.push_back(kModernTabStrip);
  }
  return config;
}

- (void)addURLToHistory {
  [ChromeEarlGrey clearBrowsingHistory];
  // Set server up.
  _visitCounter = 0;
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse, &_visitCounter));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);
  NSString* pageString = base::SysUTF8ToNSString(pageURL.GetContent());

  // Go to the page a couple of time so it shows as suggestion.
  [ChromeEarlGrey loadURL:pageURL];
  GREYAssertEqual(1, _visitCounter, @"The page should have been loaded once");
  [ChromeEarlGrey goBack];
  [[self class] closeAllTabs];
  [ChromeEarlGrey openNewTab];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];

  // TODO(crbug.com/40916974): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(pageString)];
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];
  [ChromeEarlGrey waitForWebStateVisible];
  [ChromeEarlGrey waitForPageToFinishLoading];
  GREYAssertEqual(2, _visitCounter, @"The page should have been loaded twice");

  [[self class] closeAllTabs];
  [ChromeEarlGrey openNewTab];
}

#pragma mark - Tests

// Test that tapping the prerendered suggestions opens it.
// TODO(crbug.com/40833424): Reenable.
- (void)DISABLED_testTapPrerenderSuggestions {
  // TODO(crbug.com/40553918): Re-enable the test on iPad once the alternate
  // letters problem is fixed.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(
        @"Disabled for iPad due to alternate letters educational screen.");
  }

  [self addURLToHistory];
  const GURL pageURL = self.testServer->GetURL(kPageURL);
  NSString* pageString = base::SysUTF8ToNSString(pageURL.GetContent());

  static int visitCountBeforePrerender = _visitCounter;

  // Type the beginning of the address to have the autocomplete suggestion.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(
                        [pageString substringToIndex:[pageString length] - 6])];

  // Wait until prerender request reaches the server.
  bool prerendered = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return self->_visitCounter == visitCountBeforePrerender + 1;
  });
  GREYAssertTrue(prerendered, @"Prerender did not happen");
  // Make sure the omnibox is autocompleted.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(pageString),
                                          grey_ancestor(grey_kindOfClassName(
                                              @"OmniboxTextFieldIOS")),
                                          nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open the suggestion. The suggestion needs to be the first suggestion to
  // have the prerenderer activated.
  id<GREYMatcher> rowMatcher = grey_allOf(
      grey_accessibilityValue(pageString),
      grey_ancestor(grey_accessibilityID(@"omnibox suggestion 0 0")),
      chrome_test_util::OmniboxPopupRow(), grey_sufficientlyVisible(), nil);

  [[EarlGrey selectElementWithMatcher:rowMatcher] performAction:grey_tap()];

  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
  GREYAssertEqual(visitCountBeforePrerender + 1, _visitCounter,
                  @"Prerender should have been the last load");
}

// Tests that tapping the prerendered suggestions keeps the UserAgent of the
// previous page.
// TODO(crbug.com/40708867): Test is flaky.
- (void)DISABLED_testUserAgentTypeInPreviousLoad {
  [self addURLToHistory];
  const GURL pageURL = self.testServer->GetURL(kPageURL);
  NSString* pageString = base::SysUTF8ToNSString(pageURL.GetContent());

  static int visitCountBeforePrerender = _visitCounter;

  // Load the UserAgent page.
  const GURL userAgentPageURL = self.testServer->GetURL(kUserAgentPageURL);
  [ChromeEarlGrey loadURL:userAgentPageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  // Type the beginning of the address to have the autocomplete suggestion.
  [ChromeEarlGreyUI focusOmnibox];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(
                        [pageString substringToIndex:[pageString length] - 6])];

  // Wait until prerender request reaches the server.
  bool prerendered = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return self->_visitCounter == visitCountBeforePrerender + 1;
  });
  GREYAssertTrue(prerendered, @"Prerender did not happen");

  // Open the suggestion. The suggestion needs to be the first suggestion to
  // have the prerenderer activated.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(pageString),
                                   grey_kindOfClassName(@"FadeTruncatingLabel"),
                                   grey_ancestor(grey_accessibilityID(
                                       @"omnibox suggestion 0 0")),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];

  GREYAssertEqual(visitCountBeforePrerender + 1, _visitCounter,
                  @"Prerender should have been the last load");

  // Request the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() performAction:grey_tap()];

  prerendered = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return self->_visitCounter == visitCountBeforePrerender + 2;
  });
  GREYAssertTrue(prerendered, @"Page wasn't reloaded");

  // Verify that going back returns to the mobile site.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];
  // The content of the page can be cached, check the button also.
  [ChromeEarlGreyUI openToolsMenu];
  [RequestDesktopButton() assertWithMatcher:grey_notNil()];
}

// Regression test for crbug.com/1477499. Tests that a pre-rendered tab doesn't
// lead to an incorrect data source, as can be seen after moving it in the tab
// strip.
// TODO(crbug.com/324216491): Test is flaky on devices.
- (void)MAYBE_testMovePrerenderedTabInTabStrip {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped for iPhone. The test makes use of the tab strip.");
  }

  // Do the steps to add a URL to the history to make it available via inline
  // autocomplete.
  [self addURLToHistory];
  const GURL pageURL = self.testServer->GetURL(kPageURL);
  NSString* pageString = base::SysUTF8ToNSString(pageURL.GetContent());

  static int visitCountBeforePrerender = _visitCounter;

  // Load the UserAgent page.
  const GURL userAgentPageURL = self.testServer->GetURL(kUserAgentPageURL);
  [ChromeEarlGrey loadURL:userAgentPageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  // Type the beginning of the address to have the autocomplete suggestion.
  [ChromeEarlGreyUI focusOmnibox];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(
                        [pageString substringToIndex:[pageString length] - 6])];

  // Wait until prerender request reaches the server.
  bool prerendered = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return self->_visitCounter == visitCountBeforePrerender + 1;
  });
  GREYAssertTrue(prerendered, @"Prerender did not happen");

  // Open the suggestion. The suggestion needs to be the first suggestion to
  // have the prerenderer activated.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(pageString),
                                   grey_kindOfClassName(@"FadeTruncatingLabel"),
                                   grey_ancestor(grey_accessibilityID(
                                       @"omnibox suggestion 0 0")),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];

  GREYAssertEqual(visitCountBeforePrerender + 1, _visitCounter,
                  @"Prerender should have been the last load");

  // Open another tab.
  [ChromeEarlGrey openNewTab];

  // Then move the tab that got prerendered to a different position.
  LegacyLongPressAndDragTabInTabStrip(
      [NSString stringWithUTF8String:kPageTitle], @"New Tab");
}

// Regression test for crbug.com/1482622. Tests that a pre-rendered tab doesn't
// lead to an incorrect data source, as can be seen after opening a new tab in
// the background before the pre-rendered tab.
- (void)MAYBE_testLegacyOpenTabInTabStripBeforePrerenderedTab {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped for iPhone. The test makes use of the tab strip.");
  }

  // Do the steps to add a URL to the history to make it available via inline
  // autocomplete.
  [self addURLToHistory];
  const GURL pageURL = self.testServer->GetURL(kPageURL);
  NSString* pageString = base::SysUTF8ToNSString(pageURL.GetContent());

  static int visitCountBeforePrerender = _visitCounter;

  // Open a first tab.
  [ChromeEarlGrey openNewTab];

  // Open a second tab and load the UserAgent page.
  const GURL userAgentPageURL = self.testServer->GetURL(kUserAgentPageURL);
  [ChromeEarlGrey loadURL:userAgentPageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  // Type the beginning of the address to have the autocomplete suggestion.
  NSString* typedText = [pageString substringToIndex:(pageString.length - 6)];
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:typedText];

  // Wait until prerender request reaches the server.
  bool prerendered = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return self->_visitCounter == visitCountBeforePrerender + 1;
  });
  GREYAssertEqual(_visitCounter, visitCountBeforePrerender + 1,
                  @"Visit count mismatch");
  GREYAssertTrue(prerendered, @"Prerender did not happen");

  // Open the suggestion. The suggestion needs to be the first suggestion to
  // have the prerenderer activated.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(pageString),
                                   grey_kindOfClassName(@"FadeTruncatingLabel"),
                                   grey_ancestor(grey_accessibilityID(
                                       @"omnibox suggestion 0 0")),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];

  GREYAssertEqual(visitCountBeforePrerender + 1, _visitCounter,
                  @"Prerender should have been the last load");

  // Go back to the first tab.
  [LegacyTabStripTabWithIdentifier(@"New Tab") tap];

  // Open the first Most Visited tile in a background tab (thus opening it
  // between the current NTP, and the prerendered tab).
  id<GREYMatcher> firstMostVisitedTile = grey_accessibilityID(
      [kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix
          stringByAppendingString:@"0"]);
  [[EarlGrey selectElementWithMatcher:firstMostVisitedTile]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForMainTabCount:3];
}

// Regression test for crbug.com/1482622. Tests that a pre-rendered tab doesn't
// lead to an incorrect data source, as can be seen after opening a new tab in
// the background before the pre-rendered tab.
- (void)MAYBE_testOpenTabInTabStripBeforePrerenderedTab {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped for iPhone. The test makes use of the tab strip.");
  }

  // Do the steps to add a URL to the history to make it available via inline
  // autocomplete.
  [self addURLToHistory];
  const GURL pageURL = self.testServer->GetURL(kPageURL);
  NSString* pageString = base::SysUTF8ToNSString(pageURL.GetContent());

  static int visitCountBeforePrerender = _visitCounter;

  // Open a first tab.
  [ChromeEarlGrey openNewTab];

  // Open a second tab and load the UserAgent page.
  const GURL userAgentPageURL = self.testServer->GetURL(kUserAgentPageURL);
  [ChromeEarlGrey loadURL:userAgentPageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kMobileSiteLabel];

  // Type the beginning of the address to have the autocomplete suggestion.
  NSString* beginningOfAddress =
      [pageString substringToIndex:pageString.length - 6];
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:beginningOfAddress];

  // Wait until prerender request reaches the server.
  bool prerendered = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return self->_visitCounter == visitCountBeforePrerender + 1;
  });
  GREYAssertEqual(_visitCounter, visitCountBeforePrerender + 1,
                  @"Visit count mismatch");
  GREYAssertTrue(prerendered, @"Prerender did not happen");

  // Open the suggestion. The suggestion needs to be the first suggestion to
  // have the prerenderer activated.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(pageString),
                                   grey_kindOfClassName(@"FadeTruncatingLabel"),
                                   grey_ancestor(grey_accessibilityID(
                                       @"omnibox suggestion 0 0")),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];

  GREYAssertEqual(visitCountBeforePrerender + 1, _visitCounter,
                  @"Prerender should have been the last load");

  // Go back to the first tab.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(@"New Tab"),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Open the first Most Visited tile in a background tab (thus opening it
  // between the current NTP, and the prerendered tab).
  id<GREYMatcher> firstMostVisitedTile = grey_accessibilityID(
      [kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix
          stringByAppendingString:@"0"]);
  [[EarlGrey selectElementWithMatcher:firstMostVisitedTile]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForMainTabCount:3];
}

@end
