// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/search_engines/model/search_engines_app_interface.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/browser_container/edit_menu_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/base/url_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

namespace {

const char kElementToLongPress[] = "selectid";

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
    "    This text contains a <span id='selectid'>text</span>.<br/><br/><br/>"
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

  if (request_url.path_piece() == kBasicSelectionUrl) {
    http_response->set_content(kBasicSelectionHtmlTemplate);
  } else if (request_url.path_piece() == kSearchResultUrl) {
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

// Go through the pages and find the element with accessibility
// `accessibility_label`. Returns whether the action can be found.
bool FindEditMenuAction(NSString* accessibility_label) {
  // The menu should be visible.
  [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface editMenuMatcher]]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Start on first screen (previous not visible or disabled).
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface
                                          editMenuPreviousButtonMatcher]]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                   nil)
                  error:&error];
  GREYAssert(error, @"FindEditMenuAction not called on the first page.");
  error = nil;
  [[[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              [EditMenuAppInterface
                  editMenuActionWithAccessibilityLabel:accessibility_label],
              grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_tap()
      onElementWithMatcher:[EditMenuAppInterface editMenuNextButtonMatcher]]
      assertWithMatcher:grey_sufficientlyVisible()
                  error:&error];
  return !error;
}

// Long presses on `element_id`.
void LongPressElement(const char* element_id) {
  // Use triggers_context_menu = true as this is really "triggers_browser_menu".
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:element_id],
                        true)];
}

// Convenient function to trigger the Edit Menu on `kElementToLongPress`.
// TODO(crbug.com/40264849): extract this function and have all edit menu
// tests use it.
void TriggerEditMenu() {
  [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface editMenuMatcher]]
      assertWithMatcher:grey_notVisible()];
  LongPressElement(kElementToLongPress);

  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface editMenuMatcher]]
      assertWithMatcher:grey_sufficientlyVisible()
                  error:&error];
  if (error) {
    // If edit is not visible, try to tap the element again.
    // This is possible on inputs when the first long press just selects the
    // input.
    LongPressElement(kElementToLongPress);
    [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface editMenuMatcher]]
        assertWithMatcher:grey_sufficientlyVisible()];
  }
}

}  // namespace

// Tests for the Search With Edit menu entry.
@interface SearchWithMediatorTestCase : ChromeTestCase
@property(nonatomic, strong) NSString* defaultSearchEngine;
@end

@implementation SearchWithMediatorTestCase

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

- (void)tearDown {
  [SearchEnginesAppInterface setSearchEngineTo:self.defaultSearchEngine];
  [SearchEnginesAppInterface removeSearchEngineWithName:@"test"];
  [super tearDown];
}

// Conveniently load a page that has "text" in a selectable field.
- (void)loadPage {
  GURL url = self.testServer->GetURL(kBasicSelectionUrl);
  [ChromeEarlGrey loadURL:url];
  [ChromeEarlGrey waitForWebStateContainingText:"Page Loaded"];
  [ChromeEarlGrey waitForWebStateZoomScale:1.0];
}

- (void)testSearchWith {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    // Feature disabled on iOS15-.
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS15-");
  }
  [self loadPage];
  TriggerEditMenu();
  bool found = FindEditMenuAction(@"Search with test");
  GREYAssertTrue(found, @"Search Web button not found");
  [[EarlGrey selectElementWithMatcher:
                 [EditMenuAppInterface
                     editMenuActionWithAccessibilityLabel:@"Search with test"]]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:"Search Result"];
  [ChromeEarlGrey waitForWebStateContainingText:"text"];
  GREYAssertEqual(2, [ChromeEarlGrey mainTabCount],
                  @"Search Should be in new tab");
}

- (void)testSearchWithIncognito {
  if (!base::ios::IsRunningOnIOS16OrLater()) {
    // Feature disabled on iOS15-.
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS15-");
  }
  [ChromeEarlGrey openNewIncognitoTab];
  [self loadPage];
  TriggerEditMenu();
  bool found = FindEditMenuAction(@"Search with test");
  GREYAssertTrue(found, @"Search Web button not found");
  [[EarlGrey selectElementWithMatcher:
                 [EditMenuAppInterface
                     editMenuActionWithAccessibilityLabel:@"Search with test"]]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:"Search Result"];
  [ChromeEarlGrey waitForWebStateContainingText:"text"];
  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Incognito search should stay in incognito");
  GREYAssertEqual(2, [ChromeEarlGrey incognitoTabCount],
                  @"Search Should be in new tab");
}

@end
