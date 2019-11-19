// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <TestLib/EarlGreyImpl/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/test/wpt/cwt_request_handler.h"
#import "ios/testing/earl_grey/base_earl_grey_test_case.h"
#include "net/base/port_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The port that CWTChromeDriver's HTTP server listens on.
const int kDefaultPort = 8123;

}

// Dummy test case that hosts CWTChromeDriver. CWTChromeDriver implements a
// minimal subset of the WebDriver protocol needed to run most Web Platform
// Tests. CWTChromeDriverTestCase launches a test server that listens for
// WebDriver commands, and then uses EarlGrey2's eDistantObject protocol to pass
// on corresponding messages to the app process. Each CWTChromeDriver launches a
// single instance of Chrome, but mulitple instances of CWTChromeDriver can be
// run in parallel in order to use multiple instances of Chrome.
@interface CWTChromeDriverTestCase : BaseEarlGreyTestCase
@end

@implementation CWTChromeDriverTestCase

// Dummy test that keeps the test app alive.
- (void)testRunCWTChromeDriver {
  int port = kDefaultPort;

  NSArray* arguments = NSProcessInfo.processInfo.arguments;
  NSUInteger index = [arguments indexOfObject:@"--port"];
  if (index != NSNotFound && arguments.count > index + 1) {
    NSString* portString = [arguments objectAtIndex:index + 1];
    if (net::IsPortAllowedForScheme(portString.intValue, url::kHttpScheme))
      port = portString.intValue;
    else
      LOG(ERROR) << base::SysNSStringToUTF8(portString)
                 << " is not a valid port for http";
  }

  XCTestExpectation* dummyExpectation =
      [self expectationWithDescription:@"dummy expectation"];
  CWTRequestHandler requestHandler(^{
    [dummyExpectation fulfill];
  });

  net::EmbeddedTestServer server;
  server.RegisterRequestHandler(base::BindRepeating(
      &CWTRequestHandler::HandleRequest, base::Unretained(&requestHandler)));
  bool started = server.Start(port);
  if (!started)
    XCTFail("Unable to start web server");
  LOG(INFO) << "CWTChromeDriver listening on port " << server.port();

  // The dummy expectation will only be fulfilled once all the tests using this
  // instance of CWTChromeDriver have run, so wait with a long timeout.
  const NSTimeInterval kTimeoutInSeconds = 1000000;
  [self waitForExpectationsWithTimeout:kTimeoutInSeconds handler:nil];

  bool stopped = server.ShutdownAndWaitUntilComplete();
  if (!stopped)
    XCTFail("Unable to stop web server");
}

@end
