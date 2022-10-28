// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_TEST_EARL_GREY_WEB_SHELL_TEST_CASE_H_
#define IOS_WEB_SHELL_TEST_EARL_GREY_WEB_SHELL_TEST_CASE_H_

#import "ios/testing/earl_grey/base_earl_grey_test_case.h"

namespace net {
namespace test_server {
class EmbeddedTestServer;
}  // namespace test_server
}  // namespace net

// Base class for all web shell Earl Grey tests.
@interface WebShellTestCase : BaseEarlGreyTestCase

// The EmbeddedTestServer instance that hosts HTTP requests for tests.
@property(nonatomic, readonly) net::test_server::EmbeddedTestServer* testServer;

@end

#endif  // IOS_WEB_SHELL_TEST_EARL_GREY_WEB_SHELL_TEST_CASE_H_
