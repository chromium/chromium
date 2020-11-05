// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/ios_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ios/testing/embedded_test_server_handlers.h"
#include "ios/web/common/features.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LegacyTLSTestCase : ChromeTestCase {
  // A test server that connects via TLS 1.0.
  std::unique_ptr<net::test_server::EmbeddedTestServer> _legacyTLSServer;
  // A URL for the legacy TLS test server, which should trigger legacy TLS
  // interstitials.
  GURL _legacyTLSURL;
  // A URL for the normal test server.
  GURL _safeURL;
  // Text that is found on the legacy TLS interstitial.
  std::string _interstitialContent;
  // Text that is found on |_legacyTLSURL| if the user clicks through.
  std::string _unsafeContent;
  // Text that is found on |_safeURL|.
  std::string _safeContent;
}

@end

@implementation LegacyTLSTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(web::features::kIOSLegacyTLSInterstitial);
  config.relaunch_policy = NoForceRelaunchAndResetState;
  return config;
}

- (void)setUp {
  [super setUp];

  // Setup server that will negotiate TLS 1.0.
  _legacyTLSServer = std::make_unique<net::test_server::EmbeddedTestServer>(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.version_min = net::SSL_PROTOCOL_VERSION_TLS1;
  ssl_config.version_max = net::SSL_PROTOCOL_VERSION_TLS1;
  _legacyTLSServer->SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  RegisterDefaultHandlers(_legacyTLSServer.get());
  GREYAssertTrue(_legacyTLSServer->Start(),
                 @"Legacy TLS test server failed to start.");

  _legacyTLSURL = _legacyTLSServer->GetURL("/");
  _interstitialContent =
      l10n_util::GetStringUTF8(IDS_LEGACY_TLS_PRIMARY_PARAGRAPH);
  // The legacy TLS server will cause self-signed cert warnings after we click
  // through the legacy TLS interstitial, so pull the heading string for that
  // to match on.
  _unsafeContent = l10n_util::GetStringUTF8(IDS_SSL_V2_HEADING);

  RegisterDefaultHandlers(self.testServer);
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  _safeURL = self.testServer->GetURL("/defaultresponse");
  _safeContent = "Default response";
}

// On iOS < 14, the legacy TLS interstitial should not be shown.
- (void)testLegacyTLSInterstitialNotShownOnOldVersions {
  if (base::ios::IsRunningOnIOS14OrLater()) {
    return;
  }
  [ChromeEarlGrey loadURL:_legacyTLSURL];
  [ChromeEarlGrey waitForWebStateContainingText:_unsafeContent];
}

// The remaining tests in this file are only relevant for iOS 14 or later.

// Test that loading a page from a server over TLS 1.0 causes the legacy TLS
// interstitial to show.
- (void)testLegacyTLSShowsInterstitial {
  if (!base::ios::IsRunningOnIOS14OrLater()) {
    return;
  }
  [ChromeEarlGrey loadURL:_legacyTLSURL];
  [ChromeEarlGrey waitForWebStateContainingText:_interstitialContent];
}

// Test that going back to safety returns the user to the previous page.
- (void)testLegacyTLSInterstitialBackToSafety {
  if (!base::ios::IsRunningOnIOS14OrLater()) {
    return;
  }
  // Load a benign page first.
  [ChromeEarlGrey loadURL:_safeURL];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];

  // Trigger the legacy TLS interstitial.
  [ChromeEarlGrey loadURL:_legacyTLSURL];
  [ChromeEarlGrey waitForWebStateContainingText:_interstitialContent];

  // Tap on the "Back to safety" button and verify that we navigate back to the
  // previous page.
  [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];
}

// Test that clicking through the interstitial works.
- (void)testLegacyTLSInterstitialProceed {
  if (!base::ios::IsRunningOnIOS14OrLater()) {
    return;
  }
  // Load a benign page first.
  [ChromeEarlGrey loadURL:_safeURL];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];

  // Trigger the legacy TLS interstitial.
  [ChromeEarlGrey loadURL:_legacyTLSURL];
  [ChromeEarlGrey waitForWebStateContainingText:_interstitialContent];

  // Tap on the "Proceed" link and verify that we go to the unsafe page (in this
  // case, a cert error interstitial for the test server's self-signed cert).
  [ChromeEarlGrey tapWebStateElementWithID:@"details-button"];
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-link"];
  [ChromeEarlGrey waitForWebStateContainingText:_unsafeContent];
}

// Test that clicking through the interstitial is remembered on a reload, and
// we don't show the interstitial again.
- (void)testLegacyTLSInterstitialAllowlist {
  if (!base::ios::IsRunningOnIOS14OrLater()) {
    return;
  }
  // Trigger the legacy TLS interstitial.
  [ChromeEarlGrey loadURL:_legacyTLSURL];
  [ChromeEarlGrey waitForWebStateContainingText:_interstitialContent];

  // Tap on the "Proceed" link and verify that we go to the unsafe page.
  [ChromeEarlGrey tapWebStateElementWithID:@"details-button"];
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-link"];
  [ChromeEarlGrey waitForWebStateContainingText:_unsafeContent];

  // Navigate away to another page.
  [ChromeEarlGrey loadURL:_safeURL];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];

  // Navigate to the legacy TLS page again. Legacy TLS interstitial should not
  // be shown.
  [ChromeEarlGrey loadURL:_legacyTLSURL];
  [ChromeEarlGrey waitForWebStateContainingText:_unsafeContent];
}

// Test that the allowlist is cleared after session restart and that we show the
// legacy TLS interstitial again.
- (void)testLegacyTLSInterstitialAllowlistClearedOnRestart {
  if (!base::ios::IsRunningOnIOS14OrLater()) {
    return;
  }
  // Trigger the legacy TLS interstitial.
  [ChromeEarlGrey loadURL:_legacyTLSURL];
  [ChromeEarlGrey waitForWebStateContainingText:_interstitialContent];

  // Tap on the "Proceed" link and verify that we go to the unsafe page.
  [ChromeEarlGrey tapWebStateElementWithID:@"details-button"];
  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-link"];
  [ChromeEarlGrey waitForWebStateContainingText:_unsafeContent];

  // Navigate away to another page.
  [ChromeEarlGrey loadURL:_safeURL];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];

  // Do a session restoration.
  [ChromeEarlGrey triggerRestoreViaTabGridRemoveAllUndo];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];

  // Navigate to the legacy TLS page again. The interstitial should trigger.
  [ChromeEarlGrey loadURL:_legacyTLSURL];
  [ChromeEarlGrey waitForWebStateContainingText:_interstitialContent];
}

@end
