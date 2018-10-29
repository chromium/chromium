// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>
#import <XCTest/XCTest.h>

#include "base/bind.h"
#include "base/ios/ios_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/settings_test_util.h"
#import "ios/chrome/test/app/web_view_interaction_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/features.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#import "ios/web/public/test/http_server/error_page_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state/web_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::TapWebViewElementWithId;

namespace {

// The page height of test pages. This must be big enough to triger fullscreen.
const int kPageHeightEM = 200;

// Hides the toolbar by scrolling down.
void HideToolbarUsingUI() {
  [[EarlGrey
      selectElementWithMatcher:WebViewScrollView(
                                   chrome_test_util::GetCurrentWebState())]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
}

// Asserts that the current URL is the |expectedURL| one.
void AssertURLIs(const GURL& expectedURL) {
  NSString* description = [NSString
      stringWithFormat:@"Timeout waiting for the url to be %@",
                       base::SysUTF8ToNSString(expectedURL.GetContent())];

  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                            expectedURL.GetContent())]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return (error == nil);
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(1.0, condition),
             description);
}

}  // namespace

#pragma mark - Tests

// Fullscreens tests for Chrome.
@interface FullscreenTestCase : ChromeTestCase
@end

@implementation FullscreenTestCase

- (void)setUp {
  [super setUp];
}

// Verifies that the content offset of the web view is set up at the correct
// initial value when initially displaying a PDF.
- (void)testLongPDFInitialState {
  web::test::SetUpFileBasedHttpServer();
  GURL URL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/two_pages.pdf");
  [ChromeEarlGrey loadURL:URL];

  [ChromeEarlGreyUI waitForToolbarVisible:YES];
  // Initial y scroll positions are set to make room for the toolbar.
  // TODO(crbug.com/618887) Replace use of specific values when API which
  // generates these values is exposed.
  CGFloat yOffset = 0;
  if (IsUIRefreshPhase1Enabled()) {
    if (IsIPadIdiom()) {
      yOffset = -89.0;
    } else {
      yOffset = -48.0;
    }
  } else {
    if (IsIPadIdiom()) {
      yOffset = -95.0;
    } else {
      yOffset = -56.0;
    }
  }
  if (base::FeatureList::IsEnabled(
          web::features::kBrowserContainerFullscreen) &&
      base::FeatureList::IsEnabled(web::features::kOutOfWebFullscreen)) {
    if (@available(iOS 11, *)) {
      yOffset -=
          chrome_test_util::GetCurrentWebState()->GetView().safeAreaInsets.top;
    } else {
      yOffset -= StatusBarHeight();
    }
  }
  DCHECK_LT(yOffset, 0);
  [[EarlGrey
      selectElementWithMatcher:web::WebViewScrollView(
                                   chrome_test_util::GetCurrentWebState())]
      assertWithMatcher:grey_scrollViewContentOffset(CGPointMake(0, yOffset))];
}

// Verifies that the toolbar properly appears/disappears when scrolling up/down
// on a PDF that is short in length and wide in width.
- (void)testSmallWidePDFScroll {
  web::test::SetUpFileBasedHttpServer();
  GURL URL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/single_page_wide.pdf");
  [ChromeEarlGrey loadURL:URL];

  // TODO(crbug.com/852393): Investigate why synchronization isn't working.  Is
  // an animation going on forever?
  if (@available(iOS 12, *)) {
    [[GREYConfiguration sharedInstance]
            setValue:@NO
        forConfigKey:kGREYConfigKeySynchronizationEnabled];
  }

  // Test that the toolbar is still visible after a user swipes down.
  [[EarlGrey
      selectElementWithMatcher:WebViewScrollView(
                                   chrome_test_util::GetCurrentWebState())]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [ChromeEarlGreyUI waitForToolbarVisible:YES];

  if (base::ios::IsRunningOnIOS12OrLater()) {
    // Test that the toolbar is still visible even after attempting to hide it
    // on swipe up.
    HideToolbarUsingUI();
    [ChromeEarlGreyUI waitForToolbarVisible:YES];
  } else {
    // Test that the toolbar is no longer visible after a user swipes up.
    HideToolbarUsingUI();
    [ChromeEarlGreyUI waitForToolbarVisible:NO];
  }

  // Reenable synchronization.
  if (@available(iOS 12, *)) {
    [[GREYConfiguration sharedInstance]
            setValue:@YES
        forConfigKey:kGREYConfigKeySynchronizationEnabled];
  }
}

// Verifies that the toolbar properly appears/disappears when scrolling up/down
// on a PDF that is long in length and wide in width.
- (void)testLongPDFScroll {
// TODO(crbug.com/714329): Re-enable this test on devices.
#if !TARGET_IPHONE_SIMULATOR
  EARL_GREY_TEST_DISABLED(@"Test disabled on device.");
#endif

  web::test::SetUpFileBasedHttpServer();
  GURL URL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/two_pages.pdf");
  [ChromeEarlGrey loadURL:URL];

  // Test that the toolbar is hidden after a user swipes up.
  HideToolbarUsingUI();
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Test that the toolbar is visible after a user swipes down.
  [[EarlGrey
      selectElementWithMatcher:WebViewScrollView(
                                   chrome_test_util::GetCurrentWebState())]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [ChromeEarlGreyUI waitForToolbarVisible:YES];

  // Test that the toolbar is hidden after a user swipes up.
  HideToolbarUsingUI();
  [ChromeEarlGreyUI waitForToolbarVisible:NO];
}

// Tests that link clicks from a chrome:// to chrome:// link result in the
// header being shown even if was not previously shown.
- (void)testChromeToChromeURLKeepsHeaderOnScreen {
  const GURL kChromeAboutURL("chrome://chrome-urls");
  [ChromeEarlGrey loadURL:kChromeAboutURL];
  [ChromeEarlGrey waitForWebViewContainingText:"chrome://version"];

  // Hide the toolbar. The page is not long enough to dismiss the toolbar using
  // the UI so we have to zoom in.
  const char script[] =
      "(function(){"
      "var metas = document.getElementsByTagName('meta');"
      "for (var i=0; i<metas.length; i++) {"
      "  if (metas[i].getAttribute('name') == 'viewport') {"
      "    metas[i].setAttribute('content', 'width=10');"
      "    return;"
      "  }"
      "}"
      "document.body.innerHTML += \"<meta name='viewport' content='width=10'>\""
      "})()";

  __block bool finished = false;
  chrome_test_util::GetCurrentWebState()->ExecuteJavaScript(
      base::UTF8ToUTF16(script), base::BindOnce(^(const base::Value*) {
        finished = true;
      }));

  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(1.0,
                                                          ^{
                                                            return finished;
                                                          }),
             @"JavaScript to hide the toolbar did not complete");

  // Scroll up to be sure the toolbar can be dismissed by scrolling down.
  [[EarlGrey
      selectElementWithMatcher:WebViewScrollView(
                                   chrome_test_util::GetCurrentWebState())]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Scroll to hide the UI.
  HideToolbarUsingUI();
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Test that the toolbar is visible when moving from one chrome:// link to
  // another chrome:// link.
  GREYAssert(TapWebViewElementWithId("version"), @"Failed to tap \"version\"");
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Tests hiding and showing of the header with a user scroll on a long page.
- (void)testHideHeaderUserScrollLongPage {
  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl("http://tallpage");
  // A page long enough to ensure that the toolbar goes away on scrolling.
  responses[URL] =
      base::StringPrintf("<p style='height:%dem'>a</p><p>b</p>", kPageHeightEM);
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
  // Simulate a user scroll down.
  HideToolbarUsingUI();
  [ChromeEarlGreyUI waitForToolbarVisible:NO];
  // Simulate a user scroll up.
  [[EarlGrey
      selectElementWithMatcher:WebViewScrollView(
                                   chrome_test_util::GetCurrentWebState())]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Tests that reloading of a page shows the header even if it was not shown
// previously.
- (void)testShowHeaderOnReload {
  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl("http://origin");
  // This is a tall page -- necessary to make sure scrolling can hide away the
  // toolbar safely-- and with a link to reload itself.
  responses[URL] = base::StringPrintf(
      "<p style='height:%dem'>Tall page</p>"
      "<a onclick='window.location.reload();' id='link'>link</a>",
      kPageHeightEM);
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebViewContainingText:"Tall page"];

  // Hide the toolbar.
  HideToolbarUsingUI();
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  GREYAssert(TapWebViewElementWithId("link"), @"Failed to tap \"link\"");

  // Main test is here: Make sure the header is still visible!
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Test to make sure the header is shown when a Tab opened by the current Tab is
// closed even if the toolbar was not present previously.
- (void)testShowHeaderWhenChildTabCloses {
  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl("http://origin");
  const GURL destinationURL =
      web::test::HttpServer::MakeUrl("http://destination");
  // JavaScript to open a window using window.open.
  std::string javaScript =
      base::StringPrintf("window.open(\"%s\");", destinationURL.spec().c_str());

  // A long page with a link to execute JavaScript.
  responses[URL] = base::StringPrintf(
      "<p style='height:%dem'>whatever</p>"
      "<a onclick='%s' id='link1'>link1</a>",
      kPageHeightEM, javaScript.c_str());
  // A long page with some simple text and link to close itself using
  // window.close.
  javaScript = "window.close()";
  responses[destinationURL] = base::StringPrintf(
      "<p style='height:%dem'>whatever</p><a onclick='%s' "
      "id='link2'>link2</a>",
      kPageHeightEM, javaScript.c_str());

  web::test::SetUpSimpleHttpServer(responses);
  chrome_test_util::SetContentSettingsBlockPopups(CONTENT_SETTING_ALLOW);

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebViewContainingText:"link1"];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Hide the toolbar.
  HideToolbarUsingUI();
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Open new window.
  GREYAssert(TapWebViewElementWithId("link1"), @"Failed to tap \"link1\"");

  // Check that a new Tab was created.
  [ChromeEarlGrey waitForWebViewContainingText:"link2"];
  [ChromeEarlGrey waitForMainTabCount:2];

  AssertURLIs(destinationURL);

  // Hide the toolbar.
  HideToolbarUsingUI();
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Close the tab by tapping link2.
  NSError* error = nil;
  if (!chrome_test_util::TapWebViewElementWithId("link2", &error)) {
    // Sometimes, the tap will be unsuccessful due to the window.close()
    // operation invalidating the WKWebView.  If this occurs, verify the error.
    // This results in |TapWebViewElementWithId| returning false.
    // TODO(crbug.com/824879): Remove conditional once flake is eliminated from
    // TapWebViewElementWithId() for window.close() links.
    GREYAssert(error.code == WKErrorWebViewInvalidated,
               @"Failed to receive WKErrorWebViewInvalidated error");
    GREYAssert([error.domain isEqualToString:WKErrorDomain],
               @"Failed to receive WKErrorDomain error");
  }

  [ChromeEarlGrey waitForWebViewContainingText:"link1"];

  // Make sure the toolbar is on the screen.
  [ChromeEarlGrey waitForMainTabCount:1];
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Tests that the header is shown when a regular page (non-native page) is
// loaded from a page where the header was not see before.
// Also tests that auto-hide works correctly on new page loads.
- (void)testShowHeaderOnRegularPageLoad {
  std::map<GURL, std::string> responses;
  const GURL originURL = web::test::HttpServer::MakeUrl("http://origin");
  const GURL destinationURL =
      web::test::HttpServer::MakeUrl("http://destination");

  const std::string manyLines = base::StringPrintf(
      "<p style='height:%dem'>a</p><p>End of lines</p>", kPageHeightEM);

  // A long page representing many lines and a link to the destination URL page.
  responses[originURL] = manyLines + "<a href='" + destinationURL.spec() +
                         "' id='link1'>link1</a>";
  // A long page representing many lines and a link to go back.
  responses[destinationURL] = manyLines +
                              "<a href='javascript:void(0)' "
                              "onclick='window.history.back()' "
                              "id='link2'>link2</a>";
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:originURL];

  [ChromeEarlGrey waitForWebViewContainingText:"link1"];
  // Dismiss the toolbar.
  HideToolbarUsingUI();
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Navigate to the other page.
  GREYAssert(TapWebViewElementWithId("link1"), @"Failed to tap \"link1\"");
  [ChromeEarlGrey waitForWebViewContainingText:"link2"];

  // Make sure toolbar is shown since a new load has started.
  [ChromeEarlGreyUI waitForToolbarVisible:YES];

  // Dismiss the toolbar.
  HideToolbarUsingUI();
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Go back.
  GREYAssert(TapWebViewElementWithId("link2"), @"Failed to tap \"link2\"");

  // Make sure the toolbar has loaded now that a new page has loaded.
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Tests that the header is shown when a native page is loaded from a page where
// the header was not seen before.
- (void)testShowHeaderOnNativePageLoad {
  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl("http://origin");

  // A long page representing many lines and a link to go back.
  std::string manyLines = base::StringPrintf(
      "<p style='height:%dem'>a</p>"
      "<a onclick='window.history.back()' id='link'>link</a>",
      kPageHeightEM);
  responses[URL] = manyLines;
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebViewContainingText:"link"];

  // Dismiss the toolbar.
  HideToolbarUsingUI();
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Go back to NTP, which is a native view.
  GREYAssert(TapWebViewElementWithId("link"), @"Failed to tap \"link\"");

  // Make sure the toolbar is visible now that a new page has loaded.
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Tests that the header is shown when loading an error page in a native view
// even if fullscreen was enabled previously.
- (void)testShowHeaderOnErrorPage {
  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl("http://origin");
  // A long page with some simple text -- a long page is necessary so that
  // enough content is present to ensure that the toolbar can be hidden safely.
  responses[URL] = base::StringPrintf(
      "<p style='height:%dem'>a</p>"
      "<a href=\"%s\" id=\"link\">bad link</a>",
      kPageHeightEM,
      ErrorPageResponseProvider::GetDnsFailureUrl().spec().c_str());
  std::unique_ptr<web::DataResponseProvider> provider(
      new ErrorPageResponseProvider(responses));
  web::test::SetUpHttpServer(std::move(provider));

  [ChromeEarlGrey loadURL:URL];
  HideToolbarUsingUI();
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  GREYAssert(TapWebViewElementWithId("link"), @"Failed to tap \"link\"");
  AssertURLIs(ErrorPageResponseProvider::GetDnsFailureUrl());
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

@end
