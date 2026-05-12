// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#import "base/functional/bind.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

namespace {

const char kPlainPath[] = "/plain";
const char kMetaPath[] = "/meta";
const char kCssPath[] = "/css";
const char kMediaPath[] = "/media";

std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");

  if (request.relative_url == kPlainPath) {
    response->set_content("<html><body><h1>Plain</h1></body></html>");
  } else if (request.relative_url == kMetaPath) {
    response->set_content(
        "<html><head><meta name=\"color-scheme\" content=\"light dark\"></head>"
        "<body><h1>Meta</h1></body></html>");
  } else if (request.relative_url == kCssPath) {
    response->set_content(
        "<html><head><style>:root { color-scheme: light dark; }</style></head>"
        "<body><h1>CSS Property</h1></body></html>");
  } else if (request.relative_url == kMediaPath) {
    response->set_content(
        "<html><head><style>"
        "@media (prefers-color-scheme: dark) { body { background: black; } }"
        "</style></head><body><h1>Media Query</h1></body></html>");
  } else {
    return nullptr;
  }
  return std::move(response);
}

}  // namespace

@interface DarkModeDetectionTestCase : ChromeTestCase
@end

@implementation DarkModeDetectionTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kIOSDarkModeDetection);
  return config;
}

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(base::BindRepeating(&HandleRequest));
  GREYAssertTrue(self.testServer->Start(),
                 @"EmbeddedTestServer failed to start.");

  NSError* error = [MetricsAppInterface setupHistogramTester];
  GREYAssertNil(error, @"Failed to setup histogram tester: %@", error);
}

- (void)tearDownHelper {
  NSError* error = [MetricsAppInterface releaseHistogramTester];
  GREYAssertNil(error, @"Failed to release histogram tester: %@", error);
  [super tearDownHelper];
}

// Helper to assert histogram states.
- (void)assertHistogramSupportWithMeta:(BOOL)meta
                                   css:(BOOL)css
                            mediaQuery:(BOOL)mediaQuery
                               overall:(BOOL)overall {
  // Since page loads are async, we wait until the general dark mode support
  // metric is recorded
  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Waiting for DarkModeDetection UMA metric"
                  block:^BOOL {
                    NSError* error = [MetricsAppInterface
                        expectTotalCount:1
                            forHistogram:
                                @"IOS.DarkModeDetection.SupportsDarkMode"];
                    return error == nil;
                  }];
  GREYAssert([condition waitWithTimeout:5],
             @"Failed to record SupportsDarkMode UMA metric.");

  UIUserInterfaceStyle user_interface_style =
      UITraitCollection.currentTraitCollection.userInterfaceStyle;
  BOOL is_dark = (user_interface_style == UIUserInterfaceStyleDark);
  int supported_bucket = is_dark ? 3 : 2;
  int not_supported_bucket = is_dark ? 1 : 0;

  NSError* error = nil;
  error = [MetricsAppInterface
      expectUniqueSampleWithCount:1
                        forBucket:meta ? supported_bucket : not_supported_bucket
                     forHistogram:@"IOS.DarkModeDetection.SupportsViaMeta"];
  GREYAssertNil(error, @"SupportsViaMeta assertion failed: %@", error);

  error = [MetricsAppInterface
      expectUniqueSampleWithCount:1
                        forBucket:css ? supported_bucket : not_supported_bucket
                     forHistogram:@"IOS.DarkModeDetection.SupportsViaCss"];
  GREYAssertNil(error, @"SupportsViaCss assertion failed: %@", error);

  error = [MetricsAppInterface
      expectUniqueSampleWithCount:1
                        forBucket:mediaQuery ? supported_bucket
                                             : not_supported_bucket
                     forHistogram:
                         @"IOS.DarkModeDetection.SupportsViaMediaQuery"];
  GREYAssertNil(error, @"SupportsViaMediaQuery assertion failed: %@", error);

  error = [MetricsAppInterface
      expectUniqueSampleWithCount:1
                        forBucket:overall ? supported_bucket
                                          : not_supported_bucket
                     forHistogram:@"IOS.DarkModeDetection.SupportsDarkMode"];
  GREYAssertNil(error, @"SupportsDarkMode assertion failed: %@", error);
}

// Tests that loading a plain page reports no dark mode support.
- (void)testPlainPageReportsNoSupport {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPlainPath)];
  [self assertHistogramSupportWithMeta:NO css:NO mediaQuery:NO overall:NO];
}

// Tests that loading a meta-configured page reports meta support.
- (void)testMetaDarkPageReportsMetaSupport {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kMetaPath)];
  [self assertHistogramSupportWithMeta:YES css:NO mediaQuery:NO overall:YES];
}

// Tests that loading a CSS-property-configured page reports CSS support.
- (void)testCssStyleDarkPageReportsCssSupport {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kCssPath)];
  [self assertHistogramSupportWithMeta:NO css:YES mediaQuery:NO overall:YES];
}

// Tests that loading a media-query-configured page reports media-query support.
- (void)testMediaStyleDarkPageReportsMediaQuerySupport {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kMediaPath)];
  [self assertHistogramSupportWithMeta:NO css:NO mediaQuery:YES overall:YES];
}

@end
