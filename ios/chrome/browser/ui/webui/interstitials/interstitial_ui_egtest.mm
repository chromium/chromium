// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/webui/interstitials/interstitial_ui_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/components/webui/web_ui_url_constants.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/url_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test case for chrome://interstitials WebUI page.
@interface InterstitialWebUITestCase : ChromeTestCase {
  std::unique_ptr<url::ScopedSchemeRegistryForTests> _schemeRegistry;
}

@end

@implementation InterstitialWebUITestCase

- (void)setUp {
  [super setUp];
  _schemeRegistry = std::make_unique<url::ScopedSchemeRegistryForTests>();
  url::ClearSchemesForTests();
  url::AddStandardScheme(kChromeUIScheme, url::SCHEME_WITH_HOST);
}

- (void)tearDown {
  _schemeRegistry = nullptr;
  [super tearDown];
}

// Tests that chrome://interstitials loads correctly.
- (void)testLoadInterstitialUI {
  [ChromeEarlGrey loadURL:GURL(kChromeUIIntersitialsURL)];

  [ChromeEarlGrey waitForWebStateContainingText:"Choose an interstitial"];
}

// Tests that chrome://interstitials/ssl loads correctly.
- (void)testLoadSSLInterstitialUI {
  GURL SSLInterstitialURL =
      GURL(kChromeUIIntersitialsURL).Resolve(kChromeInterstitialSslPath);
  [ChromeEarlGrey loadURL:SSLInterstitialURL];

  [ChromeEarlGrey
      waitForWebStateContainingText:"Your connection is not private"];
}

// Tests that chrome://interstitials/captiveportal loads correctly.
- (void)testLoadCaptivePortalInterstitialUI {
  GURL captivePortalInterstitialURL =
      GURL(kChromeUIIntersitialsURL)
          .Resolve(kChromeInterstitialCaptivePortalPath);
  [ChromeEarlGrey loadURL:captivePortalInterstitialURL];

  [ChromeEarlGrey waitForWebStateContainingText:"Connect to Wi-Fi"];
}

// Tests that chrome://interstitials/safe_browsing?type=malware loads correctly.
- (void)testLoadSafeBrowsingMalwareInterstitialUI {
  GURL safeBrowsingURL = GURL(kChromeUIIntersitialsURL)
                             .Resolve(kChromeInterstitialSafeBrowsingPath);
  safeBrowsingURL = net::AppendQueryParameter(
      safeBrowsingURL, kChromeInterstitialSafeBrowsingTypeQueryKey,
      kChromeInterstitialSafeBrowsingTypeMalwareValue);
  [ChromeEarlGrey loadURL:safeBrowsingURL];

  [ChromeEarlGrey
      waitForWebStateContainingText:"The site ahead contains malware"];
}

// Tests that chrome://interstitials/safe_browsing?type=phishing loads
// correctly.
- (void)testLoadSafeBrowsingPhishingInterstitialUI {
  GURL safeBrowsingURL = GURL(kChromeUIIntersitialsURL)
                             .Resolve(kChromeInterstitialSafeBrowsingPath);
  safeBrowsingURL = net::AppendQueryParameter(
      safeBrowsingURL, kChromeInterstitialSafeBrowsingTypeQueryKey,
      kChromeInterstitialSafeBrowsingTypePhishingValue);
  [ChromeEarlGrey loadURL:safeBrowsingURL];

  [ChromeEarlGrey waitForWebStateContainingText:"Deceptive site ahead"];
}

// Tests that chrome://interstitials/safe_browsing?type=unwanted loads
// correctly.
- (void)testLoadSafeBrowsingUnwantedInterstitialUI {
  GURL safeBrowsingURL = GURL(kChromeUIIntersitialsURL)
                             .Resolve(kChromeInterstitialSafeBrowsingPath);
  safeBrowsingURL = net::AppendQueryParameter(
      safeBrowsingURL, kChromeInterstitialSafeBrowsingTypeQueryKey,
      kChromeInterstitialSafeBrowsingTypeUnwantedValue);
  [ChromeEarlGrey loadURL:safeBrowsingURL];

  [ChromeEarlGrey
      waitForWebStateContainingText:"The site ahead contains harmful programs"];
}

// Tests that chrome://interstitials/safe_browsing?type=clientside_malware loads
// correctly.
- (void)testLoadSafeBrowsingClientsideMalwareInterstitialUI {
  GURL safeBrowsingURL = GURL(kChromeUIIntersitialsURL)
                             .Resolve(kChromeInterstitialSafeBrowsingPath);
  safeBrowsingURL = net::AppendQueryParameter(
      safeBrowsingURL, kChromeInterstitialSafeBrowsingTypeQueryKey,
      kChromeInterstitialSafeBrowsingTypeClientsideMalwareValue);
  [ChromeEarlGrey loadURL:safeBrowsingURL];

  [ChromeEarlGrey
      waitForWebStateContainingText:"The site ahead contains malware"];
}

// Tests that chrome://interstitials/safe_browsing?type=clientside_phishing
// loads correctly.
- (void)testLoadSafeBrowsingClientsidePhishingInterstitialUI {
  GURL safeBrowsingURL = GURL(kChromeUIIntersitialsURL)
                             .Resolve(kChromeInterstitialSafeBrowsingPath);
  safeBrowsingURL = net::AppendQueryParameter(
      safeBrowsingURL, kChromeInterstitialSafeBrowsingTypeQueryKey,
      kChromeInterstitialSafeBrowsingTypeClientsidePhishingValue);
  [ChromeEarlGrey loadURL:safeBrowsingURL];

  [ChromeEarlGrey waitForWebStateContainingText:"Deceptive site ahead"];
}

// Tests that chrome://interstitials/safe_browsing?type=billing loads correctly.
- (void)testLoadSafeBrowsingBillingInterstitialUI {
  GURL safeBrowsingURL = GURL(kChromeUIIntersitialsURL)
                             .Resolve(kChromeInterstitialSafeBrowsingPath);
  safeBrowsingURL = net::AppendQueryParameter(
      safeBrowsingURL, kChromeInterstitialSafeBrowsingTypeQueryKey,
      kChromeInterstitialSafeBrowsingTypeBillingValue);
  [ChromeEarlGrey loadURL:safeBrowsingURL];

  [ChromeEarlGrey waitForWebStateContainingText:
                      "The page ahead may try to charge you money"];
}

@end
