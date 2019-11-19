// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_TEST_CASE_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_TEST_CASE_H_

#import <XCTest/XCTest.h>

#import "base/ios/block_types.h"
#import "ios/testing/earl_grey/base_earl_grey_test_case.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"

namespace net {
namespace test_server {
class EmbeddedTestServer;
}
}

// Base class for all Chrome Earl Grey tests.
@interface ChromeTestCase : BaseEarlGreyTestCase

// Removes any UI elements that are present, to ensure it is in a clean state.
+ (void)removeAnyOpenMenusAndInfoBars;

// Closes all tabs, and waits for the UI to synchronize.
+ (void)closeAllTabs;

// The EmbeddedTestServer instance that hosts HTTP requests for tests.
@property(nonatomic, readonly) net::test_server::EmbeddedTestServer* testServer;

// Sets a block to always be executed at the end of a test during tearDown,
// whether the test passes or fails. This shall only be set once per test.
- (void)setTearDownHandler:(ProceduralBlock)tearDownHandler;

// Turns off mock authentication. It will automatically be re-enabled at the
// end of the test. This shall only be called once per test.
- (void)disableMockAuthentication;

// Stops the HTTP server. It will be re-started at the end of the test. This
// should only be called when the HTTP server is running. This shall only be
// called once per test.
- (void)stopHTTPServer;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_TEST_CASE_H_
