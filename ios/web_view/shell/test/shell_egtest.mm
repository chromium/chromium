// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#import <ChromeWebView/ChromeWebView.h>
#import <XCTest/XCTest.h>
#import "ios/testing/earl_grey/earl_grey_test.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/web_view/shell/shell_view_controller.h"
#import "ios/web_view/shell/test/earl_grey/web_view_shell_matchers.h"
#import "ios/web_view/test/web_view_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

// Returns current web view for root view controller.
CWVWebView* GetCurrentWebView() {
  ShellViewController* view_controller = static_cast<ShellViewController*>([[
      [[UIApplication sharedApplication] delegate] window] rootViewController]);
  return view_controller.webView;
}

// Waits for the current web view to contain |text|. If the condition is not met
// within a timeout, a GREYAssert is induced.
void WaitForWebViewContainingText(NSString* text) {
  GREYAssert(ios_web_view::test::WaitForWebViewContainingTextOrTimeout(
                 GetCurrentWebView(), text),
             @"Failed waiting for web view containing %@", text);
}

}  // namespace

// Test fixture for the web view shell tests.
@interface ShellTestCase : XCTestCase {
  std::unique_ptr<net::test_server::EmbeddedTestServer> _testServer;
}

@end

@implementation ShellTestCase

// Overrides |testInvocations| to skip all tests if a system alert view is
// shown, since this isn't a case a user would encounter (i.e. they would
// dismiss the alert first).
+ (NSArray*)testInvocations {
  // TODO(crbug.com/41279721): Simply skipping all tests isn't the best way to
  // handle this, it would be better to have something that is more obvious
  // on the bots that this is wrong, without making it look like test flake.
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:grey_systemAlertViewShown()]
      assertWithMatcher:grey_nil()
                  error:&error];
  if (error != nil) {
    NSLog(@"System alert view is present, so skipping all tests!");
    return @[];
  }
  return [super testInvocations];
}

- (void)setUp {
  [super setUp];

  _testServer = std::make_unique<net::EmbeddedTestServer>(
      net::test_server::EmbeddedTestServer::TYPE_HTTP);
  _testServer->ServeFilesFromSourceDirectory(
      base::FilePath(FILE_PATH_LITERAL("ios/testing/data/http_server_files/")));
  XCTAssertTrue(_testServer->Start());
}

// Tests loading simple HTML page and verifies that page contains expected text.
- (void)testLoadingURL {
  std::string URLSpec = _testServer->GetURL("/destination.html").spec();

  [[EarlGrey selectElementWithMatcher:ios_web_view::AddressField()]
      performAction:grey_replaceText(base::SysUTF8ToNSString(URLSpec))];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Go")]
      performAction:grey_tap()];

  WaitForWebViewContainingText(@"You've arrived");
}

@end
