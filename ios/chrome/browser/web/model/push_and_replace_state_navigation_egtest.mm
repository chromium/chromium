// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/net/url_test_util.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

using chrome_test_util::BackButton;
using chrome_test_util::ForwardButton;

namespace {

const char* kHistoryTestUrl =
    "/ios/testing/data/http_server_files/history.html";
const char* kNonPushedUrl = "/ios/testing/data/http_server_files/pony.html";
const char* kReplaceStateHashWithObjectURL =
    "/ios/testing/data/http_server_files/history.html#replaceWithObject";
const char* kPushStateHashStringURL =
    "/ios/testing/data/http_server_files/history.html#string";
const char* kReplaceStateHashStringURL =
    "/ios/testing/data/http_server_files/history.html#replace";
const char* kPushStatePathURL = "/ios/testing/data/http_server_files/path";
const char* kReplaceStateRootPathSpaceURL = "/ios/rep lace";

}  // namespace

static std::unique_ptr<net::test_server::HttpResponse>
HandlePushAndReplaceStateRequest(const net::test_server::HttpRequest& request) {
  if (request.relative_url == "/foo.com/foo/bar.html") {
    NSString* baseTag = @"<base href=\"/\">";
    NSString* pushAndReplaceButtons =
        @"<input type=\"button\" value=\"pushState\" "
         "id=\"pushState\" onclick=\"history.pushState("
         "{}, 'Foo', './pushed/relative/url');\"><br>"
         "<input type=\"button\" value=\"replaceState\" "
         "id=\"replaceState\" onclick=\"history.replaceState("
         "{}, 'Foo', './replaced/relative/url');\"><br>";
    NSString* simplePage =
        @"<!doctype html><html><head>%@</head><body>%@</body></html>";

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("text/html");
    response->set_content(base::SysNSStringToUTF8([NSString
        stringWithFormat:simplePage, baseTag, pushAndReplaceButtons]));
    return response;
  }

  return nullptr;
}

// Tests for pushState/replaceState navigations.
@interface PushAndReplaceStateNavigationTestCase : ChromeTestCase
@end

@implementation PushAndReplaceStateNavigationTestCase

- (void)setUp {
  [super setUp];

  self.testServer->ServeFilesFromDirectory(
      base::PathService::CheckedGet(base::DIR_ASSETS));

  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&HandlePushAndReplaceStateRequest));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests calling history.pushState() multiple times and navigating back/forward.
- (void)testHtml5HistoryPushStateThenGoBackAndForward {
  const GURL pushStateHashWithObjectURL = self.testServer->GetURL(
      "/ios/testing/data/http_server_files/history.html#pushWithObject");
  const GURL pushStateRootPathURL = self.testServer->GetURL("/ios/rootpath");
  const GURL pushStatePathSpaceURL = self.testServer->GetURL("/ios/pa%20th");
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kHistoryTestUrl)];

  // Push 3 URLs. Verify that the URL changed and the status was updated.
  [ChromeEarlGrey tapWebStateElementWithID:@"pushStateHashWithObject"];
  [self assertStatusText:@"pushStateHashWithObject"
                 withURL:pushStateHashWithObjectURL];

  [ChromeEarlGrey tapWebStateElementWithID:@"pushStateRootPath"];
  [self assertStatusText:@"pushStateRootPath" withURL:pushStateRootPathURL];

  [ChromeEarlGrey tapWebStateElementWithID:@"pushStatePathSpace"];
  [self assertStatusText:@"pushStatePathSpace" withURL:pushStatePathSpaceURL];

  // Go back and check that the page doesn't load and the status text is updated
  // by the popstate event.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [self assertStatusText:@"pushStateRootPath" withURL:pushStateRootPathURL];

  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [self assertStatusText:@"pushStateHashWithObject"
                 withURL:pushStateHashWithObjectURL];

  [ChromeEarlGrey tapWebStateElementWithID:@"goBack"];
  [self assertStatusText:nil withURL:self.testServer->GetURL(kHistoryTestUrl)];

  // Go forward 2 pages and check that the page doesn't load and the status text
  // is updated by the popstate event.
  [ChromeEarlGrey tapWebStateElementWithID:@"goForward2"];
  [self assertStatusText:@"pushStateRootPath" withURL:pushStateRootPathURL];
}

// Tests that calling replaceState() changes the current history entry.
- (void)testHtml5HistoryReplaceStateThenGoBackAndForward {
  const GURL initialURL = self.testServer->GetURL(kNonPushedUrl);
  const std::string initialOmniboxText =
      net::GetContentAndFragmentForUrl(initialURL);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kHistoryTestUrl)];

  // Replace the URL and go back then forward.
  const GURL replaceStateHashWithObjectURL =
      self.testServer->GetURL(kReplaceStateHashWithObjectURL);
  [ChromeEarlGrey tapWebStateElementWithID:@"replaceStateHashWithObject"];
  [self assertStatusText:@"replaceStateHashWithObject"
                 withURL:replaceStateHashWithObjectURL];

  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateVisibleURL:initialURL];

  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  // TODO(crbug.com/41350773): WKWebView doesn't fire load event for
  // back/forward navigation.
  [self assertStatusText:@"replaceStateHashWithObject"
                 withURL:replaceStateHashWithObjectURL];

  // Push URL then replace it. Do this twice.
  const GURL pushStateHashStringURL =
      self.testServer->GetURL(kPushStateHashStringURL);
  [ChromeEarlGrey tapWebStateElementWithID:@"pushStateHashString"];
  [self assertStatusText:@"pushStateHashString" withURL:pushStateHashStringURL];

  const GURL replaceStateHashStringURL =
      self.testServer->GetURL(kReplaceStateHashStringURL);
  [ChromeEarlGrey tapWebStateElementWithID:@"replaceStateHashString"];
  [self assertStatusText:@"replaceStateHashString"
                 withURL:replaceStateHashStringURL];

  const GURL pushStatePathURL = self.testServer->GetURL(kPushStatePathURL);
  [ChromeEarlGrey tapWebStateElementWithID:@"pushStatePath"];
  [self assertStatusText:@"pushStatePath" withURL:pushStatePathURL];

  const GURL replaceStateRootPathSpaceURL =
      self.testServer->GetURL(kReplaceStateRootPathSpaceURL);
  [ChromeEarlGrey tapWebStateElementWithID:@"replaceStateRootPathSpace"];
  [self assertStatusText:@"replaceStateRootPathSpace"
                 withURL:replaceStateRootPathSpaceURL];

  // Go back and check URLs.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [self assertStatusText:@"replaceStateHashString"
                 withURL:replaceStateHashStringURL];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [self assertStatusText:@"replaceStateHashWithObject"
                 withURL:replaceStateHashWithObjectURL];

  // Go forward and check URL.
  [ChromeEarlGrey tapWebStateElementWithID:@"goForward2"];
  [self assertStatusText:@"replaceStateRootPathSpace"
                 withURL:replaceStateRootPathSpaceURL];
}

// Tests calling history.replaceState(), then history.pushState() and then
// navigating back/forward.
- (void)testHtml5HistoryReplaceStatePushStateThenGoBackAndForward {
  const GURL replaceStateThenPushStateURL =
      self.testServer->GetURL("/ios/testing/data/http_server_files/"
                              "history.html#replaceStateThenPushState");
  const GURL firstReplaceStateURL = self.testServer->GetURL(
      "/ios/testing/data/http_server_files/history.html#firstReplaceState");

  [ChromeEarlGrey loadURL:self.testServer->GetURL(kHistoryTestUrl)];

  // Replace state and then push state. Verify that at the end, the URL changed
  // to the pushed URL and the status was updated.
  [ChromeEarlGrey tapWebStateElementWithID:@"replaceStateThenPushState"];
  [self assertStatusText:@"replaceStateThenPushState"
                 withURL:replaceStateThenPushStateURL];

  // Go back and check URL.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [self assertStatusText:@"firstReplaceState" withURL:firstReplaceStateURL];

  // Go forward and check URL.
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [self assertStatusText:@"replaceStateThenPushState"
                 withURL:replaceStateThenPushStateURL];
}

// Tests calling history.pushState(), then history.replaceState() and then
// navigating back/forward.
- (void)testHtml5HistoryPushStateReplaceStateThenGoBackAndForward {
  const GURL firstPushStateURL = self.testServer->GetURL(
      "/ios/testing/data/http_server_files/history.html#firstPushState");
  const GURL pushStateThenReplaceStateURL =
      self.testServer->GetURL("/ios/testing/data/http_server_files/"
                              "history.html#pushStateThenReplaceState");

  const GURL historyTestURL = self.testServer->GetURL(kHistoryTestUrl);
  [ChromeEarlGrey loadURL:historyTestURL];
  const std::string historyTestOmniboxText =
      net::GetContentAndFragmentForUrl(historyTestURL);

  // Push state and then replace state. Verify that at the end, the URL changed
  // to the replaceState URL and the status was updated.
  [ChromeEarlGrey tapWebStateElementWithID:@"pushStateThenReplaceState"];
  [self assertStatusText:@"pushStateThenReplaceState"
                 withURL:pushStateThenReplaceStateURL];

  // Go back and check URL.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [self assertStatusText:nil withURL:historyTestURL];

  // Go forward and check URL.
  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  [self assertStatusText:@"pushStateThenReplaceState"
                 withURL:pushStateThenReplaceStateURL];
}

// Tests that page loads occur when navigating to or past a non-pushed URL.
- (void)testHtml5HistoryNavigatingPastNonPushedURL {
  GURL nonPushedURL = self.testServer->GetURL(kNonPushedUrl);
  const GURL historyTestURL = self.testServer->GetURL(kHistoryTestUrl);
  [ChromeEarlGrey loadURL:historyTestURL];
  const std::string historyTestOmniboxText =
      net::GetContentAndFragmentForUrl(historyTestURL);

  // Push same URL twice. Verify that URL changed and the status was updated.
  const GURL pushStateHashStringURL =
      self.testServer->GetURL(kPushStateHashStringURL);
  [ChromeEarlGrey tapWebStateElementWithID:@"pushStateHashString"];
  [self assertStatusText:@"pushStateHashString" withURL:pushStateHashStringURL];
  [ChromeEarlGrey tapWebStateElementWithID:@"pushStateHashString"];
  [self assertStatusText:@"pushStateHashString" withURL:pushStateHashStringURL];

  // Load a non-pushed URL.
  [ChromeEarlGrey loadURL:nonPushedURL];

  // Load history.html and push another URL.
  [ChromeEarlGrey loadURL:historyTestURL];
  [ChromeEarlGrey tapWebStateElementWithID:@"pushStateHashString"];
  [self assertStatusText:@"pushStateHashString" withURL:pushStateHashStringURL];

  // At this point the history looks like this:
  // [NTP, history.html, #string, #string, nonPushedURL, history.html, #string]

  // Go back (to second history.html) and verify page did not load.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [self assertStatusText:nil withURL:historyTestURL];

  // Go back twice (to second #string) and verify page did load.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  // TODO(crbug.com/41350773): WKWebView doesn't fire load event for
  // back/forward navigation.
  [self assertStatusText:nil withURL:pushStateHashStringURL];

  // Go back once (to first #string) and verify page did not load.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [self assertStatusText:@"pushStateHashString" withURL:pushStateHashStringURL];

  // Go forward 4 entries at once (to third #string) and verify page did load.
  [ChromeEarlGrey tapWebStateElementWithID:@"goForward4"];

  [self assertStatusText:nil withURL:pushStateHashStringURL];

  // Go back 4 entries at once (to first #string) and verify page did load.
  [ChromeEarlGrey tapWebStateElementWithID:@"goBack4"];

  [self assertStatusText:nil withURL:pushStateHashStringURL];
}

// Tests calling pushState with unicode characters.
- (void)testHtml5HistoryPushUnicodeCharacters {
  // The GURL object %-escapes Unicode characters in the URL's fragment,
  // but the omnibox decodes them back to Unicode for display.
  const GURL pushStateUnicodeURL = self.testServer->GetURL(
      "/ios/testing/data/http_server_files/history.html#unicode\xe1\x84\x91");
  const GURL pushStateUnicode2URL = self.testServer->GetURL(
      "/ios/testing/data/http_server_files/history.html#unicode2\xe2\x88\xa2");
  const char pushStateUnicodeLabel[] = "Action: pushStateUnicodeᄑ";
  NSString* pushStateUnicodeStatus = @"pushStateUnicodeᄑ";
  const char pushStateUnicode2Label[] = "Action: pushStateUnicode2∢";
  NSString* pushStateUnicode2Status = @"pushStateUnicode2∢";

  [ChromeEarlGrey loadURL:self.testServer->GetURL(kHistoryTestUrl)];

  // Do 2 push states with unicode characters.
  [ChromeEarlGrey tapWebStateElementWithID:@"pushStateUnicode"];
  [ChromeEarlGrey waitForWebStateVisibleURL:pushStateUnicodeURL];
  [ChromeEarlGrey waitForWebStateContainingText:pushStateUnicodeLabel];

  [ChromeEarlGrey tapWebStateElementWithID:@"pushStateUnicode2"];
  [ChromeEarlGrey waitForWebStateVisibleURL:pushStateUnicode2URL];
  [ChromeEarlGrey waitForWebStateContainingText:pushStateUnicode2Label];

  // Do a push state without a unicode character.
  const GURL pushStatePathURL = self.testServer->GetURL(kPushStatePathURL);
  [ChromeEarlGrey tapWebStateElementWithID:@"pushStatePath"];

  [self assertStatusText:@"pushStatePath" withURL:pushStatePathURL];

  // Go back and check the unicode in the URL and status.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [self assertStatusText:pushStateUnicode2Status withURL:pushStateUnicode2URL];

  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [self assertStatusText:pushStateUnicodeStatus withURL:pushStateUnicodeURL];
}

// Tests that pushState/replaceState handling properly handles <base>.
- (void)testHtml5HistoryWithBase {
  GURL originURL = self.testServer->GetURL("/foo.com/foo/bar.html");
  GURL pushResultURL =
      originURL.DeprecatedGetOriginAsURL().Resolve("pushed/relative/url");
  const std::string pushResultOmniboxText =
      net::GetContentAndFragmentForUrl(pushResultURL);
  GURL replaceResultURL =
      originURL.DeprecatedGetOriginAsURL().Resolve("replaced/relative/url");
  const std::string replaceResultOmniboxText =
      net::GetContentAndFragmentForUrl(replaceResultURL);

  [ChromeEarlGrey loadURL:originURL];
  [ChromeEarlGrey tapWebStateElementWithID:@"pushState"];
  [ChromeEarlGrey waitForWebStateVisibleURL:pushResultURL];

  [ChromeEarlGrey tapWebStateElementWithID:@"replaceState"];
  [ChromeEarlGrey waitForWebStateVisibleURL:replaceResultURL];
}

#pragma mark - Utility methods

// Assert that status text `status`, if non-nil, is displayed in the webview,
// that the visible URL is as expected, and that "onload" text is not
// displayed.
- (void)assertStatusText:(NSString*)status withURL:(const GURL&)URL {
  [ChromeEarlGrey waitForWebStateNotContainingText:"onload"];

  if (status != nil) {
    NSString* statusLabel = [NSString stringWithFormat:@"Action: %@", status];
    [ChromeEarlGrey
        waitForWebStateContainingText:base::SysNSStringToUTF8(statusLabel)];
  }

  [ChromeEarlGrey waitForWebStateVisibleURL:URL];
}

@end
