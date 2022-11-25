// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/ios/ios_util.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_app_interface.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

/// Number of time URL is reloaded to add it to most visited sites.
const NSUInteger kMostVisitedLoadCount = 3;
/// Copy of `kCarouselCapacity` in OmniboxPopupCarouselCell
const NSUInteger kCarouselCapacity = 10;

#pragma mark Page

using Page = int;

/// Returns the page content of `page_number`.
std::string PageContent(int page_number) {
  return "This is page " + base::NumberToString(page_number);
}

/// Returns the page title of `page_number`.
std::string PageTitle(int page_number) {
  return "Title " + base::NumberToString(page_number);
}

/// Returns the page URL of `page_number`.
std::string PageURL(int page_number) {
  // Construct an URL conforming to `kPageURLScheme`.
  NSString* nsPageURL =
      [NSString stringWithFormat:@"/page%02d.html", page_number];
  std::string pageURL = base::SysNSStringToUTF8(nsPageURL);
  return pageURL;
}

/// URL scheme used for test pages.
const std::string kPageURLScheme = "/pageXX.html";

#pragma mark HTTP Server

/// Provides responses for the different pages.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  std::string relative_url = request.relative_url;
  if (relative_url.size() != kPageURLScheme.size()) {
    return nil;
  }
  // Retrieve the page number, assuming the relative_url conforms to
  // `kPageURLScheme`.
  std::string page_number_str = relative_url.substr(5, 2);
  // Replace the page number with `XX` and compare to `kPageURLScheme`.
  relative_url.replace(5, 2, "XX");
  if (relative_url != kPageURLScheme) {
    return nil;
  }
  int page_number = stoi(page_number_str);
  http_response->set_content("<html><head><title>" + PageTitle(page_number) +
                             "</title></head><body>" +
                             PageContent(page_number) + "</body></html>");
  return std::move(http_response);
}

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
  auto bundledConfig = std::string(omnibox::kMostVisitedTiles.name);
  config.additional_args.push_back("--enable-features=" + bundledConfig + "<" +
                                   bundledConfig);
  config.additional_args.push_back("--force-fieldtrials=" + bundledConfig +
                                   "/Test");

  // Disable AutocompleteProvider types: TYPE_SEARCH and TYPE_ON_DEVICE_HEAD.
  config.additional_args.push_back(
      "--force-fieldtrial-params=" + bundledConfig +
      ".Test:" + "DisableProviders" + "/" + "1056");

  return config;
}

- (void)setUp {
  [super setUp];

  // Start a server to be able to navigate to a web page.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
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

#pragma mark - Helpers

/// Loads the page number `pageNumber` from `testServer`.
- (void)loadPageNumber:(NSUInteger)pageNumber {
  // Page number is limited to two digits by the `kPageURLScheme`.
  DCHECK(pageNumber < 100u);
  GURL pageURL = self.testServer->GetURL(PageURL(pageNumber));
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:PageContent(pageNumber)];
}

/// Add `numberOfTiles` of most visited tiles. Load each page
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

@end
