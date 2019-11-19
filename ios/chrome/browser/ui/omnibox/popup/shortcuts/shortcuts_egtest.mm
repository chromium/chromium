// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_constants.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Web page that will show up as one of the most visited tiles.
const char kTilePageLoadedString[] = "This is a web page that you visit often";
const char kTilePageURL[] = "/tile-page.html";
const char kTilePageTitle[] = "Often visited page";

// Web page for navigation.
const char kPageLoadedString[] = "Page loaded!";
const char kPageURL[] = "/test-page.html";
const char kPageTitle[] = "Page title!";

// Provides responses for redirect and changed window location URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  if (request.relative_url == kPageURL) {
    http_response->set_content("<html><head><title>" + std::string(kPageTitle) +
                               "</title></head><body>" +
                               std::string(kPageLoadedString) +
                               "</body></html>");
    return std::move(http_response);
  }

  if (request.relative_url == kTilePageURL) {
    http_response->set_content(
        "<html><head><title>" + std::string(kTilePageTitle) +
        "</title></head><body>" + std::string(kTilePageLoadedString) +
        "</body></html>");
    return std::move(http_response);
  }

  return nil;
}

}  //  namespace

// Test case for the Omnibox Shortcuts UI.
@interface ShortcutsTestCase : ChromeTestCase
@end

@implementation ShortcutsTestCase {
  base::test::ScopedFeatureList _featureList;
}

- (void)setUp {
  [super setUp];
  // Enable the shortcuts flag.
  _featureList.InitAndEnableFeature(kOmniboxPopupShortcutIconsInZeroState);

  // Start a server to be able to navigate to a web page.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  [ChromeEarlGrey clearBrowsingHistory];
  [self prepareMostVisitedTiles];
  // Clear pasteboard
  [[UIPasteboard generalPasteboard] setItems:@[]];
}

#pragma mark - tests

// Tests that the shortcuts show up on a web page.
- (void)testShowsUp {
  [self navigateToAPage];
  [ChromeEarlGreyUI focusOmnibox];
  [[EarlGrey selectElementWithMatcher:[self mostVisitedTileMatcher]]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that shortcuts don't show up when there are other omnibox suggestions
// available, for example "what you typed" suggestion.
- (void)testNotShownWhenSuggestionsAvailable {
  [self navigateToAPage];
  [ChromeEarlGreyUI focusOmniboxAndType:@"foo"];
  [[EarlGrey selectElementWithMatcher:[self mostVisitedTileMatcher]]
      assertWithMatcher:grey_nil()];
}

- (void)testShowsUpWhenOmniboxIsEmpty {
  [self navigateToAPage];
  // Focus omnibox and hit backspace to erase the text.
  [ChromeEarlGreyUI focusOmniboxAndType:@"\b"];
  [[EarlGrey selectElementWithMatcher:[self mostVisitedTileMatcher]]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Test that tapping a most visited tile navigates to that page.
- (void)testTapMostVisitedTile {
  [self navigateToAPage];
  [ChromeEarlGreyUI focusOmnibox];
  [[EarlGrey selectElementWithMatcher:[self mostVisitedTileMatcher]]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kTilePageLoadedString];
}

- (void)testBookmarksShortcut {
  [self navigateToAPage];
  [ChromeEarlGreyUI focusOmnibox];

  // Tap on bookmarks.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(
                     @"Bookmarks")] performAction:grey_tap()];

  // Verify that the bookmarks dialog opened with Done button and "Bookmarks"
  // title.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityTrait(
                                              UIAccessibilityTraitHeader),
                                          grey_accessibilityLabel(@"Bookmarks"),
                                          nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          BookmarksNavigationBarDoneButton()]
      performAction:grey_tap()];

  // Verify that after tapping Done the omnibox is defocused.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testReadingListShortcut {
  [self navigateToAPage];
  [ChromeEarlGreyUI focusOmnibox];

  // Tap on reading list.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(
                     @"Reading List")] performAction:grey_tap()];

  // Verify that the reading list dialog opened with Done button and "Reading
  // List" title.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitHeader),
                                   grey_accessibilityLabel(@"Reading List"),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:grey_buttonTitle(@"Done")]
      performAction:grey_tap()];

  // Verify that after tapping Done the omnibox is defocused.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testRecentTabsShortcut {
  [self navigateToAPage];
  [ChromeEarlGreyUI focusOmnibox];

  // Tap on recent tabs.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(
                     @"Recent Tabs")] performAction:grey_tap()];

  // Verify that the Recent Tabs dialog opened with Done button and "Recent
  // Tabs" title.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitHeader),
                                   grey_accessibilityLabel(@"Recent Tabs"),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:grey_buttonTitle(@"Done")]
      performAction:grey_tap()];

  // Verify that after tapping Done the omnibox is defocused.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testHistoryShortcut {
  [self navigateToAPage];
  [ChromeEarlGreyUI focusOmnibox];

  // Tap on history.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabel(@"History")]
      performAction:grey_tap()];

  // Verify that the History dialog opened with Done button and "History"
  // title.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityTrait(
                                              UIAccessibilityTraitHeader),
                                          grey_accessibilityLabel(@"History"),
                                          nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:grey_buttonTitle(@"Done")]
      performAction:grey_tap()];

  // Verify that after tapping Done the omnibox is defocused.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that on the NTP the shortcuts don't show up.
- (void)testNTPShortcutsDontShowUp {
  [[self class] closeAllTabs];
  [ChromeEarlGrey openNewTab];

  // Tap the fake omnibox.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_tap()];
  // Wait for the real omnibox to be visible.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];

  // The shortcuts should not show up here.
  // The shortcuts are similar to the NTP tiles, so in this test it's necessary
  // to differentiate where the tile is actually coming from; therefore check
  // the a11y identifier of the shortcuts.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kShortcutsAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
}

#pragma mark - helpers

- (void)navigateToAPage {
  const GURL pageURL = self.testServer->GetURL(kPageURL);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
}

- (void)prepareMostVisitedTiles {
  const GURL pageURL = self.testServer->GetURL(kTilePageURL);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kTilePageLoadedString];

  // After loading URL, need to do another action before opening a new tab
  // with the icon present.
  [ChromeEarlGrey goBack];

  [[self class] closeAllTabs];
  [ChromeEarlGrey openNewTab];
}

- (id<GREYMatcher>)mostVisitedTileMatcher {
  NSString* pageTitle = base::SysUTF8ToNSString(kTilePageTitle);
  return chrome_test_util::StaticTextWithAccessibilityLabel(pageTitle);
}

@end
