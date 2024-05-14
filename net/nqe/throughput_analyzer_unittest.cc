// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/throughput_analyzer.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/base/schemeful_site.h"
#include "net/dns/mock_host_resolver.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/nqe/network_quality_estimator_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net::nqe {

namespace {

// Creates a mock resolver mapping example.com to a public IP address.
std::unique_ptr<HostResolver> CreateMockHostResolver() {
  auto host_resolver = std::make_unique<MockCachingHostResolver>(
      /*cache_invalidation_num=*/0,
      /*default_result=*/ERR_NAME_NOT_RESOLVED);

  // local.com resolves to a private IP address.
  host_resolver->rules()->AddRule("local.com", "127.0.0.1");
  host_resolver->LoadIntoCache(url::SchemeHostPort("http", "local.com", 80),
                               NetworkAnonymizationKey(), std::nullopt);
  // Hosts not listed here (e.g., "example.com") are treated as external. See
  // ThroughputAnalyzerTest.PrivateHost below.

  return host_resolver;
}

class TestThroughputAnalyzer : public internal::ThroughputAnalyzer {
 public:
  TestThroughputAnalyzer(NetworkQualityEstimator* network_quality_estimator,
                         NetworkQualityEstimatorParams* params,
                         const base::TickClock* tick_clock)
      : internal::ThroughputAnalyzer(
            network_quality_estimator,
            params,
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            base::BindRepeating(
                &TestThroughputAnalyzer::OnNewThroughputObservationAvailable,
                base::Unretained(this)),
            tick_clock,
            NetLogWithSource::Make(NetLogSourceType::NONE)) {}

  TestThroughputAnalyzer(const TestThroughputAnalyzer&) = delete;
  TestThroughputAnalyzer& operator=(const TestThroughputAnalyzer&) = delete;

  ~TestThroughputAnalyzer() override = default;

  int32_t throughput_observations_received() const {
    return throughput_observations_received_;
  }

  void OnNewThroughputObservationAvailable(int32_t downstream_kbps) {
    throughput_observations_received_++;
  }

  int64_t GetBitsReceived() const override { return bits_received_; }

  void IncrementBitsReceived(int64_t additional_bits_received) {
    bits_received_ += additional_bits_received;
  }

  using internal::ThroughputAnalyzer::CountActiveInFlightRequests;
  using internal::ThroughputAnalyzer::
      disable_throughput_measurements_for_testing;
  using internal::ThroughputAnalyzer::EraseHangingRequests;
  using internal::ThroughputAnalyzer::IsHangingWindow;

 private:
  int throughput_observations_received_ = 0;

  int64_t bits_received_ = 0;
};

using ThroughputAnalyzerTest = TestWithTaskEnvironment;

TEST_F(ThroughputAnalyzerTest, PrivateHost) {
  auto host_resolver = CreateMockHostResolver();
  EXPECT_FALSE(nqe::internal::IsPrivateHostForTesting(
      host_resolver.get(), url::SchemeHostPort("http", "example.com", 80),
      NetworkAnonymizationKey()));
  EXPECT_TRUE(nqe::internal::IsPrivateHostForTesting(
      host_resolver.get(), url::SchemeHostPort("http", "local.com", 80),
      NetworkAnonymizationKey()));
}

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
// Flaky on iOS: crbug.com/672917.
// Flaky on Android: crbug.com/1223950.
#define MAYBE_MaximumRequests DISABLED_MaximumRequests
#else
#define MAYBE_MaximumRequests MaximumRequests
#endif
TEST_F(ThroughputAnalyzerTest, MAYBE_MaximumRequests) {
  const struct TestCase {
    GURL url;
    bool is_local;
  } kTestCases[] = {
      {GURL("http://127.0.0.1/test.html"), true /* is_local */},
      {GURL("http://example.com/test.html"), false /* is_local */},
      {GURL("http://local.com/test.html"), true /* is_local */},
  };

  for (const auto& test_case : kTestCases) {
    const base::TickClock* tick_clock = base::DefaultTickClock::GetInstance();
    TestNetworkQualityEstimator network_quality_estimator;
    std::map<std::string, std::string> variation_params;
    NetworkQualityEstimatorParams params(variation_params);
    TestThroughputAnalyzer throughput_analyzer(&network_quality_estimator,
                                               &params, tick_clock);

    TestDelegate test_delegate;
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->set_host_resolver(CreateMockHostResolver());
    auto context = context_builder->Build();

    ASSERT_FALSE(
        throughput_analyzer.disable_throughput_measurements_for_testing());
    base::circular_deque<std::unique_ptr<URLRequest>> requests;

    // Start more requests than the maximum number of requests that can be held
    // in the memory.
    EXPECT_EQ(test_case.is_local,
              nqe::internal::IsPrivateHostForTesting(
                  context->host_resolver(), url::SchemeHostPort(test_case.url),
                  NetworkAnonymizationKey()));
    for (size_t i = 0; i < 1000; ++i) {
      std::unique_ptr<URLRequest> request(
          context->CreateRequest(test_case.url, DEFAULT_PRIORITY,
                                 &test_delegate, TRAFFIC_ANNOTATION_FOR_TESTS));
      throughput_analyzer.NotifyStartTransaction(*(request.get()));
      requests.push_back(std::move(request));
    }
    // Too many local requests should cause the |throughput_analyzer| to disable
    // throughput measurements.
    EXPECT_NE(test_case.is_local,
              throughput_analyzer.IsCurrentlyTrackingThroughput());
  }
}

#if BUILDFLAG(IS_IOS)
// Flaky on iOS: crbug.com/672917.
#define MAYBE_MaximumRequestsWithNetworkAnonymizationKey \
  DISABLED_MaximumRequestsWithNetworkAnonymizationKey
#else
#define MAYBE_MaximumRequestsWithNetworkAnonymizationKey \
  MaximumRequestsWithNetworkAnonymizationKey
#endif
// Make sure that the NetworkAnonymizationKey is respected when resolving a host
// from the cache.
TEST_F(ThroughputAnalyzerTest,
       MAYBE_MaximumRequestsWithNetworkAnonymizationKey) {
  const SchemefulSite kSite(GURL("https://foo.test/"));
  const auto kNetworkAnonymizationKey =
      NetworkAnonymizationKey::CreateSameSite(kSite);
  const net::NetworkIsolationKey kNetworkIsolationKey(kSite, kSite);
  const GURL kUrl = GURL("http://foo.test/test.html");
  const url::Origin kSiteOrigin = url::Origin::Create(kSite.GetURL());

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  for (bool use_network_isolation_key : {false, true}) {
    const base::TickClock* tick_clock = base::DefaultTickClock::GetInstance();
    TestNetworkQualityEstimator network_quality_estimator;
    std::map<std::string, std::string> variation_params;
    NetworkQualityEstimatorParams params(variation_params);
    TestThroughputAnalyzer throughput_analyzer(&network_quality_estimator,
                                               &params, tick_clock);

    TestDelegate test_delegate;
    auto context_builder = CreateTestURLRequestContextBuilder();
    auto mock_host_resolver = std::make_unique<MockCachingHostResolver>();

    // Add an entry to the host cache mapping kUrl to non-local IP when using an
    // empty NetworkAnonymizationKey.
    mock_host_resolver->rules()->AddRule(kUrl.host(), "1.2.3.4");
    mock_host_resolver->LoadIntoCache(url::SchemeHostPort(kUrl),
                                      NetworkAnonymizationKey(), std::nullopt);

    // Add an entry to the host cache mapping kUrl to local IP when using
    // kNetworkAnonymizationKey.
    mock_host_resolver->rules()->ClearRules();
    mock_host_resolver->rules()->AddRule(kUrl.host(), "127.0.0.1");
    mock_host_resolver->LoadIntoCache(url::SchemeHostPort(kUrl),
                                      kNetworkAnonymizationKey, std::nullopt);

    context_builder->set_host_resolver(std::move(mock_host_resolver));
    auto context = context_builder->Build();
    ASSERT_FALSE(
        throughput_analyzer.disable_throughput_measurements_for_testing());
    base::circular_deque<std::unique_ptr<URLRequest>> requests;

    // Start more requests than the maximum number of requests that can be held
    // in the memory.
    EXPECT_EQ(use_network_isolation_key,
              nqe::internal::IsPrivateHostForTesting(
                  context->host_resolver(), url::SchemeHostPort(kUrl),
                  use_network_isolation_key ? kNetworkAnonymizationKey
                                            : NetworkAnonymizationKey()));
    for (size_t i = 0; i < 1000; ++i) {
      std::unique_ptr<URLRequest> request(
          context->CreateRequest(kUrl, DEFAULT_PRIORITY, &test_delegate,
                                 TRAFFIC_ANNOTATION_FOR_TESTS));
      if (use_network_isolation_key) {
        request->set_isolation_info(net::IsolationInfo::Create(
            net::IsolationInfo::RequestType::kOther, kSiteOrigin, kSiteOrigin,
            net::SiteForCookies()));
      }
      throughput_analyzer.NotifyStartTransaction(*(request.get()));
      requests.push_back(std::move(request));
    }
    // Too many local requests should cause the |throughput_analyzer| to disable
    // throughput measurements.
    EXPECT_NE(use_network_isolation_key,
              throughput_analyzer.IsCurrentlyTrackingThroughput());
  }
}

// Tests that the throughput observation is taken only if there are sufficient
// number of requests in-flight.
TEST_F(ThroughputAnalyzerTest, TestMinRequestsForThroughputSample) {
  const base::TickClock* tick_clock = base::DefaultTickClock::GetInstance();
  TestNetworkQualityEstimator network_quality_estimator;
  std::map<std::string, std::string> variation_params;
  variation_params["throughput_hanging_requests_cwnd_size_multiplier"] = "-1";
  NetworkQualityEstimatorParams params(variation_params);
  // Set HTTP RTT to a large value so that the throughput observation window
  // is not detected as hanging. In practice, this would be provided by
  // |network_quality_estimator| based on the recent observations.
  network_quality_estimator.SetStartTimeNullHttpRtt(base::Seconds(100));

  for (size_t num_requests = 1;
       num_requests <= params.throughput_min_requests_in_flight() + 1;
       ++num_requests) {
    TestThroughputAnalyzer throughput_analyzer(&network_quality_estimator,
                                               &params, tick_clock);
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->set_host_resolver(CreateMockHostResolver());
    auto context = context_builder->Build();

    // TestDelegates must be before URLRequests that point to them.
    std::vector<TestDelegate> not_local_test_delegates(num_requests);
    std::vector<std::unique_ptr<URLRequest>> requests_not_local;
    for (auto& delegate : not_local_test_delegates) {
      // We don't care about completion, except for the first one (see below).
      delegate.set_on_complete(base::DoNothing());
      std::unique_ptr<URLRequest> request_not_local(context->CreateRequest(
          GURL("http://example.com/echo.html"), DEFAULT_PRIORITY, &delegate,
          TRAFFIC_ANNOTATION_FOR_TESTS));
      request_not_local->Start();
      requests_not_local.push_back(std::move(request_not_local));
    }
    not_local_test_delegates[0].RunUntilComplete();

    EXPECT_EQ(0, throughput_analyzer.throughput_observations_received());

    for (const auto& request : requests_not_local) {
      throughput_analyzer.NotifyStartTransaction(*request);
    }

    // Increment the bytes received count to emulate the bytes received for
    // |request_local| and |requests_not_local|.
    throughput_analyzer.IncrementBitsReceived(100 * 1000 * 8);

    for (const auto& request : requests_not_local) {
      throughput_analyzer.NotifyRequestCompleted(*request);
    }
    base::RunLoop().RunUntilIdle();

    int expected_throughput_observations =
        num_requests >= params.throughput_min_requests_in_flight() ? 1 : 0;
    EXPECT_EQ(expected_throughput_observations,
              throughput_analyzer.throughput_observations_received());
  }
}

// Tests that the hanging requests are dropped from the |requests_|, and
// throughput observation window is ended.
TEST_F(ThroughputAnalyzerTest, TestHangingRequests) {
  static const struct {
    int hanging_request_duration_http_rtt_multiplier;
    base::TimeDelta http_rtt;
    base::TimeDelta requests_hang_duration;
    bool expect_throughput_observation;
  } tests[] = {
      {
          // |requests_hang_duration| is less than 5 times the HTTP RTT.
          // Requests should not be marked as hanging.
          5,
          base::Milliseconds(1000),
          base::Milliseconds(3000),
          true,
      },
      {
          // |requests_hang_duration| is more than 5 times the HTTP RTT.
          // Requests should be marked as hanging.
          5,
          base::Milliseconds(200),
          base::Milliseconds(3000),
          false,
      },
      {
          // |requests_hang_duration| is less than
          // |hanging_request_min_duration_msec|. Requests should not be marked
          // as hanging.
          1,
          base::Milliseconds(100),
          base::Milliseconds(100),
          true,
      },
      {
          // |requests_hang_duration| is more than
          // |hanging_request_min_duration_msec|. Requests should be marked as
          // hanging.
          1,
          base::Milliseconds(2000),
          base::Milliseconds(3100),
          false,
      },
      {
          // |requests_hang_duration| is less than 5 times the HTTP RTT.
          // Requests should not be marked as hanging.
          5,
          base::Seconds(2),
          base::Seconds(1),
          true,
      },
      {
          // HTTP RTT is unavailable. Requests should not be marked as hanging.
          5,
          base::Seconds(-1),
          base::Seconds(-1),
          true,
      },
  };

  for (const auto& test : tests) {
    base::HistogramTester histogram_tester;
    const base::TickClock* tick_clock = base::DefaultTickClock::GetInstance();
    TestNetworkQualityEstimator network_quality_estimator;
    if (test.http_rtt >= base::TimeDelta())
      network_quality_estimator.SetStartTimeNullHttpRtt(test.http_rtt);
    std::map<std::string, std::string> variation_params;
    // Set the transport RTT multiplier to a large value so that the hanging
    // request decision is made only on the basis of the HTTP RTT.
    variation_params
        ["hanging_request_http_rtt_upper_bound_transport_rtt_multiplier"] =
            "10000";
    variation_params["throughput_hanging_requests_cwnd_size_multiplier"] = "-1";
    variation_params["hanging_request_duration_http_rtt_multiplier"] =
        base::NumberToString(test.hanging_request_duration_http_rtt_multiplier);

    NetworkQualityEstimatorParams params(variation_params);

    const size_t num_requests = params.throughput_min_requests_in_flight();
    TestThroughputAnalyzer throughput_analyzer(&network_quality_estimator,
                                               &params, tick_clock);
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->set_host_resolver(CreateMockHostResolver());
    auto context = context_builder->Build();

    // TestDelegates must be before URLRequests that point to them.
    std::vector<TestDelegate> not_local_test_delegates(num_requests);
    std::vector<std::unique_ptr<URLRequest>> requests_not_local;
    for (size_t i = 0; i < num_requests; ++i) {
      // We don't care about completion, except for the first one (see below).
      not_local_test_delegates[i].set_on_complete(base::DoNothing());
      std::unique_ptr<URLRequest> request_not_local(context->CreateRequest(
          GURL("http://example.com/echo.html"), DEFAULT_PRIORITY,
          &not_local_test_delegates[i], TRAFFIC_ANNOTATION_FOR_TESTS));
      request_not_local->Start();
      requests_not_local.push_back(std::move(request_not_local));
    }

    not_local_test_delegates[0].RunUntilComplete();

    EXPECT_EQ(0, throughput_analyzer.throughput_observations_received());

    for (size_t i = 0; i < num_requests; ++i) {
      throughput_analyzer.NotifyStartTransaction(*requests_not_local.at(i));
    }

    // Increment the bytes received count to emulate the bytes received for
    // |request_local| and |requests_not_local|.
    throughput_analyzer.IncrementBitsReceived(100 * 1000 * 8);

    // Mark in-flight requests as hanging requests (if specified in the test
    // params).
    if (test.requests_hang_duration >= base::TimeDelta())
      base::PlatformThread::Sleep(test.requests_hang_duration);

    EXPECT_EQ(num_requests, throughput_analyzer.CountActiveInFlightRequests());

    for (size_t i = 0; i < num_requests; ++i) {
      throughput_analyzer.NotifyRequestCompleted(*requests_not_local.at(i));
      if (!test.expect_throughput_observation) {
        // All in-flight requests should be marked as hanging, and thus should
        // be deleted from the set of in-flight requests.
        EXPECT_EQ(0u, throughput_analyzer.CountActiveInFlightRequests());
      } else {
        // One request should be deleted at one time.
        EXPECT_EQ(requests_not_local.size() - i - 1,
                  throughput_analyzer.CountActiveInFlightRequests());
      }
    }

    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(test.expect_throughput_observation,
              throughput_analyzer.throughput_observations_received() > 0);
  }
}

// Tests that the check for hanging requests is done at most once per second.
TEST_F(ThroughputAnalyzerTest, TestHangingRequestsCheckedOnlyPeriodically) {
  base::SimpleTestTickClock tick_clock;

  TestNetworkQualityEstimator network_quality_estimator;
  network_quality_estimator.SetStartTimeNullHttpRtt(base::Seconds(1));
  std::map<std::string, std::string> variation_params;
  variation_params["hanging_request_duration_http_rtt_multiplier"] = "5";
  variation_params["hanging_request_min_duration_msec"] = "2000";
  NetworkQualityEstimatorParams params(variation_params);

  TestThroughputAnalyzer throughput_analyzer(&network_quality_estimator,
                                             &params, &tick_clock);

  TestDelegate test_delegate;
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_host_resolver(CreateMockHostResolver());
  auto context = context_builder->Build();
  std::vector<std::unique_ptr<URLRequest>> requests_not_local;

  for (size_t i = 0; i < 2; ++i) {
    std::unique_ptr<URLRequest> request_not_local(context->CreateRequest(
        GURL("http://example.com/echo.html"), DEFAULT_PRIORITY, &test_delegate,
        TRAFFIC_ANNOTATION_FOR_TESTS));
    request_not_local->Start();
    requests_not_local.push_back(std::move(request_not_local));
  }

  std::unique_ptr<URLRequest> some_other_request(context->CreateRequest(
      GURL("http://example.com/echo.html"), DEFAULT_PRIORITY, &test_delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS));

  test_delegate.RunUntilComplete();

  // First request starts at t=1. The second request starts at t=2. The first
  // request would be marked as hanging at t=6, and the second request at t=7
  // seconds.
  for (size_t i = 0; i < 2; ++i) {
    tick_clock.Advance(base::Milliseconds(1000));
    throughput_analyzer.NotifyStartTransaction(*requests_not_local.at(i));
  }

  EXPECT_EQ(2u, throughput_analyzer.CountActiveInFlightRequests());
  tick_clock.Advance(base::Milliseconds(3500));
  // Current time is t = 5.5 seconds.
  throughput_analyzer.EraseHangingRequests(*some_other_request);
  EXPECT_EQ(2u, throughput_analyzer.CountActiveInFlightRequests());

  tick_clock.Advance(base::Milliseconds(1000));
  // Current time is t = 6.5 seconds.  One request should be marked as hanging.
  throughput_analyzer.EraseHangingRequests(*some_other_request);
  EXPECT_EQ(1u, throughput_analyzer.CountActiveInFlightRequests());

  // Current time is t = 6.5 seconds. Calling NotifyBytesRead again should not
  // run the hanging request checker since the last check was at t=6.5 seconds.
  throughput_analyzer.EraseHangingRequests(*some_other_request);
  EXPECT_EQ(1u, throughput_analyzer.CountActiveInFlightRequests());

  tick_clock.Advance(base::Milliseconds(600));
  // Current time is t = 7.1 seconds. Calling NotifyBytesRead again should not
  // run the hanging request checker since the last check was at t=6.5 seconds
  // (less than 1 second ago).
  throughput_analyzer.EraseHangingRequests(*some_other_request);
  EXPECT_EQ(1u, throughput_analyzer.CountActiveInFlightRequests());

  tick_clock.Advance(base::Milliseconds(400));
  // Current time is t = 7.5 seconds. Calling NotifyBytesRead again should run
  // the hanging request checker since the last check was at t=6.5 seconds (at
  // least 1 second ago).
  throughput_analyzer.EraseHangingRequests(*some_other_request);
  EXPECT_EQ(0u, throughput_analyzer.CountActiveInFlightRequests());
}

// Tests that the last received time for a request is updated when data is
// received for that request.
TEST_F(ThroughputAnalyzerTest, TestLastReceivedTimeIsUpdated) {
  base::SimpleTestTickClock tick_clock;

  TestNetworkQualityEstimator network_quality_estimator;
  network_quality_estimator.SetStartTimeNullHttpRtt(base::Seconds(1));
  std::map<std::string, std::string> variation_params;
  variation_params["hanging_request_duration_http_rtt_multiplier"] = "5";
  variation_params["hanging_request_min_duration_msec"] = "2000";
  NetworkQualityEstimatorParams params(variation_params);

  TestThroughputAnalyzer throughput_analyzer(&network_quality_estimator,
                                             &params, &tick_clock);

  TestDelegate test_delegate;
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_host_resolver(CreateMockHostResolver());
  auto context = context_builder->Build();

  std::unique_ptr<URLRequest> request_not_local(context->CreateRequest(
      GURL("http://example.com/echo.html"), DEFAULT_PRIORITY, &test_delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  request_not_local->Start();

  test_delegate.RunUntilComplete();

  std::unique_ptr<URLRequest> some_other_request(context->CreateRequest(
      GURL("http://example.com/echo.html"), DEFAULT_PRIORITY, &test_delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS));

  // Start time for the request is t=0 second. The request will be marked as
  // hanging at t=5 seconds.
  throughput_analyzer.NotifyStartTransaction(*request_not_local);

  tick_clock.Advance(base::Milliseconds(4000));
  // Current time is t=4.0 seconds.

  throughput_analyzer.EraseHangingRequests(*some_other_request);
  EXPECT_EQ(1u, throughput_analyzer.CountActiveInFlightRequests());

  //  The request will be marked as hanging at t=9 seconds.
  throughput_analyzer.NotifyBytesRead(*request_not_local);
  tick_clock.Advance(base::Milliseconds(4000));
  // Current time is t=8 seconds.
  throughput_analyzer.EraseHangingRequests(*some_other_request);
  EXPECT_EQ(1u, throughput_analyzer.CountActiveInFlightRequests());

  tick_clock.Advance(base::Milliseconds(2000));
  // Current time is t=10 seconds.
  throughput_analyzer.EraseHangingRequests(*some_other_request);
  EXPECT_EQ(0u, throughput_analyzer.CountActiveInFlightRequests());
}

// Test that a request that has been hanging for a long time is deleted
// immediately when EraseHangingRequests is called even if the last hanging
// request check was done recently.
TEST_F(ThroughputAnalyzerTest, TestRequestDeletedImmediately) {
  base::SimpleTestTickClock tick_clock;

  TestNetworkQualityEstimator network_quality_estimator;
  network_quality_estimator.SetStartTimeNullHttpRtt(base::Seconds(1));
  std::map<std::string, std::string> variation_params;
  variation_params["hanging_request_duration_http_rtt_multiplier"] = "2";
  NetworkQualityEstimatorParams params(variation_params);

  TestThroughputAnalyzer throughput_analyzer(&network_quality_estimator,
                                             &params, &tick_clock);

  TestDelegate test_delegate;
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_host_resolver(CreateMockHostResolver());
  auto context = context_builder->Build();

  std::unique_ptr<URLRequest> request_not_local(context->CreateRequest(
      GURL("http://example.com/echo.html"), DEFAULT_PRIORITY, &test_delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  request_not_local->Start();

  test_delegate.RunUntilComplete();

  // Start time for the request is t=0 second. The request will be marked as
  // hanging at t=2 seconds.
  throughput_analyzer.NotifyStartTransaction(*request_not_local);
  EXPECT_EQ(1u, throughput_analyzer.CountActiveInFlightRequests());

  tick_clock.Advance(base::Milliseconds(2900));
  // Current time is t=2.9 seconds.

  throughput_analyzer.EraseHangingRequests(*request_not_local);
  EXPECT_EQ(1u, throughput_analyzer.CountActiveInFlightRequests());

  // |request_not_local| should be deleted since it has been idle for 2.4
  // seconds.
  tick_clock.Advance(base::Milliseconds(500));
  throughput_analyzer.NotifyBytesRead(*request_not_local);
  EXPECT_EQ(0u, throughput_analyzer.CountActiveInFlightRequests());
}

#if BUILDFLAG(IS_IOS)
// Flaky on iOS: crbug.com/672917.
#define MAYBE_TestThroughputWithMultipleRequestsOverlap \
  DISABLED_TestThroughputWithMultipleRequestsOverlap
#else
#define MAYBE_TestThroughputWithMultipleRequestsOverlap \
  TestThroughputWithMultipleRequestsOverlap
#endif
// Tests if the throughput observation is taken correctly when local and network
// requests overlap.
TEST_F(ThroughputAnalyzerTest,
       MAYBE_TestThroughputWithMultipleRequestsOverlap) {
  static const struct {
    bool start_local_request;
    bool local_request_completes_first;
    bool expect_throughput_observation;
  } tests[] = {
      {
          false, false, true,
      },
      {
          true, false, false,
      },
      {
          true, true, true,
      },
  };

  for (const auto& test : tests) {
    const base::TickClock* tick_clock = base::DefaultTickClock::GetInstance();
    TestNetworkQualityEstimator network_quality_estimator;
    // Localhost requests are not allowed for estimation purposes.
    std::map<std::string, std::string> variation_params;
    variation_params["throughput_hanging_requests_cwnd_size_multiplier"] = "-1";
    NetworkQualityEstimatorParams params(variation_params);

    TestThroughputAnalyzer throughput_analyzer(&network_quality_estimator,
                                               &params, tick_clock);

    TestDelegate local_delegate;
    local_delegate.set_on_complete(base::DoNothing());
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->set_host_resolver(CreateMockHostResolver());
    auto context = context_builder->Build();
    std::unique_ptr<URLRequest> request_local;

    // TestDelegates must be before URLRequests that point to them.
    std::vector<TestDelegate> not_local_test_delegates(
        params.throughput_min_requests_in_flight());
    std::vector<std::unique_ptr<URLRequest>> requests_not_local;
    for (size_t i = 0; i < params.throughput_min_requests_in_flight(); ++i) {
      // We don't care about completion, except for the first one (see below).
      not_local_test_delegates[i].set_on_complete(base::DoNothing());
      std::unique_ptr<URLRequest> request_not_local(context->CreateRequest(
          GURL("http://example.com/echo.html"), DEFAULT_PRIORITY,
          &not_local_test_delegates[i], TRAFFIC_ANNOTATION_FOR_TESTS));
      request_not_local->Start();
      requests_not_local.push_back(std::move(request_not_local));
    }

    if (test.start_local_request) {
      request_local = context->CreateRequest(GURL("http://127.0.0.1/echo.html"),
                                             DEFAULT_PRIORITY, &local_delegate,
                                             TRAFFIC_ANNOTATION_FOR_TESTS);
      request_local->Start();
    }

    // Wait until the first not-local request completes.
    not_local_test_delegates[0].RunUntilComplete();

    EXPECT_EQ(0, throughput_analyzer.throughput_observations_received());

    // If |test.start_local_request| is true, then |request_local| starts
    // before |request_not_local|, and ends after |request_not_local|. Thus,
    // network quality estimator should not get a chance to record throughput
    // observation from |request_not_local| because of ongoing local request
    // at all times.
    if (test.start_local_request)
      throughput_analyzer.NotifyStartTransaction(*request_local);

    for (const auto& request : requests_not_local) {
      throughput_analyzer.NotifyStartTransaction(*request);
    }

    if (test.local_request_completes_first) {
      ASSERT_TRUE(test.start_local_request);
      throughput_analyzer.NotifyRequestCompleted(*request_local);
    }

    // Increment the bytes received count to emulate the bytes received for
    // |request_local| and |requests_not_local|.
    throughput_analyzer.IncrementBitsReceived(100 * 1000 * 8);

    for (const auto& request : requests_not_local) {
      throughput_analyzer.NotifyRequestCompleted(*request);
    }
    if (test.start_local_request && !test.local_request_completes_first)
      throughput_analyzer.NotifyRequestCompleted(*request_local);

    // Pump the message loop to let analyzer tasks get processed.
    base::RunLoop().RunUntilIdle();

    int expected_throughput_observations =
        test.expect_throughput_observation ? 1 : 0;
    EXPECT_EQ(expected_throughput_observations,
              throughput_analyzer.throughput_observations_received());
  }
}

// Tests if the throughput observation is taken correctly when two network
// requests overlap.
TEST_F(ThroughputAnalyzerTest, TestThroughputWithNetworkRequestsOverlap) {
  static const struct {
    size_t throughput_min_requests_in_flight;
    size_t number_requests_in_flight;
    int64_t increment_bits;
    bool expect_throughput_observation;
  } tests[] = {
      {
          1, 2, 100 * 1000 * 8, true,
      },
      {
          3, 1, 100 * 1000 * 8, false,
      },
      {
          3, 2, 100 * 1000 * 8, false,
      },
      {
          3, 3, 100 * 1000 * 8, true,
      },
      {
          3, 4, 100 * 1000 * 8, true,
      },
      {
          1, 2, 1, false,
      },
  };

  for (const auto& test : tests) {
    const base::TickClock* tick_clock = base::DefaultTickClock::GetInstance();
    TestNetworkQualityEstimator network_quality_estimator;
    // Localhost requests are not allowed for estimation purposes.
    std::map<std::string, std::string> variation_params;
    variation_params["throughput_min_requests_in_flight"] =
        base::NumberToString(test.throughput_min_requests_in_flight);
    variation_params["throughput_hanging_requests_cwnd_size_multiplier"] = "-1";
    NetworkQualityEstimatorParams params(variation_params);
    // Set HTTP RTT to a large value so that the throughput observation window
    // is not detected as hanging. In practice, this would be provided by
    // |network_quality_estimator| based on the recent observations.
    network_quality_estimator.SetStartTimeNullHttpRtt(base::Seconds(100));

    TestThroughputAnalyzer throughput_analyzer(&network_quality_estimator,
                                               &params, tick_clock);
    auto context_builder = CreateTestURLRequestContextBuilder();
    context_builder->set_host_resolver(CreateMockHostResolver());
    auto context = context_builder->Build();

    EXPECT_EQ(0, throughput_analyzer.throughput_observations_received());

    // TestDelegates must be before URLRequests that point to them.
    std::vector<TestDelegate> in_flight_test_delegates(
        test.number_requests_in_flight);
    std::vector<std::unique_ptr<URLRequest>> requests_in_flight;
    for (size_t i = 0; i < test.number_requests_in_flight; ++i) {
      // We don't care about completion, except for the first one (see below).
      in_flight_test_delegates[i].set_on_complete(base::DoNothing());
      std::unique_ptr<URLRequest> request_network_1 = context->CreateRequest(
          GURL("http://example.com/echo.html"), DEFAULT_PRIORITY,
          &in_flight_test_delegates[i], TRAFFIC_ANNOTATION_FOR_TESTS);
      requests_in_flight.push_back(std::move(request_network_1));
      requests_in_flight.back()->Start();
    }

    in_flight_test_delegates[0].RunUntilComplete();

    EXPECT_EQ(0, throughput_analyzer.throughput_observations_received());

    for (size_t i = 0; i < test.number_requests_in_flight; ++i) {
      URLRequest* request = requests_in_flight.at(i).get();
      throughput_analyzer.NotifyStartTransaction(*request);
    }

    // Increment the bytes received count to emulate the bytes received for
    // |request_network_1| and |request_network_2|.
    throughput_analyzer.IncrementBitsReceived(test.increment_bits);

    for (size_t i = 0; i < test.number_requests_in_flight; ++i) {
      URLRequest* request = requests_in_flight.at(i).get();
      throughput_analyzer.NotifyRequestCompleted(*request);
    }

    base::RunLoop().RunUntilIdle();

    // Only one observation should be taken since two requests overlap.
    if (test.expect_throughput_observation) {
      EXPECT_EQ(1, throughput_analyzer.throughput_observations_received());
    } else {
      EXPECT_EQ(0, throughput_analyzer.throughput_observations_received());
    }
  }
}

// Tests if the throughput observation is taken correctly when the start and end
// of network requests overlap, and the minimum number of in flight requests
// when taking an observation is more than 1.
TEST_F(ThroughputAnalyzerTest, TestThroughputWithMultipleNetworkRequests) {
  const base::test::ScopedRunLoopTimeout increased_run_timeout(
      FROM_HERE, TestTimeouts::action_max_timeout());

  const base::TickClock* tick_clock = base::DefaultTickClock::GetInstance();
  TestNetworkQualityEstimator network_quality_estimator;
  std::map<std::string, std::string> variation_params;
  variation_params["throughput_min_requests_in_flight"] = "3";
  variation_params["throughput_hanging_requests_cwnd_size_multiplier"] = "-1";
  NetworkQualityEstimatorParams params(variation_params);
  // Set HTTP RTT to a large value so that the throughput observation window
  // is not detected as hanging. In practice, this would be provided by
  // |network_quality_estimator| based on the recent observations.
  network_quality_estimator.SetStartTimeNullHttpRtt(base::Seconds(100));

  TestThroughputAnalyzer throughput_analyzer(&network_quality_estimator,
                                             &params, tick_clock);
  TestDelegate test_delegate;
  auto context_builder = CreateTestURLRequestContextBuilder();
  context_builder->set_host_resolver(CreateMockHostResolver());
  auto context = context_builder->Build();

  EXPECT_EQ(0, throughput_analyzer.throughput_observations_received());

  std::unique_ptr<URLRequest> request_1 = context->CreateRequest(
      GURL("http://example.com/echo.html"), DEFAULT_PRIORITY, &test_delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  std::unique_ptr<URLRequest> request_2 = context->CreateRequest(
      GURL("http://example.com/echo.html"), DEFAULT_PRIORITY, &test_delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  std::unique_ptr<URLRequest> request_3 = context->CreateRequest(
      GURL("http://example.com/echo.html"), DEFAULT_PRIORITY, &test_delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  std::unique_ptr<URLRequest> request_4 = context->CreateRequest(
      GURL("http://example.com/echo.html"), DEFAULT_PRIORITY, &test_delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS);

  request_1->Start();
  request_2->Start();
  request_3->Start();
  request_4->Start();

  // We dispatched four requests, so wait for four completions.
  for (int i = 0; i < 4; ++i)
    test_delegate.RunUntilComplete();

  EXPECT_EQ(0, throughput_analyzer.throughput_observations_received());

  throughput_analyzer.NotifyStartTransaction(*(request_1.get()));
  throughput_analyzer.NotifyStartTransaction(*(request_2.get()));

  const size_t increment_bits = 100 * 1000 * 8;

  // Increment the bytes received count to emulate the bytes received for
  // |request_1| and |request_2|.
  throughput_analyzer.IncrementBitsReceived(increment_bits);

  throughput_analyzer.NotifyRequestCompleted(*(request_1.get()));
  base::RunLoop().RunUntilIdle();

  // No observation should be taken since only 1 request is in flight.
  EXPECT_EQ(0, throughput_analyzer.throughput_observations_received());

  throughput_analyzer.NotifyStartTransaction(*(request_3.get()));
  throughput_analyzer.NotifyStartTransaction(*(request_4.get()));
  EXPECT_EQ(0, throughput_analyzer.throughput_observations_received());

  // 3 requests are in flight which is at least as many as the minimum number of
  // in flight requests required. An observation should be taken.
  throughput_analyzer.IncrementBitsReceived(increment_bits);

  // Only one observation should be taken since two requests overlap.
  throughput_analyzer.NotifyRequestCompleted(*(request_2.get()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, throughput_analyzer.throughput_observations_received());
  throughput_analyzer.NotifyRequestCompleted(*(request_3.get()));
  throughput_analyzer.NotifyRequestCompleted(*(request_4.get()));
  EXPECT_EQ(1, throughput_analyzer.throughput_observations_received());
}

TEST_F(ThroughputAnalyzerTest, TestHangingWindow) {
  static constexpr size_t kCwndSizeKilobytes = 10 * 1.5;
  static constexpr size_t kCwndSizeBits = kCwndSizeKilobytes * 1000 * 8;

  base::SimpleTestTickClock tick_clock;

  TestNetworkQualityEstimator network_quality_estimator;
  int64_t http_rtt_msec = 1000;
  network_quality_estimator.SetStartTimeNullHttpRtt(
      base::Milliseconds(http_rtt_msec));
  std::map<std::string, std::string> variation_params;
  variation_params["throughput_hanging_requests_cwnd_size_multiplier"] = "1";
  NetworkQualityEstimatorParams params(variation_params);

  TestThroughputAnalyzer throughput_analyzer(&network_quality_estimator,
                                             &params, &tick_clock);

  const struct {
    size_t bits_received;
    base::TimeDelta window_duration;
    bool expected_hanging;
  } tests[] = {
      {100, base::Milliseconds(http_rtt_msec), true},
      {kCwndSizeBits - 1, base::Milliseconds(http_rtt_msec), true},
      {kCwndSizeBits + 1, base::Milliseconds(http_rtt_msec), false},
      {2 * (kCwndSizeBits - 1), base::Milliseconds(http_rtt_msec * 2), true},
      {2 * (kCwndSizeBits + 1), base::Milliseconds(http_rtt_msec * 2), false},
      {kCwndSizeBits / 2 - 1, base::Milliseconds(http_rtt_msec / 2), true},
      {kCwndSizeBits / 2 + 1, base::Milliseconds(http_rtt_msec / 2), false},
  };

  for (const auto& test : tests) {
    base::HistogramTester histogram_tester;
    EXPECT_EQ(test.expected_hanging,
              throughput_analyzer.IsHangingWindow(test.bits_received,
                                                  test.window_duration));
  }
}

}  // namespace

}  // namespace net::nqe
