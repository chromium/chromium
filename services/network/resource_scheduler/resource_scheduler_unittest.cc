// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/resource_scheduler/resource_scheduler.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/timer.h"
#include "net/base/features.h"
#include "net/base/host_port_pair.h"
#include "net/base/isolation_info.h"
#include "net/base/load_timing_info.h"
#include "net/base/request_priority.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/resource_scheduler/resource_scheduler_params_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"

using std::string;

namespace network {

namespace {

// Verifies that (i) Exactly one sample is recorded in |histogram_name|; and,
// (ii) The sample value is at least |min_value|.
void ExpectSampleIsAtLeastSpecifiedValue(
    const base::HistogramTester& histogram_tester,
    const std::string& histogram_name,
    int min_value) {
  histogram_tester.ExpectTotalCount(histogram_name, 1);

  // Verify if the recorded unique sample is in the same bucket to which
  // |min_value| belongs to.
  if (histogram_tester.GetBucketCount(histogram_name, min_value) == 1) {
    return;
  }

  // Verify if the recorded unique sample is in a bucket that contains samples
  // larger than |min_value|.
  const std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples(histogram_name);
  EXPECT_EQ(1u, buckets.size());
  bool sample_found = false;
  for (const auto& bucket : buckets) {
    if (bucket.count > 0) {
      // Verify that the sample is at least |min_value|.
      EXPECT_GE(bucket.min, min_value);
      sample_found = true;
    }
  }
  EXPECT_TRUE(sample_found);
}

class TestRequestFactory;

const int kChildId = 30;
const int kRouteId = 75;
const int kChildId2 = 43;
const int kRouteId2 = 67;
const int kChildId3 = 100;
const int kRouteId3 = 100;
const int kBackgroundChildId = 35;
const int kBackgroundRouteId = 43;
const int kBrowserChildId = mojom::kBrowserProcessId;
const int kBrowserRouteId = 1;

const size_t kNumResourceSchedulerClients = 20;
const size_t kMaxNumDelayableRequestsPerHostPerClient = 6;

class TestRequest {
 public:
  TestRequest(std::unique_ptr<net::URLRequest> url_request,
              std::unique_ptr<ResourceScheduler::ScheduledResourceRequest>
                  scheduled_request,
              ResourceScheduler* scheduler)
      : started_(false),
        url_request_(std::move(url_request)),
        scheduled_request_(std::move(scheduled_request)),
        scheduler_(scheduler) {
    scheduled_request_->set_resume_callback(
        base::BindOnce(&TestRequest::Resume, base::Unretained(this)));
  }
  virtual ~TestRequest() {
    // The URLRequest must still be valid when the ScheduledResourceRequest is
    // destroyed, so that it can unregister itself.
    scheduled_request_.reset();
  }

  bool started() const { return started_; }

  void Start() {
    bool deferred = false;
    scheduled_request_->WillStartRequest(&deferred);
    started_ = !deferred;
  }

  void ChangePriority(net::RequestPriority new_priority, int intra_priority) {
    scheduler_->ReprioritizeRequest(url_request_.get(), new_priority,
                                    intra_priority);
  }

  const net::URLRequest* url_request() const { return url_request_.get(); }

  virtual void Resume() { started_ = true; }

 private:
  bool started_;
  std::unique_ptr<net::URLRequest> url_request_;
  std::unique_ptr<ResourceScheduler::ScheduledResourceRequest>
      scheduled_request_;
  ResourceScheduler* scheduler_;
};

class CancelingTestRequest : public TestRequest {
 public:
  CancelingTestRequest(
      std::unique_ptr<net::URLRequest> url_request,
      std::unique_ptr<ResourceScheduler::ScheduledResourceRequest>
          scheduled_request,
      ResourceScheduler* scheduler)
      : TestRequest(std::move(url_request),
                    std::move(scheduled_request),
                    scheduler) {}

  void set_request_to_cancel(std::unique_ptr<TestRequest> request_to_cancel) {
    request_to_cancel_ = std::move(request_to_cancel);
  }

 private:
  void Resume() override {
    TestRequest::Resume();
    request_to_cancel_.reset();
  }

  std::unique_ptr<TestRequest> request_to_cancel_;
};

class ResourceSchedulerTest : public testing::Test {
 protected:
  ResourceSchedulerTest() {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        net::features::kPartitionHttpServerPropertiesByNetworkIsolationKey);
    // This has to be done after initializing the feature list, since the value
    // of the feature is cached.
    context_ = std::make_unique<net::TestURLRequestContext>(true);
    context_->set_network_quality_estimator(&network_quality_estimator_);
    context_->Init();

    InitializeScheduler();
  }

  ~ResourceSchedulerTest() override { CleanupScheduler(); }

  // Done separately from construction to allow for modification of command
  // line flags in tests.
  void InitializeScheduler() {
    CleanupScheduler();

    // Destroys previous scheduler.
    scheduler_ = std::make_unique<ResourceScheduler>(&tick_clock_);

    scheduler()->SetResourceSchedulerParamsManagerForTests(
        resource_scheduler_params_manager_);

    scheduler_->OnClientCreated(kChildId, kRouteId,
                                &network_quality_estimator_);
    scheduler_->OnClientCreated(kBackgroundChildId, kBackgroundRouteId,
                                &network_quality_estimator_);
    scheduler_->OnClientCreated(kBrowserChildId, kBrowserRouteId,
                                &network_quality_estimator_);
  }

  ResourceSchedulerParamsManager FixedParamsManager(
      size_t max_delayable_requests) const {
    ResourceSchedulerParamsManager::ParamsForNetworkQualityContainer c;
    for (int i = 0; i != net::EFFECTIVE_CONNECTION_TYPE_LAST; ++i) {
      auto type = static_cast<net::EffectiveConnectionType>(i);
      c[type] = ResourceSchedulerParamsManager::ParamsForNetworkQuality(
          max_delayable_requests, 0.0, false, base::nullopt);
    }
    return ResourceSchedulerParamsManager(std::move(c));
  }

  void SetMaxDelayableRequests(size_t max_delayable_requests) {
    scheduler()->SetResourceSchedulerParamsManagerForTests(
        ResourceSchedulerParamsManager(
            FixedParamsManager(max_delayable_requests)));
  }

  void CleanupScheduler() {
    if (scheduler_) {
      scheduler_->OnClientDeleted(kChildId, kRouteId);
      scheduler_->OnClientDeleted(kBackgroundChildId, kBackgroundRouteId);
      scheduler_->OnClientDeleted(kBrowserChildId, kBrowserRouteId);
    }
  }

  std::unique_ptr<net::URLRequest> NewURLRequestWithChildAndRoute(
      const char* url,
      net::RequestPriority priority,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      int child_id,
      int route_id) {
    std::unique_ptr<net::URLRequest> url_request(context_->CreateRequest(
        GURL(url), priority, nullptr, traffic_annotation));
    return url_request;
  }

  std::unique_ptr<net::URLRequest> NewURLRequest(
      const char* url,
      net::RequestPriority priority) {
    return NewURLRequestWithChildAndRoute(
        url, priority, TRAFFIC_ANNOTATION_FOR_TESTS, kChildId, kRouteId);
  }

  std::unique_ptr<TestRequest> NewRequestWithRoute(
      const char* url,
      net::RequestPriority priority,
      int route_id) {
    return NewRequestWithChildAndRoute(url, priority, kChildId, route_id);
  }

  std::unique_ptr<TestRequest> NewRequestWithChildAndRoute(
      const char* url,
      net::RequestPriority priority,
      int child_id,
      int route_id) {
    return GetNewTestRequest(url, priority, TRAFFIC_ANNOTATION_FOR_TESTS,
                             child_id, route_id, true, net::IsolationInfo());
  }

  std::unique_ptr<TestRequest> NewRequest(const char* url,
                                          net::RequestPriority priority) {
    return NewRequestWithChildAndRoute(url, priority, kChildId, kRouteId);
  }

  std::unique_ptr<TestRequest> NewBackgroundRequest(
      const char* url,
      net::RequestPriority priority) {
    return NewRequestWithChildAndRoute(url, priority, kBackgroundChildId,
                                       kBackgroundRouteId);
  }

  std::unique_ptr<TestRequest> NewBrowserRequest(
      const char* url,
      net::RequestPriority priority) {
    return NewRequestWithChildAndRoute(url, priority, kBrowserChildId,
                                       kBrowserRouteId);
  }

  std::unique_ptr<TestRequest> NewBrowserRequestWithAnnotationTag(
      const char* url,
      net::RequestPriority priority,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) {
    return GetNewTestRequest(url, priority, traffic_annotation, kBrowserChildId,
                             kBrowserRouteId, true, net::IsolationInfo());
  }

  std::unique_ptr<TestRequest> NewSyncRequest(const char* url,
                                              net::RequestPriority priority) {
    return NewSyncRequestWithChildAndRoute(url, priority, kChildId, kRouteId);
  }

  std::unique_ptr<TestRequest> NewBackgroundSyncRequest(
      const char* url,
      net::RequestPriority priority) {
    return NewSyncRequestWithChildAndRoute(url, priority, kBackgroundChildId,
                                           kBackgroundRouteId);
  }

  std::unique_ptr<TestRequest> NewSyncRequestWithChildAndRoute(
      const char* url,
      net::RequestPriority priority,
      int child_id,
      int route_id) {
    return GetNewTestRequest(url, priority, TRAFFIC_ANNOTATION_FOR_TESTS,
                             child_id, route_id, false, net::IsolationInfo());
  }

  std::unique_ptr<TestRequest> NewRequestWithIsolationInfo(
      const char* url,
      net::RequestPriority priority,
      const net::IsolationInfo& isolation_info) {
    return GetNewTestRequest(url, priority, TRAFFIC_ANNOTATION_FOR_TESTS,
                             kChildId, kRouteId, true, isolation_info);
  }

  std::unique_ptr<TestRequest> GetNewTestRequest(
      const char* url,
      net::RequestPriority priority,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      int child_id,
      int route_id,
      bool is_async,
      const net::IsolationInfo& isolation_info) {
    std::unique_ptr<net::URLRequest> url_request(NewURLRequestWithChildAndRoute(
        url, priority, traffic_annotation, child_id, route_id));
    url_request->set_isolation_info(isolation_info);

    auto scheduled_request = scheduler_->ScheduleRequest(
        child_id, route_id, is_async, url_request.get());
    auto request = std::make_unique<TestRequest>(
        std::move(url_request), std::move(scheduled_request), scheduler());
    request->Start();
    return request;
  }

  void ChangeRequestPriority(TestRequest* request,
                             net::RequestPriority new_priority,
                             int intra_priority = 0) {
    request->ChangePriority(new_priority, intra_priority);
  }

  void RequestLimitOverrideConfigTestHelper(bool experiment_status) {
    InitializeThrottleDelayableExperiment(experiment_status, 0.0);

    // Set the effective connection type to Slow-2G, which is slower than the
    // threshold configured in |InitializeThrottleDelayableExperiment|. Needs
    // to be done before initializing the scheduler because the client is
    // created on the call to |InitializeScheduler|, which is where the initial
    // limits for the delayable requests in flight are computed.
    network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
        net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
    // Initialize the scheduler.
    InitializeScheduler();

    // Throw in one high priority request to ensure that high priority requests
    // do not depend on anything.
    std::unique_ptr<TestRequest> high2(
        NewRequest("http://host/high2", net::HIGHEST));
    EXPECT_TRUE(high2->started());

    // Should match the configuration set by
    // |InitializeThrottleDelayableExperiment|
    const int kOverriddenNumRequests = 2;

    std::vector<std::unique_ptr<TestRequest>> lows_singlehost;
    // Queue the maximum number of delayable requests that should be started
    // before the resource scheduler starts throttling delayable requests.
    for (int i = 0; i < kOverriddenNumRequests; ++i) {
      std::string url = "http://host/low" + base::NumberToString(i);
      lows_singlehost.push_back(NewRequest(url.c_str(), net::LOWEST));
      EXPECT_TRUE(lows_singlehost[i]->started());
    }

    std::unique_ptr<TestRequest> second_last_singlehost(
        NewRequest("http://host/s_last", net::LOWEST));
    std::unique_ptr<TestRequest> last_singlehost(
        NewRequest("http://host/last", net::LOWEST));

    if (experiment_status) {
      // Experiment enabled, hence requests should be limited.
      // Second last should not start because there are |kOverridenNumRequests|
      // delayable requests already in-flight.
      EXPECT_FALSE(second_last_singlehost->started());

      // Completion of a delayable request must result in starting of the
      // second-last request.
      lows_singlehost.erase(lows_singlehost.begin());
      base::RunLoop().RunUntilIdle();
      EXPECT_TRUE(second_last_singlehost->started());
      EXPECT_FALSE(last_singlehost->started());

      // Completion of another delayable request must result in starting of the
      // last request.
      lows_singlehost.erase(lows_singlehost.begin());
      base::RunLoop().RunUntilIdle();
      EXPECT_TRUE(last_singlehost->started());
    } else {
      // Requests should start because the default limit is 10.
      EXPECT_TRUE(second_last_singlehost->started());
      EXPECT_TRUE(last_singlehost->started());
    }
  }

  void ConfigureDelayRequestsOnMultiplexedConnectionsFieldTrial() {
    std::map<net::EffectiveConnectionType,
             ResourceSchedulerParamsManager::ParamsForNetworkQuality>
        params_for_network_quality_container;
    ResourceSchedulerParamsManager::ParamsForNetworkQuality params_slow_2g(
        8, 3.0, true, base::nullopt);
    ResourceSchedulerParamsManager::ParamsForNetworkQuality params_2g(
        8, 3.0, true, base::nullopt);

    params_for_network_quality_container
        [net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G] = params_slow_2g;
    params_for_network_quality_container[net::EFFECTIVE_CONNECTION_TYPE_2G] =
        params_2g;

    resource_scheduler_params_manager_.Reset(
        params_for_network_quality_container);
  }

  void InitializeThrottleDelayableExperiment(bool lower_delayable_count_enabled,
                                             double non_delayable_weight) {
    std::map<net::EffectiveConnectionType,
             ResourceSchedulerParamsManager::ParamsForNetworkQuality>
        params_for_network_quality_container;
    ResourceSchedulerParamsManager::ParamsForNetworkQuality params_slow_2g(
        8, 3.0, false, base::nullopt);
    ResourceSchedulerParamsManager::ParamsForNetworkQuality params_3g(
        10, 0.0, false, base::nullopt);

    if (lower_delayable_count_enabled) {
      params_slow_2g.max_delayable_requests = 2;
      params_slow_2g.non_delayable_weight = 0.0;
      params_3g.max_delayable_requests = 4;
      params_3g.non_delayable_weight = 0.0;
    }
    if (non_delayable_weight > 0.0) {
      if (!lower_delayable_count_enabled)
        params_slow_2g.max_delayable_requests = 8;
      params_slow_2g.non_delayable_weight = non_delayable_weight;
    }

    params_for_network_quality_container
        [net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G] = params_slow_2g;
    params_for_network_quality_container[net::EFFECTIVE_CONNECTION_TYPE_3G] =
        params_3g;

    resource_scheduler_params_manager_.Reset(
        params_for_network_quality_container);
  }

  void InitializeMaxQueuingDelayExperiment(base::TimeDelta max_queuing_time) {
    std::map<net::EffectiveConnectionType,
             ResourceSchedulerParamsManager::ParamsForNetworkQuality>
        params_for_network_quality_container;

    ResourceSchedulerParamsManager::ParamsForNetworkQuality params_slow_2g(
        8, 3.0, true, base::nullopt);
    params_slow_2g.max_queuing_time = max_queuing_time;
    params_for_network_quality_container
        [net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G] = params_slow_2g;

    resource_scheduler_params_manager_.Reset(
        params_for_network_quality_container);
  }

  void NonDelayableThrottlesDelayableHelper(double non_delayable_weight) {
    // Should be in sync with .cc for ECT SLOW_2G,
    const int kDefaultMaxNumDelayableRequestsPerClient = 8;
    // Initialize the experiment.
    InitializeThrottleDelayableExperiment(false, non_delayable_weight);
    network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
        net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

    InitializeScheduler();
    // Start one non-delayable request.
    std::unique_ptr<TestRequest> non_delayable_request(
        NewRequest("http://host/medium", net::MEDIUM));
    // Start |kDefaultMaxNumDelayableRequestsPerClient - 1 *
    // |non_delayable_weight|  delayable requests. They should all start.
    std::vector<std::unique_ptr<TestRequest>> delayable_requests;
    for (int i = 0;
         i < kDefaultMaxNumDelayableRequestsPerClient - non_delayable_weight;
         ++i) {
      delayable_requests.push_back(NewRequest(
          base::StringPrintf("http://host%d/low", i).c_str(), net::LOWEST));
      EXPECT_TRUE(delayable_requests.back()->started());
    }
    // The next delayable request should not start.
    std::unique_ptr<TestRequest> last_low(
        NewRequest("http://lasthost/low", net::LOWEST));
    EXPECT_FALSE(last_low->started());
  }

  void ConfigureProactiveThrottlingExperimentFor2G(
      double http_rtt_multiplier_for_proactive_throttling) {
    std::map<net::EffectiveConnectionType,
             ResourceSchedulerParamsManager::ParamsForNetworkQuality>
        params_for_network_quality_container;
    ResourceSchedulerParamsManager::ParamsForNetworkQuality params_2g;

    params_2g.http_rtt_multiplier_for_proactive_throttling =
        http_rtt_multiplier_for_proactive_throttling;

    params_for_network_quality_container[net::EFFECTIVE_CONNECTION_TYPE_2G] =
        params_2g;

    resource_scheduler_params_manager_.Reset(
        params_for_network_quality_container);

    network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
        net::EFFECTIVE_CONNECTION_TYPE_2G);
    base::RunLoop().RunUntilIdle();

    InitializeScheduler();
  }

  ResourceScheduler* scheduler() { return scheduler_.get(); }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ResourceScheduler> scheduler_;
  net::TestNetworkQualityEstimator network_quality_estimator_;
  std::unique_ptr<net::TestURLRequestContext> context_;
  ResourceSchedulerParamsManager resource_scheduler_params_manager_;
  base::SimpleTestTickClock tick_clock_;
};

TEST_F(ResourceSchedulerTest, OneIsolatedLowRequest) {
  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/1", net::LOWEST));
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, OneLowLoadsUntilCriticalComplete) {
  base::HistogramTester histogram_tester;
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_4G);
  InitializeScheduler();

  SetMaxDelayableRequests(1);
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  std::unique_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(high->started());
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());

  SetMaxDelayableRequests(10);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(low2->started());

  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low2->started());

  histogram_tester.ExpectTotalCount(
      "ResourceScheduler.RequestQueuingDuration.Priority" +
          base::NumberToString(net::HIGHEST),
      1);
  histogram_tester.ExpectTotalCount(
      "ResourceScheduler.RequestQueuingDuration.Priority" +
          base::NumberToString(net::LOWEST),
      2);
}

TEST_F(ResourceSchedulerTest, MaxRequestsPerHostForSpdyWhenNotDelayable) {
  InitializeScheduler();
  context_->http_server_properties()->SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), net::NetworkIsolationKey(),
      true);

  // Add more than max-per-host low-priority requests.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (size_t i = 0; i < kMaxNumDelayableRequestsPerHostPerClient + 1; ++i)
    requests.push_back(NewRequest("https://spdyhost/low", net::LOWEST));

  // No throttling.
  for (const auto& request : requests)
    EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest,
       MaxRequestsPerHostForSpdyWhenNotDelayableWithIsolationInfo) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://foo.test/"));
  const net::IsolationInfo kIsolationInfo1 =
      net::IsolationInfo::CreateForInternalRequest(kOrigin1);
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://bar.test/"));
  const net::IsolationInfo kIsolationInfo2 =
      net::IsolationInfo::CreateForInternalRequest(kOrigin2);

  InitializeScheduler();
  context_->http_server_properties()->SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443),
      kIsolationInfo1.network_isolation_key(), true);

  // Add more than max-per-host low-priority requests.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (size_t i = 0; i < kMaxNumDelayableRequestsPerHostPerClient + 1; ++i) {
    requests.push_back(NewRequestWithIsolationInfo(
        "https://spdyhost/low", net::LOWEST, kIsolationInfo1));
    // No throttling.
    EXPECT_TRUE(requests.back()->started());
  }
  requests.clear();

  // Requests with different IsolationInfos should be throttled as if they
  // don't support H2.

  for (size_t i = 0; i < kMaxNumDelayableRequestsPerHostPerClient + 1; ++i) {
    requests.push_back(NewRequestWithIsolationInfo(
        "https://spdyhost/low", net::LOWEST, net::IsolationInfo()));
    EXPECT_EQ(i < kMaxNumDelayableRequestsPerHostPerClient,
              requests.back()->started());
  }
  requests.clear();

  for (size_t i = 0; i < kMaxNumDelayableRequestsPerHostPerClient + 1; ++i) {
    requests.push_back(NewRequestWithIsolationInfo(
        "https://spdyhost/low", net::LOWEST, kIsolationInfo2));
    EXPECT_EQ(i < kMaxNumDelayableRequestsPerHostPerClient,
              requests.back()->started());
  }
  requests.clear();
}

TEST_F(ResourceSchedulerTest, BackgroundRequestStartsImmediately) {
  const int route_id = 0;  // Indicates a background request.
  std::unique_ptr<TestRequest> request(
      NewRequestWithRoute("http://host/1", net::LOWEST, route_id));
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, CancelOtherRequestsWhileResuming) {
  SetMaxDelayableRequests(1);

  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low1(
      NewRequest("http://host/low1", net::LOWEST));

  std::unique_ptr<net::URLRequest> url_request(
      NewURLRequest("http://host/low2", net::LOWEST));
  auto scheduled_request =
      scheduler()->ScheduleRequest(kChildId, kRouteId, true, url_request.get());
  std::unique_ptr<CancelingTestRequest> low2(new CancelingTestRequest(
      std::move(url_request), std::move(scheduled_request), scheduler()));
  low2->Start();

  std::unique_ptr<TestRequest> low3(
      NewRequest("http://host/low3", net::LOWEST));
  low2->set_request_to_cancel(std::move(low3));
  std::unique_ptr<TestRequest> low4(
      NewRequest("http://host/low4", net::LOWEST));

  EXPECT_TRUE(high->started());
  EXPECT_FALSE(low2->started());

  SetMaxDelayableRequests(10);
  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low1->started());
  EXPECT_TRUE(low2->started());
  EXPECT_TRUE(low4->started());
}

TEST_F(ResourceSchedulerTest, LimitedNumberOfDelayableRequestsInFlight) {
  // Throw in one high priority request to make sure that's not a factor.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  EXPECT_TRUE(high->started());

  const int kDefaultMaxNumDelayableRequestsPerClient =
      10;  // Should match the .cc.
  const int kMaxNumDelayableRequestsPerHost = 6;
  std::vector<std::unique_ptr<TestRequest>> lows_singlehost;
  // Queue up to the per-host limit (we subtract the current high-pri request).
  for (int i = 0; i < kMaxNumDelayableRequestsPerHost - 1; ++i) {
    string url = "http://host/low" + base::NumberToString(i);
    lows_singlehost.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(lows_singlehost[i]->started());
  }

  std::unique_ptr<TestRequest> second_last_singlehost(
      NewRequest("http://host/last", net::LOWEST));
  std::unique_ptr<TestRequest> last_singlehost(
      NewRequest("http://host/s_last", net::LOWEST));

  EXPECT_FALSE(second_last_singlehost->started());

  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(second_last_singlehost->started());
  EXPECT_FALSE(last_singlehost->started());

  lows_singlehost.erase(lows_singlehost.begin());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(last_singlehost->started());

  // Queue more requests from different hosts until we reach the total limit.
  int expected_slots_left = kDefaultMaxNumDelayableRequestsPerClient -
                            kMaxNumDelayableRequestsPerHost;
  EXPECT_GT(expected_slots_left, 0);
  std::vector<std::unique_ptr<TestRequest>> lows_different_host;
  base::RunLoop().RunUntilIdle();
  for (int i = 0; i < expected_slots_left; ++i) {
    string url = "http://host" + base::NumberToString(i) + "/low";
    lows_different_host.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(lows_different_host[i]->started());
  }

  std::unique_ptr<TestRequest> last_different_host(
      NewRequest("http://host_new/last", net::LOWEST));
  EXPECT_FALSE(last_different_host->started());
}

TEST_F(ResourceSchedulerTest, RaisePriorityAndStart) {
  // Dummies to enforce scheduling.
  SetMaxDelayableRequests(1);
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/req", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::LOWEST));
  EXPECT_FALSE(request->started());

  ChangeRequestPriority(request.get(), net::HIGHEST);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, RaisePriorityInQueue) {
  // Dummies to enforce scheduling.
  SetMaxDelayableRequests(1);
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::IDLE));
  std::unique_ptr<TestRequest> idle(NewRequest("http://host/idle", net::IDLE));
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  ChangeRequestPriority(request.get(), net::LOWEST);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  const int kDefaultMaxNumDelayableRequestsPerClient = 10;
  std::vector<std::unique_ptr<TestRequest>> lows;
  for (int i = 0; i < kDefaultMaxNumDelayableRequestsPerClient - 1; ++i) {
    string url = "http://host/low" + base::NumberToString(i);
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
  }

  SetMaxDelayableRequests(kDefaultMaxNumDelayableRequestsPerClient);
  high.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(request->started());
  EXPECT_FALSE(idle->started());
}

TEST_F(ResourceSchedulerTest, LowerPriority) {
  SetMaxDelayableRequests(1);
  // Dummies to enforce scheduling.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::LOWEST));
  std::unique_ptr<TestRequest> idle(NewRequest("http://host/idle", net::IDLE));
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  ChangeRequestPriority(request.get(), net::IDLE);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  const int kDefaultMaxNumDelayableRequestsPerClient =
      10;  // Should match the .cc.
  // 2 fewer filler requests: 1 for the "low" dummy at the start, and 1 for the
  // one at the end, which will be tested.
  const int kNumFillerRequests = kDefaultMaxNumDelayableRequestsPerClient - 2;
  std::vector<std::unique_ptr<TestRequest>> lows;
  for (int i = 0; i < kNumFillerRequests; ++i) {
    string url = "http://host" + base::NumberToString(i) + "/low";
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
  }

  SetMaxDelayableRequests(10);
  high.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(request->started());
  EXPECT_TRUE(idle->started());
}

// Verify that browser requests are not throttled by the resource scheduler.
TEST_F(ResourceSchedulerTest, LowerPriorityBrowserRequestsNotThrottled) {
  SetMaxDelayableRequests(1);
  // Dummies to enforce scheduling.
  std::unique_ptr<TestRequest> high(
      NewBrowserRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(
      NewBrowserRequest("http://host/low", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewBrowserRequest("http://host/req", net::LOWEST));
  std::unique_ptr<TestRequest> idle(
      NewBrowserRequest("http://host/idle", net::IDLE));
  EXPECT_TRUE(request->started());
  EXPECT_TRUE(idle->started());

  const int kDefaultMaxNumDelayableRequestsPerClient =
      10;  // Should match the .cc.

  // Create more requests than kDefaultMaxNumDelayableRequestsPerClient. All
  // requests should be started immediately.
  std::vector<std::unique_ptr<TestRequest>> lows;
  for (int i = 0; i < kDefaultMaxNumDelayableRequestsPerClient + 1; ++i) {
    string url = "http://host" + base::NumberToString(i) + "/low";
    lows.push_back(NewBrowserRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(lows.back()->started());
  }
}

// Verify that browser requests are throttled by the resource scheduler only
// when all the conditions are met.
TEST_F(ResourceSchedulerTest,
       LowerPriorityBrowserRequestsThrottleP2PConnections) {
  const struct {
    std::string test_case;
    size_t p2p_active_connections;
    net::EffectiveConnectionType effective_connection_type;
    bool enable_pausing_behavior;
    bool set_field_trial_param;
    bool expected_browser_initiated_traffic_started;
  } tests[] = {
      {
          "Field trial set",
          1u,
          net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
          true,
          true,
          false,
      },
      {
          "Field trial not set",
          1u,
          net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
          false,
          true,
          true,
      },
      {
          "Network fast",
          1u,
          net::EFFECTIVE_CONNECTION_TYPE_4G,
          true,
          true,
          true,
      },
      {
          "No active p2p connections",
          0u,
          net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
          true,
          true,
          true,
      },
      {
          "Field trial param not set, default params used",
          1u,
          net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
          true,
          false,
          false,
      },
  };

  for (const auto& test : tests) {
    base::test::ScopedFeatureList scoped_feature_list;
    if (test.enable_pausing_behavior) {
      base::FieldTrialParams field_trial_params;
      if (test.set_field_trial_param) {
        field_trial_params["throttled_traffic_annotation_tags"] = "727528";
      }
      scoped_feature_list.InitAndEnableFeatureWithParameters(
          features::kPauseBrowserInitiatedHeavyTrafficForP2P,
          field_trial_params);
    } else {
      scoped_feature_list.InitAndDisableFeature(
          features::kPauseBrowserInitiatedHeavyTrafficForP2P);
    }
    InitializeScheduler();

    network_quality_estimator_
        .SetAndNotifyObserversOfP2PActiveConnectionsCountChange(
            test.p2p_active_connections);
    network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
        test.effective_connection_type);

    std::string url = "http://host/browser-initiatited";

    net::NetworkTrafficAnnotationTag tag = net::DefineNetworkTrafficAnnotation(
        "metrics_report_uma",
        "Traffic annotation for unit, browser and other tests");
    // (COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH(""));
    std::unique_ptr<TestRequest> lows = (NewBrowserRequestWithAnnotationTag(
        url.c_str(), net::LOWEST, tag));  //"metrics_report_uma"));
    EXPECT_EQ(test.expected_browser_initiated_traffic_started, lows->started())
        << " test_case=" << test.test_case;
  }
}

// Verify that browser requests that are currently queued are dispatched to the
// network as soon as the active P2P connections count drops to 0.
TEST_F(ResourceSchedulerTest, P2PConnectionWentAway) {
  const struct {
    int seconds_to_pause_requests_after_end_of_p2p_connections;
    bool expect_lows_started;
  } tests[] = {
      {// When |seconds_to_pause_requests_after_end_of_p2p_connections| is 0,
       // running the RunLoop should cause the timer to fire and dispatch
       // queued browser-initiated requests.
       0, true},
      {60, false},
  };

  for (const auto& test : tests) {
    base::test::ScopedFeatureList scoped_feature_list;
    base::HistogramTester histogram_tester;
    base::FieldTrialParams field_trial_params;
    field_trial_params["throttled_traffic_annotation_tags"] = "727528";
    field_trial_params
        ["seconds_to_pause_requests_after_end_of_p2p_connections"] =
            base::NumberToString(
                test.seconds_to_pause_requests_after_end_of_p2p_connections);
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kPauseBrowserInitiatedHeavyTrafficForP2P, field_trial_params);
    InitializeScheduler();

    network_quality_estimator_
        .SetAndNotifyObserversOfP2PActiveConnectionsCountChange(1u);
    network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
        net::EFFECTIVE_CONNECTION_TYPE_2G);

    std::string url = "http://host/browser-initiatited";

    net::NetworkTrafficAnnotationTag tag = net::DefineNetworkTrafficAnnotation(
        "metrics_report_uma",
        "Traffic annotation for unit, browser and other tests");
    // (COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH(""));
    std::unique_ptr<TestRequest> lows = (NewBrowserRequestWithAnnotationTag(
        url.c_str(), net::LOWEST, tag));  //"metrics_report_uma"));
    EXPECT_FALSE(lows->started());

    network_quality_estimator_
        .SetAndNotifyObserversOfP2PActiveConnectionsCountChange(2u);
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(lows->started());

    network_quality_estimator_
        .SetAndNotifyObserversOfP2PActiveConnectionsCountChange(0u);
    EXPECT_FALSE(lows->started());

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(test.expect_lows_started, lows->started());

    histogram_tester.ExpectTotalCount(
        "ResourceScheduler.BrowserInitiatedHeavyRequest.QueuingDuration",
        test.expect_lows_started ? 1u : 0u);
  }
}

// Verify that the previously queued browser requests are dispatched to the
// network when the network quality becomes faster.
TEST_F(ResourceSchedulerTest,
       RequestThrottleOnlyOnSlowConnectionsWithP2PRequests) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  base::FieldTrialParams field_trial_params;
  field_trial_params["throttled_traffic_annotation_tags"] = "727528";
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPauseBrowserInitiatedHeavyTrafficForP2P, field_trial_params);
  InitializeScheduler();

  network_quality_estimator_
      .SetAndNotifyObserversOfP2PActiveConnectionsCountChange(1u);
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  std::string url = "http://host/browser-initiatited";

  net::NetworkTrafficAnnotationTag tag = net::DefineNetworkTrafficAnnotation(
      "metrics_report_uma",
      "Traffic annotation for unit, browser and other tests");
  // (COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH(""));
  std::unique_ptr<TestRequest> lows = (NewBrowserRequestWithAnnotationTag(
      url.c_str(), net::LOWEST, tag));  //"metrics_report_uma"));
  EXPECT_FALSE(lows->started());

  network_quality_estimator_
      .SetAndNotifyObserversOfP2PActiveConnectionsCountChange(2u);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(lows->started());

  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_4G);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(lows->started());
  histogram_tester.ExpectTotalCount(
      "ResourceScheduler.BrowserInitiatedHeavyRequest.QueuingDuration", 1u);
}

TEST_F(ResourceSchedulerTest, ReprioritizedRequestGoesToBackOfQueue) {
  // Dummies to enforce scheduling.
  SetMaxDelayableRequests(1);
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::LOWEST));
  std::unique_ptr<TestRequest> idle(NewRequest("http://host/idle", net::IDLE));
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  const int kDefaultMaxNumDelayableRequestsPerClient = 0;
  std::vector<std::unique_ptr<TestRequest>> lows;
  for (int i = 0; i < kDefaultMaxNumDelayableRequestsPerClient; ++i) {
    string url = "http://host/low" + base::NumberToString(i);
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
  }

  SetMaxDelayableRequests(kDefaultMaxNumDelayableRequestsPerClient);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  ChangeRequestPriority(request.get(), net::LOWEST);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request->started());
  EXPECT_FALSE(idle->started());
}

TEST_F(ResourceSchedulerTest, HigherIntraPriorityGoesToFrontOfQueue) {
  // Dummies to enforce scheduling.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  const int kDefaultMaxNumDelayableRequestsPerClient =
      10;  // Should match the .cc.
  std::vector<std::unique_ptr<TestRequest>> lows;
  for (int i = 0; i < kDefaultMaxNumDelayableRequestsPerClient; ++i) {
    string url = "http://host/low" + base::NumberToString(i);
    lows.push_back(NewRequest(url.c_str(), net::IDLE));
  }

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::IDLE));
  EXPECT_FALSE(request->started());

  ChangeRequestPriority(request.get(), net::IDLE, 1);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request->started());

  high.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, NonHTTPSchedulesImmediately) {
  // Dummies to enforce scheduling.
  SetMaxDelayableRequests(1);
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  std::unique_ptr<TestRequest> low2(
      NewRequest("http://host/low2", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("chrome-extension://req", net::LOWEST));
  EXPECT_TRUE(low->started());
  EXPECT_FALSE(low2->started());
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, SpdyProxySchedulesImmediately) {
  InitializeScheduler();
  SetMaxDelayableRequests(1);

  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::IDLE));
  EXPECT_FALSE(request->started());
}

TEST_F(ResourceSchedulerTest, NewSpdyHostInDelayableRequests) {
  base::test::ScopedFeatureList scoped_feature_list;
  InitializeScheduler();

  const int kDefaultMaxNumDelayableRequestsPerClient =
      10;  // Should match the .cc.

  std::unique_ptr<TestRequest> low1_spdy(
      NewRequest("http://spdyhost1:8080/low", net::LOWEST));
  // Cancel a request after we learn the server supports SPDY.
  std::vector<std::unique_ptr<TestRequest>> lows;
  for (int i = 0; i < kDefaultMaxNumDelayableRequestsPerClient - 1; ++i) {
    string url = "http://host" + base::NumberToString(i) + "/low";
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
  }
  std::unique_ptr<TestRequest> low1(NewRequest("http://host/low", net::LOWEST));
  EXPECT_FALSE(low1->started());
  context_->http_server_properties()->SetSupportsSpdy(
      url::SchemeHostPort("http", "spdyhost1", 8080),
      net::NetworkIsolationKey(), true);
  low1_spdy.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low1->started());

  low1.reset();
  base::RunLoop().RunUntilIdle();
  std::unique_ptr<TestRequest> low2_spdy(
      NewRequest("http://spdyhost2:8080/low", net::IDLE));
  // Reprioritize a request after we learn the server supports SPDY.
  EXPECT_TRUE(low2_spdy->started());
  context_->http_server_properties()->SetSupportsSpdy(
      url::SchemeHostPort("http", "spdyhost2", 8080),
      net::NetworkIsolationKey(), true);
  ChangeRequestPriority(low2_spdy.get(), net::LOWEST);
  base::RunLoop().RunUntilIdle();
  std::unique_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(low2->started());
}

// Similar to NewSpdyHostInDelayableRequests test above, but tests the behavior
// when |delay_requests_on_multiplexed_connections| is true.
TEST_F(ResourceSchedulerTest,
       NewDelayableSpdyHostInDelayableRequestsSlowConnection) {
  ConfigureDelayRequestsOnMultiplexedConnectionsFieldTrial();
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_2G);
  InitializeScheduler();

  // Maximum number of delayable requests allowed when effective connection type
  // is 2G.
  const int max_delayable_requests_per_client_ect_2g = 8;

  std::unique_ptr<TestRequest> low1_spdy(
      NewRequest("http://spdyhost1:8080/low", net::LOWEST));
  EXPECT_TRUE(low1_spdy->started());
  // Cancel a request after we learn the server supports SPDY.
  std::vector<std::unique_ptr<TestRequest>> lows;
  for (int i = 0; i < max_delayable_requests_per_client_ect_2g - 1; ++i) {
    string url = "http://host" + base::NumberToString(i) + "/low";
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(lows.back()->started());
  }
  std::unique_ptr<TestRequest> low1(NewRequest("http://host/low", net::LOWEST));
  EXPECT_FALSE(low1->started());
  context_->http_server_properties()->SetSupportsSpdy(
      url::SchemeHostPort("http", "spdyhost1", 8080),
      net::NetworkIsolationKey(), true);
  low1_spdy.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low1->started());

  low1.reset();
  base::RunLoop().RunUntilIdle();
  std::unique_ptr<TestRequest> low2_spdy(
      NewRequest("http://spdyhost2:8080/low", net::IDLE));
  // Reprioritize a request after we learn the server supports SPDY.
  EXPECT_TRUE(low2_spdy->started());
  context_->http_server_properties()->SetSupportsSpdy(
      url::SchemeHostPort("http", "spdyhost2", 8080),
      net::NetworkIsolationKey(), true);
  ChangeRequestPriority(low2_spdy.get(), net::LOWEST);
  base::RunLoop().RunUntilIdle();
  std::unique_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_FALSE(low2->started());

  // SPDY requests are not started either.
  std::unique_ptr<TestRequest> low3_spdy(
      NewRequest("http://spdyhost1:8080/low", net::LOWEST));
  EXPECT_FALSE(low3_spdy->started());
}

// Async revalidations which are not started when the tab is closed must be
// started at some point, or they will hang around forever and prevent other
// async revalidations to the same URL from being issued.
TEST_F(ResourceSchedulerTest, RequestStartedAfterClientDeleted) {
  SetMaxDelayableRequests(1);
  scheduler_->OnClientCreated(kChildId2, kRouteId2,
                              &network_quality_estimator_);
  std::unique_ptr<TestRequest> high(NewRequestWithChildAndRoute(
      "http://host/high", net::HIGHEST, kChildId2, kRouteId2));
  std::unique_ptr<TestRequest> lowest1(NewRequestWithChildAndRoute(
      "http://host/lowest", net::LOWEST, kChildId2, kRouteId2));
  std::unique_ptr<TestRequest> lowest2(NewRequestWithChildAndRoute(
      "http://host/lowest", net::LOWEST, kChildId2, kRouteId2));
  EXPECT_FALSE(lowest2->started());

  scheduler_->OnClientDeleted(kChildId2, kRouteId2);
  high.reset();
  lowest1.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(lowest2->started());
}

// The ResourceScheduler::Client destructor calls
// LoadAnyStartablePendingRequests(), which may start some pending requests.
// This test is to verify that requests will be started at some point
// even if they were not started by the destructor.
TEST_F(ResourceSchedulerTest, RequestStartedAfterClientDeletedManyDelayable) {
  scheduler_->OnClientCreated(kChildId2, kRouteId2,
                              &network_quality_estimator_);
  std::unique_ptr<TestRequest> high(NewRequestWithChildAndRoute(
      "http://host/high", net::HIGHEST, kChildId2, kRouteId2));
  const int kDefaultMaxNumDelayableRequestsPerClient = 10;
  std::vector<std::unique_ptr<TestRequest>> delayable_requests;
  for (int i = 0; i < kDefaultMaxNumDelayableRequestsPerClient + 1; ++i) {
    delayable_requests.push_back(NewRequestWithChildAndRoute(
        "http://host/lowest", net::LOWEST, kChildId2, kRouteId2));
  }
  std::unique_ptr<TestRequest> lowest(NewRequestWithChildAndRoute(
      "http://host/lowest", net::LOWEST, kChildId2, kRouteId2));
  EXPECT_FALSE(lowest->started());

  scheduler_->OnClientDeleted(kChildId2, kRouteId2);
  high.reset();
  delayable_requests.clear();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(lowest->started());
}

// Tests that the maximum number of delayable requests is overridden when the
// experiment is enabled.
TEST_F(ResourceSchedulerTest, RequestLimitOverrideEnabled) {
  RequestLimitOverrideConfigTestHelper(true);
}

// Tests that the maximum number of delayable requests is not overridden when
// the experiment is disabled.
TEST_F(ResourceSchedulerTest, RequestLimitOverrideDisabled) {
  RequestLimitOverrideConfigTestHelper(false);
}

// Test that the limit is not overridden when the effective connection type is
// not equal to any of the values provided in the experiment configuration.
TEST_F(ResourceSchedulerTest, RequestLimitOverrideOutsideECTRange) {
  base::test::ScopedFeatureList scoped_feature_list;
  InitializeThrottleDelayableExperiment(true, 0.0);
  InitializeScheduler();
  for (net::EffectiveConnectionType ect :
       {net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
        net::EFFECTIVE_CONNECTION_TYPE_OFFLINE,
        net::EFFECTIVE_CONNECTION_TYPE_4G}) {
    // Set the effective connection type to a value for which the experiment
    // should not be run.
    network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
        ect);

    // Throw in one high priority request to ensure that high priority requests
    // do not depend on anything.
    std::unique_ptr<TestRequest> high(
        NewRequest("http://host/high", net::HIGHEST));
    EXPECT_TRUE(high->started());

    // Should be in sync with resource_scheduler.cc.
    const int kDefaultMaxNumDelayableRequestsPerClient = 10;

    std::vector<std::unique_ptr<TestRequest>> lows_singlehost;
    // Queue up to the maximum limit. Use different host names to prevent the
    // per host limit from kicking in.
    for (int i = 0; i < kDefaultMaxNumDelayableRequestsPerClient; ++i) {
      // Keep unique hostnames to prevent the per host limit from kicking in.
      std::string url = "http://host" + base::NumberToString(i) + "/low";
      lows_singlehost.push_back(NewRequest(url.c_str(), net::LOWEST));
      EXPECT_TRUE(lows_singlehost[i]->started());
    }

    std::unique_ptr<TestRequest> last_singlehost(
        NewRequest("http://host/last", net::LOWEST));

    // Last should not start because the maximum requests that can be in-flight
    // have already started.
    EXPECT_FALSE(last_singlehost->started());
  }
}

// Test that a change in network conditions midway during loading
// changes the behavior of the resource scheduler.
TEST_F(ResourceSchedulerTest, RequestLimitOverrideNotFixedForPageLoad) {
  InitializeThrottleDelayableExperiment(true, 0.0);
  // ECT value is in range for which the limit is overridden to 2.
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  InitializeScheduler();

  // Throw in one high priority request to ensure that high priority requests
  // do not depend on anything.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  EXPECT_TRUE(high->started());

  // Should be based on the value set by
  // |InitializeThrottleDelayableExperiment| for the given range.
  const int kOverriddenNumRequests = 2;

  std::vector<std::unique_ptr<TestRequest>> lows_singlehost;
  // Queue up to the overridden limit.
  for (int i = 0; i < kOverriddenNumRequests; ++i) {
    // Keep unique hostnames to prevent the per host limit from kicking in.
    std::string url = "http://host" + base::NumberToString(i) + "/low";
    lows_singlehost.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(lows_singlehost[i]->started());
  }

  std::unique_ptr<TestRequest> second_last_singlehost(
      NewRequest("http://host/slast", net::LOWEST));

  // This new request should not start because the limit has been reached.
  EXPECT_FALSE(second_last_singlehost->started());
  lows_singlehost.erase(lows_singlehost.begin());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(second_last_singlehost->started());

  //  |last_singlehost_before_ect_change| should not start because of the
  //  limits.
  std::unique_ptr<TestRequest> last_singlehost_before_ect_change(
      NewRequest("http://host/last_singlehost_before_ect_change", net::LOWEST));
  EXPECT_FALSE(last_singlehost_before_ect_change->started());

  // Change the ECT to go outside the experiment buckets and change the network
  // type to 4G. This should affect the limit which should affect requests
  // that are already queued (e.g., |last_singlehost_before_ect_change|), and
  // requests that arrive later (e.g., |last_singlehost_after_ect_change|).
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_4G);
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<TestRequest> last_singlehost_after_ect_change(
      NewRequest("http://host/last_singlehost_after_ect_change", net::LOWEST));

  // Both requests should start because the limits should have changed.
  EXPECT_TRUE(last_singlehost_before_ect_change->started());
  EXPECT_TRUE(last_singlehost_after_ect_change->started());
}

// Test that when the network quality changes such that the new limit is lower,
// the new delayable requests don't start until the number of requests in
// flight have gone below the new limit.
TEST_F(ResourceSchedulerTest, RequestLimitReducedAcrossPageLoads) {
  InitializeThrottleDelayableExperiment(true, 0.0);
  // ECT value is in range for which the limit is overridden to 4.
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_3G);
  InitializeScheduler();

  // Throw in one high priority request to ensure that high priority requests
  // do not depend on anything.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  EXPECT_TRUE(high->started());

  // The number of delayable requests allowed for the first page load.
  const int kNumDelayableHigh = 4;
  // The number of delayable requests allowed for the second page load.
  const int kNumDelayableLow = 2;

  std::vector<std::unique_ptr<TestRequest>> delayable_first_page;
  // Queue up to the overridden limit.
  for (int i = 0; i < kNumDelayableHigh; ++i) {
    // Keep unique hostnames to prevent the per host limit from kicking in.
    std::string url = "http://host" + base::NumberToString(i) + "/low1";
    delayable_first_page.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(delayable_first_page[i]->started());
  }
  // Change the network quality so that the ECT value is in range for which the
  // limit is overridden to 2. The effective connection type is set to
  // Slow-2G.
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  // Ensure that high priority requests still start.
  std::unique_ptr<TestRequest> high2(
      NewRequest("http://host/high2", net::HIGHEST));
  EXPECT_TRUE(high->started());

  // Generate requests from second page. None of them should start because the
  // new limit is |kNumDelayableLow| and there are already |kNumDelayableHigh|
  // requests in flight.
  std::vector<std::unique_ptr<TestRequest>> delayable_second_page;
  for (int i = 0; i < kNumDelayableLow; ++i) {
    // Keep unique hostnames to prevent the per host limit from kicking in.
    std::string url = "http://host" + base::NumberToString(i) + "/low2";
    delayable_second_page.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_FALSE(delayable_second_page[i]->started());
  }

  // Finish 2 requests from first page load.
  for (int i = 0; i < kNumDelayableHigh - kNumDelayableLow; ++i) {
    delayable_first_page.pop_back();
  }
  base::RunLoop().RunUntilIdle();

  // Nothing should start because there are already |kNumDelayableLow| requests
  // in flight.
  for (int i = 0; i < kNumDelayableLow; ++i) {
    EXPECT_FALSE(delayable_second_page[i]->started());
  }

  // Remove all requests from the first page.
  delayable_first_page.clear();
  base::RunLoop().RunUntilIdle();

  // Check that the requests from page 2 have started, since now there are 2
  // empty slots.
  for (int i = 0; i < kNumDelayableLow; ++i) {
    EXPECT_TRUE(delayable_second_page[i]->started());
  }

  // No new delayable request should start since there are already
  // |kNumDelayableLow| requests in flight.
  std::string url =
      "http://host" + base::NumberToString(kNumDelayableLow) + "/low3";
  delayable_second_page.push_back(NewRequest(url.c_str(), net::LOWEST));
  EXPECT_FALSE(delayable_second_page.back()->started());
}

TEST_F(ResourceSchedulerTest, ThrottleDelayableDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kThrottleDelayable);

  InitializeScheduler();
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_2G);
  // Insert one non-delayable request. This should not affect the number of
  // delayable requests started.
  std::unique_ptr<TestRequest> medium(
      NewRequest("http://host/medium", net::MEDIUM));
  ASSERT_TRUE(medium->started());
  // Start |kDefaultMaxNumDelayableRequestsPerClient| delayable requests and
  // verify that they all started.
  // When one high priority request is in flight, the number of low priority
  // requests allowed in flight are |max_delayable_requests| -
  // |non_delayable_weight|  = 8 - 3 = 5.
  std::vector<std::unique_ptr<TestRequest>> delayable_requests;
  for (int i = 0; i < 5; ++i) {
    delayable_requests.push_back(NewRequest(
        base::StringPrintf("http://host%d/low", i).c_str(), net::LOWEST));
    EXPECT_TRUE(delayable_requests.back()->started());
  }

  delayable_requests.push_back(
      NewRequest("http://host/low-blocked", net::LOWEST));
  EXPECT_FALSE(delayable_requests.back()->started());
}

// Test that the default limit is used for delayable requests when the
// experiment is enabled, but the current effective connection type is higher
// than the maximum effective connection type set in the experiment
// configuration.
TEST_F(ResourceSchedulerTest, NonDelayableThrottlesDelayableOutsideECT) {
  const double kNonDelayableWeight = 2.0;
  const int kDefaultMaxNumDelayableRequestsPerClient =
      10;  // Should be in sync with cc.
  // Initialize the experiment with |kNonDelayableWeight| as the weight of
  // non-delayable requests.
  InitializeThrottleDelayableExperiment(false, kNonDelayableWeight);
  // Experiment should not run when the effective connection type is faster
  // than 2G.
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_3G);

  InitializeScheduler();
  // Insert one non-delayable request. This should not affect the number of
  // delayable requests started.
  std::unique_ptr<TestRequest> medium(
      NewRequest("http://host/medium", net::MEDIUM));
  ASSERT_TRUE(medium->started());
  // Start |kDefaultMaxNumDelayableRequestsPerClient| delayable requests and
  // verify that they all started.
  std::vector<std::unique_ptr<TestRequest>> delayable_requests;
  for (int i = 0; i < kDefaultMaxNumDelayableRequestsPerClient; ++i) {
    delayable_requests.push_back(NewRequest(
        base::StringPrintf("http://host%d/low", i).c_str(), net::LOWEST));
    EXPECT_TRUE(delayable_requests.back()->started());
  }
}

// Test that delayable requests are throttled by the right amount as the number
// of non-delayable requests in-flight change.
TEST_F(ResourceSchedulerTest, NonDelayableThrottlesDelayableVaryNonDelayable) {
  const double kNonDelayableWeight = 2.0;
  const int kDefaultMaxNumDelayableRequestsPerClient =
      8;  // Should be in sync with cc.
  // Initialize the experiment with |kNonDelayableWeight| as the weight of
  // non-delayable requests.
  InitializeThrottleDelayableExperiment(false, kNonDelayableWeight);
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  InitializeScheduler();
  for (int num_non_delayable = 0; num_non_delayable < 10; ++num_non_delayable) {
    base::RunLoop().RunUntilIdle();
    // Start the non-delayable requests.
    std::vector<std::unique_ptr<TestRequest>> non_delayable_requests;
    for (int i = 0; i < num_non_delayable; ++i) {
      non_delayable_requests.push_back(NewRequest(
          base::StringPrintf("http://host%d/medium", i).c_str(), net::MEDIUM));
      ASSERT_TRUE(non_delayable_requests.back()->started());
    }
    // Start |kDefaultMaxNumDelayableRequestsPerClient| - |num_non_delayable| *
    // |kNonDelayableWeight| delayable requests. They should all start.
    std::vector<std::unique_ptr<TestRequest>> delayable_requests;
    for (int i = 0; i < kDefaultMaxNumDelayableRequestsPerClient -
                            num_non_delayable * kNonDelayableWeight;
         ++i) {
      delayable_requests.push_back(NewRequest(
          base::StringPrintf("http://host%d/low", i).c_str(), net::LOWEST));
      EXPECT_TRUE(delayable_requests.back()->started());
    }
    // The next delayable request should not start.
    std::unique_ptr<TestRequest> last_low(
        NewRequest("http://lasthost/low", net::LOWEST));
    EXPECT_FALSE(last_low->started());
  }
}

// Test that UMA counts are correctly recorded for the number of active resource
// scheduler clients.
TEST_F(ResourceSchedulerTest, NumActiveResourceSchedulerClientsUMA) {
  std::unique_ptr<base::HistogramTester> histogram_tester(
      new base::HistogramTester);
  // Check that 0 is recorded when a new client is created and there are no
  // active scheduler clients in the background.
  scheduler_->OnClientCreated(kChildId2, kRouteId2,
                              &network_quality_estimator_);
  histogram_tester->ExpectTotalCount(
      "ResourceScheduler.ActiveSchedulerClientsCount", 1);
  histogram_tester->ExpectUniqueSample(
      "ResourceScheduler.ActiveSchedulerClientsCount", 0, 1);

  // Test that UMA data remains the same even when a new request starts and a
  // scheduler client becomes active.
  std::unique_ptr<TestRequest> high1(
      NewRequest("http://host/high", net::HIGHEST));
  EXPECT_TRUE(high1->started());
  histogram_tester->ExpectUniqueSample(
      "ResourceScheduler.ActiveSchedulerClientsCount", 0, 1);

  // Test that UMA data is recorded when a new client starts. Check that the
  // total number of samples is 2. Also, check that 1 active resource scheduler
  // client is recorded.
  scheduler_->OnClientCreated(kChildId3, kRouteId3,
                              &network_quality_estimator_);
  histogram_tester->ExpectTotalCount(
      "ResourceScheduler.ActiveSchedulerClientsCount", 2);
  histogram_tester->ExpectBucketCount(
      "ResourceScheduler.ActiveSchedulerClientsCount", 1, 1);
  scheduler_->OnClientDeleted(kChildId3, kRouteId3);
  scheduler_->OnClientDeleted(kChildId2, kRouteId2);
  histogram_tester.reset(new base::HistogramTester);

  // Test that UMA counts are recorded correctly when multiple scheduler clients
  // are created in sequence. There are at most 20 active clients.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (size_t i = 0; i < kNumResourceSchedulerClients; ++i) {
    scheduler_->OnClientCreated(kChildId3 + i, kRouteId3 + i,
                                &network_quality_estimator_);
    requests.push_back(NewRequestWithChildAndRoute(
        "http://host/medium", net::LOWEST, kChildId3 + i, kRouteId3 + i));
    EXPECT_TRUE(requests[i]->started());
    histogram_tester->ExpectTotalCount(
        "ResourceScheduler.ActiveSchedulerClientsCount", 1 + i);
    histogram_tester->ExpectBucketCount(
        "ResourceScheduler.ActiveSchedulerClientsCount", 1 + i, 1);
  }
  histogram_tester.reset(new base::HistogramTester);

  // Test that UMA counts are recorded correctly when a sequence of resource
  // scheduler clients are deleted in sequence. Note: Create a new client
  // each time in order to update the UMA counts.
  for (size_t i = 0; i < kNumResourceSchedulerClients; ++i) {
    scheduler_->OnClientDeleted(kChildId3 + 19 - i, kRouteId3 + 19 - i);
    scheduler_->OnClientCreated(kChildId2, kRouteId2,
                                &network_quality_estimator_);
    histogram_tester->ExpectBucketCount(
        "ResourceScheduler.ActiveSchedulerClientsCount", 20 - i, 1);
    scheduler_->OnClientDeleted(kChildId2, kRouteId2);
  }
}

// Test that each non-delayable request in-flight results in the reduction of
// one in the limit of delayable requests in-flight when the non-delayable
// request weight is 1.
TEST_F(ResourceSchedulerTest, NonDelayableThrottlesDelayableWeight1) {
  NonDelayableThrottlesDelayableHelper(1.0);
}

// Test that each non-delayable request in-flight results in the reduction of
// three in the limit of delayable requests in-flight when the non-delayable
// request weight is 3.
TEST_F(ResourceSchedulerTest, NonDelayableThrottlesDelayableWeight3) {
  NonDelayableThrottlesDelayableHelper(3.0);
}

// Test that UMA counts are recorded for the number of delayable requests
// in-flight when a non-delayable request starts.
TEST_F(ResourceSchedulerTest, NumDelayableAtStartOfNonDelayableUMA) {
  std::unique_ptr<base::HistogramTester> histogram_tester(
      new base::HistogramTester);
  // Check that 0 is recorded when a non-delayable request starts and there are
  // no delayable requests in-flight.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  EXPECT_TRUE(high->started());
  histogram_tester->ExpectUniqueSample(
      "ResourceScheduler.NumDelayableRequestsInFlightAtStart.NonDelayable", 0,
      1);
  histogram_tester.reset(new base::HistogramTester);
  // Check that nothing is recorded when delayable request is started in the
  // presence of a non-delayable request.
  std::unique_ptr<TestRequest> low1(
      NewRequest("http://host/low1", net::LOWEST));
  EXPECT_TRUE(low1->started());
  histogram_tester->ExpectTotalCount(
      "ResourceScheduler.NumDelayableRequestsInFlightAtStart.NonDelayable", 0);
  // Check that nothing is recorded when a delayable request is started in the
  // presence of another delayable request.
  std::unique_ptr<TestRequest> low2(
      NewRequest("http://host/low2", net::LOWEST));
  histogram_tester->ExpectTotalCount(
      "ResourceScheduler.NumDelayableRequestsInFlightAtStart.NonDelayable", 0);
  // Check that UMA is recorded when a non-delayable startes in the presence of
  // delayable requests and that the correct value is recorded.
  std::unique_ptr<TestRequest> high2(
      NewRequest("http://host/high2", net::HIGHEST));
  histogram_tester->ExpectUniqueSample(
      "ResourceScheduler.NumDelayableRequestsInFlightAtStart.NonDelayable", 2,
      1);
}

TEST_F(ResourceSchedulerTest, Simple) {
  SetMaxDelayableRequests(1);
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/req", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::LOWEST));

  EXPECT_FALSE(request->started());
}

TEST_F(ResourceSchedulerTest, MultipleInstances_1) {
  SetMaxDelayableRequests(1);
  // In some circumstances there may exist multiple instances.
  ResourceScheduler another_scheduler(base::DefaultTickClock::GetInstance());
  another_scheduler.SetResourceSchedulerParamsManagerForTests(
      ResourceSchedulerParamsManager(FixedParamsManager(99)));

  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/req", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::LOWEST));

  // This request should be throttled as it's handled by |scheduler_|.
  EXPECT_FALSE(request->started());
}

TEST_F(ResourceSchedulerTest, MultipleInstances_2) {
  SetMaxDelayableRequests(1);
  ResourceScheduler another_scheduler(base::DefaultTickClock::GetInstance());
  another_scheduler.OnClientCreated(kChildId, kRouteId,
                                    &network_quality_estimator_);

  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/req", net::LOWEST));

  std::unique_ptr<TestRequest> request(NewRequestWithChildAndRoute(
      "http://host/req", net::LOWEST, kChildId, kRouteId));

  EXPECT_FALSE(request->started());

  {
    another_scheduler.SetResourceSchedulerParamsManagerForTests(
        FixedParamsManager(1));
    std::unique_ptr<net::URLRequest> url_request(NewURLRequestWithChildAndRoute(
        "http://host/another", net::LOWEST, TRAFFIC_ANNOTATION_FOR_TESTS,
        kChildId, kRouteId));
    auto scheduled_request = another_scheduler.ScheduleRequest(
        kChildId, kRouteId, true, url_request.get());
    auto another_request = std::make_unique<TestRequest>(
        std::move(url_request), std::move(scheduled_request),
        &another_scheduler);
    another_request->Start();

    // This should not be throttled as it's handled by |another_scheduler|.
    EXPECT_TRUE(another_request->started());
  }

  another_scheduler.OnClientDeleted(kChildId, kRouteId);
}

// Verify that when |delay_requests_on_multiplexed_connections| is true, spdy
// hosts are not subject to kMaxNumDelayableRequestsPerHostPerClient limit, but
// are still subject to kDefaultMaxNumDelayableRequestsPerClient limit.
TEST_F(ResourceSchedulerTest,
       MaxRequestsPerHostForSpdyWhenDelayableSlowConnections) {
  ConfigureDelayRequestsOnMultiplexedConnectionsFieldTrial();
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  InitializeScheduler();
  context_->http_server_properties()->SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), net::NetworkIsolationKey(),
      true);

  // Should be in sync with resource_scheduler.cc for effective connection type
  // of 2G.
  const size_t kDefaultMaxNumDelayableRequestsPerClient = 8;

  ASSERT_LT(kMaxNumDelayableRequestsPerHostPerClient,
            kDefaultMaxNumDelayableRequestsPerClient);

  // Add more than kMaxNumDelayableRequestsPerHostPerClient low-priority
  // requests. They should all be allowed.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (size_t i = 0; i < kMaxNumDelayableRequestsPerHostPerClient + 1; ++i) {
    requests.push_back(NewRequest("https://spdyhost/low", net::LOWEST));
    EXPECT_TRUE(requests[i]->started());
  }

  // Requests to SPDY servers should not be subject to
  // kMaxNumDelayableRequestsPerHostPerClient limit. They should only be subject
  // to kDefaultMaxNumDelayableRequestsPerClient limit.
  for (size_t i = kMaxNumDelayableRequestsPerHostPerClient + 1;
       i < kDefaultMaxNumDelayableRequestsPerClient + 1; i++) {
    EXPECT_EQ(i, requests.size());
    requests.push_back(NewRequest("https://spdyhost/low", net::LOWEST));
    EXPECT_EQ(i < kDefaultMaxNumDelayableRequestsPerClient,
              requests[i]->started());
  }
}

TEST_F(ResourceSchedulerTest,
       MaxRequestsPerHostForSpdyWhenDelayableSlowConnectionsWithIsolationInfo) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://foo.test/"));
  const net::IsolationInfo kIsolationInfo1 =
      net::IsolationInfo::CreateForInternalRequest(kOrigin1);
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://bar.test/"));
  const net::IsolationInfo kIsolationInfo2 =
      net::IsolationInfo::CreateForInternalRequest(kOrigin2);

  ConfigureDelayRequestsOnMultiplexedConnectionsFieldTrial();
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  InitializeScheduler();
  context_->http_server_properties()->SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443),
      kIsolationInfo1.network_isolation_key(), true);

  // Should be in sync with resource_scheduler.cc for effective connection type
  // of 2G.
  const size_t kDefaultMaxNumDelayableRequestsPerClient = 8;

  ASSERT_LT(kMaxNumDelayableRequestsPerHostPerClient,
            kDefaultMaxNumDelayableRequestsPerClient);

  // Add more than kMaxNumDelayableRequestsPerHostPerClient low-priority
  // requests. They should all be allowed.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (size_t i = 0; i < kMaxNumDelayableRequestsPerHostPerClient + 1; ++i) {
    requests.push_back(NewRequestWithIsolationInfo(
        "https://spdyhost/low", net::LOWEST, kIsolationInfo1));
    EXPECT_TRUE(requests[i]->started());
  }

  // Requests to SPDY servers should not be subject to
  // kMaxNumDelayableRequestsPerHostPerClient limit. They should only be subject
  // to kDefaultMaxNumDelayableRequestsPerClient limit.
  for (size_t i = kMaxNumDelayableRequestsPerHostPerClient + 1;
       i < kDefaultMaxNumDelayableRequestsPerClient + 1; i++) {
    EXPECT_EQ(i, requests.size());
    requests.push_back(NewRequestWithIsolationInfo(
        "https://spdyhost/low", net::LOWEST, kIsolationInfo1));
    EXPECT_EQ(i < kDefaultMaxNumDelayableRequestsPerClient,
              requests[i]->started());
  }
  requests.clear();

  // Requests with other IsolationInfos are subject to the
  // kMaxNumDelayableRequestsPerHostPerClient limit.
  for (size_t i = 0; i < kMaxNumDelayableRequestsPerHostPerClient + 1; ++i) {
    requests.push_back(NewRequestWithIsolationInfo(
        "https://spdyhost/low", net::LOWEST, net::IsolationInfo()));
    EXPECT_EQ(i < kMaxNumDelayableRequestsPerHostPerClient,
              requests[i]->started());
  }
  requests.clear();

  for (size_t i = 0; i < kMaxNumDelayableRequestsPerHostPerClient + 1; ++i) {
    requests.push_back(NewRequestWithIsolationInfo(
        "https://spdyhost/low", net::LOWEST, kIsolationInfo2));
    EXPECT_EQ(i < kMaxNumDelayableRequestsPerHostPerClient,
              requests[i]->started());
  }
}

// Verify that when |delay_requests_on_multiplexed_connections| is false, spdy
// hosts are not subject to kMaxNumDelayableRequestsPerHostPerClient or
// kDefaultMaxNumDelayableRequestsPerClient limits.
TEST_F(ResourceSchedulerTest,
       MaxRequestsPerHostForSpdyWhenDelayableFastConnections) {
  ConfigureDelayRequestsOnMultiplexedConnectionsFieldTrial();
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_4G);

  InitializeScheduler();
  context_->http_server_properties()->SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), net::NetworkIsolationKey(),
      true);

  // Should be in sync with resource_scheduler.cc for effective connection type
  // of 4G.
  const size_t kDefaultMaxNumDelayableRequestsPerClient = 10;

  ASSERT_LT(kMaxNumDelayableRequestsPerHostPerClient,
            kDefaultMaxNumDelayableRequestsPerClient);

  // Add more than kDefaultMaxNumDelayableRequestsPerClient low-priority
  // requests. They should all be allowed.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (size_t i = 0; i < kDefaultMaxNumDelayableRequestsPerClient + 1; ++i) {
    requests.push_back(NewRequest("https://spdyhost/low", net::LOWEST));
    EXPECT_TRUE(requests[i]->started());
  }
}

// Verify that when |delay_requests_on_multiplexed_connections| is true,
// non-spdy hosts are still subject to kMaxNumDelayableRequestsPerHostPerClient
// limit.
TEST_F(ResourceSchedulerTest,
       MaxRequestsPerHostForNonSpdyWhenDelayableSlowConnections) {
  ConfigureDelayRequestsOnMultiplexedConnectionsFieldTrial();
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  InitializeScheduler();

  // Add more than kMaxNumDelayableRequestsPerHostPerClient delayable requests.
  // They should not all be allowed.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (size_t i = 0; i < kMaxNumDelayableRequestsPerHostPerClient + 1; ++i)
    requests.push_back(NewRequest("https://non_spdyhost/low", net::LOWEST));

  // kMaxNumDelayableRequestsPerHostPerClient should apply for non-spdy host.
  for (size_t i = 0; i < requests.size(); ++i) {
    EXPECT_EQ(i < kMaxNumDelayableRequestsPerHostPerClient,
              requests[i]->started());
  }
}

// Verify that when |delay_requests_on_multiplexed_connections| is true,
// non-spdy requests are still subject to
// kDefaultMaxNumDelayableRequestsPerClient limit.
TEST_F(ResourceSchedulerTest,
       DelayableRequestLimitSpdyDelayableSlowConnections) {
  ConfigureDelayRequestsOnMultiplexedConnectionsFieldTrial();
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  InitializeScheduler();

  // Throw in one high priority request to ensure that high priority requests
  // do not depend on anything.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  EXPECT_TRUE(high->started());

  // Should be in sync with resource_scheduler.cc for effective connection type
  // (ECT) 2G. For ECT of 2G, number of low priority requests allowed are:
  // 8 - 3 * count of high priority requests in flight. That expression computes
  // to 8 - 3 * 1  = 5.
  const int max_low_priority_requests_allowed = 5;

  std::vector<std::unique_ptr<TestRequest>> lows_singlehost;
  // Queue up to the maximum limit. Use different host names to prevent the
  // per host limit from kicking in.
  for (int i = 0; i < max_low_priority_requests_allowed; ++i) {
    // Keep unique hostnames to prevent the per host limit from kicking in.
    std::string url = "http://host" + base::NumberToString(i) + "/low";
    lows_singlehost.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(lows_singlehost[i]->started()) << i;
  }

  std::unique_ptr<TestRequest> last_singlehost(
      NewRequest("http://host/last", net::LOWEST));

  // Last should not start because the maximum requests that can be in-flight
  // have already started.
  EXPECT_FALSE(last_singlehost->started());
}

// Verify that when |max_queuing_time| is set, requests queued for too long
// duration are dispatched to the network.
TEST_F(ResourceSchedulerTest, MaxQueuingDelaySet) {
  base::HistogramTester histogram_tester;
  base::TimeDelta max_queuing_time = base::TimeDelta::FromSeconds(15);
  InitializeMaxQueuingDelayExperiment(max_queuing_time);
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  InitializeScheduler();

  // Throw in one high priority request to ensure that high priority requests
  // do not depend on anything.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  EXPECT_TRUE(high->started());

  // Should be in sync with resource_scheduler.cc for effective connection type
  // (ECT) 2G. For ECT of 2G, number of low priority requests allowed are:
  // 8 - 3 * count of high priority requests in flight. That expression computes
  // to 8 - 3 * 1  = 5.
  const int max_low_priority_requests_allowed = 5;

  std::vector<std::unique_ptr<TestRequest>> lows_singlehost;
  // Queue up to the maximum limit. Use different host names to prevent the
  // per host limit from kicking in.
  for (int i = 0; i < max_low_priority_requests_allowed + 10; ++i) {
    // Keep unique hostnames to prevent the per host limit from kicking in.
    std::string url = "http://host" + base::NumberToString(i) + "/low";
    lows_singlehost.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_EQ(i < max_low_priority_requests_allowed,
              lows_singlehost[i]->started());
  }

  // Advance the clock by more than |max_queuing_time|.
  tick_clock_.SetNowTicks(base::DefaultTickClock::GetInstance()->NowTicks() +
                          max_queuing_time + base::TimeDelta::FromSeconds(1));

  // Since the requests have been queued for too long, they should now be
  // dispatched. Trigger the calculation of queuing time by Triggering the
  // finish of a single request.
  lows_singlehost[0].reset();
  base::RunLoop().RunUntilIdle();

  for (int i = 1; i < max_low_priority_requests_allowed + 10; ++i) {
    EXPECT_TRUE(lows_singlehost[i]->started());
  }

  histogram_tester.ExpectUniqueSample(
      "ResourceScheduler.DelayableRequests."
      "WaitTimeToAvoidContentionWithNonDelayableRequest",
      0, 1);

  // Delete the requests. This should trigger the end of the requests which in
  // turn would trigger recording of the metrics.
  for (int i = 1; i < max_low_priority_requests_allowed + 10; ++i)
    lows_singlehost[i].reset();

  // No non-delayable request started after the start of the delayable request.
  // Metric should be recorded as 0 milliseconds.
  histogram_tester.ExpectUniqueSample(
      "ResourceScheduler.DelayableRequests."
      "WaitTimeToAvoidContentionWithNonDelayableRequest",
      0, max_low_priority_requests_allowed + 10);
}

// Verify that when |max_queuing_time| is not set, requests queued for too long
// duration are not dispatched to the network.
TEST_F(ResourceSchedulerTest, MaxQueuingDelayNotSet) {
  base::TimeDelta max_queuing_time = base::TimeDelta::FromSeconds(15);
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  InitializeScheduler();

  // Throw in one high priority request to ensure that high priority requests
  // do not depend on anything.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  EXPECT_TRUE(high->started());

  // Should be in sync with resource_scheduler.cc for effective connection type
  // (ECT) 2G. For ECT of 2G, number of low priority requests allowed are:
  // 8 - 3 * count of high priority requests in flight. That expression computes
  // to 8 - 3 * 1  = 5.
  const int max_low_priority_requests_allowed = 5;

  std::vector<std::unique_ptr<TestRequest>> lows_singlehost;
  // Queue up to the maximum limit. Use different host names to prevent the
  // per host limit from kicking in.
  for (int i = 0; i < max_low_priority_requests_allowed + 10; ++i) {
    // Keep unique hostnames to prevent the per host limit from kicking in.
    std::string url = "http://host" + base::NumberToString(i) + "/low";
    lows_singlehost.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_EQ(i < max_low_priority_requests_allowed,
              lows_singlehost[i]->started());
  }

  // Advance the clock by more than |max_queuing_time|.
  tick_clock_.SetNowTicks(base::DefaultTickClock::GetInstance()->NowTicks() +
                          max_queuing_time + base::TimeDelta::FromSeconds(1));

  // Triggering the finish of a single request should not trigger dispatch of
  // requests that have been queued for too long.
  lows_singlehost[0].reset();
  base::RunLoop().RunUntilIdle();

  // Starting at i=1 since the request at index 0 has been deleted.
  for (int i = 1; i < max_low_priority_requests_allowed + 10; ++i) {
    EXPECT_EQ(i < max_low_priority_requests_allowed + 1,
              lows_singlehost[i]->started());
  }
}

// Verify that when the timer for dispatching long queued requests is fired,
// then the long queued requests are dispatched to the network.
TEST_F(ResourceSchedulerTest, MaxQueuingDelayTimerFires) {
  base::TimeDelta max_queuing_time = base::TimeDelta::FromSeconds(15);
  InitializeMaxQueuingDelayExperiment(max_queuing_time);
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  InitializeScheduler();

  // Throw in one high priority request to ensure that high priority requests
  // do not depend on anything.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  EXPECT_TRUE(high->started());

  // Should be in sync with resource_scheduler.cc for effective connection type
  // (ECT) 2G. For ECT of 2G, number of low priority requests allowed are:
  // 8 - 3 * count of high priority requests in flight. That expression computes
  // to 8 - 3 * 1  = 5.
  const int max_low_priority_requests_allowed = 5;

  std::vector<std::unique_ptr<TestRequest>> lows_singlehost;
  // Queue up to the maximum limit. Use different host names to prevent the
  // per host limit from kicking in.
  for (int i = 0; i < max_low_priority_requests_allowed + 10; ++i) {
    // Keep unique hostnames to prevent the per host limit from kicking in.
    std::string url = "http://host" + base::NumberToString(i) + "/low";
    lows_singlehost.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_EQ(i < max_low_priority_requests_allowed,
              lows_singlehost[i]->started());
  }

  // Advance the clock by more than |max_queuing_time|.
  tick_clock_.SetNowTicks(base::DefaultTickClock::GetInstance()->NowTicks() +
                          max_queuing_time + base::TimeDelta::FromSeconds(1));

  // Since the requests have been queued for too long, they should now be
  // dispatched. Trigger the calculation of queuing time by calling
  // DispatchLongQueuedRequestsForTesting().
  scheduler()->DispatchLongQueuedRequestsForTesting();
  base::RunLoop().RunUntilIdle();

  for (int i = 0; i < max_low_priority_requests_allowed + 10; ++i) {
    EXPECT_TRUE(lows_singlehost[i]->started());
  }
}

// Verify that when the timer for dispatching long queued requests is not fired,
// then the long queued requests are not dispatched to the network.
TEST_F(ResourceSchedulerTest, MaxQueuingDelayTimerNotFired) {
  base::TimeDelta max_queuing_time = base::TimeDelta::FromSeconds(15);
  InitializeMaxQueuingDelayExperiment(max_queuing_time);
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  InitializeScheduler();

  // Throw in one high priority request to ensure that high priority requests
  // do not depend on anything.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  EXPECT_TRUE(high->started());

  // Should be in sync with resource_scheduler.cc for effective connection type
  // (ECT) 2G. For ECT of 2G, number of low priority requests allowed are:
  // 8 - 3 * count of high priority requests in flight. That expression computes
  // to 8 - 3 * 1  = 5.
  const int max_low_priority_requests_allowed = 5;

  std::vector<std::unique_ptr<TestRequest>> lows_singlehost;
  // Queue up to the maximum limit. Use different host names to prevent the
  // per host limit from kicking in.
  for (int i = 0; i < max_low_priority_requests_allowed + 10; ++i) {
    // Keep unique hostnames to prevent the per host limit from kicking in.
    std::string url = "http://host" + base::NumberToString(i) + "/low";
    lows_singlehost.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_EQ(i < max_low_priority_requests_allowed,
              lows_singlehost[i]->started());
  }

  // Advance the clock by more than |max_queuing_time|.
  tick_clock_.SetNowTicks(base::DefaultTickClock::GetInstance()->NowTicks() +
                          max_queuing_time + base::TimeDelta::FromSeconds(1));

  // Since the requests have been queued for too long, they are now eligible for
  // disptaching. However, since the timer is not fired, the requests would not
  // be dispatched.
  base::RunLoop().RunUntilIdle();

  for (int i = 0; i < max_low_priority_requests_allowed + 10; ++i) {
    EXPECT_EQ(i < max_low_priority_requests_allowed,
              lows_singlehost[i]->started())
        << " i=" << i;
  }
}

// Verify that the timer to dispatch long queued requests starts only when there
// are requests in-flight.
TEST_F(ResourceSchedulerTest, MaxQueuingDelayTimerRunsOnRequestSchedule) {
  base::TimeDelta max_queuing_time = base::TimeDelta::FromSeconds(15);
  InitializeMaxQueuingDelayExperiment(max_queuing_time);
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  // Should be in sync with resource_scheduler.cc for effective connection type
  // (ECT) 2G. For ECT of 2G, number of low priority requests allowed are:
  // 8 - 3 * count of high priority requests in flight. That expression computes
  // to 8 - 3 * 1  = 5.
  const int max_low_priority_requests_allowed = 5;

  std::vector<std::unique_ptr<TestRequest>> lows_singlehost;

  InitializeScheduler();
  EXPECT_FALSE(scheduler()->IsLongQueuedRequestsDispatchTimerRunning());

  // Throw in one high priority request to ensure that high priority requests
  // do not depend on anything.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  EXPECT_TRUE(high->started());

  for (int i = 0; i < max_low_priority_requests_allowed + 10; ++i) {
    // Keep unique hostnames to prevent the per host limit from kicking in.
    std::string url = "http://host" + base::NumberToString(i) + "/low";
    lows_singlehost.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_EQ(i < max_low_priority_requests_allowed,
              lows_singlehost[i]->started());
  }
  // Timer should be running since there are pending requests.
  EXPECT_TRUE(scheduler()->IsLongQueuedRequestsDispatchTimerRunning());

  // Simulate firing of timer. The timer should restart since there is at least
  // one request in flight.
  scheduler()->DispatchLongQueuedRequestsForTesting();
  EXPECT_TRUE(scheduler()->IsLongQueuedRequestsDispatchTimerRunning());

  // Simulate firing of timer. The timer should not restart since there is no
  // request in flight.
  high.reset();
  for (auto& request : lows_singlehost) {
    request.reset();
  }
  scheduler()->DispatchLongQueuedRequestsForTesting();
  EXPECT_FALSE(scheduler()->IsLongQueuedRequestsDispatchTimerRunning());

  // Start a new set of requests, and verify timer still works correctly.
  std::unique_ptr<TestRequest> high2(
      NewRequest("http://host/high", net::HIGHEST));
  EXPECT_TRUE(high2->started());
  // Timer not started because there is no pending requests.
  EXPECT_FALSE(scheduler()->IsLongQueuedRequestsDispatchTimerRunning());

  // Start some requests which end up pending.
  for (int i = 0; i < max_low_priority_requests_allowed + 10; ++i) {
    // Keep unique hostnames to prevent the per host limit from kicking in.
    std::string url = "http://host" + base::NumberToString(i) + "/low";
    lows_singlehost.push_back(NewRequest(url.c_str(), net::LOWEST));
  }
  EXPECT_TRUE(scheduler()->IsLongQueuedRequestsDispatchTimerRunning());
}

// Starts a delayable request followed by a non-delayable request. The delayable
// request finishes after the start of the non-delayable request. Verifies that
// the histogram that records the time difference between the start of delayable
// requests and the start of non-delayable requests is recorded properly.
TEST_F(ResourceSchedulerTest, NonDelayableRequestArrivesAfterDelayableStarts) {
  base::HistogramTester histogram_tester;

  base::TimeDelta max_queuing_time = base::TimeDelta::FromSeconds(15);
  InitializeMaxQueuingDelayExperiment(max_queuing_time);

  InitializeScheduler();

  // Throw in one low priority request. When the request finishes histograms
  // should be recorded.
  std::unique_ptr<TestRequest> low(NewRequest("http://host/low", net::LOWEST));
  EXPECT_TRUE(low->started());

  const base::TimeDelta delay = base::TimeDelta::FromSeconds(5);
  tick_clock_.SetNowTicks(base::TimeTicks::Now() + delay);

  // Start a high priority request before |low| finishes.
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  EXPECT_TRUE(high->started());

  histogram_tester.ExpectTotalCount(
      "ResourceScheduler.DelayableRequests."
      "WaitTimeToAvoidContentionWithNonDelayableRequest",
      0);

  // When the delayable request finishes, metrics should be recorded.
  low.reset();

  ExpectSampleIsAtLeastSpecifiedValue(
      histogram_tester,
      "ResourceScheduler.DelayableRequests."
      "WaitTimeToAvoidContentionWithNonDelayableRequest",
      delay.InMilliseconds());
}

// Starts and ends non-delayable requests to verify that the duration between
// non-delayable requests is recorded correctly.
TEST_F(ResourceSchedulerTest, NonDelayableToNonDelayableMetrics) {
  base::HistogramTester histogram_tester_1;

  base::TimeDelta max_queuing_time = base::TimeDelta::FromSeconds(15);
  InitializeMaxQueuingDelayExperiment(max_queuing_time);

  InitializeScheduler();

  // Throw in one low priority request. When the request finishes histograms
  // should be recorded.
  std::unique_ptr<TestRequest> high_1(
      NewRequest("http://host/high_1", net::HIGHEST));
  EXPECT_TRUE(high_1->started());

  const base::TimeDelta high1_start_to_high2_start =
      base::TimeDelta::FromSeconds(5);
  tick_clock_.SetNowTicks(base::TimeTicks::Now() + high1_start_to_high2_start);

  // Start a high priority request before |high_1| finishes.
  std::unique_ptr<TestRequest> high_2(
      NewRequest("http://host/high_2", net::HIGHEST));
  EXPECT_TRUE(high_2->started());

  ExpectSampleIsAtLeastSpecifiedValue(
      histogram_tester_1,
      "ResourceScheduler.NonDelayableLastStartToNonDelayableStart",
      high1_start_to_high2_start.InMilliseconds());

  ExpectSampleIsAtLeastSpecifiedValue(
      histogram_tester_1,
      "ResourceScheduler.NonDelayableLastStartToNonDelayableStart."
      "NonDelayableInFlight",
      high1_start_to_high2_start.InMilliseconds());

  ExpectSampleIsAtLeastSpecifiedValue(
      histogram_tester_1,
      "ResourceScheduler.NonDelayableLastStartOrEndToNonDelayableStart",
      high1_start_to_high2_start.InMilliseconds());

  // No non-delayable request has ended yet.
  histogram_tester_1.ExpectTotalCount(
      "ResourceScheduler.NonDelayableLastEndToNonDelayableStart", 0);

  const base::TimeDelta high2_start_to_high2_end =
      base::TimeDelta::FromSeconds(7);
  tick_clock_.Advance(high2_start_to_high2_end);

  high_1.reset();
  high_2.reset();

  base::HistogramTester histogram_tester_2;

  const base::TimeDelta high2_end_to_high3_start =
      base::TimeDelta::FromSeconds(2);
  tick_clock_.Advance(high2_end_to_high3_start);
  // Start a high priority request after |high_1| and |high_2| finishes.
  std::unique_ptr<TestRequest> high_3(
      NewRequest("http://host/high_3", net::HIGHEST));
  EXPECT_TRUE(high_3->started());
  ExpectSampleIsAtLeastSpecifiedValue(
      histogram_tester_2,
      "ResourceScheduler.NonDelayableLastStartToNonDelayableStart",
      (high2_start_to_high2_end + high2_end_to_high3_start).InMilliseconds());

  ExpectSampleIsAtLeastSpecifiedValue(
      histogram_tester_2,
      "ResourceScheduler.NonDelayableLastEndToNonDelayableStart",
      high2_end_to_high3_start.InMilliseconds());

  ExpectSampleIsAtLeastSpecifiedValue(
      histogram_tester_2,
      "ResourceScheduler.NonDelayableLastEndToNonDelayableStart."
      "NonDelayableNotInFlight",
      high2_end_to_high3_start.InMilliseconds());

  ExpectSampleIsAtLeastSpecifiedValue(
      histogram_tester_2,
      "ResourceScheduler.NonDelayableLastStartOrEndToNonDelayableStart",
      high2_end_to_high3_start.InMilliseconds());
}

// Verify that when the proactive throttling is enabled, then delayable
// requests are throttled.
TEST_F(ResourceSchedulerTest, ProactiveThrottlingExperiment) {
  const struct {
    std::string test_case;
    bool enable_http_rtt_multiplier_for_proactive_throttling;
  } tests[] = {
      {
          "Enable proactive throttling",
          true,
      },
      {
          "Disabled proactive throttling",
          false,
      },
  };

  for (const auto& test : tests) {
    double http_rtt_multiplier_for_proactive_throttling = 5;
    base::TimeDelta http_rtt = base::TimeDelta::FromSeconds(1);

    if (test.enable_http_rtt_multiplier_for_proactive_throttling) {
      ConfigureProactiveThrottlingExperimentFor2G(
          http_rtt_multiplier_for_proactive_throttling);
    } else {
      ConfigureProactiveThrottlingExperimentFor2G(-1);
    }

    network_quality_estimator_.SetStartTimeNullHttpRtt(http_rtt);
    base::RunLoop().RunUntilIdle();

    base::TimeDelta threshold_requests_anticipation =
        http_rtt_multiplier_for_proactive_throttling * http_rtt;

    std::unique_ptr<TestRequest> high_1(
        NewRequest("http://host/high_1", net::HIGHEST));
    EXPECT_TRUE(high_1->started());
    high_1.reset();

    std::unique_ptr<TestRequest> low_1(
        NewRequest("http://host/low_1", net::LOWEST));
    EXPECT_NE(test.enable_http_rtt_multiplier_for_proactive_throttling,
              low_1->started())
        << " test_case=" << test.test_case;

    // Advancing the clock by a duration less than
    // |threshold_requests_anticipation| should not cause low priority requests
    // to start.
    tick_clock_.Advance(threshold_requests_anticipation -
                        base::TimeDelta::FromMilliseconds(1));
    std::unique_ptr<TestRequest> low_2(
        NewRequest("http://host/low_2", net::LOWEST));
    EXPECT_NE(test.enable_http_rtt_multiplier_for_proactive_throttling,
              low_2->started());

    // Advancing the clock by |threshold_requests_anticipation| should cause low
    // priority requests to start.
    tick_clock_.Advance(base::TimeDelta::FromMilliseconds(100));
    std::unique_ptr<TestRequest> low_3(
        NewRequest("http://host/low_3", net::LOWEST));
    EXPECT_TRUE(low_3->started());

    // Verify that high priority requests are not throttled.
    std::unique_ptr<TestRequest> high_2(
        NewRequest("http://host/high_2", net::HIGHEST));
    EXPECT_TRUE(high_2->started());
  }
}

// Verify that when the proactive throttling is enabled, then delayable
// requests are throttled, and the non-delayable requests are not throttled.
TEST_F(ResourceSchedulerTest,
       ProactiveThrottlingDoesNotThrottleHighPriorityRequests) {
  double http_rtt_multiplier_for_proactive_throttling = 5;
  ConfigureProactiveThrottlingExperimentFor2G(
      http_rtt_multiplier_for_proactive_throttling);

  base::TimeDelta http_rtt = base::TimeDelta::FromSeconds(1);

  network_quality_estimator_.SetStartTimeNullHttpRtt(http_rtt);
  base::RunLoop().RunUntilIdle();

  base::TimeDelta threshold_requests_anticipation =
      http_rtt_multiplier_for_proactive_throttling * http_rtt;

  std::unique_ptr<TestRequest> high_1(
      NewRequest("http://host/high_1", net::HIGHEST));
  EXPECT_TRUE(high_1->started());
  high_1.reset();

  std::unique_ptr<TestRequest> low_1(
      NewRequest("http://host/low_1", net::LOWEST));
  EXPECT_FALSE(low_1->started());

  // Advancing the clock by a duration less than
  // |threshold_requests_anticipation| should not cause low priority requests
  // to start.
  tick_clock_.Advance(threshold_requests_anticipation -
                      base::TimeDelta::FromMilliseconds(1));
  std::unique_ptr<TestRequest> low_2(
      NewRequest("http://host/low_2", net::LOWEST));
  EXPECT_FALSE(low_2->started());

  // Verify that high priority requests are not throttled.
  std::unique_ptr<TestRequest> high_2(
      NewRequest("http://host/high_2", net::HIGHEST));
  EXPECT_TRUE(high_2->started());
  high_2.reset();

  // End existing requests.
  low_1.reset();
  low_2.reset();

  // Start a new high priority request.
  std::unique_ptr<TestRequest> high_3(
      NewRequest("http://host/high_3", net::HIGHEST));
  EXPECT_TRUE(high_3->started());
  high_3.reset();

  // Verify that newly arriving delayalbe requests are still throttled.
  std::unique_ptr<TestRequest> low_3(
      NewRequest("http://host/low_3", net::LOWEST));
  EXPECT_FALSE(low_3->started());

  // Current ECT is 2G. If another notification for ECT 2G is received, |low_3|
  // is not started.
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_2G);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(low_3->started());

  // Current ECT is 2G. If a notification for ECT 4G is received, |low_3| is
  // started.
  network_quality_estimator_.SetAndNotifyObserversOfEffectiveConnectionType(
      net::EFFECTIVE_CONNECTION_TYPE_4G);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low_3->started());
}

// Verify that when the proactive throttling is enabled, then delayable
// requests are throttled, and the non-delayable requests are not throttled.
TEST_F(ResourceSchedulerTest, ProactiveThrottling_UnthrottledOnTimerFired) {
  double http_rtt_multiplier_for_proactive_throttling = 5;
  ConfigureProactiveThrottlingExperimentFor2G(
      http_rtt_multiplier_for_proactive_throttling);

  base::TimeDelta http_rtt = base::TimeDelta::FromSeconds(1);

  network_quality_estimator_.SetStartTimeNullHttpRtt(http_rtt);
  base::RunLoop().RunUntilIdle();

  base::TimeDelta threshold_requests_anticipation =
      http_rtt_multiplier_for_proactive_throttling * http_rtt;

  std::unique_ptr<TestRequest> high_1(
      NewRequest("http://host/high_1", net::HIGHEST));
  EXPECT_TRUE(high_1->started());
  high_1.reset();

  std::unique_ptr<TestRequest> low_1(
      NewRequest("http://host/low_1", net::LOWEST));
  EXPECT_FALSE(low_1->started());

  // Advancing the clock by a duration less than
  // |threshold_requests_anticipation| should not cause low priority requests
  // to start.
  tick_clock_.Advance(threshold_requests_anticipation +
                      base::TimeDelta::FromMilliseconds(1));

  // Since the requests have been queued for too long, they should now be
  // dispatched. Trigger the scheduling of the queued requests by calling
  // DispatchLongQueuedRequestsForTesting().
  scheduler()->DispatchLongQueuedRequestsForTesting();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low_1->started());
}

}  // unnamed namespace

}  // namespace network
