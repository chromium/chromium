// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/test/ios/wait_util.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/histogram_test_util.h"
#import "ios/chrome/test/app/settings_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/app/web_view_interaction_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/ui/fullscreen_provider.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/element_selector.h"
#import "ios/web/public/web_state/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state/web_state.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::ContextMenuCopyButton;
using chrome_test_util::OmniboxText;
using chrome_test_util::OpenLinkInNewTabButton;
using chrome_test_util::SystemSelectionCallout;
using chrome_test_util::SystemSelectionCalloutCopyButton;
using web::test::ElementSelector;

namespace {
// Directory containing the |kLogoPagePath| and |kLogoPageImageSourcePath|
// resources.
const char kServerFilesDir[] = "ios/testing/data/http_server_files/";
// Path to a page containing the chromium logo and the text |kLogoPageText|.
const char kLogoPagePath[] = "/chromium_logo_page.html";
// Path to the chromium logo.
const char kLogoPageImageSourcePath[] = "/chromium_logo.png";
// The DOM element ID of the chromium image on the logo page.
const char kLogoPageChromiumImageId[] = "chromium_image";
// The text of the message on the logo page.
const char kLogoPageText[] = "Page with some text and the chromium logo image.";

// URL to a page with a static message.
const char kDestinationPageUrl[] = "/destination";
// HTML content of the destination page.
const char kDestinationHtml[] =
    "<html><body><script>document.title='new doc'</script>"
    "<span id=\"message\">You made it!</span>"
    "</body></html>";
// The DOM element ID of the message on the destination page.
const char kDestinationPageTextId[] = "message";
// The text of the message on the destination page.
const char kDestinationPageText[] = "You made it!";

// URL to a page with a link to the destination page.
const char kInitialPageUrl[] = "/scenarioContextMenuOpenInNewTab";
// HTML content of a page with a link to the destination page.
const char kInitialPageHtml[] =
    "<html><body><a style='margin-left:50px' href='/destination' id='link'>"
    "link</a></body></html>";
// The DOM element ID of the link to the destination page.
const char kInitialPageDestinationLinkId[] = "link";
// The text of the link to the destination page.
const char kInitialPageDestinationLinkText[] = "link";

// Matcher for the open image button in the context menu.
id<GREYMatcher> OpenImageButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_CONTENT_CONTEXT_OPENIMAGE);
}

// Matcher for the open image in new tab button in the context menu.
id<GREYMatcher> OpenImageInNewTabButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_CONTENT_CONTEXT_OPENIMAGENEWTAB);
}

// Provides responses for initial page and destination URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  if (request.relative_url == kInitialPageUrl) {
    // The initial page contains a link to the destination page.
    http_response->set_content(kInitialPageHtml);
  } else if (request.relative_url == kDestinationPageUrl) {
    http_response->set_content(kDestinationHtml);
  } else {
    return nullptr;
  }

  return std::move(http_response);
}

// Waits for the context menu item to disappear. TODO(crbug.com/682871): Remove
// this once EarlGrey is synchronized with context menu.
void WaitForContextMenuItemDisappeared(
    id<GREYMatcher> context_menu_item_button) {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:context_menu_item_button]
        assertWithMatcher:grey_nil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             @"Waiting for matcher %@ failed.", context_menu_item_button);
}

// Long press on |element_id| to trigger context menu.
void LongPressElement(const char* element_id) {
  id<GREYMatcher> web_view_matcher =
      web::WebViewInWebState(chrome_test_util::GetCurrentWebState());
  [[EarlGrey selectElementWithMatcher:web_view_matcher]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        ElementSelector::ElementSelectorId(element_id),
                        true /* menu should appear */)];
}

//  Tap on |context_menu_item_button| context menu item.
void TapOnContextMenuButton(id<GREYMatcher> context_menu_item_button) {
  [[EarlGrey selectElementWithMatcher:context_menu_item_button]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:context_menu_item_button]
      performAction:grey_tap()];
  WaitForContextMenuItemDisappeared(context_menu_item_button);
}

// A simple wrapper that sleeps for 1s to wait for the animation, triggered from
// opening a new tab through context menu, to finish before selecting tab.
// TODO(crbug.com/643792): Remove this function when the bug is fixed.
void SelectTabAtIndexInCurrentMode(NSUInteger index) {
  // Delay for 1 second.
  GREYCondition* myCondition = [GREYCondition conditionWithName:@"delay"
                                                          block:^BOOL {
                                                            return NO;
                                                          }];
  [myCondition waitWithTimeout:1];

  chrome_test_util::SelectTabAtIndexInCurrentMode(index);
}

}  // namespace

// Context menu tests for Chrome.
@interface ContextMenuTestCase : ChromeTestCase
@end

@implementation ContextMenuTestCase

+ (void)setUp {
  [super setUp];
  chrome_test_util::SetContentSettingsBlockPopups(CONTENT_SETTING_ALLOW);
}

+ (void)tearDown {
  chrome_test_util::SetContentSettingsBlockPopups(CONTENT_SETTING_DEFAULT);
  [super tearDown];
}

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  self.testServer->ServeFilesFromSourceDirectory(
      base::FilePath(kServerFilesDir));
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

// Tests that selecting "Open Image" from the context menu properly opens the
// image in the current tab.
- (void)testOpenImageInCurrentTabFromContextMenu {
  const GURL pageURL = self.testServer->GetURL(kLogoPagePath);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebViewContainingText:kLogoPageText];

  LongPressElement(kLogoPageChromiumImageId);
  TapOnContextMenuButton(OpenImageButton());
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify url.
  const GURL imageURL = self.testServer->GetURL(kLogoPageImageSourcePath);
  [[EarlGrey selectElementWithMatcher:OmniboxText(imageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that selecting "Open Image in New Tab" from the context menu properly
// opens the image in a new background tab.
// TODO(crbug.com/817810): Enable this test.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testOpenImageInNewTabFromContextMenu \
  testOpenImageInNewTabFromContextMenu
#else
#define MAYBE_testOpenImageInNewTabFromContextMenu \
  FLAKY_testOpenImageInNewTabFromContextMenu
#endif
- (void)MAYBE_testOpenImageInNewTabFromContextMenu {
  const GURL pageURL = self.testServer->GetURL(kLogoPagePath);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebViewContainingText:kLogoPageText];

  LongPressElement(kLogoPageChromiumImageId);
  TapOnContextMenuButton(OpenImageInNewTabButton());

  [ChromeEarlGrey waitForMainTabCount:2];
  SelectTabAtIndexInCurrentMode(1U);
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify url.
  const GURL imageURL = self.testServer->GetURL(kLogoPageImageSourcePath);
  [[EarlGrey selectElementWithMatcher:OmniboxText(imageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests "Open in New Tab" on context menu.
// TODO(crbug.com/817810): Enable this test.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testContextMenuOpenInNewTab testContextMenuOpenInNewTab
#else
#define MAYBE_testContextMenuOpenInNewTab FLAKY_testContextMenuOpenInNewTab
#endif
- (void)MAYBE_testContextMenuOpenInNewTab {
  const GURL initialURL = self.testServer->GetURL(kInitialPageUrl);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey waitForWebViewContainingText:kInitialPageDestinationLinkText];

  LongPressElement(kInitialPageDestinationLinkId);
  TapOnContextMenuButton(OpenLinkInNewTabButton());

  [ChromeEarlGrey waitForMainTabCount:2];
  SelectTabAtIndexInCurrentMode(1U);
  [ChromeEarlGrey waitForWebViewContainingText:kDestinationPageText];

  // Verify url.
  const GURL destinationURL = self.testServer->GetURL(kDestinationPageUrl);
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that the context menu is displayed for an image url.
// TODO(crbug.com/817810): Enable this test.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testContextMenuDisplayedOnImage testContextMenuDisplayedOnImage
#else
#define MAYBE_testContextMenuDisplayedOnImage \
  FLAKY_testContextMenuDisplayedOnImage
#endif
- (void)MAYBE_testContextMenuDisplayedOnImage {
  const GURL imageURL = self.testServer->GetURL(kLogoPageImageSourcePath);
  [ChromeEarlGrey loadURL:imageURL];

  // Calculate a point inside the displayed image. Javascript can not be used to
  // find the element because no DOM exists.  If the viewport is adjusted using
  // the contentInset, the top inset needs to be added to the touch point.
  id<CRWWebViewProxy> webViewProxy =
      chrome_test_util::GetCurrentWebState()->GetWebViewProxy();
  BOOL usesContentInset =
      webViewProxy.shouldUseViewContentInset ||
      ios::GetChromeBrowserProvider()->GetFullscreenProvider()->IsInitialized();
  CGFloat topInset = usesContentInset ? webViewProxy.contentInset.top : 0.0;
  CGPoint point = CGPointMake(
      CGRectGetMidX([chrome_test_util::GetActiveViewController() view].bounds),
      topInset + 20.0);

  id<GREYMatcher> web_view_matcher =
      web::WebViewInWebState(chrome_test_util::GetCurrentWebState());
  [[EarlGrey selectElementWithMatcher:web_view_matcher]
      performAction:grey_longPressAtPointWithDuration(
                        point, kGREYLongPressDefaultDuration)];

  TapOnContextMenuButton(OpenImageInNewTabButton());
  [ChromeEarlGrey waitForMainTabCount:2];
  SelectTabAtIndexInCurrentMode(1U);
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify url.
  [[EarlGrey selectElementWithMatcher:OmniboxText(imageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that the element fetch duration is logged once.
- (void)testContextMenuElementFetchDurationMetric {
  chrome_test_util::HistogramTester histogramTester;

  const GURL pageURL = self.testServer->GetURL(kLogoPagePath);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebViewContainingText:kLogoPageText];

  LongPressElement(kLogoPageChromiumImageId);
  TapOnContextMenuButton(OpenImageButton());
  [ChromeEarlGrey waitForPageToFinishLoading];

  histogramTester.ExpectTotalCount("ContextMenu.DOMElementFetchDuration", 1,
                                   ^(NSString* error) {
                                     GREYFail(error);
                                   });
}

// Tests that system touches are cancelled when the context menu is shown.
- (void)testContextMenuCancelSystemTouchesMetric {
  chrome_test_util::HistogramTester histogramTester;

  const GURL pageURL = self.testServer->GetURL(kLogoPagePath);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebViewContainingText:kLogoPageText];

  LongPressElement(kLogoPageChromiumImageId);
  TapOnContextMenuButton(OpenImageButton());
  [ChromeEarlGrey waitForPageToFinishLoading];

  // Verify that system touches were cancelled.
  histogramTester.ExpectTotalCount("ContextMenu.CancelSystemTouches", 1,
                                   ^(NSString* error) {
                                     GREYFail(error);
                                   });
}

// Tests that the system selected text callout is displayed instead of the
// context menu when user long presses on plain text.
- (void)testContextMenuSelectedTextCallout {
  chrome_test_util::HistogramTester histogramTester;

  // Load the destination page directly because it has a plain text message on
  // it.
  const GURL destinationURL = self.testServer->GetURL(kDestinationPageUrl);
  [ChromeEarlGrey loadURL:destinationURL];
  [ChromeEarlGrey waitForWebViewContainingText:kDestinationPageText];

  LongPressElement(kDestinationPageTextId);

  // Verify that context menu is not shown.
  [[EarlGrey selectElementWithMatcher:ContextMenuCopyButton()]
      assertWithMatcher:grey_nil()];

  // Verify that system text selection callout is displayed.
  [[[EarlGrey selectElementWithMatcher:SystemSelectionCalloutCopyButton()]
      inRoot:SystemSelectionCallout()] assertWithMatcher:grey_notNil()];

  // Verify that system touches were not cancelled.
  histogramTester.ExpectTotalCount("ContextMenu.CancelSystemTouches", 0,
                                   ^(NSString* error) {
                                     GREYFail(error);
                                   });
}

@end
