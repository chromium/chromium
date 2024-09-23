// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tabs/ui_bundled/tab_strip_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

namespace {

// URL to a destination page with a static message.
const char kDestinationPageUrl[] = "/destination";
// HTML content of the destination page.
const char kDestinationHtml[] =
    "<html><head><meta name='viewport' content='width=device-width, "
    "initial-scale=1.0, maximum-scale=1.0, user-scalable=no' "
    "/></head><body><script>document.title='new doc'</script>"
    "<center><span id=\"message\">You made it!</span></center>"
    "</body></html>";
// Visible content of the destination page.
const char kDestinationContent[] = "You made it!";

// URL to an initial page with a link to the destination page.
const char kInitialPageUrl[] = "/scenarioDragURLToTabStrip";
// HTML content of an initial page with a link to the destination page.
// Note that this string contains substrings that must exactly match the next
// two constants (`kInitialPageLinkId` and `kInitialPageDestinationLinkText`).
// If these were more complex or more liable to change, a sprintf template and
// runtime composition should be used instead.
const char kInitialPageHtml[] =
    "<html><head><meta name='viewport' content='width=device-width, "
    "initial-scale=1.0, maximum-scale=1.0, user-scalable=no' /></head><body><a "
    "style='margin-left:150px' href='/destination' id='link' aria-label='link'>"
    "link</a></body></html>";

// Accessibility ID of the link on the initial page. The `aria-label` attribute
// makes this visible to UIKit as an accessibility ID.
NSString* const kInitialPageLinkId = @"link";

// The text of the link to the destination page.
const char kInitialPageDestinationLinkText[] = "link";

// Provides responses for initial page and destination URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  if (request.relative_url == kInitialPageUrl) {
    http_response->set_content(kInitialPageHtml);
  } else if (request.relative_url == kDestinationPageUrl) {
    http_response->set_content(kDestinationHtml);
  } else {
    return nullptr;
  }

  return std::move(http_response);
}

// Finds the element with the given `identifier` of given `type`.
XCUIElement* GetElementMatchingIdentifier(XCUIApplication* app,
                                          NSString* identifier,
                                          XCUIElementType type) {
  XCUIElementQuery* query = [[app.windows.firstMatch
      descendantsMatchingType:type] matchingIdentifier:identifier];
  return [query elementBoundByIndex:0];
}

// Drags and drops the element with the given `src_identifier` identifier in the
// tab strip with the given `tab_strip_identifier` identifier.
void DragDrop(NSString* src_identifier, NSString* tab_strip_identifier) {
  XCUIApplication* app = [[XCUIApplication alloc] init];
  XCUIElement* src_element =
      GetElementMatchingIdentifier(app, src_identifier, XCUIElementTypeAny);
  XCUICoordinate* start_point =
      [src_element coordinateWithNormalizedOffset:CGVectorMake(0.5, 0.5)];

  XCUIElement* dst_element = GetElementMatchingIdentifier(
      app, tab_strip_identifier, XCUIElementTypeAny);
  XCUICoordinate* end_point =
      [dst_element coordinateWithNormalizedOffset:CGVectorMake(0.5, 0.75)];

  [start_point pressForDuration:1.5
           thenDragToCoordinate:end_point
                   withVelocity:XCUIGestureVelocityDefault
            thenHoldForDuration:1.0];
}

}  // namespace

// Tests for the tab strip shown on iPad.
@interface LegacyTabStripTestCase : ChromeTestCase
@end

@implementation LegacyTabStripTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_disabled.push_back(kModernTabStrip);
  return config;
}

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

// Test switching tabs using the tab strip.
- (void)testTabStripSwitchTabs {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  // TODO(crbug.com/41010830):  Make this test also handle the 'collapsed' tab
  // case.
  const int kNumberOfTabs = 3;
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://about")];
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://version")];

  // Note that the tab ordering wraps.  E.g. if A, B, and C are open,
  // and C is the current tab, the 'next' tab is 'A'.
  for (int i = 0; i < kNumberOfTabs + 1; i++) {
    GREYAssertTrue([ChromeEarlGrey mainTabCount] > 1,
                   [ChromeEarlGrey mainTabCount] ? @"Only one tab open."
                                                 : @"No more tabs.");
    NSString* nextTabTitle = [ChromeEarlGrey nextTabTitle];

    [[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_text(nextTabTitle),
                                            grey_sufficientlyVisible(), nil)]
        performAction:grey_tap()];

    GREYAssertEqualObjects([ChromeEarlGrey currentTabTitle], nextTabTitle,
                           @"The selected tab did not change to the next tab.");
  }
}

// Tests dragging URL into regular tab strip.
// TODO(crbug.com/330842850): Test is flaky.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testDragAndDropURLIntoRegularTabStrip \
  FLAKY_testDragAndDropURLIntoRegularTabStrip
#else
#define MAYBE_testDragAndDropURLIntoRegularTabStrip \
  testDragAndDropURLIntoRegularTabStrip
#endif
- (void)MAYBE_testDragAndDropURLIntoRegularTabStrip {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:kInitialPageDestinationLinkText];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Drag and drop the link on the page to the tab strip. This should open
  // a new regular tab.
  DragDrop(kInitialPageLinkId, kRegularTabStripId);
  GREYWaitForAppToIdle(@"App failed to idle");

  [ChromeEarlGrey waitForWebStateContainingText:kDestinationContent];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests dragging URL into incognito tab strip.
- (void)testDragAndDropURLIntoIncognitoTabStrip {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No tab strip on this device.");
  }

  [ChromeEarlGrey closeAllNormalTabs];
  [ChromeEarlGrey openNewIncognitoTab];

  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:kInitialPageDestinationLinkText];
  [ChromeEarlGrey waitForIncognitoTabCount:1];

  // Drag and drop the link on the page to the tab strip. This should open
  // a new incognito tab.
  DragDrop(kInitialPageLinkId, kIncognitoTabStripId);
  GREYWaitForAppToIdle(@"App failed to idle");

  [ChromeEarlGrey waitForWebStateContainingText:kDestinationContent];
  [ChromeEarlGrey waitForIncognitoTabCount:2];
}

@end
