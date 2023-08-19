// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"

#import "base/base_paths.h"
#import "base/path_service.h"
#import "ios/web/public/test/http_server/http_server.h"

@implementation WebHttpServerChromeTestCase

- (void)setUp {
  [super setUp];

  web::test::HttpServer& server = web::test::HttpServer::GetSharedInstance();
  if (server.IsRunning()) {
    return;
  }

  server.StartOrDie(base::PathService::CheckedGet(base::DIR_ASSETS));
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
