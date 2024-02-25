// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/net/url_test_util.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util.h"

using base::test::ios::kWaitForUIElementTimeout;
using chrome_test_util::NTPCollectionView;
using chrome_test_util::BackButton;
using chrome_test_util::ForwardButton;
using chrome_test_util::OmniboxText;

namespace {

// URL for the test window.history.go() test file.  The page at this URL
// contains several buttons that trigger window.history commands.  Additionally
// the page contains several divs used to display the state of the page:
// - A div that is populated with `kOnLoadText` when the onload event fires.
// - A div that is populated with `kNoOpText` 1s after a button is tapped.
// - A div that is populated with `kPopStateReceivedText` when a popstate event
//   is received by the page.
// - A div that is populated with the state object (if it's a string) upon the
//   receipt of a popstate event.
// - A div that is populated with `kHashChangeReceivedText` when a hashchange
//   event is received.
// When a button on the page is tapped, all pre-existing div text is cleared,
// so matching against this webview text after a button is tapped ensures that
// the state is set in response to the most recently executed script.
const char kWindowHistoryGoTestURL[] = "/history_go.html";
// URL for a file based test page which gives a simple string response.
const char kSimpleFileBasedTestURL[] = "/pony.html";

// Strings used by history_go.html.
const char kOnLoadText[] = "OnLoadText";
const char kNoOpText[] = "NoOpText";

// Button ids for history_go.html.
NSString* const kGoNoParameterID = @"go-no-parameter";
NSString* const kGoZeroID = @"go-zero";
NSString* const kGoForwardID = @"go-forward";
NSString* const kGoTwoID = @"go-2";
NSString* const kGoBackID = @"go-back";
NSString* const kGoBackTwoID = @"go-back-2";

// URLs and labels for testWindowLocation* tests.
NSString* kHashChangeWithHistoryLabel = @"hashChangedWithHistory";
NSString* kHashChangeWithoutHistoryLabel = @"hashChangedWithoutHistory";
const char kPage1URL[] = "/page1/";
const char kHashChangedWithHistoryURL[] = "/page1/#hashChangedWithHistory";
const char kHashChangedWithoutHistoryURL[] =
    "/page1/#hashChangedWithoutHistory";
const char kNoHashChangeText[] = "No hash change";
// An HTML page with two links that run JavaScript when they're clicked. The
// first link updates `window.location.hash`, the second link changes
// `window.location`.
const char kHashChangedHTML[] =
    "<html><body>"
    "<a href='javascript:window.location.hash=\"#hashChangedWithHistory\"' "
    "   id=\"hashChangedWithHistory\"'>hashChangedWithHistory</a><br />"
    "<a href='javascript:"
    "           window.location.replace(\"#hashChangedWithoutHistory\")' "
    "   id=\"hashChangedWithoutHistory\">hashChangedWithoutHistory</a>"
    "</body></html>";

// URLs for server redirect tests.
const char kRedirectIndexURL[] = "/redirect";
const char kRedirectWindowURL[] = "/redirectWindow.html";
const char kDestinationURL[] = "/destination.html";
// Default URL for a sample html page. It is registered in the default handlers.
const char kDefaultPageURL[] = "/defaultresponse";

// Provides responses for redirect and changed window location URLs.
std::unique_ptr<net::test_server::HttpResponse> RedirectHandlers(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_OK);
  if (request.relative_url == kRedirectIndexURL) {
    http_response->set_content(
        "<p><a href=\"server-redirect?destination.html\""
        "      id=\"redirect301\">redirect301</a></p>"
        "<p><a href=\"client-redirect?destination.html\""
        "      id=\"redirectRefresh\">redirectRefresh</a></p>"
        "<p><a href=\"redirectWindow.html\""
        "      id=\"redirectWindow\">redirectWindow</a></p>");
  } else if (request.relative_url == kRedirectWindowURL) {
    http_response->set_content(
        "<head>"
        "  <meta HTTP-EQUIV=\"REFRESH\" content=\"0; url=destination.html\">"
        "</head>"
        "<body>Redirecting"
        "  <script>window.open(\"destination.html\", \"_self\");</script>"
        "</body>");
  } else {
    return nullptr;
  }
  return std::move(http_response);
}

// Provides responses for redirect and changed window location URLs.
std::unique_ptr<net::test_server::HttpResponse> WindowLocationHashHandlers(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_OK);
  if (request.relative_url != kPage1URL) {
    return nullptr;
  }
  http_response->set_content(kHashChangedHTML);
  return std::move(http_response);
}

}  // namespace

// Integration tests for navigating history via JavaScript and the forward and
// back buttons.
@interface NavigationTestCase : ChromeTestCase

// Adds hashchange listener to the page that changes the inner html of the page
// to `content` when a hashchange is detected.
- (void)addHashChangeListenerWithContent:(std::string)content;

// Loads index page for redirect operations, taps the link with `redirectLabel`
// and then perform series of back-forward navigations asserting the proper
// behavior.
- (void)verifyBackAndForwardAfterRedirect:(std::string)redirectLabel;

@end

@implementation NavigationTestCase

#pragma mark window.history.go operations

// Tests reloading the current page via window.history.go() with no parameters.
- (void)testHistoryGoNoParameter {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  // Load the history test page and ensure that its onload text is visible.
  const GURL windowHistoryURL =
      self.testServer->GetURL(kWindowHistoryGoTestURL);
  [ChromeEarlGrey loadURL:windowHistoryURL];
  [ChromeEarlGrey waitForWebStateContainingText:kOnLoadText];

  // Tap on the window.history.go() button.  This will clear `kOnLoadText`, so
  // the subsequent check for `kOnLoadText` will only pass if a reload has
  // occurred.
  [ChromeEarlGrey tapWebStateElementWithID:kGoNoParameterID];

  // Verify that the onload text is reset.
  [ChromeEarlGrey waitForWebStateContainingText:kOnLoadText];
}

// Tests reloading the current page via history.go(0).
- (void)testHistoryGoDeltaZero {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  // Load the history test page and ensure that its onload text is visible.
  const GURL windowHistoryURL =
      self.testServer->GetURL(kWindowHistoryGoTestURL);
  [ChromeEarlGrey loadURL:windowHistoryURL];
  [ChromeEarlGrey waitForWebStateContainingText:kOnLoadText];

  // Tap on the window.history.go() button.  This will clear `kOnLoadText`, so
  // the subsequent check for `kOnLoadText` will only pass if a reload has
  // occurred.
  [ChromeEarlGrey tapWebStateElementWithID:kGoZeroID];

  // Verify that the onload text is reset.
  [ChromeEarlGrey waitForWebStateContainingText:kOnLoadText];
}

// Tests that calling window.history.go() with an offset that is out of bounds
// is a no-op.
- (void)testHistoryGoOutOfBounds {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  // Load the history test page and ensure that its onload text is visible.
  const GURL windowHistoryURL =
      self.testServer->GetURL(kWindowHistoryGoTestURL);
  [ChromeEarlGrey loadURL:windowHistoryURL];
  [ChromeEarlGrey waitForWebStateContainingText:kOnLoadText];

  // Tap on the window.history.go(2) button.  This will clear all div text, so
  // the subsequent check for `kNoOpText` will only pass if no navigations have
  // occurred.
  [ChromeEarlGrey tapWebStateElementWithID:kGoTwoID];
  [ChromeEarlGrey waitForWebStateContainingText:kNoOpText];

  // Tap on the window.history.go(-2) button.  This will clear all div text, so
  // the subsequent check for `kNoOpText` will only pass if no navigations have
  // occurred.
  [ChromeEarlGrey tapWebStateElementWithID:kGoBackTwoID];
  [ChromeEarlGrey waitForWebStateContainingText:kNoOpText];
}

// Tests going back and forward via history.go().
- (void)testHistoryGoDelta {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL firstURL = self.testServer->GetURL(kWindowHistoryGoTestURL);
  const GURL secondURL = self.testServer->GetURL("/memory_usage.html");
  const GURL thirdURL = self.testServer->GetURL(kSimpleFileBasedTestURL);
  const GURL fourthURL = self.testServer->GetURL("/history.html");

  // Load 4 pages.
  [ChromeEarlGrey loadURL:firstURL];
  [ChromeEarlGrey loadURL:secondURL];
  [ChromeEarlGrey loadURL:thirdURL];
  [ChromeEarlGrey loadURL:fourthURL];
  [ChromeEarlGrey waitForWebStateContainingText:"onload"];

  // Tap button to go back 3 pages.
  [ChromeEarlGrey tapWebStateElementWithID:@"goBack3"];
  [ChromeEarlGrey waitForWebStateContainingText:kOnLoadText];
  [[EarlGrey selectElementWithMatcher:OmniboxText(firstURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Tap button to go forward 2 pages.
  [ChromeEarlGrey tapWebStateElementWithID:kGoTwoID];
  [ChromeEarlGrey waitForWebStateContainingText:"pony"];
  [[EarlGrey selectElementWithMatcher:OmniboxText(thirdURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that calls to window.history.go() that span multiple documents causes
// a load to occur.
- (void)testHistoryCrossDocumentLoad {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  // Load the history test page and ensure that its onload text is visible.
  const GURL windowHistoryURL =
      self.testServer->GetURL(kWindowHistoryGoTestURL);
  [ChromeEarlGrey loadURL:windowHistoryURL];
  [ChromeEarlGrey waitForWebStateContainingText:kOnLoadText];

  const GURL sampleURL = self.testServer->GetURL(kSimpleFileBasedTestURL);
  [ChromeEarlGrey loadURL:sampleURL];

  [ChromeEarlGrey loadURL:windowHistoryURL];
  [ChromeEarlGrey waitForWebStateContainingText:kOnLoadText];

  // Tap the window.history.go(-2) button.  This will clear the current page's
  // `kOnLoadText`, so the subsequent check will only pass if another load
  // occurs.
  [ChromeEarlGrey tapWebStateElementWithID:kGoBackTwoID];
  [ChromeEarlGrey waitForWebStateContainingText:kOnLoadText];
}

#pragma mark window.history.[back/forward] operations

// Tests going back via history.back() then forward via forward button.
- (void)testHistoryBackNavigation {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  // Navigate to a URL.
  const GURL firstURL = self.testServer->GetURL(kSimpleFileBasedTestURL);
  [ChromeEarlGrey loadURL:firstURL];

  // Navigate to an HTML page with a back button.
  const GURL secondURL = self.testServer->GetURL(kWindowHistoryGoTestURL);
  [ChromeEarlGrey loadURL:secondURL];

  // Tap the back button in the HTML and verify the first URL is loaded.
  [ChromeEarlGrey tapWebStateElementWithID:kGoBackID];
  [ChromeEarlGrey waitForWebStateContainingText:"pony"];
  [[EarlGrey selectElementWithMatcher:OmniboxText(firstURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Tap the forward button in the toolbar and verify the second URL is loaded.
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OmniboxText(secondURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests going back via back button then forward via history.forward().
- (void)testHistoryForwardNavigation {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  // Navigate to an HTML page with a forward button.
  const GURL firstURL = self.testServer->GetURL(kWindowHistoryGoTestURL);
  [ChromeEarlGrey loadURL:firstURL];

  // Navigate to some other page.
  const GURL secondURL = self.testServer->GetURL(kSimpleFileBasedTestURL);
  [ChromeEarlGrey loadURL:secondURL];

  // Tap the back button in the toolbar and verify the page with forward button
  // is loaded.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kOnLoadText];
  [[EarlGrey selectElementWithMatcher:OmniboxText(firstURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Tap the forward button in the HTML and verify the second URL is loaded.
  [ChromeEarlGrey tapWebStateElementWithID:kGoForwardID];
  [ChromeEarlGrey waitForWebStateContainingText:"pony"];
  [[EarlGrey selectElementWithMatcher:OmniboxText(secondURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Verify that the forward button is visible but not enabled.
  id<GREYMatcher> disabledForwardButton =
      grey_allOf(ForwardButton(),
                 grey_accessibilityTrait(UIAccessibilityTraitNotEnabled), nil);
  [[EarlGrey selectElementWithMatcher:disabledForwardButton]
      assertWithMatcher:grey_notNil()];
}

// Test back-and-forward navigation from and to NTP.
- (void)testHistoryBackAndForwardAroundNTP {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL testURL = self.testServer->GetURL(kSimpleFileBasedTestURL);
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:"pony"];

  // Tap the back button and verify NTP is loaded.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForPageToFinishLoading];
  [[EarlGrey selectElementWithMatcher:NTPCollectionView()]
      assertWithMatcher:grey_notNil()];

  // Tap the forward button and verify test page is loaded.
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:"pony"];
}

#pragma mark window.location.hash operations

// Loads a URL and modifies window.location.hash, then goes back and forward
// and verifies the URLs and that hashchange event is fired.
- (void)testWindowLocationChangeHash {
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&WindowLocationHashHandlers));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL page1URL = self.testServer->GetURL(kPage1URL);
  const GURL hashChangedWithHistoryURL =
      self.testServer->GetURL(kHashChangedWithHistoryURL);

  [ChromeEarlGrey loadURL:page1URL];

  // Click link to update location.hash and go to new URL (same page).
  [ChromeEarlGrey tapWebStateElementWithID:kHashChangeWithHistoryLabel];

  // Navigate back to original URL. This should fire a hashchange event.
  std::string backHashChangeContent = "backHashChange";
  [self addHashChangeListenerWithContent:backHashChangeContent];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  const std::string page1OmniboxText =
      net::GetContentAndFragmentForUrl(page1URL);
  [[EarlGrey selectElementWithMatcher:OmniboxText(page1OmniboxText)]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForWebStateContainingText:backHashChangeContent];

  // Navigate forward to the new URL. This should fire a hashchange event.
  std::string forwardHashChangeContent = "forwardHashChange";
  [self addHashChangeListenerWithContent:forwardHashChangeContent];
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  const std::string hashChangedWithHistoryOmniboxText =
      net::GetContentAndFragmentForUrl(hashChangedWithHistoryURL);
  [[EarlGrey
      selectElementWithMatcher:OmniboxText(hashChangedWithHistoryOmniboxText)]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForWebStateContainingText:forwardHashChangeContent];

  // Load a hash URL directly. This shouldn't fire a hashchange event.
  std::string hashChangeContent = "FAIL_loadUrlHashChange";
  [self addHashChangeListenerWithContent:hashChangeContent];
  [ChromeEarlGrey loadURL:hashChangedWithHistoryURL];
  [ChromeEarlGrey waitForWebStateNotContainingText:hashChangeContent];
}

// Loads a URL and replaces its location, then updates its location.hash
// and verifies that going back returns to the replaced entry.
- (void)testWindowLocationReplaceAndChangeHash {
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&WindowLocationHashHandlers));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL page1URL = self.testServer->GetURL(kPage1URL);
  const GURL hashChangedWithHistoryURL =
      self.testServer->GetURL(kHashChangedWithHistoryURL);
  const GURL hashChangedWithoutHistoryURL =
      self.testServer->GetURL(kHashChangedWithoutHistoryURL);

  [ChromeEarlGrey loadURL:page1URL];

  // Tap link to replace the location value.
  [ChromeEarlGrey tapWebStateElementWithID:kHashChangeWithoutHistoryLabel];
  const std::string hashChangedWithoutHistoryOmniboxText =
      net::GetContentAndFragmentForUrl(hashChangedWithoutHistoryURL);
  [[EarlGrey selectElementWithMatcher:OmniboxText(
                                          hashChangedWithoutHistoryOmniboxText)]
      assertWithMatcher:grey_notNil()];

  // Tap link to update the location.hash with a new value.
  [ChromeEarlGrey tapWebStateElementWithID:kHashChangeWithHistoryLabel];
  const std::string hashChangedWithHistoryOmniboxText =
      net::GetContentAndFragmentForUrl(hashChangedWithHistoryURL);
  [[EarlGrey
      selectElementWithMatcher:OmniboxText(hashChangedWithHistoryOmniboxText)]
      assertWithMatcher:grey_notNil()];

  // Navigate back and verify that the URL that replaced window.location
  // has been reached.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OmniboxText(
                                          hashChangedWithoutHistoryOmniboxText)]
      assertWithMatcher:grey_notNil()];
}

// Loads a URL and modifies window.location.hash twice, verifying that there is
// only one entry in the history by navigating back.
- (void)testWindowLocationChangeToSameHash {
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&WindowLocationHashHandlers));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL page1URL = self.testServer->GetURL(kPage1URL);
  const GURL hashChangedWithHistoryURL =
      self.testServer->GetURL(kHashChangedWithHistoryURL);

  [ChromeEarlGrey loadURL:page1URL];

  // Tap link to update location.hash with a new value.
  [ChromeEarlGrey tapWebStateElementWithID:kHashChangeWithHistoryLabel];
  const std::string hashChangedWithHistoryOmniboxText =
      net::GetContentAndFragmentForUrl(hashChangedWithHistoryURL);
  [[EarlGrey
      selectElementWithMatcher:OmniboxText(hashChangedWithHistoryOmniboxText)]
      assertWithMatcher:grey_notNil()];

  // Tap link to update location.hash with the same value.
  [ChromeEarlGrey tapWebStateElementWithID:kHashChangeWithHistoryLabel];

  // Tap back once to return to original URL.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  const std::string page1OmniboxText =
      net::GetContentAndFragmentForUrl(page1URL);
  [[EarlGrey selectElementWithMatcher:OmniboxText(page1OmniboxText)]
      assertWithMatcher:grey_notNil()];

  // Navigate forward and verify the URL.
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:OmniboxText(hashChangedWithHistoryOmniboxText)]
      assertWithMatcher:grey_notNil()];
}

#pragma mark Redirect operations

// Navigates to a page that immediately redirects to another page via JavaScript
// then verifies the browsing history.
- (void)testJavaScriptRedirect {
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&RedirectHandlers));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  // A starting page.
  const GURL initialURL = self.testServer->GetURL(kDefaultPageURL);
  // A page that redirects immediately via the window.open JavaScript method.
  const GURL originURL = self.testServer->GetURL(kRedirectWindowURL);
  const GURL destinationURL = self.testServer->GetURL(kDestinationURL);

  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey loadURL:originURL];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Navigating back takes the user to the new tab page.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OmniboxText(initialURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Navigating forward take the user to destination page.
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Test to load a page that contains a redirect window, then does multiple back
// and forth navigations.
- (void)testRedirectWindow {
  [self verifyBackAndForwardAfterRedirect:"redirectWindow"];
}

// Test to load a page that contains a redirect refresh, then does multiple back
// and forth navigations.
- (void)testRedirectRefresh {
  [self verifyBackAndForwardAfterRedirect:"redirectRefresh"];
}

// Test to load a page that performs a 301 redirect, then does multiple back and
// forth navigations.
- (void)test301Redirect {
  [self verifyBackAndForwardAfterRedirect:"redirect301"];
}

#pragma mark Utility methods

- (void)addHashChangeListenerWithContent:(std::string)content {
  NSString* const script =
      [NSString stringWithFormat:
                    @"document.body.innerHTML = '%s';"
                     "window.addEventListener('hashchange', function(event) {"
                     "   document.body.innerHTML = '%s';"
                     "});",
                    kNoHashChangeText, content.c_str()];

  [ChromeEarlGrey evaluateJavaScriptForSideEffect:script];
}

- (void)verifyBackAndForwardAfterRedirect:(std::string)redirectLabel {
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&RedirectHandlers));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL indexURL(self.testServer->GetURL(kRedirectIndexURL));
  const GURL destinationURL(self.testServer->GetURL(kDestinationURL));
  const GURL lastURL(self.testServer->GetURL(kDefaultPageURL));

  // Load index, tap on redirect link, and assert that the page is redirected
  // to the proper destination.
  [ChromeEarlGrey loadURL:indexURL];
  [ChromeEarlGrey
      tapWebStateElementWithID:
          [NSString stringWithCString:redirectLabel.c_str()
                             encoding:[NSString defaultCStringEncoding]]];
  [ChromeEarlGrey waitForWebStateContainingText:"You've arrived"];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Navigate to a new URL, navigate back and assert that the resulting page is
  // the proper destination.
  [ChromeEarlGrey loadURL:lastURL];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:"You've arrived"];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Navigate back and assert that the resulting page is the initial index.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:redirectLabel];
  [[EarlGrey selectElementWithMatcher:OmniboxText(indexURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Navigate forward and assert the the resulting page is the proper
  // destination.
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:"You've arrived"];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that navigating forward from a WebUI URL works when resuming from
// session restore. This is a regression test for https://crbug.com/814790.
- (void)testRestoreHistoryToWebUIAndNavigateForward {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL destinationURL = self.testServer->GetURL(kSimpleFileBasedTestURL);
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [ChromeEarlGrey loadURL:destinationURL];
  [ChromeEarlGrey goBack];

  [self triggerRestoreByRestartingApplication];

  [ChromeEarlGrey waitForWebStateContainingText:"Revision"];
  [[EarlGrey selectElementWithMatcher:OmniboxText("chrome://version")]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:"pony"];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that navigating forward from NTP works when resuming from session
// restore. This is a regression test for https://crbug.com/814790.
- (void)testRestoreHistoryToNTPAndNavigateForward {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL destinationURL = self.testServer->GetURL(kSimpleFileBasedTestURL);
  [ChromeEarlGrey loadURL:destinationURL];
  [ChromeEarlGrey goBack];

  [self triggerRestoreByRestartingApplication];

  [ChromeEarlGrey goForward];

  // Navigating right after session restore seems to sometimes be slow, so wait
  // with twice the usual timeout.
  [ChromeEarlGrey waitForWebStateContainingText:"pony"];
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that restoring a placeholder URL is correctly restored.  This is a
// regression test from http://crbug.com/1011758.
- (void)testRestoreHistoryToPlaceholderURL {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL destinationURL("chrome://crash");
  [ChromeEarlGrey loadURL:destinationURL];
  [self triggerRestoreByRestartingApplication];
  [[EarlGrey selectElementWithMatcher:OmniboxText("chrome://crash")]
      assertWithMatcher:grey_notNil()];
}

- (void)testEdgeSwipe {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kSimpleFileBasedTestURL)];
  [ChromeEarlGrey waitForWebStateContainingText:"pony"];
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/history.html")];

  // Edge swipes don't work with EG, use XCUI directly.
  XCUIApplication* app = [[XCUIApplication alloc] init];

  // Swiping back from WKWebView to WKWebView or to NTP seems fine with an edge
  // of zero.
  CGFloat leftEdge = 0;
  XCUICoordinate* leftEdgeCoord =
      [app coordinateWithNormalizedOffset:CGVectorMake(leftEdge, 0.5)];
  XCUICoordinate* swipeRight =
      [leftEdgeCoord coordinateWithOffset:CGVectorMake(600, 0.5)];

  // Swipe back twice.
  [leftEdgeCoord pressForDuration:0.1f thenDragToCoordinate:swipeRight];
  GREYWaitForAppToIdle(@"App failed to idle");
  [leftEdgeCoord pressForDuration:0.1f thenDragToCoordinate:swipeRight];
  GREYWaitForAppToIdle(@"App failed to idle");

  // Verify the NTP is visible.
  [ChromeEarlGrey waitForPageToFinishLoading];
  [[EarlGrey selectElementWithMatcher:NTPCollectionView()]
      assertWithMatcher:grey_notNil()];

  // Swiping forward on a WKWebView works with an edge of one, but swiping
  // forward from the NTP seems to fail with one, so use 0.99.
  CGFloat rightEdgeNTP = 0.99;
  CGFloat rightEdge = 1;
  XCUICoordinate* rightEdgeCoordFromNTP =
      [app coordinateWithNormalizedOffset:CGVectorMake(rightEdgeNTP, 0.5)];
  XCUICoordinate* swipeLeftFromNTP =
      [rightEdgeCoordFromNTP coordinateWithOffset:CGVectorMake(-600, 0.5)];

  // Swiping forward twice and verify each page.
  [rightEdgeCoordFromNTP pressForDuration:0.1f
                     thenDragToCoordinate:swipeLeftFromNTP];
  GREYWaitForAppToIdle(@"App failed to idle");
  [ChromeEarlGrey waitForWebStateContainingText:"pony"];

  XCUICoordinate* rightEdgeCoord =
      [app coordinateWithNormalizedOffset:CGVectorMake(rightEdge, 0.5)];
  XCUICoordinate* swipeLeft =
      [rightEdgeCoord coordinateWithOffset:CGVectorMake(-600, 0.5)];
  [rightEdgeCoord pressForDuration:0.1f thenDragToCoordinate:swipeLeft];
  GREYWaitForAppToIdle(@"App failed to idle");
  [ChromeEarlGrey waitForWebStateContainingText:"onload"];
}

@end
