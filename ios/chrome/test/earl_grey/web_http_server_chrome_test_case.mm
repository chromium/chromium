// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/web/public/test/http_server/http_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation WebHttpServerChromeTestCase

- (void)setUp {
  [super setUp];

  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  if (server.IsRunning()) {
    return;
  }

  NSString* bundlePath = [NSBundle bundleForClass:[self class]].resourcePath;
  server.StartOrDie(base::FilePath(base::SysNSStringToUTF8(bundlePath)));
}

- (void)tearDown {
  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  if (!server.IsRunning()) {
    return;
  }

  server.Stop();

  [super tearDown];
}

@end
