// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/url_util.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

namespace {

// The number of links on the test page.
const size_t kLinksCount = 56250;

// Returns a page with a large number of links.
std::unique_ptr<net::test_server::HttpResponse> LargeLinksPageResponse(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url.find("/links") != 0) {
    return nullptr;
  }

  std::string content = "<html><body>";
  for (size_t i = 0; i < kLinksCount; i++) {
    content += base::StringPrintf(
        "<a href='http://example.com/%zu'>Link %zu</a> ", i, i);
  }
  content += "</body></html>";

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->set_content(content);
  return response;
}

}  // namespace

@interface PageContextBaseTestCase : ChromeTestCase
@end

@implementation PageContextBaseTestCase

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&LargeLinksPageResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

- (void)runLinksTestWithRichExtraction:(BOOL)rich
                        actionableMode:(BOOL)actionable
                            expectNull:(BOOL)expectNull {
  GURL url = self.testServer->GetURL("/links");
  [ChromeEarlGrey loadURL:url];

  // Wait for the last link to be present to ensure page is loaded.
  std::string lastLinkText = base::StringPrintf("Link %zu", kLinksCount - 1);
  [ChromeEarlGrey waitForWebStateContainingText:lastLinkText];

  NSData* apcData =
      [PageContextAppInterface fetchLatestAPCWithRichExtraction:rich
                                                 actionableMode:actionable];
  if (expectNull) {
    GREYAssertNil(apcData, @"Expected APC to be nil");
  } else {
    GREYAssertNotNil(apcData, @"Expected APC to be non-nil");
  }
}

@end

#pragma mark - IPC Optimization Enabled (Default)

@interface PageContextTestCase : PageContextBaseTestCase
@end

@implementation PageContextTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kPageContextIPCOptimization);
  return config;
}

- (void)DISABLED_testLargePage_RichOn_ActionableOn {
  // TODO(crbug.com/507473141): this test returns null in some cases, but
  // ideally it should not. We appear to run over a size limit with rich +
  // actionable mode, even with IPC optimization enabled.
  [self runLinksTestWithRichExtraction:YES actionableMode:YES expectNull:NO];
}

- (void)testLargePage_RichOn_ActionableOff {
  [self runLinksTestWithRichExtraction:YES actionableMode:NO expectNull:NO];
}

- (void)testLargePage_RichOff {
  [self runLinksTestWithRichExtraction:NO actionableMode:NO expectNull:NO];
}

@end

@interface PageContextIPCOptimizationDisabledTestCase : PageContextBaseTestCase
@end

@implementation PageContextIPCOptimizationDisabledTestCase

// TODO(crbug.com/507473141): it is not ideal that any of these tests return
// null, but lacking the IPC optimization means rich extraction fails more
// frequently. These tests should be removed once IPC optimization is the
// default.
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_disabled.push_back(kPageContextIPCOptimization);
  return config;
}

- (void)testLargePage_RichOn_ActionableOn_ExpectNull {
  [self runLinksTestWithRichExtraction:YES actionableMode:YES expectNull:YES];
}

- (void)testLargePage_RichOn_ActionableOff_ExpectNull {
  [self runLinksTestWithRichExtraction:YES actionableMode:NO expectNull:YES];
}

- (void)testLargePage_RichOff {
  [self runLinksTestWithRichExtraction:NO actionableMode:NO expectNull:NO];
}

@end
