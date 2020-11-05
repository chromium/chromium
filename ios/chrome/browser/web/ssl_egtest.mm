// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ios/testing/embedded_test_server_handlers.h"
#include "ios/web/common/features.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SSLTestCase : ChromeTestCase {
  std::unique_ptr<net::test_server::EmbeddedTestServer> _HTTPSServer;
}

@end

@implementation SSLTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;
  return config;
}

- (void)setUp {
  [super setUp];
  _HTTPSServer = std::make_unique<net::test_server::EmbeddedTestServer>(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  RegisterDefaultHandlers(_HTTPSServer.get());
}

// Test loading a page with a bad SSL certificate from the NTP, to avoid
// https://crbug.com/1067250 from regressing.
- (void)testBadSSLOnNTP {
  GREYAssertTrue(_HTTPSServer->Start(), @"Test server failed to start.");

  const GURL pageURL = _HTTPSServer->GetURL("/echo");
  [ChromeEarlGrey loadURL:pageURL];

  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_SSL_V2_HEADING)];
}

// Test loading a page with a bad SSL certificate during session restore, to
// avoid regressing https://crbug.com/1050808.
- (void)testBadSSLInSessionRestore {
  GREYAssertTrue(_HTTPSServer->Start(), @"Test server failed to start.");

  GURL pageURL = _HTTPSServer->GetURL("/echo");
  [ChromeEarlGrey loadURL:pageURL];

  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_SSL_V2_HEADING)];

  [ChromeEarlGrey triggerRestoreViaTabGridRemoveAllUndo];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_SSL_V2_HEADING)];
}

@end
