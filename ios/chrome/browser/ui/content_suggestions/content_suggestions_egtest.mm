// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_app_interface.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/mac/url_conversions.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/strings/grit/ui_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-extra-semi"
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(ContentSuggestionsAppInterface);
#pragma clang diagnostic pop
#endif  // defined(CHROME_EARL_GREY_2)

namespace {

const char kPageLoadedString[] = "Page loaded!";
const char kPageURL[] = "/test-page.html";
const char kPageTitle[] = "Page title!";

//  Scrolls the collection view in order to have the toolbar menu icon visible.
void ScrollUp() {
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(chrome_test_util::ToolsMenuButton(),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionUp, 150)
      onElementWithMatcher:chrome_test_util::ContentSuggestionCollectionView()]
      assertWithMatcher:grey_notNil()];
}

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

// Select the cell with the |matcher| by scrolling the collection.
// 200 is a reasonable scroll displacement that works for all UI elements, while
// not being too slow.
GREYElementInteraction* CellWithMatcher(id<GREYMatcher> matcher) {
  // Start the scroll from the middle of the screen in case the bottom of the
  // screen is obscured by the bottom toolbar.
  id<GREYAction> action =
      grey_scrollInDirectionWithStartPoint(kGREYDirectionDown, 230, 0.5, 0.5);
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, grey_sufficientlyVisible(),
                                          nil)]
         usingSearchAction:action
      onElementWithMatcher:chrome_test_util::ContentSuggestionCollectionView()];
}

}  // namespace

#pragma mark - TestCase

// Test case for the ContentSuggestion UI.
@interface ContentSuggestionsTestCase : ChromeTestCase

@end

@implementation ContentSuggestionsTestCase

#pragma mark - Setup/Teardown

#if defined(CHROME_EARL_GREY_2)
+ (void)setUpForTestCase {
  [super setUpForTestCase];
  [self setUpHelper];
}
#elif defined(CHROME_EARL_GREY_1)
+ (void)setUp {
  [super setUp];
  [self setUpHelper];
}
#else
#error Not an EarlGrey Test
#endif

+ (void)setUpHelper {
  [self closeAllTabs];

  [ContentSuggestionsAppInterface setUpService];
}

+ (void)tearDown {
  [self closeAllTabs];

  [ContentSuggestionsAppInterface resetService];

  [super tearDown];
}

- (void)setUp {
  [super setUp];
  [ContentSuggestionsAppInterface makeSuggestionsAvailable];
}

- (void)tearDown {
  [ContentSuggestionsAppInterface disableSuggestions];
  [ChromeEarlGrey clearBrowsingHistory];
  [super tearDown];
}

#pragma mark - Tests

// Tests that the additional items (when more is pressed) are kept when
// switching tabs.
- (void)testAdditionalItemsKept {
  // Set server up.
  self.testServer->RegisterRequestHandler(base::Bind(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  // Add 3 suggestions, persisted accross page loads.
  [ContentSuggestionsAppInterface
        addNumberOfSuggestions:3
      additionalSuggestionsURL:net::NSURLWithGURL(pageURL)];

  // Tap on more, which adds 10 elements.
  [CellWithMatcher(chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_CONTENT_SUGGESTIONS_FOOTER_TITLE)) performAction:grey_tap()];

  // Make sure some items are loaded.
  [CellWithMatcher(grey_accessibilityID(@"AdditionalSuggestion2"))
      assertWithMatcher:grey_notNil()];

  // Open a new Tab.
  ScrollUp();
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey waitForMainTabCount:2];

  // Go back to the previous tab.
  [ChromeEarlGrey selectTabAtIndex:0];

  // Make sure the additional items are still displayed.
  [CellWithMatcher(grey_accessibilityID(@"AdditionalSuggestion2"))
      assertWithMatcher:grey_notNil()];
}

// Tests that when the page is reloaded using the tools menu, the suggestions
// are updated.
- (void)testReloadPage {
  // Add 2 suggestions, persisted accross page loads.
  [ContentSuggestionsAppInterface addNumberOfSuggestions:2
                                additionalSuggestionsURL:nil];

  // Change the suggestions to have one the second one.
  [ContentSuggestionsAppInterface addSuggestionNumber:2];

  // Check that the first suggestion is still displayed.
  [CellWithMatcher(grey_accessibilityID(@"http://chromium.org/1"))
      assertWithMatcher:grey_notNil()];

  // Reload the page using the tools menu.
  [ChromeEarlGreyUI reload];

  // Check that the first suggestion is no longer displayed.
  [CellWithMatcher(grey_accessibilityID(@"http://chromium.org/1"))
      assertWithMatcher:grey_nil()];
  [CellWithMatcher(grey_accessibilityID(@"http://chromium.org/2"))
      assertWithMatcher:grey_notNil()];
}

// Tests that when tapping a suggestion, it is opened. When going back, the
// disposition of the collection takes into account the previous scroll, even
// when more is tapped.
- (void)testOpenPageAndGoBackWithMoreContent {
// TODO(crbug.com/786960): re-enable when fixed. Tests may pass on EG2
#if defined(CHROME_EARL_GREY_1)
  EARL_GREY_TEST_DISABLED(@"Fails on iOS 11.0.");
#endif

  // Set server up.
  self.testServer->RegisterRequestHandler(base::Bind(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  // Add 3 suggestions, persisted accross page loads.
  [ContentSuggestionsAppInterface
        addNumberOfSuggestions:3
      additionalSuggestionsURL:net::NSURLWithGURL(pageURL)];

  // Tap on more, which adds 10 elements.
  [CellWithMatcher(chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_CONTENT_SUGGESTIONS_FOOTER_TITLE)) performAction:grey_tap()];

  // Make sure to scroll to the bottom.
  [CellWithMatcher(grey_accessibilityID(kContentSuggestionsLearnMoreIdentifier))
      assertWithMatcher:grey_notNil()];

  // Open the last item. After the extra space of the last suggestion is
  // removed, this test case fails on iPhoneX. Double-Tap on the last suggestion
  // is a workaround.
  // TODO(crbug.com/979143): Find out the reason and fix it. Also consider
  // converting the test case to EG2 or deprecating MDCCollectionView.
  [CellWithMatcher(grey_accessibilityID(@"AdditionalSuggestion9"))
      performAction:grey_doubleTap()];

  // Check that the page has been opened.
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          pageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:1];
  [ChromeEarlGrey waitForIncognitoTabCount:0];

  // Go back.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];

  // Check that the first items are visible as the collection should be
  // scrolled.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(@"http://chromium.org/3")]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the "Learn More" cell is present only if there is a suggestion in
// the section.
- (void)testLearnMore {
  id<GREYAction> action =
      grey_scrollInDirectionWithStartPoint(kGREYDirectionDown, 200, 0.5, 0.5);
  [[[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kContentSuggestionsLearnMoreIdentifier)]
         usingSearchAction:action
      onElementWithMatcher:chrome_test_util::ContentSuggestionCollectionView()]
      assertWithMatcher:grey_nil()];

  [ContentSuggestionsAppInterface addNumberOfSuggestions:1
                                additionalSuggestionsURL:nil];

  [CellWithMatcher(grey_accessibilityID(kContentSuggestionsLearnMoreIdentifier))
      assertWithMatcher:grey_sufficientlyVisible()];
}

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
    [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                            ContentSuggestionCollectionView()]
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
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ButtonWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)]
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
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_NEW_TAB_MOST_VISITED_ITEM_REMOVED)]
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

  if (![ChromeEarlGrey isRegularXRegularSizeClass]) {
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ButtonWithAccessibilityLabelId(
                       IDS_APP_CANCEL)] assertWithMatcher:grey_interactable()];
  }

  // No read later.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST)]
      assertWithMatcher:grey_nil()];
}

#pragma mark - Test utils

// Setup a most visited tile, and open the context menu by long pressing on it.
- (void)setupMostVisitedTileLongPress {
  self.testServer->RegisterRequestHandler(base::Bind(&StandardResponse));
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

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(pageTitle)]
      performAction:grey_longPress()];
}

@end
