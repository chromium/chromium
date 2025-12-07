// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/browser_container/ui_bundled/edit_menu_app_interface.h"
#import "ios/chrome/browser/browser_container/ui_bundled/edit_menu_matchers.h"
#import "ios/chrome/browser/popup_menu/ui_bundled/popup_menu_constants.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/ui/constants.h"
#import "ios/chrome/browser/search_engines/model/search_engines_app_interface.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/base/url_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

namespace {

const char kCSSSelectorToLongPress[] = "em";

// Returns an ElementSelector for `ElementToLongPress`.
ElementSelector* ElementToLongPressSelector() {
  return [ElementSelector selectorWithCSSSelector:kCSSSelectorToLongPress];
}

// An HTML template that puts some text in a simple span element.
const char kBasicSelectionUrl[] = "/basic";
const char kBasicSelectionHtmlTemplate[] =
    "<html>"
    "  <head>"
    "    <meta name='viewport' content='width=device-width, "
    "      initial-scale=1.0, maximum-scale=1.0, user-scalable=no' "
    "    />"
    "  </head>"
    "  <body>"
    "    Page Loaded <br/><br/>"
    "    This text contains a <em>text</em>.<br/><br/><br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "    Other very interesting text<br/>"
    "  </body>"
    "</html>";

// A fake search result page that displays the search query.
const char kSearchResultUrl[] = "/search";
const char kSearchResultHtmlTemplate[] =
    "<html>"
    "  <head>"
    "    <meta name='viewport' content='width=device-width, "
    "      initial-scale=1.0, maximum-scale=1.0, user-scalable=no' "
    "    />"
    "  </head>"
    "  <body>"
    "    Search Result <br/><br/>"
    "    SEARCH_QUERY"
    "  </body>"
    "</html>";

// Provides responses for initial page and destination URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  GURL request_url = request.GetURL();

  if (request_url.path() == kBasicSelectionUrl) {
    http_response->set_content(kBasicSelectionHtmlTemplate);
  } else if (request_url.path() == kSearchResultUrl) {
    std::string html = kSearchResultHtmlTemplate;
    std::string query;
    bool has_query = net::GetValueForKeyInQuery(request_url, "q", &query);
    if (has_query) {
      base::ReplaceFirstSubstringAfterOffset(&html, 0, "SEARCH_QUERY", query);
    }
    http_response->set_content(html);
  } else {
    return nullptr;
  }

  return std::move(http_response);
}

}  // namespace

// Tests for the Search With Edit menu entry.
@interface SearchWithMediatorTestCase : ChromeTestCase
@property(nonatomic, copy) NSString* defaultSearchEngine;
@end

@implementation SearchWithMediatorTestCase
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  if ([self isRunningTest:@selector(testSearchWithReaderMode)]) {
    config.features_enabled.push_back(kEnableReaderMode);
    config.features_enabled.push_back(kEnableReaderModeInUS);
  }
  return config;
}

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
  self.defaultSearchEngine = [SearchEnginesAppInterface defaultSearchEngine];
  GURL url = self.testServer->GetURL(kSearchResultUrl);
  [SearchEnginesAppInterface
      addSearchEngineWithName:@"test"
                          URL:base::SysUTF8ToNSString(url.spec() +
                                                      "?q={searchTerms}")
                   setDefault:YES];
}

- (void)tearDownHelper {
  [SearchEnginesAppInterface setSearchEngineTo:self.defaultSearchEngine];
  [SearchEnginesAppInterface removeSearchEngineWithName:@"test"];
  [super tearDownHelper];
}

// Conveniently load a page that has "text" in a selectable field.
- (void)loadPage {
  GURL url = self.testServer->GetURL(kBasicSelectionUrl);
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"Page Loaded"];
  [ChromeEarlGrey waitForWebStateZoomScale:1.0];
}

- (void)testSearchWith {
  [self loadPage];
  [ChromeEarlGreyUI triggerEditMenu:ElementToLongPressSelector()];
  id<GREYMatcher> matcher =
      FindEditMenuActionWithAccessibilityLabel(@"Search with test");
  GREYAssertNotEqual(matcher, nil, @"Search Web button not found");
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:"Search Result"];
  [ChromeEarlGrey waitForWebStateContainingText:"text"];
  GREYAssertEqual(2UL, [ChromeEarlGrey mainTabCount],
                  @"Search Should be in new tab");
}

- (void)testSearchWithIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [self loadPage];
  [ChromeEarlGreyUI triggerEditMenu:ElementToLongPressSelector()];
  id<GREYMatcher> matcher =
      FindEditMenuActionWithAccessibilityLabel(@"Search with test");
  GREYAssertNotEqual(matcher, nil, @"Search Web button not found");
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:"Search Result"];
  [ChromeEarlGrey waitForWebStateContainingText:"text"];
  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Incognito search should stay in incognito");
  GREYAssertEqual(2UL, [ChromeEarlGrey incognitoTabCount],
                  @"Search Should be in new tab");
}

- (void)testSearchWithReaderMode {
  [self loadPage];

  // Open Reader Mode UI.
  GREYAssertTrue(
      [ChromeEarlGrey showReaderModeAndWaitUntilReaderModeWebStateIsReady],
      @"Reader mode content could not be loaded");

  // Wait for Reader Mode UI to appear on-screen.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kReaderModeViewAccessibilityIdentifier)];

  [ChromeEarlGreyUI triggerEditMenu:ElementToLongPressSelector()];
  id<GREYMatcher> matcher =
      FindEditMenuActionWithAccessibilityLabel(@"Search with test");
  GREYAssertNotEqual(matcher, nil, @"Search Web button not found");
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:"Search Result"];
  [ChromeEarlGrey waitForWebStateContainingText:"text"];
  GREYAssertEqual(2UL, [ChromeEarlGrey mainTabCount],
                  @"Search Should be in new tab");
}

@end
