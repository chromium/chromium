// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include <map>
#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/chrome/test/earl_grey/scoped_block_popups_pref.h"
#include "ios/net/url_test_util.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ios/web/public/test/http_server/data_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "net/http/http_response_headers.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::OmniboxText;
using chrome_test_util::OmniboxContainingText;

namespace {

// URL used for the reload test.
const char kReloadTestUrl[] = "http://mock/reloadTest";

// Returns the number of serviced requests in HTTP body.
class ReloadResponseProvider : public web::DataResponseProvider {
 public:
  ReloadResponseProvider() : request_number_(0) {}

  // URL used for the reload test.
  static GURL GetReloadTestUrl() {
    return web::test::HttpServer::MakeUrl(kReloadTestUrl);
  }

  bool CanHandleRequest(const Request& request) override {
    return request.url == ReloadResponseProvider::GetReloadTestUrl();
  }

  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override {
    DCHECK_EQ(ReloadResponseProvider::GetReloadTestUrl(), request.url);
    *headers = GetDefaultResponseHeaders();
    *response_body = GetResponseBody(request_number_++);
  }

  // static
  static std::string GetResponseBody(int request_number) {
    return base::StringPrintf("Load request %d", request_number);
  }

 private:
  int request_number_;  // Count of requests received by the response provider.
};

}  // namespace

// Tests web browsing scenarios.
@interface BrowsingTestCase : ChromeTestCase
@end

@implementation BrowsingTestCase

// Matcher for the title of the current tab (on tablet only).
id<GREYMatcher> TabWithTitle(const std::string& tab_title) {
  id<GREYMatcher> notPartOfOmnibox =
      grey_not(grey_ancestor(chrome_test_util::Omnibox()));
  return grey_allOf(grey_accessibilityLabel(base::SysUTF8ToNSString(tab_title)),
                    notPartOfOmnibox, nil);
}

// Tests that page successfully reloads.
- (void)testReload {
  // Set up test HTTP server responses.
  std::unique_ptr<web::DataResponseProvider> provider(
      new ReloadResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  GURL URL = ReloadResponseProvider::GetReloadTestUrl();
  [ChromeEarlGrey loadURL:URL];
  std::string expectedBodyBeforeReload(
      ReloadResponseProvider::GetResponseBody(0 /* request number */));
  [ChromeEarlGrey waitForWebStateContainingText:expectedBodyBeforeReload];

  [ChromeEarlGreyUI reload];
  std::string expectedBodyAfterReload(
      ReloadResponseProvider::GetResponseBody(1 /* request_number */));
  [ChromeEarlGrey waitForWebStateContainingText:expectedBodyAfterReload];
}

// Tests that a tab's title is based on the URL when no other information is
// available.
- (void)testBrowsingTabTitleSetFromURL {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Tab Title not displayed on handset.");
  }

  web::test::SetUpFileBasedHttpServer();

  const GURL destinationURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
  [ChromeEarlGrey loadURL:destinationURL];

  // Add 3 for the "://" which is not considered part of the scheme
  std::string URLWithoutScheme =
      destinationURL.spec().substr(destinationURL.scheme().length() + 3);

  [[EarlGrey selectElementWithMatcher:TabWithTitle(URLWithoutScheme)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that after a PDF is loaded, the title appears in the tab bar on iPad.
- (void)testPDFLoadTitle {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Tab Title not displayed on handset.");
  }

  web::test::SetUpFileBasedHttpServer();

  const GURL destinationURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/testpage.pdf");
  [ChromeEarlGrey loadURL:destinationURL];

  // Add 3 for the "://" which is not considered part of the scheme
  std::string URLWithoutScheme =
      destinationURL.spec().substr(destinationURL.scheme().length() + 3);

  [[EarlGrey selectElementWithMatcher:TabWithTitle(URLWithoutScheme)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that tab title is set to the specified title from a JavaScript.
- (void)testBrowsingTabTitleSetFromScript {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Tab Title not displayed on handset.");
  }

  const char* kPageTitle = "Some title";
  const GURL URL = GURL(base::StringPrintf(
      "data:text/html;charset=utf-8,<script>document.title = "
      "\"%s\"</script>",
      kPageTitle));
  [ChromeEarlGrey loadURL:URL];

  [[EarlGrey selectElementWithMatcher:TabWithTitle(kPageTitle)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests clicking a link with target="_blank" and "event.stopPropagation()"
// opens a new tab.
- (void)testBrowsingStopPropagation {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl("http://stopPropagation");
  const GURL destinationURL =
      web::test::HttpServer::MakeUrl("http://destination");
  // This is a page with a link to |kDestination|.
  responses[URL] = base::StringPrintf(
      "<a id='link' href='%s' target='_blank' "
      "onclick='event.stopPropagation()'>link</a>",
      destinationURL.spec().c_str());
  // This is the destination page; it just contains some text.
  responses[destinationURL] = "You've arrived!";
  web::test::SetUpSimpleHttpServer(responses);

  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_ALLOW);

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForMainTabCount:1];

  [ChromeEarlGrey tapWebStateElementWithID:@"link"];
  [ChromeEarlGrey waitForMainTabCount:2];

  // Verify the new tab was opened with the expected URL.
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests clicking a relative link with target="_blank" and
// "event.stopPropagation()" opens a new tab.
- (void)testBrowsingStopPropagationRelativePath {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl("http://stopPropRel");
  const GURL destinationURL =
      web::test::HttpServer::MakeUrl("http://stopPropRel/#test");
  // This is page with a relative link to "#test".
  responses[URL] =
      "<a id='link' href='#test' target='_blank' "
      "onclick='event.stopPropagation()'>link</a>";
  // This is the page that should be showing at the end of the test.
  responses[destinationURL] = "You've arrived!";
  web::test::SetUpSimpleHttpServer(responses);

  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_ALLOW);

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForMainTabCount:1];

  [ChromeEarlGrey tapWebStateElementWithID:@"link"];

  [ChromeEarlGrey waitForMainTabCount:2];

  // Verify the new tab was opened with the expected URL.
  const std::string omniboxText =
      net::GetContentAndFragmentForUrl(destinationURL);
  [[EarlGrey selectElementWithMatcher:OmniboxText(omniboxText)]
      assertWithMatcher:grey_notNil()];
}

// Tests that clicking a link with URL changed by onclick uses the href of the
// anchor tag instead of the one specified in JavaScript. Also verifies a new
// tab is opened by target '_blank'.
// TODO(crbug.com/688223): WKWebView does not open a new window as expected by
// this test.
- (void)DISABLED_testBrowsingPreventDefaultWithLinkOpenedByJavascript {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl(
      "http://preventDefaultWithLinkOpenedByJavascript");
  const GURL anchorURL =
      web::test::HttpServer::MakeUrl("http://anchorDestination");
  const GURL destinationURL =
      web::test::HttpServer::MakeUrl("http://javaScriptDestination");
  // This is a page with a link where the href and JavaScript are setting the
  // destination to two different URLs so the test can verify which one the
  // browser uses.
  responses[URL] = base::StringPrintf(
      "<a id='link' href='%s' target='_blank' "
      "onclick='window.location.href=\"%s\"; "
      "event.stopPropagation()' id='link'>link</a>",
      anchorURL.spec().c_str(), destinationURL.spec().c_str());
  responses[anchorURL] = "anchor destination";

  web::test::SetUpSimpleHttpServer(responses);

  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_ALLOW);

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForMainTabCount:1];

  [ChromeEarlGrey tapWebStateElementWithID:@"link"];
  [ChromeEarlGrey waitForMainTabCount:2];

  // Verify the new tab was opened with the expected URL.
  [[EarlGrey selectElementWithMatcher:OmniboxText(anchorURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests tapping a link that navigates to a page that immediately navigates
// again via document.location.href.
- (void)testBrowsingWindowDataLinkScriptRedirect {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL =
      web::test::HttpServer::MakeUrl("http://windowDataLinkScriptRedirect");
  const GURL intermediateURL =
      web::test::HttpServer::MakeUrl("http://intermediate");
  const GURL destinationURL =
      web::test::HttpServer::MakeUrl("http://destination");
  // This is a page with a link to the intermediate page.
  responses[URL] =
      base::StringPrintf("<a id='link' href='%s' target='_blank'>link</a>",
                         intermediateURL.spec().c_str());
  // This intermediate page uses JavaScript to immediately navigate to the
  // destination page.
  responses[intermediateURL] =
      base::StringPrintf("<script>document.location.href=\"%s\"</script>",
                         destinationURL.spec().c_str());
  // This is the page that should be showing at the end of the test.
  responses[destinationURL] = "You've arrived!";

  web::test::SetUpSimpleHttpServer(responses);

  ScopedBlockPopupsPref prefSetter(CONTENT_SETTING_ALLOW);

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForMainTabCount:1];

  [ChromeEarlGrey tapWebStateElementWithID:@"link"];
  [ChromeEarlGrey waitForMainTabCount:2];

  // Verify the new tab was opened with the expected URL.
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that a link with a JavaScript-based navigation changes the page and
// that the back button works as expected afterwards.
- (void)testBrowsingJavaScriptBasedNavigation {
  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl("http://origin");
  const GURL destURL = web::test::HttpServer::MakeUrl("http://destination");
  // Page containing a link with onclick attribute that sets window.location
  // to the destination URL.
  responses[URL] = base::StringPrintf(
      "<a href='#' onclick=\"window.location='%s';\" id='link'>Link</a>",
      destURL.spec().c_str());
  // Page with some text.
  responses[destURL] = "You've arrived!";
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey tapWebStateElementWithID:@"link"];

  [[EarlGrey selectElementWithMatcher:OmniboxText(destURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:"Link"];

  if ([ChromeEarlGrey isSlimNavigationManagerEnabled]) {
    // Using partial match for Omnibox text because the displayed URL is now
    // "http://origin/#" due to the link click. This is consistent with all
    // other browsers.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
        assertWithMatcher:chrome_test_util::OmniboxContainingText(
                              URL.GetContent())];
    GREYAssertEqual(web::test::HttpServer::MakeUrl("http://origin/#"),
                    [ChromeEarlGrey webStateVisibleURL],
                    @"Unexpected URL after going back");
  } else {
    [[EarlGrey selectElementWithMatcher:OmniboxText(URL.GetContent())]
        assertWithMatcher:grey_notNil()];
  }
}

// Tests that a link with WebUI URL does not trigger a load. WebUI pages may
// have increased power and using the same web process (which may potentially
// be controlled by an attacker) is dangerous.
- (void)testTapLinkWithWebUIURL {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL(web::test::HttpServer::MakeUrl("http://pageWithWebUILink"));
  const char kPageHTML[] =
      "<script>"
      "  function printMsg() {"
      "    document.body.appendChild(document.createTextNode('Hello world!'));"
      "  }"
      "</script>"
      "<a href='chrome://version' id='link' onclick='printMsg()'>Version</a>";
  responses[URL] = kPageHTML;
  web::test::SetUpSimpleHttpServer(responses);

  // Assert that test is starting with one tab.
  [ChromeEarlGrey waitForMainTabCount:1];
  [ChromeEarlGrey waitForIncognitoTabCount:0];

  [ChromeEarlGrey loadURL:URL];

  // Tap on chrome://version link.
  [ChromeEarlGrey tapWebStateElementWithID:@"link"];

  // Verify that page did not change by checking its URL and message printed by
  // onclick event.
  [[EarlGrey selectElementWithMatcher:OmniboxText("chrome://version")]
      assertWithMatcher:grey_nil()];
  [ChromeEarlGrey waitForWebStateContainingText:"Hello world!"];

  // Verify that no new tabs were open which could load chrome://version.
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests that evaluating user JavaScript that causes navigation correctly
// modifies history.
- (void)testBrowsingUserJavaScriptNavigation {
  // TODO(crbug.com/703855): Keyboard entry inside the omnibox fails only on
  // iPad.
  if ([ChromeEarlGrey isIPadIdiom])
    return;

  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL startURL = web::test::HttpServer::MakeUrl("http://startpage");
  responses[startURL] = "<html><body><p>Ready to begin.</p></body></html>";
  const GURL targetURL = web::test::HttpServer::MakeUrl("http://targetpage");
  responses[targetURL] = "<html><body><p>You've arrived!</p></body></html>";
  web::test::SetUpSimpleHttpServer(responses);

  // Load the first page and run JS (using the codepath that user-entered JS in
  // the omnibox would take, not page-triggered) that should navigate.
  [ChromeEarlGrey loadURL:startURL];

  NSString* script =
      [NSString stringWithFormat:@"javascript:window.location='%s'",
                                 targetURL.spec().c_str()];

  [ChromeEarlGreyUI focusOmniboxAndType:script];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Go")]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForPageToFinishLoading];

  [[EarlGrey selectElementWithMatcher:OmniboxText(targetURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGrey goBack];
  [[EarlGrey selectElementWithMatcher:OmniboxText(startURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that evaluating non-navigation user JavaScript doesn't affect history.
- (void)testBrowsingUserJavaScriptWithoutNavigation {
  // TODO(crbug.com/703855): Keyboard entry inside the omnibox fails only on
  // iPad.
  if ([ChromeEarlGrey isIPadIdiom])
    return;

  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL firstURL = web::test::HttpServer::MakeUrl("http://firstURL");
  const std::string firstResponse = "Test Page 1";
  const GURL secondURL = web::test::HttpServer::MakeUrl("http://secondURL");
  const std::string secondResponse = "Test Page 2";
  responses[firstURL] = firstResponse;
  responses[secondURL] = secondResponse;
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:firstURL];
  [ChromeEarlGrey loadURL:secondURL];

  // Execute some JavaScript in the omnibox.
  [ChromeEarlGreyUI focusOmniboxAndType:@"javascript:document.write('foo')\n"];
  [ChromeEarlGrey waitForWebStateContainingText:"foo"];

  // Verify that the JavaScript did not affect history by going back and then
  // forward again.
  [ChromeEarlGrey goBack];
  [[EarlGrey selectElementWithMatcher:OmniboxText(firstURL.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey goForward];
  [[EarlGrey selectElementWithMatcher:OmniboxText(secondURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

@end
