// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "ios/web/common/features.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"

namespace {
const char kPage1[] = "/page1.html";
const char kPageLinkID[] = "IDForLinkInPlatformPage";
const char kLinkPage[] = "/link.html";
const char kLinkLoaded[] = "Link Page Loaded";
const char kLinkPageLinkID[] = "linkPageIDForLink";
const char kWindowOpenJSPage[] = "/window_open.html";
const char kIFramePage[] = "/iframe.html";

// Handler for the test server. Depending of the URL of the page, it can returns
// a page displaying the platform, a page with a link to the platform page, an
// iframe, or a page opening a new page with JavaScript window.open.
std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path() == kPage1) {
    auto result = std::make_unique<net::test_server::BasicHttpResponse>();
    result->set_content_type("text/html");
    std::string href = std::string(kLinkPage).substr(1);
    result->set_content(
        "<html><body><div><a id=\"" + std::string(kPageLinkID) + "\" href=\"" +
        href +
        "\">page with link</a></div><p id=\"platformResult\"/><script "
        "type=\"text/javascript\">"
        "document.getElementById(\"platformResult\").innerHTML = "
        "navigator.platform;</script></body></html>");
    return std::move(result);
  }
  if (request.GetURL().path() == kLinkPage) {
    auto result = std::make_unique<net::test_server::BasicHttpResponse>();
    result->set_content_type("text/html");
    std::string href = std::string(kPage1).substr(1);
    result->set_content("<html><body>" + std::string(kLinkLoaded) + "<a id=\"" +
                        kLinkPageLinkID + "\" href=\"" + href +
                        "\">page with platform</a></body></html>");
    return std::move(result);
  }
  if (request.GetURL().path() == kWindowOpenJSPage) {
    auto result = std::make_unique<net::test_server::BasicHttpResponse>();
    result->set_content_type("text/html");
    std::string href = std::string(kPage1).substr(1);
    result->set_content(
        "<html><body><button id=\"button\" "
        "onclick=\"openWithJS()\">button</button><script "
        "type=\"text/javascript\">function openWithJS() {window.open(\"" +
        href + "\");}</script></body></html>");
    return std::move(result);
  }
  if (request.GetURL().path() == kIFramePage) {
    auto result = std::make_unique<net::test_server::BasicHttpResponse>();
    result->set_content_type("text/html");
    std::string href = std::string(kLinkPage).substr(1);
    result->set_content("<html><body><iframe src=\"" + href +
                        "\"/></body></html>");
    return std::move(result);
  }

  return nullptr;
}

// Returns the platform name of the current device.
std::string platform() {
  return [ChromeEarlGrey isMobileModeByDefault]
             ? base::SysNSStringToUTF8([[UIDevice currentDevice] model])
             : "MacIntel";
}

}  // namespace

// Test cases to make sure that the platform advertised by the navigator is the
// correct one.
@interface ContentModeTestCase : ChromeTestCase
@end

@implementation ContentModeTestCase

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(base::BindRepeating(&HandleRequest));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests the platform when the page is directly loaded.
- (void)testPageLoad {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPage1)];
  [ChromeEarlGrey waitForWebStateContainingText:platform()];
}

// Tests the platform after a back navigation on a loaded page.
- (void)testBackForward {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPage1)];
  [ChromeEarlGrey waitForWebStateContainingText:platform()];

  [ChromeEarlGrey loadURL:self.testServer->GetURL(kLinkPage)];
  [ChromeEarlGrey waitForWebStateContainingText:kLinkLoaded];

  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:platform()];
}

// Tests the platform when the page is loaded after a rendered navigation.
- (void)testRendererLoad {
  // Load the first page and do a renderer-initialized navigation to the second
  // page.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kLinkPage)];
  [ChromeEarlGrey
      tapWebStateElementWithID:base::SysUTF8ToNSString(kLinkPageLinkID)];

  [ChromeEarlGrey waitForWebStateContainingText:platform()];
}

// Tests the platform when the page is opened after a window.open JS event.
- (void)testJSWindowOpenPage {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kWindowOpenJSPage)];
  [ChromeEarlGrey tapWebStateElementWithID:@"button"];

  [ChromeEarlGrey waitForWebStateContainingText:platform()];
}

// Tests the platform when the page is inside an iframe.
- (void)testIFrameNavigation {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kIFramePage)];
  [ChromeEarlGrey tapWebStateElementInIFrameWithID:kLinkPageLinkID];

  [ChromeEarlGrey waitForWebStateFrameContainingText:platform()];
  [ChromeEarlGrey tapWebStateElementInIFrameWithID:kPageLinkID];
  [ChromeEarlGrey waitForWebStateFrameContainingText:kLinkLoaded];

  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateFrameContainingText:platform()];
}

@end
