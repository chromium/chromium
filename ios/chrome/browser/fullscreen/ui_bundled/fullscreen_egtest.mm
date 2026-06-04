// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/fullscreen_app_interface.h"
#import "ios/chrome/browser/popup_menu/public/popup_menu_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/scoped_block_popups_pref.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/common/features.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

using chrome_test_util::WebStateScrollViewMatcher;

namespace {

// The page height of test pages. This must be big enough to triger fullscreen.
const int kPageHeightEM = 400;

// Tolerance for width increase check.
const CGFloat kViewportFitCoverTolerance = 5.0;

// Hides the toolbar by scrolling down.
void HideToolbarUsingUI() {
  [[EarlGrey selectElementWithMatcher:WebStateScrollViewMatcher()]
      performAction:grey_swipeSlowInDirection(kGREYDirectionUp)];
}

// A PDF itself can take a little longer to appear even after the page is
// loaded. Instead, do an additional wait for the internal PDF class to appear
// in the view hierarchy.
void WaitforPDFExtensionView() {
  if (@available(iOS 26, *)) {
    [ChromeEarlGrey waitForPageToFinishLoading];
    return;
  }
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_kindOfClass(NSClassFromString(
                                            @"PDFExtensionTopView"))]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };

  NSString* errorMessage = @"PDFExtensionTopView was not visible";
  GREYAssert(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, condition),
             errorMessage);
}

// Helper function to create HTML responses.
std::unique_ptr<net::test_server::HttpResponse> CreateHttpResponse(
    const std::string& content) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->set_content(content);
  return response;
}

int GetWidth(const base::Value& value) {
  if (value.is_double()) {
    return static_cast<int>(value.GetDouble());
  }
  return value.GetInt();
}

// Helper function to return the HTML string for the fixed bottom element test.
std::string GetFixedBottomHtml() {
  return "<!DOCTYPE html>\n"
         "<html>\n"
         "<head>\n"
         "  <meta name='viewport' content='width=device-width, "
         "initial-scale=1.0'>\n"
         "  <style>\n"
         "    body { margin: 0; height: 2000px; }\n"
         "    #fixed { \n"
         "      position: fixed; \n"
         "      bottom: 0; \n"
         "      left: 0; \n"
         "      right: 0; \n"
         "      height: 50px; \n"
         "      background-color: red; \n"
         "    }\n"
         "  </style>\n"
         "</head>\n"
         "<body>\n"
         "  <div id='fixed'>Fixed</div>\n"
         "</body>\n"
         "</html>";
}

// Helper function to create 404 Not Found responses.
std::unique_ptr<net::test_server::HttpResponse> NotFoundResponse() {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_NOT_FOUND);
  return response;
}

}  // namespace

#pragma mark - Tests

// Fullscreens tests for Chrome.
// TODO(crbug.com/40849153): Remove the "ZZZ" when the bug is fixed.
@interface ZZZFullscreenTestCase : ChromeTestCase
@end

@interface ZZZFullscreenTestCase () {
  // A map of request URLs to HTML responses.
  std::map<std::string, std::string> _responses;
}
@end

@implementation ZZZFullscreenTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_disabled.push_back(web::features::kSmoothScrollingDefault);
  config.features_disabled.push_back(kFullscreenRefactoring);
  // TODO(crbug.com/511992708): Fix these tests when Chrome Next is enabled.
  config.features_disabled.push_back(kChromeNextIa);
  config.features_enabled.push_back(kHideToolbarsInOverflowMenu);
  return config;
}

- (void)setUp {
  [super setUp];

  // Disable translate to avoid the info bar that block the top toolbar.
  [ChromeEarlGrey setBoolValue:NO
                   forUserPref:translate::prefs::kOfferTranslateEnabled];

  [ChromeEarlGrey setBoolValue:NO
             forLocalStatePref:omnibox::kIsOmniboxInBottomPosition];

  auto* responses = &_responses;
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      [](std::map<std::string, std::string>* responses,
         const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == "/two_pages.pdf" ||
            request.relative_url == "/single_page_wide.pdf") {
          return nullptr;
        }
        auto it = responses->find(request.relative_url);
        if (it != responses->end()) {
          return CreateHttpResponse(it->second);
        }
        return NotFoundResponse();
      },
      responses));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

- (void)tearDownHelper {
  // Reactivate translation.
  [ChromeEarlGrey setBoolValue:YES
                   forUserPref:translate::prefs::kOfferTranslateEnabled];
  [super tearDownHelper];
}

// Verifies that the content offset of the web view is set up at the correct
// initial value when initially displaying a PDF.
- (void)testLongPDFInitialState {
  GURL URL = self.testServer->GetURL("/two_pages.pdf");
  [ChromeEarlGrey loadURL:URL];
  WaitforPDFExtensionView();
  [ChromeEarlGreyUI waitForToolbarVisible:YES];

  // Initial y scroll positions are set to make room for the toolbar.
  // Starting from iOS 26, PDFs are using the framing resizing strategy. The
  // webView frame starts below the toolbar. Since the container itself is
  // already positioned correctly on the screen, the content offset should be 0
  // to show the start of the document.
  CGFloat expectedYOffset = 0;
  if (!@available(iOS 26, *)) {
    expectedYOffset = -[FullscreenAppInterface currentViewportInsets].top;
    DCHECK_LT(expectedYOffset, 0);
  }
  [[EarlGrey selectElementWithMatcher:WebStateScrollViewMatcher()]
      assertWithMatcher:grey_scrollViewContentOffset(
                            CGPointMake(0, expectedYOffset))];
}

// Verifies that the toolbar is not hidden when scrolling a short pdf, as the
// entire document is visible without hiding the toolbar.
- (void)testSmallWidePDFScroll {
  GURL URL = self.testServer->GetURL("/single_page_wide.pdf");
  [ChromeEarlGrey loadURL:URL];
  WaitforPDFExtensionView();

  // Test that the toolbar is still visible after a user swipes down.
  // Use a slow swipe here because in this combination of conditions (one
  // page PDF, overscroll actions enabled, fast swipe), the
  // `UIScrollViewDelegate scrollViewDidEndDecelerating:` is not called leading
  // to an EarlGrey infinite wait.
  [[EarlGrey selectElementWithMatcher:WebStateScrollViewMatcher()]
      performAction:grey_swipeSlowInDirection(kGREYDirectionDown)];
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Verifies that the toolbar properly appears/disappears when scrolling up/down
// on a PDF that is long in length and wide in width.
- (void)testLongPDFScroll {
  GURL URL = self.testServer->GetURL("/two_pages.pdf");
  [ChromeEarlGrey loadURL:URL];
  WaitforPDFExtensionView();

  // Test that the toolbar is hidden after a user swipes up.
  [[EarlGrey selectElementWithMatcher:WebStateScrollViewMatcher()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 200)];
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Test that the toolbar is visible after a user swipes down.
  [[EarlGrey selectElementWithMatcher:WebStateScrollViewMatcher()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [ChromeEarlGreyUI waitForToolbarVisible:YES];

  // Test that the toolbar is hidden after a user swipes up.
  [[EarlGrey selectElementWithMatcher:WebStateScrollViewMatcher()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 200)];
  [ChromeEarlGreyUI waitForToolbarVisible:NO];
}

// Tests that link clicks from a chrome:// to chrome:// link result in the
// header being shown even if was not previously shown.
// TODO(crbug.com/461735565): Test flaky on device and simulator.
- (void)DISABLED_testChromeToChromeURLKeepsHeaderOnScreen {
  const GURL kChromeAboutURL("chrome://chrome-urls");
  [ChromeEarlGrey loadURL:kChromeAboutURL];
  [ChromeEarlGrey waitForWebStateContainingText:"chrome://version"];

  // Hide the toolbar. The page is not long enough to dismiss the toolbar using
  // the UI so we have to zoom in.
  NSString* script = @"(function(){"
                      "var metas = document.getElementsByTagName('meta');"
                      "for (var i=0; i<metas.length; i++) {"
                      "  if (metas[i].getAttribute('name') == 'viewport') {"
                      "    metas[i].setAttribute('content', 'width=10');"
                      "    return;"
                      "  }"
                      "}"
                      "document.body.innerHTML += \"<meta name='viewport' "
                      "content='width=10'>\""
                      "})()";
  [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];

  // Scroll up to be sure the toolbar can be dismissed by scrolling down.
  [[EarlGrey selectElementWithMatcher:WebStateScrollViewMatcher()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Scroll to hide the UI.
  HideToolbarUsingUI();
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Test that the toolbar is visible when moving from one chrome:// link to
  // another chrome:// link. The script below queries for the
  // "chrome://version" link on the chrome://chrome-urls page, and clicks on
  // it. The link is in the shadow DOM of the chrome-urls-app custom element
  // that contains the page's UI.
  NSString* clickLinkScript =
      @"document.body.querySelector('chrome-urls-app')"
       ".shadowRoot.querySelector('a[href=\"chrome://version\"]').click()";
  [ChromeEarlGrey evaluateJavaScriptForSideEffect:clickLinkScript];
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Tests hiding and showing of the header with a user scroll on a long page.
- (void)testHideHeaderUserScrollLongPage {
  _responses["/tallpage"] =
      base::StringPrintf("<p style='height:%dem'>a</p><p>b</p>", kPageHeightEM);

  GURL URL = self.testServer->GetURL("/tallpage");
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
  // Simulate a user scroll down.
  HideToolbarUsingUI();
  [ChromeEarlGreyUI waitForToolbarVisible:NO];
  // Simulate a user scroll up.
  [[EarlGrey selectElementWithMatcher:WebStateScrollViewMatcher()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Tests that reloading of a page shows the header even if it was not shown
// previously.
- (void)testShowHeaderOnReload {
  _responses["/origin"] = base::StringPrintf(
      "<p style='height:%dem'>Tall page</p>"
      "<a onclick='window.location.reload();' id='link'>link</a>",
      kPageHeightEM);

  GURL URL = self.testServer->GetURL("/origin");
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Tall page"];

  // Hide the toolbar.
  HideToolbarUsingUI();
  [ChromeEarlGreyUI waitForToolbarVisible:NO];
  [ChromeEarlGrey tapWebStateElementWithID:@"link"];
  // Main test is here: Make sure the header is still visible!
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Test to make sure the header is shown when a Tab opened by the current Tab is
// closed even if the toolbar was not present previously.
- (void)testShowHeaderWhenChildTabCloses {
  const GURL URL = self.testServer->GetURL("/origin");
  const GURL destinationURL = self.testServer->GetURL("/destination");
  // JavaScript to open a window using window.open.
  std::string javaScript =
      base::StringPrintf("window.open(\"%s\");", destinationURL.spec().c_str());

  // A long page with a link to execute JavaScript.
  _responses["/origin"] =
      base::StringPrintf("<p style='height:%dem'>whatever</p>"
                         "<a onclick='%s' id='link1'>link1</a>",
                         kPageHeightEM, javaScript.c_str());
  // A long page with some simple text and link to close itself using
  // window.close.
  javaScript = "window.close()";
  _responses["/destination"] =
      base::StringPrintf("<p style='height:%dem'>whatever</p><a onclick='%s' "
                         "id='link2'>link2</a>",
                         kPageHeightEM, javaScript.c_str());

  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_ALLOW);

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"link1"];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Hide the toolbar.
  HideToolbarUsingUI();
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Open new window.
  [ChromeEarlGrey tapWebStateElementWithID:@"link1"];

  // Check that a new Tab was created.
  [ChromeEarlGrey waitForWebStateContainingText:"link2"];
  [ChromeEarlGrey waitForMainTabCount:2];

  [ChromeEarlGrey waitForWebStateVisibleURL:destinationURL];

  // Hide the toolbar.
  HideToolbarUsingUI();
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Close the tab by tapping link2.
  [ChromeEarlGrey tapWebStateElementWithID:@"link2"];

  [ChromeEarlGrey waitForWebStateContainingText:"link1"];

  // Make sure the toolbar is on the screen.
  [ChromeEarlGrey waitForMainTabCount:1];
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Tests that the header is shown when a regular page (non-native page) is
// loaded from a page where the header was not see before.
// Also tests that auto-hide works correctly on new page loads.
- (void)testShowHeaderOnRegularPageLoad {
  const std::string manyLines = base::StringPrintf(
      "<p style='height:%dem'>a</p><p>End of lines</p>", kPageHeightEM);

  _responses["/origin"] =
      manyLines + "<a href='/destination' id='link1'>link1</a>";
  _responses["/destination"] = manyLines + "<a href='javascript:void(0)' "
                                           "onclick='window.history.back()' "
                                           "id='link2'>link2</a>";

  GURL originURL = self.testServer->GetURL("/origin");
  [ChromeEarlGrey loadURL:originURL];

  [ChromeEarlGrey waitForWebStateContainingText:"link1"];
  // Dismiss the toolbar.
  HideToolbarUsingUI();
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Navigate to the other page.
  [ChromeEarlGrey tapWebStateElementWithID:@"link1"];
  [ChromeEarlGrey waitForWebStateContainingText:"link2"];

  // Make sure toolbar is shown since a new load has started.
  [ChromeEarlGreyUI waitForToolbarVisible:YES];

  // Dismiss the toolbar.
  HideToolbarUsingUI();
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Go back.
  [ChromeEarlGrey tapWebStateElementWithID:@"link2"];

  // Make sure the toolbar has loaded now that a new page has loaded.
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Tests that the header is shown when a native page is loaded from a page where
// the header was not seen before.
- (void)testShowHeaderOnNativePageLoad {
  _responses["/origin"] = base::StringPrintf(
      "<p style='height:%dem'>a</p>"
      "<a onclick='window.history.back()' id='link'>link</a>",
      kPageHeightEM);

  GURL URL = self.testServer->GetURL("/origin");

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"link"];

  // Dismiss the toolbar.
  HideToolbarUsingUI();
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Go back to NTP, which is a native view.
  [ChromeEarlGrey tapWebStateElementWithID:@"link"];

  // Make sure the toolbar is visible now that a new page has loaded.
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Tests that the header is shown when loading an error page in a native view
// even if fullscreen was enabled previously.
// TODO(crbug.com/437072563): Test flaky on device.
#if !TARGET_IPHONE_SIMULATOR
#define MAYBE_testShowHeaderOnErrorPage DISABLED_testShowHeaderOnErrorPage
#else
#define MAYBE_testShowHeaderOnErrorPage testShowHeaderOnErrorPage
#endif
- (void)MAYBE_testShowHeaderOnErrorPage {
  GURL errorURL = self.testServer->GetURL("/mock/bad/");

  _responses["/origin"] =
      base::StringPrintf("<p style='height:%dem'>a</p>"
                         "<a href=\"%s\" id=\"link\">bad link</a>",
                         kPageHeightEM, errorURL.spec().c_str());

  GURL URL = self.testServer->GetURL("/origin");
  [ChromeEarlGrey loadURL:URL];
  HideToolbarUsingUI();
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  [ChromeEarlGrey tapWebStateElementWithID:@"link"];
  [ChromeEarlGrey
      waitForWebStateVisibleURL:self.testServer->GetURL("/mock/bad/")];
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Tests collapsing of toolbar when a user scroll on a long page and rotate.
- (void)testCollapseToolbarOnScrollAndRotate {
  _responses["/tallpage"] =
      base::StringPrintf("<p style='height:%dem'>a</p><p>b</p>", kPageHeightEM);

  GURL URL = self.testServer->GetURL("/tallpage");

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGreyUI waitForToolbarVisible:YES];

  // Scroll and check that toolbar is collapsed.
  HideToolbarUsingUI();
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Rotate and check that toolbar is still collapsed.
  [EarlGrey rotateInterfaceToOrientation:UIInterfaceOrientationLandscapeLeft
                                   error:nil];
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Cancel the rotation.
  [EarlGrey rotateInterfaceToOrientation:UIInterfaceOrientationPortrait
                                   error:nil];
}

// Tests that the toolbar reappears after backgrounding and foregrounding the
// app during or after a fast scroll.
- (void)testShowFullToolbarAfterBackgroundDuringFastScroll {
  _responses["/tallpage"] =
      base::StringPrintf("<p style='height:%dem'>a</p><p>b</p>", kPageHeightEM);

  const GURL URL = self.testServer->GetURL("/tallpage");

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGreyUI waitForToolbarVisible:YES];

  [[EarlGrey selectElementWithMatcher:WebStateScrollViewMatcher()]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Tests that tapping on the collapsed primary toolbar exits force fullscreen
// mode.
- (void)testTapOnCollapsedToolbarExitsForceFullscreenMode {
  _responses["/tallpage"] =
      base::StringPrintf("<p style='height:%dem'>a</p><p>b</p>", kPageHeightEM);

  GURL URL = self.testServer->GetURL("/tallpage");
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGreyUI waitForToolbarVisible:YES];

  // Open the tools menu.
  [ChromeEarlGreyUI openToolsMenu];

  // Tap on "Hide Toolbars" in the tools menu.
  [ChromeEarlGreyUI
      tapToolsMenuAction:grey_accessibilityID(kToolsMenuHideToolbars)];

  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Scroll down and up to ensure we are in forced fullscreen mode and the
  // toolbars stay hidden.
  [[EarlGrey selectElementWithMatcher:WebStateScrollViewMatcher()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 250)];
  [ChromeEarlGreyUI waitForToolbarVisible:NO];
  [[EarlGrey selectElementWithMatcher:WebStateScrollViewMatcher()]
      performAction:grey_scrollInDirection(kGREYDirectionUp, 250)];
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Tap on the primary toolbar (which is collapsed).
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PrimaryToolbar()]
      performAction:grey_tap()];

  // Verify that it exits force fullscreen mode and the toolbar is visible.
  [ChromeEarlGreyUI waitForToolbarVisible:YES];

  // Long press on the omnibox to show the context menu.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::DefocusedLocationView(),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_longPress()];

  // Tap on "Hide Toolbars" in the context menu.
  id<GREYMatcher> hideToolbarsButton = grey_allOf(
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_HIDE_TOOLBARS)),
      grey_not(grey_kindOfClass([UILabel class])), grey_sufficientlyVisible(),
      nil);
  [[EarlGrey selectElementWithMatcher:hideToolbarsButton]
      performAction:grey_tap()];

  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Scroll down and up to ensure we are in forced fullscreen mode and the
  // toolbars stay hidden.
  [[EarlGrey selectElementWithMatcher:WebStateScrollViewMatcher()]
      performAction:grey_scrollInDirection(kGREYDirectionDown, 250)];
  [ChromeEarlGreyUI waitForToolbarVisible:NO];
  [[EarlGrey selectElementWithMatcher:WebStateScrollViewMatcher()]
      performAction:grey_scrollInDirection(kGREYDirectionUp, 250)];
  [ChromeEarlGreyUI waitForToolbarVisible:NO];

  // Tap on the primary toolbar (which is collapsed).
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PrimaryToolbar()]
      performAction:grey_tap()];

  // Verify that it exits force fullscreen mode and the toolbar is visible.
  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Tests that viewport-fit=cover works as intended in landscape mode.
// The test loads a page with a lime green content div and a button to toggle
// the viewport-fit meta tag.
// 1. In landscape, without viewport-fit=cover, the side gutters (safe area)
//    should be white (background color).
// 2. Tapping the button adds viewport-fit=cover to the viewport meta tag.
// 3. The content should then expand into the safe area, making the gutters
//    lime green.
- (void)testViewportFitCover {
  if ([ChromeEarlGrey isFullscreenSmoothScrollingSupported]) {
    EARL_GREY_TEST_SKIPPED(@"Smooth scrolling not supported.");
  }

  _responses["/viewport-fit"] =
      "<!DOCTYPE html>"
      "<html>"
      "<head>"
      "  <meta id='viewport' name='viewport' "
      "content='width=device-width, initial-scale=1.0'>"
      "  <style>"
      "    html { background-color: white; }"
      "    body { background-color: white; margin: 0; }"
      "    #content { "
      "      background-color: lime; "
      "      position: absolute; "
      "      top: 0; left: 0; right: 0; bottom: 0; "
      "    }"
      "    #toggle { "
      "              position: absolute; top: 50%; left: 50%; "
      "              transform: translate(-50%, -50%); "
      "              width: 200px; height: 100px; font-size: 20px; "
      "z-index: 100; "
      "              background-color: black; color: white; border: "
      "none; }"
      "  </style>"
      "  <script>"
      "    function toggle() {"
      "      var oldMeta = document.getElementById('viewport');"
      "      var newMeta = document.createElement('meta');"
      "      newMeta.id = 'viewport';"
      "      newMeta.name = 'viewport';"
      "      if "
      "(oldMeta.getAttribute('content').includes('viewport-fit=cover'))"
      " {"
      "        newMeta.setAttribute('content', 'width=device-width, "
      "initial-scale=1.0');"
      "        document.getElementById('toggle').innerText = 'Toggle "
      "(now auto)';"
      "      } else {"
      "        newMeta.setAttribute('content', 'width=device-width, "
      "initial-scale=1.0, viewport-fit=cover');"
      "        document.getElementById('toggle').innerText = 'Toggle "
      "(now cover)';"
      "      }"
      "      oldMeta.parentNode.replaceChild(newMeta, oldMeta);"
      "    }"
      "  </script>"
      "</head>"
      "<body>"
      "  <div id='content'>"
      "    <button id='toggle' onclick='toggle()'>Toggle (now "
      "auto)</button>"
      "  </div>"
      "</body>"
      "</html>";

  GURL URL = self.testServer->GetURL("/viewport-fit");
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Toggle (now auto)"];

  // Rotate to landscape.
  [EarlGrey rotateInterfaceToOrientation:UIInterfaceOrientationLandscapeLeft
                                   error:nil];

  UIEdgeInsets safeArea = [FullscreenAppInterface currentWindowSafeArea];
  CGFloat inset = safeArea.left + safeArea.right;

  // Read the width of the content area before toggling.
  int widthBefore = GetWidth([ChromeEarlGrey
      evaluateJavaScript:@"document.getElementById('content').offsetWidth"]);

  // Toggle viewport-fit=cover.
  [ChromeEarlGrey tapWebStateElementWithID:@"toggle"];

  if (inset > 0) {
    // Wait for the width to increase.
    ConditionBlock condition = ^{
      base::Value widthValue = [ChromeEarlGrey
          evaluateJavaScript:@"document.getElementById('content').offsetWidth"];
      int widthAfter = GetWidth(widthValue);
      // Check that the increase is roughly similar to the safe area insets.
      return widthAfter - widthBefore >= inset - kViewportFitCoverTolerance;
    };
    GREYAssert(
        WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, condition),
        @"Width did not increase after enabling viewport-fit=cover");
  } else {
    // If no insets, wait for the text to update to confirm action completed.
    [ChromeEarlGrey waitForWebStateContainingText:"Toggle (now cover)"
                                          timeout:kWaitForJSCompletionTimeout];

    int widthAfter = GetWidth([ChromeEarlGrey
        evaluateJavaScript:@"document.getElementById('content').offsetWidth"]);
    GREYAssertEqual(widthBefore, widthAfter,
                    @"Width changed even without safe area insets");
  }

  // Rotate back to portrait.
  [EarlGrey rotateInterfaceToOrientation:UIInterfaceOrientationPortrait
                                   error:nil];
}

- (void)testFixedElementBottom {
  // TODO(crbug.com/514648248): Re-enable this test on iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Disabled for iPad (crbug.com/514648248).");
  }

  _responses["/fixed-bottom"] = GetFixedBottomHtml();

  GURL URL = self.testServer->GetURL("/fixed-bottom");
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Fixed"];

  int elementBottom = GetWidth([ChromeEarlGrey
      evaluateJavaScript:
          @"document.getElementById('fixed').getBoundingClientRect().bottom"]);
  int screenHeight =
      [ChromeEarlGrey screenPositionOfScreenWithNumber:0].size.height;
  UIEdgeInsets insets = [FullscreenAppInterface currentViewportInsets];
  UIEdgeInsets safeArea = [FullscreenAppInterface currentWindowSafeArea];

  int screenBottom = insets.top + elementBottom;
  int expectedScreenBottom =
      screenHeight - ([FullscreenAppInterface isFullscreenRefactoringEnabled]
                          ? insets.bottom
                          : MAX(insets.bottom, safeArea.bottom));

  GREYAssertEqual(screenBottom, expectedScreenBottom,
                  @"Fixed element bottom should be on top of the bottom "
                  @"toolbar or safe area.");
}

@end

#pragma mark - Smooth scrolling enabled Tests

// Fullscreens tests for Chrome.
@interface FullscreenSmoothScrollingTestCase : ZZZFullscreenTestCase
@end

@implementation FullscreenSmoothScrollingTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(web::features::kSmoothScrollingDefault);
  config.features_enabled.push_back(kHideToolbarsInOverflowMenu);
  config.features_disabled.push_back(
      web::features::kSmoothScrollingUseDelegate);
  config.features_disabled.push_back(kFullscreenRefactoring);
  // TODO(crbug.com/511992708): Fix these tests when Chrome Next is enabled.
  config.features_disabled.push_back(kChromeNextIa);
  return config;
}

// This is currently needed to prevent this test case from being ignored.
- (void)testEmpty {
}

// Override this tests as when smooth scrolling is enabled, PDF resizing
// strategy is using content inset so the offset initial values are different.
- (void)testLongPDFInitialState {
  if (![ChromeEarlGrey isFullscreenSmoothScrollingSupported]) {
    EARL_GREY_TEST_SKIPPED(@"Smooth scrolling not supported.");
  }
  GURL URL = self.testServer->GetURL("/two_pages.pdf");
  [ChromeEarlGrey loadURL:URL];
  WaitforPDFExtensionView();
  [ChromeEarlGreyUI waitForToolbarVisible:YES];

  // Initial y scroll positions are set to make room for the toolbar.
  CGFloat expectedYOffset = -[FullscreenAppInterface currentViewportInsets].top;
  DCHECK_LT(expectedYOffset, 0);
  [[EarlGrey selectElementWithMatcher:WebStateScrollViewMatcher()]
      assertWithMatcher:grey_scrollViewContentOffset(
                            CGPointMake(0, expectedYOffset))];
}

@end

#pragma mark - Bottom omnibox Tests

// Fullscreens tests for Chrome with bottom omnibox enabled by default.
@interface FullscreenBottomOmniboxTestCase : ZZZFullscreenTestCase
@end

@implementation FullscreenBottomOmniboxTestCase

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey setBoolValue:YES
             forLocalStatePref:omnibox::kIsOmniboxInBottomPosition];
}

// This is currently needed to prevent this test case from being ignored.
- (void)testEmpty {
}

- (void)testLongPDFScroll {
  [super testLongPDFScroll];
}

@end

#pragma mark - No broadcaster tests

// Fullscreens tests for Smooth scrolling implementation listenning to
// UIScrollViewDelegate instead of using broadcaster.
@interface FullscreenNoBroadcasterTestCase : ZZZFullscreenTestCase
@end

@implementation FullscreenNoBroadcasterTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(web::features::kSmoothScrollingDefault);
  config.features_enabled.push_back(web::features::kSmoothScrollingUseDelegate);
  config.features_enabled.push_back(kHideToolbarsInOverflowMenu);
  config.features_disabled.push_back(kFullscreenRefactoring);
  // TODO(crbug.com/511992708): Fix these tests when Chrome Next is enabled.
  config.features_disabled.push_back(kChromeNextIa);
  return config;
}

// This is currently needed to prevent this test case from being ignored.
- (void)testEmpty {
}

// Override this tests as when smooth scrolling is enabled, PDF resizing
// strategy is using content inset so the offset initial values are different.
- (void)testLongPDFInitialState {
  if (![ChromeEarlGrey isFullscreenSmoothScrollingSupported]) {
    EARL_GREY_TEST_SKIPPED(@"Smooth scrolling not supported.");
  }
  GURL URL = self.testServer->GetURL("/two_pages.pdf");
  [ChromeEarlGrey loadURL:URL];
  WaitforPDFExtensionView();
  [ChromeEarlGreyUI waitForToolbarVisible:YES];

  // Initial y scroll positions are set to make room for the toolbar.
  CGFloat expectedYOffset = -[FullscreenAppInterface currentViewportInsets].top;
  DCHECK_LT(expectedYOffset, 0);
  [[EarlGrey selectElementWithMatcher:WebStateScrollViewMatcher()]
      assertWithMatcher:grey_scrollViewContentOffset(
                            CGPointMake(0, expectedYOffset))];
}

@end

#pragma mark - FullscreenRefactoring tests

// Fullscreens tests for FullscreenRefactoring implementation.
@interface FullscreenRefactoringTestCase : ZZZFullscreenTestCase
@end

@implementation FullscreenRefactoringTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kFullscreenRefactoring);
  config.features_enabled.push_back(kHideToolbarsInOverflowMenu);
  config.features_disabled.push_back(web::features::kSmoothScrollingDefault);
  config.features_enabled.push_back(kChromeNextIa);
  return config;
}

// This is currently needed to prevent this test case from being ignored.
- (void)testEmpty {
}

// Tests that starting the app in landscape mode does not cause a crash.
- (void)testStartInLandscape {
  // Load a webpage first, so that it is restored on relaunch.
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [ChromeEarlGrey waitForWebStateVisible];

  // Rotate the simulator to landscape orientation.
  [EarlGrey rotateInterfaceToOrientation:UIInterfaceOrientationLandscapeLeft
                                   error:nil];

  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Verify the app successfully started in landscape mode.
  UIInterfaceOrientation orientation = [ChromeEarlGrey interfaceOrientation];
  GREYAssertTrue(UIInterfaceOrientationIsLandscape(orientation),
                 @"App should start in landscape mode");

  // Wait for the restored web page to become visible after startup.
  [ChromeEarlGrey waitForWebStateVisible];
}

// TODO(crbug.com/499969010): Ensure PDFs display properly with new Fullscreen
// implementation.
- (void)testLongPDFInitialState {
  EARL_GREY_TEST_SKIPPED(@"Skipped for FullscreenRefactoringTestCase.");
}

// TODO(crbug.com/499969010): Ensure PDFs display properly with new Fullscreen
// implementation.
- (void)testLongPDFScroll {
  EARL_GREY_TEST_SKIPPED(@"Skipped for FullscreenRefactoringTestCase.");
}

// TODO(crbug.com/500414020): Implement force fullscreen in refactored code.
- (void)testTapOnCollapsedToolbarExitsForceFullscreenMode {
  EARL_GREY_TEST_SKIPPED(@"Skipped for FullscreenRefactoringTestCase.");
}

// Viewport-fit=cover is not supported for FullscreenRefactoring.
- (void)testViewportFitCover {
  EARL_GREY_TEST_SKIPPED(@"Skipped for FullscreenRefactoringTestCase.");
}

@end
