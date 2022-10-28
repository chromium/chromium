// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/earl_grey/web_shell_test_case.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation WebShellTestCase {
  std::unique_ptr<net::EmbeddedTestServer> _testServer;
}

- (net::EmbeddedTestServer*)testServer {
  if (!_testServer) {
    _testServer = std::make_unique<net::EmbeddedTestServer>();
    NSString* bundlePath = [NSBundle bundleForClass:[self class]].resourcePath;
    _testServer->ServeFilesFromDirectory(
        base::FilePath(base::SysNSStringToUTF8(bundlePath))
            .AppendASCII("ios/testing/data/http_server_files/"));
    GREYAssert(_testServer->Start(), @"EmbeddedTestServer failed to start.");
  }
  return _testServer.get();
}

@end
