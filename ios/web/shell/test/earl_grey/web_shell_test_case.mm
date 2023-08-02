// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/earl_grey/web_shell_test_case.h"

#import "base/apple/bundle_locations.h"
#import "base/base_paths.h"
#import "base/path_service.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

@implementation WebShellTestCase {
  std::unique_ptr<net::EmbeddedTestServer> _testServer;
}

+ (void)initialize {
  if (self == [WebShellTestCase class]) {
    base::apple::SetOverrideFrameworkBundle(
        [NSBundle bundleForClass:[WebShellTestCase class]]);
  }
}

- (net::EmbeddedTestServer*)testServer {
  if (!_testServer) {
    _testServer = std::make_unique<net::EmbeddedTestServer>();
    _testServer->ServeFilesFromDirectory(
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .AppendASCII("ios/testing/data/http_server_files/"));
    GREYAssert(_testServer->Start(), @"EmbeddedTestServer failed to start.");
  }
  return _testServer.get();
}

@end
