// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

namespace {

const char kWebShareButtonId[] = "shareButton";

const char kWebShareStatusSuccess[] = "success";
const char kWebShareStatusFailure[] = "failure";

const char kWebShareValidLinkUrl[] = "/share_link.html";
const char kWebShareFileUrl[] = "/share_file.html";
const char kWebShareRelativeLinkUrl[] = "/share_relative_link.html";
const char kWebShareRelativeFilenameFileUrl[] = "/share_filename_file.html";
const char kWebShareUrlObjectUrl[] = "/share_url_object.html";

const char kWebSharePageContents[] =
    "<html>"
    "<head>"
    "<script>"
    "async function tryUrl() {"
    "  document.getElementById(\"result\").innerHTML = '';"
    "  try {"
    "    var opts = {url: %s};"
    "    await navigator.share(opts);"
    "    document.getElementById(\"result\").innerHTML = 'success';"
    "  } catch {"
    "    document.getElementById(\"result\").innerHTML = 'failure';"
    "  }"
    "}"
    "</script>"
    "</head><body>"
    "<button id=\"shareButton\" onclick=\"tryUrl()\">Share</button>"
    "<div id=\"result\"></div>"
    "</body></html>";

std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("text/html");

  if (request.relative_url == kWebShareValidLinkUrl) {
    std::string content =
        base::StringPrintf(kWebSharePageContents, "\"https://example.com\"");
    http_response->set_content(content);
  } else if (request.relative_url == kWebShareFileUrl) {
    std::string content =
        base::StringPrintf(kWebSharePageContents, "\"file:///Users/u/data\"");
    http_response->set_content(content);
  } else if (request.relative_url == kWebShareRelativeLinkUrl) {
    std::string content =
        base::StringPrintf(kWebSharePageContents, "\"/something.png\"");
    http_response->set_content(content);
  } else if (request.relative_url == kWebShareRelativeFilenameFileUrl) {
    std::string content =
        base::StringPrintf(kWebSharePageContents, "\"filename.zip\"");
    http_response->set_content(content);
  } else if (request.relative_url == kWebShareUrlObjectUrl) {
    std::string content =
        base::StringPrintf(kWebSharePageContents, "window.location");
    http_response->set_content(content);
  } else {
    return nullptr;
  }
  return std::move(http_response);
}

}  // namespace

@interface WebShareTestCase : ChromeTestCase
@end

@implementation WebShareTestCase

- (void)setUp {
  [super setUp];

  self.testServer->RegisterRequestHandler(base::BindRepeating(&HandleRequest));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that a fully specified url can be shared.
- (void)testShareUrl {
  // TODO(crbug.com/1468173): Failing on iOS17.
  if (@available(iOS 17.0, *)) {
    XCTSkip(@"Tapping kWebShareButtonId is failing on iOS17 beta 5.");
  }

  const GURL pageURL = self.testServer->GetURL(kWebShareValidLinkUrl);
  [ChromeEarlGrey loadURL:pageURL];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kWebShareButtonId)];

  [ChromeEarlGrey tapButtonInActivitySheetWithID:@"Copy"];

  [ChromeEarlGrey waitForWebStateContainingText:kWebShareStatusSuccess];
}

// Tests that a relative  url can be shared.
- (void)testShareRelativeUrl {
  // TODO(crbug.com/1468173): Failing on iOS17.
  if (@available(iOS 17.0, *)) {
    XCTSkip(@"Tapping kWebShareButtonId is failing on iOS17 beta 5.");
  }

  const GURL pageURL = self.testServer->GetURL(kWebShareRelativeLinkUrl);
  [ChromeEarlGrey loadURL:pageURL];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kWebShareButtonId)];

  [ChromeEarlGrey tapButtonInActivitySheetWithID:@"Copy"];

  [ChromeEarlGrey waitForWebStateContainingText:kWebShareStatusSuccess];
}

// Tests that a relative url can be shared when the filename starts with "file".
- (void)testShareRelativeFilenameUrl {
  // TODO(crbug.com/1468173): Failing on iOS17.
  if (@available(iOS 17.0, *)) {
    XCTSkip(@"Tapping kWebShareButtonId is failing on iOS17 beta 5.");
  }

  const GURL pageURL =
      self.testServer->GetURL(kWebShareRelativeFilenameFileUrl);
  [ChromeEarlGrey loadURL:pageURL];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kWebShareButtonId)];

  [ChromeEarlGrey tapButtonInActivitySheetWithID:@"Copy"];

  [ChromeEarlGrey waitForWebStateContainingText:kWebShareStatusSuccess];
}

// Tests that a "file://" url can not be shared.
- (void)testShareFileUrl {
  const GURL pageURL = self.testServer->GetURL(kWebShareFileUrl);
  [ChromeEarlGrey loadURL:pageURL];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kWebShareButtonId)];

  [ChromeEarlGrey waitForWebStateContainingText:kWebShareStatusFailure];

  // Share sheet should not display.
  [ChromeEarlGrey verifyActivitySheetNotVisible];
}

// Tests that an url object can be shared.
- (void)testShareUrlObject {
  // TODO(crbug.com/1468173): Failing on iOS17.
  if (@available(iOS 17.0, *)) {
    XCTSkip(@"Tapping kWebShareButtonId is failing on iOS17 beta 5.");
  }
  const GURL pageURL = self.testServer->GetURL(kWebShareUrlObjectUrl);
  [ChromeEarlGrey loadURL:pageURL];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kWebShareButtonId)];

  [ChromeEarlGrey tapButtonInActivitySheetWithID:@"Copy"];

  [ChromeEarlGrey waitForWebStateContainingText:kWebShareStatusSuccess];
}

@end
