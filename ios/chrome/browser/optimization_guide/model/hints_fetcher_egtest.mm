// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "base/containers/flat_set.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/optimization_guide/core/optimization_guide_enums.h"
#import "components/optimization_guide/core/optimization_guide_switches.h"
#import "components/optimization_guide/core/optimization_guide_test_util.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_test_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

namespace {

void AppendSwitch(std::vector<std::string>* args,
                  const std::string& cli_switch) {
  args->push_back(std::string("--") + cli_switch);
}

// Handler for the hints server.
std::unique_ptr<net::test_server::HttpResponse> HandleGetHintsRequest(
    const std::string& origin_host,
    const optimization_guide::HintsFetcherRemoteResponseType& response_type,
    size_t& count_hints_requests_received,
    const net::test_server::HttpRequest& request) {
  // Fail the response if it does not have the expected attributes.
  if (request.method != net::test_server::METHOD_POST)
    return nullptr;
  optimization_guide::proto::GetHintsRequest hints_request;
  if (!hints_request.ParseFromString(request.content))
    return nullptr;
  if (hints_request.hosts().empty() && hints_request.urls().empty())
    return nullptr;
  // TODO(crbug.com/40103566): Verify that hosts count in the hint does not
  // exceed MaxHostsForOptimizationGuideServiceHintsFetch()

  count_hints_requests_received++;

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();

  if (response_type ==
      optimization_guide::HintsFetcherRemoteResponseType::kSuccessful) {
    response->set_code(net::HTTP_OK);

    optimization_guide::proto::GetHintsResponse get_hints_response;

    optimization_guide::proto::Hint* hint = get_hints_response.add_hints();
    hint->set_key_representation(optimization_guide::proto::HOST);
    hint->set_key(origin_host);
    optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
    page_hint->set_page_pattern("page pattern");

    std::string serialized_request;
    get_hints_response.SerializeToString(&serialized_request);
    response->set_content(serialized_request);
  } else if (response_type ==
             optimization_guide::HintsFetcherRemoteResponseType::
                 kUnsuccessful) {
    response->set_code(net::HTTP_NOT_FOUND);

  } else if (response_type ==
             optimization_guide::HintsFetcherRemoteResponseType::kMalformed) {
    response->set_code(net::HTTP_OK);

    std::string serialized_request = "Not a proto";
    response->set_content(serialized_request);
  } else if (response_type ==
             optimization_guide::HintsFetcherRemoteResponseType::kHung) {
    return std::make_unique<net::test_server::HungResponse>();
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  return std::move(response);
}

}  // namespace

@interface HintsFetcherEGTestCase : ChromeTestCase {
  std::unique_ptr<net::EmbeddedTestServer> origin_server;
}
@property optimization_guide::HintsFetcherRemoteResponseType response_type;

// Count of hints requests received so far by the hints server
// `self.testServer`.
@property size_t count_hints_requests_received;

// Set of hosts and URLs for which a hints request is
// expected to arrive. This set is verified to match with the set of hosts and
// URLs present in the hints request. If null, then the verification is not
// done.

@property std::optional<base::flat_set<std::string>>
    expect_hints_request_for_hosts_and_urls_;
@end

@implementation HintsFetcherEGTestCase
@synthesize count_hints_requests_received = _count_hints_requests_received;
@synthesize response_type = _response_type;

#pragma mark - Helpers

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // TODO(crbug.com/40103566): Convert to directly use the kOptimizationHints
  // feature.
  config.additional_args.push_back("--enable-features=OptimizationHints");
  AppendSwitch(&config.additional_args,
               optimization_guide::switches::kPurgeHintsStore);
  AppendSwitch(
      &config.additional_args,
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  AppendSwitch(&config.additional_args,
               optimization_guide::switches::kFetchHintsOverrideTimer);
  AppendSwitch(&config.additional_args,
               optimization_guide::switches::kDebugLoggingEnabled);
  config.additional_args.push_back("--force-variation-ids=4");
  return config;
}

- (void)setUp {
  [super setUp];
  self.count_hints_requests_received = 0;
  self.response_type =
      optimization_guide::HintsFetcherRemoteResponseType::kSuccessful;

  origin_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  GREYAssertTrue(origin_server->Start(),
                 @"Origin test server failed to start.");

  // The tests use `self.testServer` as the optimization guide hints server.
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &HandleGetHintsRequest, origin_server->base_url().host(),
      std::cref(_response_type), std::ref(_count_hints_requests_received)));
  GREYAssertTrue(self.testServer->Start(), @"Hints server failed to start.");

  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Failed to set up histogram tester.");
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];

  NSString* hints_server_host =
      base::SysUTF8ToNSString(self.testServer->base_url().spec());

  [OptimizationGuideTestAppInterface setGetHintsURL:hints_server_host];
  [OptimizationGuideTestAppInterface
      setComponentUpdateHints:base::SysUTF8ToNSString(
                                  origin_server->base_url().host())];
  [OptimizationGuideTestAppInterface
      registerOptimizationType:optimization_guide::proto::OptimizationType::
                                   NOSCRIPT];
}

- (void)tearDown {
  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Failed to release histogram tester.");
  [super tearDown];
}

#pragma mark - Tests

// The tests in this file should correspond to the tests in
// //chrome/browser/optimization_guide/hints_fetcher_browsertest.cc.
// TODO(crbug.com/40194556): Add more EG2 tests so that the different pieces of
// optimization guide hints fetching are integration tested. This includes tests
// that verify hints fetcher failure cases, fetching of hints for multiple open
// tabs at startup, hints are cleared when browsing history is cleared, etc.
// TODO(crbug.com/366045251): Re-enable once fixed.
- (void)DISABLED_testHintsFetchBasic {
  [ChromeEarlGrey loadURL:GURL("https://foo.com/test")];
  // Wait for the hints to be served.
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForPageLoadTimeout,
                 ^{
                   return self.count_hints_requests_received == 1;
                 }),
             @"Hints server did not receive hints request");
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:
                                static_cast<int>(
                                    optimization_guide::
                                        RaceNavigationFetchAttemptStatus::
                                            kRaceNavigationFetchHostAndURL)
                         forHistogram:@"OptimizationGuide.HintsManager."
                                      @"RaceNavigationFetchAttemptStatus"],
      @"Host and URL race fetch histogram missing");
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:static_cast<int>(net::HTTP_OK)
                         forHistogram:@"OptimizationGuide.HintsFetcher."
                                      @"GetHintsRequest.Status"],
      @"hints request histogram missing");
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:static_cast<int>(net::OK)
                         forHistogram:@"OptimizationGuide.HintsFetcher."
                                      @"GetHintsRequest.NetErrorCode"],
      @"hints request histogram missing");
  GREYAssertNil(
      [MetricsAppInterface
          expectUniqueSampleWithCount:1
                            forBucket:1
                         forHistogram:@"OptimizationGuide.HintsFetcher."
                                      @"GetHintsRequest.HintCount"],
      @"hints request histogram missing");
  [ChromeEarlGrey closeAllTabs];
}

@end
