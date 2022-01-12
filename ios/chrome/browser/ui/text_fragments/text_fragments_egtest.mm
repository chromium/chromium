// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "components/shared_highlighting/core/common/shared_highlighting_features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kTestURL[] = "/testPage";
const char kURLWithFragment[] = "/testPage/#:~:text=Lorem%20ipsum";
const char kHTMLOfTestPage[] =
    "<html><body><p>"
    "<span id='target'>Lorem ipsum<span> dolor sit amet, consectetur "
    "adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore "
    "magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco "
    "laboris nisi ut aliquip ex ea commodo consequat."
    "</p></body></html>";
const char kTestPageTextSample[] = "Lorem ipsum";

std::unique_ptr<net::test_server::HttpResponse> LoadHtml(
    const std::string& html,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_content_type("text/html");
  http_response->set_content(html);
  return std::move(http_response);
}

}  // namespace

// Test class verifying behavior of interactions with text fragments in web
// pages.
@interface TextFragmentsTestCase : ChromeTestCase
@end

@implementation TextFragmentsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      shared_highlighting::kIOSSharedHighlightingV2);
  return config;
}

- (void)setUp {
  [super setUp];

  RegisterDefaultHandlers(self.testServer);
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&net::test_server::HandlePrefixedRequest, kTestURL,
                          base::BindRepeating(&LoadHtml, kHTMLOfTestPage)));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

- (void)testOpenMenu {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kURLWithFragment)];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageTextSample];

  [ChromeEarlGrey tapWebStateElementWithID:@"target"];

  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      grey_text(l10n_util::GetNSString(
                          IDS_IOS_SHARED_HIGHLIGHT_MENU_TITLE))];
}

@end
