// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include <memory>
#include <vector>

#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_command_line.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/mock_content_suggestions_provider.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_header_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_learn_more_item.h"
#include "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_provider_test_singleton.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_test_utils.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/history_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/strings/grit/ui_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using namespace ntp_snippets;
using testing::_;
using testing::Invoke;
using testing::WithArg;

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

// Returns a suggestion created from the |category|, |suggestion_id| and the
// |url|.
ContentSuggestion Suggestion(Category category,
                             std::string suggestion_id,
                             GURL url) {
  ContentSuggestion suggestion(category, suggestion_id, url);
  suggestion.set_title(base::UTF8ToUTF16(url.spec()));

  return suggestion;
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

// Current non-incognito browser state.
@property(nonatomic, assign, readonly) ios::ChromeBrowserState* browserState;
// Mock provider from the singleton.
@property(nonatomic, assign, readonly) MockContentSuggestionsProvider* provider;
// Article category, used by the singleton.
@property(nonatomic, assign, readonly) Category category;

@end

@implementation ContentSuggestionsTestCase

#pragma mark - Setup/Teardown

+ (void)setUp {
  [super setUp];

  [self closeAllTabs];
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();

  // Sets the ContentSuggestionsService associated with this browserState to a
  // service with no provider registered, allowing to register fake providers
  // which do not require internet connection. The previous service is deleted.
  IOSChromeContentSuggestionsServiceFactory::GetInstance()->SetTestingFactory(
      browserState,
      base::BindRepeating(&CreateChromeContentSuggestionsService));

  ContentSuggestionsService* service =
      IOSChromeContentSuggestionsServiceFactory::GetForBrowserState(
          browserState);
  [[ContentSuggestionsTestSingleton sharedInstance]
      registerArticleProvider:service];
}

+ (void)tearDown {
  [self closeAllTabs];
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();

  // Resets the Service associated with this browserState to a service with
  // default providers. The previous service is deleted.
  IOSChromeContentSuggestionsServiceFactory::GetInstance()->SetTestingFactory(
      browserState,
      base::BindRepeating(&CreateChromeContentSuggestionsServiceWithProviders));
  [super tearDown];
}

// Per crbug.com/845186, Disable flakey iPad Retina tests that are limited
// to iOS 10.2.
+ (NSArray*)testInvocations {
#if TARGET_IPHONE_SIMULATOR
  if (IsIPadIdiom() && !base::ios::IsRunningOnOrLater(10, 3, 0))
    return @[];
#endif  // TARGET_IPHONE_SIMULATOR
  return [super testInvocations];
}

- (void)setUp {
  self.provider->FireCategoryStatusChanged(self.category,
                                           CategoryStatus::AVAILABLE);
  [super setUp];
}

- (void)tearDown {
  self.provider->FireCategoryStatusChanged(
      self.category, CategoryStatus::ALL_SUGGESTIONS_EXPLICITLY_DISABLED);
  GREYAssertTrue(chrome_test_util::ClearBrowsingHistory(),
                 @"Clearing Browsing History timed out");
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
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
  std::vector<ContentSuggestion> suggestions;
  suggestions.emplace_back(
      Suggestion(self.category, "chromium1", GURL("http://chromium.org/1")));
  suggestions.emplace_back(
      Suggestion(self.category, "chromium2", GURL("http://chromium.org/2")));
  suggestions.emplace_back(
      Suggestion(self.category, "chromium3", GURL("http://chromium.org/3")));
  self.provider->FireSuggestionsChanged(self.category, std::move(suggestions));

  // Set up the action when "More" is tapped.
  AdditionalSuggestionsHelper helper(pageURL);
  EXPECT_CALL(*self.provider, FetchMock(_, _, _))
      .WillOnce(WithArg<2>(Invoke(
          &helper, &AdditionalSuggestionsHelper::SendAdditionalSuggestions)));

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
  chrome_test_util::SelectTabAtIndexInCurrentMode(0);

  // Make sure the additional items are still displayed.
  [CellWithMatcher(grey_accessibilityID(@"AdditionalSuggestion2"))
      assertWithMatcher:grey_notNil()];
}


// Tests that a switch for the ContentSuggestions exists in the settings. The
// behavior depends on having a real remote provider, so it cannot be tested
// here.
- (void)testPrivacySwitch {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsMenuPrivacyButton()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_IOS_OPTIONS_SEARCH_URL_SUGGESTIONS)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that when tapping a suggestion, it is opened. When going back, the
// disposition of the collection takes into account the previous scroll, even
// when more is tapped.
- (void)testOpenPageAndGoBackWithMoreContent {
  // Set server up.
  self.testServer->RegisterRequestHandler(base::Bind(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  // Add 3 suggestions, persisted accross page loads.
  std::vector<ContentSuggestion> suggestions;
  suggestions.emplace_back(
      Suggestion(self.category, "chromium1", GURL("http://chromium.org/1")));
  suggestions.emplace_back(
      Suggestion(self.category, "chromium2", GURL("http://chromium.org/2")));
  suggestions.emplace_back(
      Suggestion(self.category, "chromium3", GURL("http://chromium.org/3")));
  self.provider->FireSuggestionsChanged(self.category, std::move(suggestions));

  // Set up the action when "More" is tapped.
  AdditionalSuggestionsHelper helper(pageURL);
  EXPECT_CALL(*self.provider, FetchMock(_, _, _))
      .WillOnce(WithArg<2>(Invoke(
          &helper, &AdditionalSuggestionsHelper::SendAdditionalSuggestions)));

  // Tap on more, which adds 10 elements.
  [CellWithMatcher(chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_CONTENT_SUGGESTIONS_FOOTER_TITLE)) performAction:grey_tap()];

  // Make sure to scroll to the bottom.
  [CellWithMatcher(grey_accessibilityID(
      [ContentSuggestionsLearnMoreItem accessibilityIdentifier]))
      assertWithMatcher:grey_notNil()];

  // Open the last item.
  [CellWithMatcher(grey_accessibilityID(@"AdditionalSuggestion9"))
      performAction:grey_tap()];

  // Check that the page has been opened.
  [ChromeEarlGrey waitForWebViewContainingText:kPageLoadedString];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          pageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:1];
  [ChromeEarlGrey waitForIncognitoTabCount:0];

  // Go back.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];

  // Test that the omnibox is visible and taking full width, before any scroll
  // happen on iPhone.
  if (!IsRegularXRegularSizeClass()) {
    // Test that the omnibox is still pinned to the top of the screen and
    // under the safe area.
    CGFloat safeAreaTop = 0;
    if (@available(iOS 11, *)) {
      safeAreaTop = ntp_home::CollectionView().safeAreaInsets.top;
    } else {
      safeAreaTop = StatusBarHeight();
    }

    CGFloat contentOffset = ntp_home::CollectionView().contentOffset.y;
    CGFloat fakeOmniboxOrigin = ntp_home::FakeOmnibox().frame.origin.y;
    CGFloat pinnedOffset = contentOffset - (fakeOmniboxOrigin - safeAreaTop);
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            [ContentSuggestionsHeaderItem
                                                accessibilityIdentifier])]
        assertWithMatcher:ntp_home::HeaderPinnedOffset(pinnedOffset)];
  }

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
  [[[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                           [ContentSuggestionsLearnMoreItem
                                               accessibilityIdentifier])]
         usingSearchAction:action
      onElementWithMatcher:chrome_test_util::ContentSuggestionCollectionView()]
      assertWithMatcher:grey_nil()];

  std::vector<ContentSuggestion> suggestions;
  suggestions.emplace_back(
      Suggestion(self.category, "chromium", GURL("http://chromium.org")));
  self.provider->FireSuggestionsChanged(self.category, std::move(suggestions));

  [CellWithMatcher(grey_accessibilityID(
      [ContentSuggestionsLearnMoreItem accessibilityIdentifier]))
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
  chrome_test_util::SelectTabAtIndexInCurrentMode(1);
  [ChromeEarlGrey waitForWebViewContainingText:kPageLoadedString];
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
  [ChromeEarlGrey waitForWebViewContainingText:kPageLoadedString];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          pageURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  GREYAssertTrue(chrome_test_util::IsIncognitoMode(),
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
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(
                                pageTitle),
                            grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

// Tests that the context menu has the correct actions.
- (void)testMostVisitedLongPress {
  [self setupMostVisitedTileLongPress];

  if (!IsRegularXRegularSizeClass()) {
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

#pragma mark - Properties

- (ios::ChromeBrowserState*)browserState {
  return chrome_test_util::GetOriginalBrowserState();
}

- (MockContentSuggestionsProvider*)provider {
  return [[ContentSuggestionsTestSingleton sharedInstance] provider];
}

- (Category)category {
  return Category::FromKnownCategory(KnownCategories::ARTICLES);
}

#pragma mark - Test utils

// Setup a most visited tile, and open the context menu by long pressing on it.
- (void)setupMostVisitedTileLongPress {
  self.testServer->RegisterRequestHandler(base::Bind(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);
  NSString* pageTitle = base::SysUTF8ToNSString(kPageTitle);

  // Clear history and verify that the tile does not exist.
  GREYAssertTrue(chrome_test_util::ClearBrowsingHistory(),
                 @"Clearing Browsing History timed out");
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebViewContainingText:kPageLoadedString];

  // After loading URL, need to do another action before opening a new tab
  // with the icon present.
  [ChromeEarlGrey goBack];

  [[self class] closeAllTabs];
  chrome_test_util::OpenNewTab();
  // TODO(crbug.com/783192): ChromeEarlGrey should have a method to open a new
  // tab and synchronize with the UI.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(pageTitle)]
      performAction:grey_longPress()];
}

@end
