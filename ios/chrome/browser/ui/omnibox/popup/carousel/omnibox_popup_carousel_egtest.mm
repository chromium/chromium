// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_app_interface.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_test_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/strings/grit/ui_strings.h"

using omnibox::Page;
using omnibox::PageContent;
using omnibox::PageTitle;
using omnibox::PageURL;

namespace {

/// Number of time URL is reloaded to add it to most visited sites.
const NSUInteger kMostVisitedLoadCount = 3;
/// Copy of `kCarouselCapacity` in OmniboxPopupCarouselCell
const NSUInteger kCarouselCapacity = 10;

#pragma mark Matchers

/// Returns the matcher for tile with `title`.
id<GREYMatcher> TileWithTitle(const std::string& title) {
  return grey_allOf(
      grey_accessibilityLabel(base::SysUTF8ToNSString(title)),
      grey_accessibilityID(kOmniboxCarouselControlLabelAccessibilityIdentifier),
      grey_interactable(), nil);
}

/// Returns the matcher for the carousel containing most visited tiles.
id<GREYMatcher> CarouselMatcher() {
  return grey_allOf(
      grey_accessibilityID(kOmniboxCarouselCellAccessibilityIdentifier),
      grey_interactable(), nil);
}

}  // namespace

@interface OmniboxPopupCarouselTestCase : ChromeTestCase

@end

@implementation OmniboxPopupCarouselTestCase

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
      base::BindRepeating(&omnibox::OmniboxHTTPResponses));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  [ChromeEarlGrey clearBrowsingHistory];

  // Block page 0 from top sites so it won't appear in most visited sites. Page
  // zero is used to navigate to the omnibox in `focusOmniboxFromWebPageZero`.
  GURL pageZeroURL = self.testServer->GetURL(PageURL(0));
  NSString* pageZeroURLSpec = base::SysUTF8ToNSString(pageZeroURL.spec());
  [OmniboxAppInterface blockURLFromTopSites:pageZeroURLSpec];
}

// Tests adding most visited tiles by visiting sites multiple times.
- (void)testAddingMostVisitedTiles {
  // TODO(crbug.com/40066782): Test consistently failed on ipad simulator.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Failing on iPad Simulator");
  }

  [self addNumberOfMostVisitedTiles:kCarouselCapacity];
  [self focusOmniboxFromWebPageZero];
  [[EarlGrey selectElementWithMatcher:TileWithTitle(PageTitle(1))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests tapping the first tile and scroll to tap the last tile.
- (void)testTappingAndScrollingMostVisitedTiles {
  [self addNumberOfMostVisitedTiles:kCarouselCapacity];

  // Test tapping the first tile.
  [self focusOmniboxFromWebPageZero];
  Page firstTilePage = Page(1);
  [[EarlGrey selectElementWithMatcher:TileWithTitle(PageTitle(firstTilePage))]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:PageContent(firstTilePage)];

  // Test tapping the last tile.
  [self focusOmniboxFromWebPageZero];
  Page lastTilePage = Page(kCarouselCapacity);
  [[[EarlGrey selectElementWithMatcher:TileWithTitle(PageTitle(lastTilePage))]
         usingSearchAction:grey_swipeSlowInDirection(kGREYDirectionLeft)
      onElementWithMatcher:CarouselMatcher()] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:PageContent(lastTilePage)];
}

#pragma mark - Context Menu

// Tests deleting most visited tiles from context menu.
- (void)testDeleteMostVisitedTiles {
  // Visit page 1 and 2 multiple times.
  [self addNumberOfMostVisitedTiles:2];
  id<GREYMatcher> tile1 = TileWithTitle(PageTitle(Page(1)));
  id<GREYMatcher> tile2 = TileWithTitle(PageTitle(Page(2)));

  [self focusOmniboxFromWebPageZero];
  // Delete tiles 1 and 2.
  [self deleteMostVisitedTile:tile1];
  [self deleteMostVisitedTile:tile2];

  [ChromeEarlGrey openNewTab];

  // Visit page 1, 2 and 3 multiple times.
  [self addNumberOfMostVisitedTiles:3];
  id<GREYMatcher> tile3 = TileWithTitle(PageTitle(Page(3)));

  [self focusOmniboxFromWebPageZero];
  // `tile1` should not be there since it was removed earlier.
  [[EarlGrey selectElementWithMatcher:tile1] assertWithMatcher:grey_nil()];
  // `tile2` should not be there since it was removed earlier.
  [[EarlGrey selectElementWithMatcher:tile2] assertWithMatcher:grey_nil()];
  // `tile3` should be the only tile visible.
  [[EarlGrey selectElementWithMatcher:tile3]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the carousel is not shown when there are no tiles.
- (void)testEmptyMostVisitedTiles {
  [self focusOmniboxFromWebPageZero];
  // There should be no carousel when no tiles have been added.
  [[EarlGrey selectElementWithMatcher:CarouselMatcher()]
      assertWithMatcher:grey_nil()];
}

// Tests that deleting all tiles hides the carousel.
- (void)testDeletingAllTilesHidesCarousel {
  // Add page 1 and 2 to most visited sites.
  [self addNumberOfMostVisitedTiles:2];
  id<GREYMatcher> tile1 = TileWithTitle(PageTitle(Page(1)));
  id<GREYMatcher> tile2 = TileWithTitle(PageTitle(Page(2)));

  [self focusOmniboxFromWebPageZero];
  // Long press and delete `tile1`.
  [self deleteMostVisitedTile:tile1];
  // Delete the second tile.
  [self deleteMostVisitedTile:tile2];

  // Check that the carousel is removed when there are no tiles.
  [[EarlGrey selectElementWithMatcher:CarouselMatcher()]
      assertWithMatcher:grey_nil()];

  [ChromeEarlGrey openNewTab];
  [self focusOmniboxFromWebPageZero];

  // Check that the carousel is still not visible when refocusing the omnibox.
  [[EarlGrey selectElementWithMatcher:CarouselMatcher()]
      assertWithMatcher:grey_nil()];
}

// Tests the "Copy URL" action of carousel context menu.
- (void)testMostVisitedTileCopyURL {
  [self addNumberOfMostVisitedTiles:1];
  Page page1 = Page(1);
  id<GREYMatcher> tile1 = TileWithTitle(PageTitle(page1));

  [self focusOmniboxFromWebPageZero];
  [self longPressMostVisitedTile:tile1];

  GURL page1ServerURL = self.testServer->GetURL(PageURL(page1));
  NSString* page1URLStr = base::SysUTF8ToNSString(page1ServerURL.spec());
  [ChromeEarlGrey verifyCopyLinkActionWithText:page1URLStr];
}

// Tests the "Share" action of the carousel context menu.
- (void)testMostVisitedShare {
  [self addNumberOfMostVisitedTiles:1];
  Page page1 = Page(1);
  id<GREYMatcher> tile1 = TileWithTitle(PageTitle(page1));

  [self focusOmniboxFromWebPageZero];
  [self longPressMostVisitedTile:tile1];

  GURL page1ServerURL = self.testServer->GetURL(PageURL(page1));
  NSString* page1Title = base::SysUTF8ToNSString(PageTitle(page1));
  [ChromeEarlGrey verifyShareActionWithURL:page1ServerURL pageTitle:page1Title];
}

// Tests the "Open in New Tab" action of the carousel context menu.
- (void)testMostVisitedNewTab {
  [self addNumberOfMostVisitedTiles:2];
  Page page1 = Page(1);
  id<GREYMatcher> tile1 = TileWithTitle(PageTitle(page1));
  GURL page1ServerURL = self.testServer->GetURL(PageURL(page1));

  [self focusOmniboxFromWebPageZero];
  [self longPressMostVisitedTile:tile1];

  [ChromeEarlGrey verifyOpenInNewTabActionWithURL:page1ServerURL.GetContent()];
}

// Tests the "Open in New Incognito Tab" action of the carousel context menu.
- (void)testMostVisitedNewIncognitoTab {
  [self addNumberOfMostVisitedTiles:2];
  Page page1 = Page(1);
  id<GREYMatcher> tile1 = TileWithTitle(PageTitle(page1));
  GURL page1ServerURL = self.testServer->GetURL(PageURL(page1));

  [self focusOmniboxFromWebPageZero];
  [self longPressMostVisitedTile:tile1];

  [ChromeEarlGrey
      verifyOpenInIncognitoActionWithURL:page1ServerURL.GetContent()];
}

#pragma mark - Helpers

/// Loads the page number `pageNumber` from `testServer`.
- (void)loadPageNumber:(NSUInteger)pageNumber {
  // Page number is limited to two digits by the `kPageURLScheme`.
  DCHECK(pageNumber < 100u);
  GURL pageURL = self.testServer->GetURL(PageURL(pageNumber));
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:PageContent(pageNumber)];
}

/// Add pages [1, `numberOfTiles`] to most visited tiles. Load each page
/// `kMostVisitedLoadCount` time.
- (void)addNumberOfMostVisitedTiles:(NSUInteger)numberOfTiles {
  DCHECK(numberOfTiles <= kCarouselCapacity);
  for (NSUInteger k = 0; k < kMostVisitedLoadCount; ++k) {
    for (NSUInteger i = 1; i <= numberOfTiles; ++i) {
      [self loadPageNumber:i];
    }
  }
  [ChromeEarlGrey goBack];
  [[self class] closeAllTabs];
  [ChromeEarlGrey openNewTab];
}

/// Navigate to a page number 0 and tap the omnibox. This should show the most
/// visited tiles.
- (void)focusOmniboxFromWebPageZero {
  [self loadPageNumber:0];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_tap()];
}

/// Long press on `tile` to show the context menu.
- (void)longPressMostVisitedTile:(id<GREYMatcher>)tile {
  [[[EarlGrey selectElementWithMatcher:tile]
         usingSearchAction:grey_swipeSlowInDirection(kGREYDirectionLeft)
      onElementWithMatcher:CarouselMatcher()] performAction:grey_longPress()];
}

/// Long press on `tile` and select delete in the context menu.
- (void)deleteMostVisitedTile:(id<GREYMatcher>)tile {
  [self longPressMostVisitedTile:tile];
  // Tap on remove.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_SUGGESTIONS_REMOVE)]
      performAction:grey_tap()];
  // Check tile is removed.
  [[EarlGrey selectElementWithMatcher:tile] assertWithMatcher:grey_nil()];
}

@end
