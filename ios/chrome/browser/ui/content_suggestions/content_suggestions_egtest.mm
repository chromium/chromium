// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>
#import <vector>

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/new_tab_page_app_interface.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/mac/url_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/strings/grit/ui_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kPageLoadedString[] = "Page loaded!";
const char kPageURL[] = "/test-page.html";
const char kPageTitle[] = "Page title!";

// Provides responses for redirect and changed window location URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kPageURL) {
    return nullptr;
  }
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content("<html><head><title>" + std::string(kPageTitle) +
                             "</title></head><body>" +
                             std::string(kPageLoadedString) + "</body></html>");
  return std::move(http_response);
}

}  // namespace

#pragma mark - TestCase

// Test case for the ContentSuggestion UI.
@interface ContentSuggestionsTestCase : ChromeTestCase

@end

@implementation ContentSuggestionsTestCase

#pragma mark - Setup/Teardown

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kEnableFeedAblation);
  return config;
}

+ (void)setUpForTestCase {
  [super setUpForTestCase];
  [self setUpHelper];
}

+ (void)setUpHelper {
  [self closeAllTabs];
}

+ (void)tearDown {
  [self closeAllTabs];

  [super tearDown];
}

- (void)setUp {
  [super setUp];
}

- (void)tearDown {
  [ChromeEarlGrey clearBrowsingHistory];
  [super tearDown];
}

#pragma mark - Tests

// Tests the "Open in New Tab" action of the Most Visited context menu.
- (void)testMostVisitedNewTab {
  [self setupMostVisitedTileLongPress];
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  // Open in new tab.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)]
      performAction:grey_tap()];

  // Check a new page in normal model is opened.
  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey waitForIncognitoTabCount:0];

  // Check that the tab has been opened in background.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             @"Collection view not visible");

  // Check the page has been correctly opened.
  [ChromeEarlGrey selectTabAtIndex:1];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          pageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests the "Open in New Incognito Tab" action of the Most Visited context
// menu.
- (void)testMostVisitedNewIncognitoTab {
  [self setupMostVisitedTileLongPress];
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  // Open in new incognito tab.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OpenLinkInIncognitoButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForMainTabCount:1];
  [ChromeEarlGrey waitForIncognitoTabCount:1];

  // Check that the tab has been opened in foreground.
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          pageURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Test did not switch to incognito");
}

// action.
- (void)testMostVisitedRemoveUndo {
  [self setupMostVisitedTileLongPress];
  const GURL pageURL = self.testServer->GetURL(kPageURL);
  NSString* pageTitle = base::SysUTF8ToNSString(kPageTitle);

  // Tap on remove.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_REMOVE)]
      performAction:grey_tap()];

  // Check the tile is removed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              chrome_test_util::StaticTextWithAccessibilityLabel(pageTitle),
              grey_sufficientlyVisible(), nil)] assertWithMatcher:grey_nil()];

  // Check the snack bar notifying the user that an element has been removed is
  // displayed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(@"MDCSnackbarMessageTitleAutomationIdentifier")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on undo.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE)]
      performAction:grey_tap()];

  // Check the tile is back.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:
            grey_allOf(
                chrome_test_util::StaticTextWithAccessibilityLabel(pageTitle),
                grey_sufficientlyVisible(), nil)]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };
  NSString* errorMessage =
      @"The tile wasn't added back after hitting 'Undo' on the snackbar";
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             errorMessage);

  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(
                                pageTitle),
                            grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

// Tests that the context menu has the correct actions.
- (void)testMostVisitedLongPress {
  [self setupMostVisitedTileLongPress];

  // No "Add to Reading List" item.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST)]
      assertWithMatcher:grey_nil()];
}

#pragma mark - Test utils

// Setup a most visited tile, and open the context menu by long pressing on it.
- (void)setupMostVisitedTileLongPress {
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);
  NSString* pageTitle = base::SysUTF8ToNSString(kPageTitle);

  // Clear history and verify that the tile does not exist.
  [ChromeEarlGrey clearBrowsingHistory];
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];

  // After loading URL, need to do another action before opening a new tab
  // with the icon present.
  [ChromeEarlGrey goBack];

  [[self class] closeAllTabs];
  [ChromeEarlGrey openNewTab];

  id<GREYMatcher> matcher =
      grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(pageTitle),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_longPress()];
}

@end
