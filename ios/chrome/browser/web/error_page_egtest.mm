// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <string>

#include "base/bind.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ios/testing/embedded_test_server_handlers.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns ERR_CONNECTION_CLOSED error message.
std::string GetErrorMessage() {
  return net::ErrorToShortString(net::ERR_CONNECTION_CLOSED);
}
}  // namespace

// Tests critical user journeys reloated to page load errors.
@interface ErrorPageTestCase : ChromeTestCase
// YES if test server is replying with valid HTML content (URL query). NO if
// test server closes the socket.
@property(atomic) bool serverRespondsWithContent;
@end

@implementation ErrorPageTestCase
@synthesize serverRespondsWithContent = _serverRespondsWithContent;

- (void)setUp {
  [super setUp];

  RegisterDefaultHandlers(self.testServer);
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, "/echo-query",
      base::BindRepeating(&testing::HandleEchoQueryOrCloseSocket,
                          std::cref(_serverRespondsWithContent))));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Loads the URL which fails to load, then sucessfully reloads the page.
- (void)testReloadErrorPage {
  // No response leads to ERR_CONNECTION_CLOSED error.
  self.serverRespondsWithContent = NO;
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo-query?foo")];
  [ChromeEarlGrey waitForWebStateContainingText:GetErrorMessage()];

  // Reload the page, which should load without errors.
  self.serverRespondsWithContent = YES;
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebStateContainingText:"foo"];
}

// Sucessfully loads the page, stops the server and reloads the page.
- (void)testReloadPageAfterServerIsDown {
  // Sucessfully load the page.
  self.serverRespondsWithContent = YES;
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo-query?foo")];
  [ChromeEarlGrey waitForWebStateContainingText:"foo"];

  // Reload the page, no response leads to ERR_CONNECTION_CLOSED error.
  self.serverRespondsWithContent = NO;
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebStateContainingText:GetErrorMessage()];
}

@end
