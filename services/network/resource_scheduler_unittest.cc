// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/resource_scheduler.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/timer.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_timing_info.h"
#include "net/base/request_priority.h"
#include "net/http/http_server_properties_impl.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/resource_scheduler_params_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/scheme_host_port.h"

using std::string;

namespace network {

namespace {

class TestRequestFactory;

const int kChildId = 30;
const int kRouteId = 75;
const int kChildId2 = 43;
const int kRouteId2 = 67;
const int kBackgroundChildId = 35;
const int kBackgroundRouteId = 43;

// Sync below with cc file.
const char kPrioritySupportedRequestsDelayable[] =
    "PrioritySupportedRequestsDelayable";
const char kHeadPrioritySupportedRequestsDelayable[] =
    "HeadPriorityRequestsDelayable";
const char kNetworkSchedulerYielding[] = "NetworkSchedulerYielding";
const size_t kMaxNumDelayableRequestsPerHostPerClient = 6;

void ConfigureYieldFieldTrial(
    int max_requests_before_yielding,
    int max_yield_ms,
    base::test::ScopedFeatureList* scoped_feature_list) {
  const std::string kTrialName = "TrialName";
  const std::string kGroupName = "GroupName";  // Value not used
  const std::string kNetworkSchedulerYielding = "NetworkSchedulerYielding";

  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);

  std::map<std::string, std::string> params;
  params["MaxRequestsBeforeYieldingParam"] =
      base::IntToString(max_requests_before_yielding);
  params["MaxYieldMs"] = base::IntToString(max_yield_ms);
  ASSERT_TRUE(
      base::FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
          kTrialName, kGroupName, params));

  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->RegisterFieldTrialOverride(
      kNetworkSchedulerYielding, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      trial.get());
  scoped_feature_list->InitWithFeatureList(std::move(feature_list));
}

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
        base::BindRepeating(&TestRequest::Resume, base::Unretained(this)));
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
  ResourceSchedulerTest() : field_trial_list_(nullptr) {
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
    InitializeScheduler();
    context_.set_http_server_properties(&http_server_properties_);
    context_.set_network_quality_estimator(&network_quality_estimator_);
  }

  ~ResourceSchedulerTest() override { CleanupScheduler(); }

  // Done separately from construction to allow for modification of command
  // line flags in tests.
  void InitializeScheduler(bool enabled = true) {
    CleanupScheduler();

    // Destroys previous scheduler.
    scheduler_.reset(new ResourceScheduler(enabled, &tick_clock_));

    scheduler()->SetResourceSchedulerParamsManagerForTests(
        resource_scheduler_params_manager_);

    scheduler_->OnClientCreated(kChildId, kRouteId,
                                &network_quality_estimator_);
    scheduler_->OnClientCreated(kBackgroundChildId, kBackgroundRouteId,
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
    }
  }

  std::unique_ptr<net::URLRequest> NewURLRequestWithChildAndRoute(
      const char* url,
      net::RequestPriority priority,
      int child_id,
      int route_id) {
    std::unique_ptr<net::URLRequest> url_request(context_.CreateRequest(
        GURL(url), priority, nullptr, TRAFFIC_ANNOTATION_FOR_TESTS));
    return url_request;
  }

  std::unique_ptr<net::URLRequest> NewURLRequest(
      const char* url,
      net::RequestPriority priority) {
    return NewURLRequestWithChildAndRoute(url, priority, kChildId, kRouteId);
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
    return GetNewTestRequest(url, priority, child_id, route_id, true);
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
    return GetNewTestRequest(url, priority, child_id, route_id, false);
  }

  std::unique_ptr<TestRequest> GetNewTestRequest(const char* url,
                                                 net::RequestPriority priority,
                                                 int child_id,
                                                 int route_id,
                                                 bool is_async) {
    std::unique_ptr<net::URLRequest> url_request(
        NewURLRequestWithChildAndRoute(url, priority, child_id, route_id));
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
    network_quality_estimator_.set_effective_connection_type(
        net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
    // Initialize the scheduler.
    InitializeScheduler();

    // Throw in one high priority request to ensure that it does not matter once
    // a body exists.
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
      std::string url = "http://host/low" + base::IntToString(i);
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
    network_quality_estimator_.set_effective_connection_type(
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

  ResourceScheduler* scheduler() { return scheduler_.get(); }

  base::MessageLoop message_loop_;
  std::unique_ptr<ResourceScheduler> scheduler_;
  net::HttpServerPropertiesImpl http_server_properties_;
  net::TestNetworkQualityEstimator network_quality_estimator_;
  net::TestURLRequestContext context_;
  ResourceSchedulerParamsManager resource_scheduler_params_manager_;
  base::FieldTrialList field_trial_list_;
  base::SimpleTestTickClock tick_clock_;
};

TEST_F(ResourceSchedulerTest, OneIsolatedLowRequest) {
  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/1", net::LOWEST));
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, OneLowLoadsUntilCriticalComplete) {
  base::HistogramTester histogram_tester;

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
          base::IntToString(net::HIGHEST),
      1);
  histogram_tester.ExpectTotalCount(
      "ResourceScheduler.RequestQueuingDuration.Priority" +
          base::IntToString(net::LOWEST),
      2);
}

TEST_F(ResourceSchedulerTest, SchedulerYieldsOnSpdy) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(kNetworkSchedulerYielding, "");
  InitializeScheduler();

  // The second low-priority request should yield.
  scheduler_->SetMaxRequestsBeforeYieldingForTesting(1);

  // Set a custom yield time.
  scheduler_->SetYieldTimeForTesting(base::TimeDelta::FromMilliseconds(42));

  // Use a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  scheduler_->SetTaskRunnerForTesting(task_runner);

  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

  std::unique_ptr<TestRequest> request(
      NewRequest("https://spdyhost/low", net::LOWEST));
  std::unique_ptr<TestRequest> request2(
      NewRequest("https://spdyhost/low", net::LOWEST));
  std::unique_ptr<TestRequest> request3(
      NewRequest("https://spdyhost/low", net::LOWEST));

  // Just before the yield task runs, only the first request should have
  // started.
  task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(41));
  EXPECT_TRUE(request->started());
  EXPECT_FALSE(request2->started());
  EXPECT_FALSE(request3->started());

  // Yield is done, run the next task.
  task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  EXPECT_TRUE(request2->started());
  EXPECT_FALSE(request3->started());

  // Just before the yield task runs, only the first two requests should have
  // started.
  task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(41));
  EXPECT_FALSE(request3->started());

  // Yield is done, run the next task.
  task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  EXPECT_TRUE(request3->started());
}

// Same as SchedulerYieldsOnSpdy but uses FieldTrial Parameters for
// configuration.
TEST_F(ResourceSchedulerTest, SchedulerYieldFieldTrialParams) {
  base::test::ScopedFeatureList scoped_feature_list;

  ConfigureYieldFieldTrial(1 /* requests before yielding */,
                           42 /* yield time */, &scoped_feature_list);
  InitializeScheduler();

  // Make sure the parameters were properly set.
  EXPECT_EQ(42, scheduler_->yield_time().InMilliseconds());
  EXPECT_EQ(1, scheduler_->max_requests_before_yielding());

  // Use a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  scheduler_->SetTaskRunnerForTesting(task_runner);

  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

  std::unique_ptr<TestRequest> request(
      NewRequest("https://spdyhost/low", net::LOWEST));
  std::unique_ptr<TestRequest> request2(
      NewRequest("https://spdyhost/low", net::LOWEST));

  // Just before the yield task runs, only the first request should have
  // started.
  task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(41));
  EXPECT_TRUE(request->started());
  EXPECT_FALSE(request2->started());

  // Yield is done, run the next task.
  task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  EXPECT_TRUE(request2->started());
}

TEST_F(ResourceSchedulerTest, YieldingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("", kNetworkSchedulerYielding);
  InitializeScheduler();

  // We're setting a yield parameter, but no yielding will happen since it's
  // disabled.
  scheduler_->SetMaxRequestsBeforeYieldingForTesting(1);

  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

  std::unique_ptr<TestRequest> request(
      NewRequest("https://spdyhost/low", net::LOWEST));
  std::unique_ptr<TestRequest> request2(
      NewRequest("https://spdyhost/low", net::LOWEST));
  EXPECT_TRUE(request->started());
  EXPECT_TRUE(request2->started());
}

TEST_F(ResourceSchedulerTest, SchedulerDoesNotYieldH1) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(kNetworkSchedulerYielding, "");
  InitializeScheduler();
  SetMaxDelayableRequests(1);

  // Use a testing task runner so that we can control time.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  scheduler_->SetTaskRunnerForTesting(task_runner);

  // Yield after each request.
  scheduler_->SetMaxRequestsBeforeYieldingForTesting(1);
  scheduler_->SetYieldTimeForTesting(base::TimeDelta::FromMilliseconds(42));

  std::unique_ptr<TestRequest> request(
      NewRequest("https://host/low", net::LOWEST));
  std::unique_ptr<TestRequest> request2(
      NewRequest("https://host/low", net::LOWEST));

  EXPECT_TRUE(request->started());
  EXPECT_FALSE(request2->started());

  // Finish the first task so that the second can start.
  request = nullptr;

  // Run tasks without advancing time, if there were yielding the next task
  // wouldn't start.
  task_runner->RunUntilIdle();

  // The next task started, so there was no yielding.
  EXPECT_TRUE(request2->started());
}

TEST_F(ResourceSchedulerTest, SchedulerDoesNotYieldAltSchemes) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(kNetworkSchedulerYielding, "");
  InitializeScheduler();

  // Yield after each request.
  scheduler_->SetMaxRequestsBeforeYieldingForTesting(1);
  scheduler_->SetYieldTimeForTesting(base::TimeDelta::FromMilliseconds(42));

  std::unique_ptr<TestRequest> request(
      NewRequest("yyy://host/low", net::LOWEST));
  std::unique_ptr<TestRequest> request2(
      NewRequest("zzz://host/low", net::LOWEST));

  EXPECT_TRUE(request->started());
  EXPECT_TRUE(request2->started());
}

TEST_F(ResourceSchedulerTest, SchedulerDoesNotYieldSyncRequests) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(kNetworkSchedulerYielding, "");
  InitializeScheduler();

  // The second low-priority request should yield.
  scheduler_->SetMaxRequestsBeforeYieldingForTesting(1);

  // Use spdy so that we don't throttle.
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

  std::unique_ptr<TestRequest> request(
      NewRequest("https://spdyhost/low", net::LOWEST));
  std::unique_ptr<TestRequest> request2(
      NewRequest("https://spdyhost/low", net::LOWEST));  // yields

  // Add a synchronous request, it shouldn't yield.
  std::unique_ptr<TestRequest> sync_request(
      NewSyncRequest("http://spdyhost/low", net::LOWEST));

  EXPECT_TRUE(request->started());
  EXPECT_FALSE(request2->started());
  EXPECT_TRUE(sync_request->started());  // The sync request started.

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request2->started());
}

TEST_F(ResourceSchedulerTest, MaxRequestsPerHostForSpdyWhenNotDelayable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("",
                                          kPrioritySupportedRequestsDelayable);

  InitializeScheduler();
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

  // Add more than max-per-host low-priority requests.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (size_t i = 0; i < kMaxNumDelayableRequestsPerHostPerClient + 1; ++i)
    requests.push_back(NewRequest("https://spdyhost/low", net::LOWEST));

  // No throttling.
  for (const auto& request : requests)
    EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, MaxRequestsPerHostForSpdyWhenDelayable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      kPrioritySupportedRequestsDelayable,
      kHeadPrioritySupportedRequestsDelayable);

  InitializeScheduler();
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

  // Add more than max-per-host low-priority requests.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (size_t i = 0; i < kMaxNumDelayableRequestsPerHostPerClient + 1; ++i)
    requests.push_back(NewRequest("https://spdyhost/low", net::LOWEST));

  // Only kMaxNumDelayableRequestsPerHostPerClient in body.
  for (size_t i = 0; i < requests.size(); ++i) {
    if (i < kMaxNumDelayableRequestsPerHostPerClient)
      EXPECT_TRUE(requests[i]->started());
    else
      EXPECT_FALSE(requests[i]->started());
  }
}

TEST_F(ResourceSchedulerTest, MaxRequestsPerHostForSpdyWhenHeadDelayable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      kHeadPrioritySupportedRequestsDelayable,
      kPrioritySupportedRequestsDelayable);

  InitializeScheduler();
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

  // Add more than max-per-host low-priority requests.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (size_t i = 0; i < kMaxNumDelayableRequestsPerHostPerClient + 1; ++i)
    requests.push_back(NewRequest("https://spdyhost/low", net::LOWEST));

  // No throttling.
  for (const auto& request : requests)
    EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, ThrottlesHeadWhenHeadDelayable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(
      kHeadPrioritySupportedRequestsDelayable,
      kPrioritySupportedRequestsDelayable);

  InitializeScheduler();
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

  // Add more than max-per-host low-priority requests.
  std::vector<std::unique_ptr<TestRequest>> requests;
  for (size_t i = 0; i < kMaxNumDelayableRequestsPerHostPerClient + 1; ++i)
    requests.push_back(NewRequest("https://spdyhost/low", net::LOWEST));

  base::RunLoop().RunUntilIdle();

  // No throttling.
  for (const auto& request : requests)
    EXPECT_TRUE(request->started());
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
  // The yielding feature will sometimes yield requests before they get a
  // chance to start, which conflicts this test. So disable the feature.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("", kNetworkSchedulerYielding);

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
    string url = "http://host/low" + base::IntToString(i);
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
    string url = "http://host" + base::IntToString(i) + "/low";
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
    string url = "http://host/low" + base::IntToString(i);
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
    string url = "http://host" + base::IntToString(i) + "/low";
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
  }

  SetMaxDelayableRequests(10);
  high.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(request->started());
  EXPECT_TRUE(idle->started());
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
    string url = "http://host/low" + base::IntToString(i);
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
    string url = "http://host/low" + base::IntToString(i);
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
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("",
                                          kPrioritySupportedRequestsDelayable);
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
  scoped_feature_list.InitFromCommandLine("",
                                          kPrioritySupportedRequestsDelayable);
  InitializeScheduler();

  const int kDefaultMaxNumDelayableRequestsPerClient =
      10;  // Should match the .cc.

  std::unique_ptr<TestRequest> low1_spdy(
      NewRequest("http://spdyhost1:8080/low", net::LOWEST));
  // Cancel a request after we learn the server supports SPDY.
  std::vector<std::unique_ptr<TestRequest>> lows;
  for (int i = 0; i < kDefaultMaxNumDelayableRequestsPerClient - 1; ++i) {
    string url = "http://host" + base::IntToString(i) + "/low";
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
  }
  std::unique_ptr<TestRequest> low1(NewRequest("http://host/low", net::LOWEST));
  EXPECT_FALSE(low1->started());
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("http", "spdyhost1", 8080), true);
  low1_spdy.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low1->started());

  low1.reset();
  base::RunLoop().RunUntilIdle();
  std::unique_ptr<TestRequest> low2_spdy(
      NewRequest("http://spdyhost2:8080/low", net::IDLE));
  // Reprioritize a request after we learn the server supports SPDY.
  EXPECT_TRUE(low2_spdy->started());
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("http", "spdyhost2", 8080), true);
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
  network_quality_estimator_.set_effective_connection_type(
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
    string url = "http://host" + base::IntToString(i) + "/low";
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(lows.back()->started());
  }
  std::unique_ptr<TestRequest> low1(NewRequest("http://host/low", net::LOWEST));
  EXPECT_FALSE(low1->started());
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("http", "spdyhost1", 8080), true);
  low1_spdy.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low1->started());

  low1.reset();
  base::RunLoop().RunUntilIdle();
  std::unique_ptr<TestRequest> low2_spdy(
      NewRequest("http://spdyhost2:8080/low", net::IDLE));
  // Reprioritize a request after we learn the server supports SPDY.
  EXPECT_TRUE(low2_spdy->started());
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("http", "spdyhost2", 8080), true);
  ChangeRequestPriority(low2_spdy.get(), net::LOWEST);
  base::RunLoop().RunUntilIdle();
  std::unique_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_FALSE(low2->started());

  // SPDY requests are not started either.
  std::unique_ptr<TestRequest> low3_spdy(
      NewRequest("http://spdyhost1:8080/low", net::LOWEST));
  EXPECT_FALSE(low3_spdy->started());
}

TEST_F(ResourceSchedulerTest, NewDelayableSpdyHostInDelayableRequests) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(kPrioritySupportedRequestsDelayable,
                                          "");
  InitializeScheduler();

  const int kDefaultMaxNumDelayableRequestsPerClient =
      10;  // Should match the .cc.

  std::unique_ptr<TestRequest> low1_spdy(
      NewRequest("http://spdyhost1:8080/low", net::LOWEST));
  // Cancel a request after we learn the server supports SPDY.
  std::vector<std::unique_ptr<TestRequest>> lows;
  for (int i = 0; i < kDefaultMaxNumDelayableRequestsPerClient - 1; ++i) {
    string url = "http://host" + base::IntToString(i) + "/low";
    lows.push_back(NewRequest(url.c_str(), net::LOWEST));
  }
  std::unique_ptr<TestRequest> low1(NewRequest("http://host/low", net::LOWEST));
  EXPECT_FALSE(low1->started());
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("http", "spdyhost1", 8080), true);
  low1_spdy.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(low1->started());

  low1.reset();
  base::RunLoop().RunUntilIdle();
  std::unique_ptr<TestRequest> low2_spdy(
      NewRequest("http://spdyhost2:8080/low", net::IDLE));
  // Reprioritize a request after we learn the server supports SPDY.
  EXPECT_TRUE(low2_spdy->started());
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("http", "spdyhost2", 8080), true);
  ChangeRequestPriority(low2_spdy.get(), net::LOWEST);
  base::RunLoop().RunUntilIdle();
  std::unique_ptr<TestRequest> low2(NewRequest("http://host/low", net::LOWEST));
  EXPECT_FALSE(low2->started());
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
    network_quality_estimator_.set_effective_connection_type(ect);

    // The limit will matter only once the page has a body, since delayable
    // requests are not loaded before that.
    scheduler()->DeprecatedOnNavigate(kChildId, kRouteId);

    // Throw in one high priority request to ensure that it does not matter once
    // a body exists.
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
      std::string url = "http://host" + base::IntToString(i) + "/low";
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

// Test that a change in network conditions midway during loading does not
// change the behavior of the resource scheduler.
TEST_F(ResourceSchedulerTest, RequestLimitOverrideFixedForPageLoad) {
  base::test::ScopedFeatureList scoped_feature_list;
  InitializeThrottleDelayableExperiment(true, 0.0);
  // ECT value is in range for which the limit is overridden to 2.
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  InitializeScheduler();

  // The limit will matter only once the page has a body, since delayable
  // requests are not loaded before that.
  scheduler()->DeprecatedOnNavigate(kChildId, kRouteId);

  // Throw in one high priority request to ensure that it does not matter once
  // a body exists.
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
    std::string url = "http://host" + base::IntToString(i) + "/low";
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

  // Change the ECT to go outside the experiment buckets and change the network
  // type to 4G. This should not affect the limit calculated at the beginning of
  // the page load.
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_4G);
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<TestRequest> last_singlehost(
      NewRequest("http://host/last", net::LOWEST));

  // Last should not start because the limit should not have changed.
  EXPECT_FALSE(last_singlehost->started());

  // The limit should change when there is a new page navigation.
  scheduler()->DeprecatedOnNavigate(kChildId, kRouteId);
  std::unique_ptr<TestRequest> high2(
      NewRequest("http://host/high2", net::HIGHEST));
  EXPECT_TRUE(high2->started());
  high2.reset();

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(last_singlehost->started());
}

// Test that when the network quality changes such that the new limit is lower,
// and an |DeprecatedOnNavigate| event occurs, the new delayable requests don't
// start until the number of requests in flight have gone below the new limit.
TEST_F(ResourceSchedulerTest, RequestLimitReducedAcrossPageLoads) {
  base::test::ScopedFeatureList scoped_feature_list;
  InitializeThrottleDelayableExperiment(true, 0.0);
  // ECT value is in range for which the limit is overridden to 4.
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_3G);
  InitializeScheduler();

  // The limit will matter only once the page has a body, since delayable
  // requests are not loaded before that.
  scheduler()->DeprecatedOnNavigate(kChildId, kRouteId);

  // Throw in one high priority request to ensure that it does not matter once
  // a body exists.
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
    std::string url = "http://host" + base::IntToString(i) + "/low1";
    delayable_first_page.push_back(NewRequest(url.c_str(), net::LOWEST));
    EXPECT_TRUE(delayable_first_page[i]->started());
  }
  // Change the network quality so that the ECT value is in range for which the
  // limit is overridden to 2. The effective connection type is set to
  // Slow-2G.
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  // Trigger a navigation event which will recompute limits. Also insert a body,
  // because the limit matters only after the body exists.
  scheduler()->DeprecatedOnNavigate(kChildId, kRouteId);

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
    std::string url = "http://host" + base::IntToString(i) + "/low2";
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
      "http://host" + base::IntToString(kNumDelayableLow) + "/low3";
  delayable_second_page.push_back(NewRequest(url.c_str(), net::LOWEST));
  EXPECT_FALSE(delayable_second_page.back()->started());
}

TEST_F(ResourceSchedulerTest, ThrottleDelayableDisabled) {
  base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();

  const char kTrialName[] = "TrialName";
  const char kGroupName[] = "GroupName";

  base::FieldTrial* field_trial =
      base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);

  base::test::ScopedFeatureList scoped_feature_list;

  std::unique_ptr<base::FeatureList> feature_list(
      std::make_unique<base::FeatureList>());

  feature_list->RegisterFieldTrialOverride(
      "ThrottleDelayable", base::FeatureList::OVERRIDE_DISABLE_FEATURE,
      field_trial);
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  InitializeScheduler();
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_2G);
  scheduler()->DeprecatedOnNavigate(kChildId, kRouteId);
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
  base::test::ScopedFeatureList scoped_feature_list;
  const double kNonDelayableWeight = 2.0;
  const int kDefaultMaxNumDelayableRequestsPerClient =
      10;  // Should be in sync with cc.
  // Initialize the experiment with |kNonDelayableWeight| as the weight of
  // non-delayable requests.
  InitializeThrottleDelayableExperiment(false, kNonDelayableWeight);
  // Experiment should not run when the effective connection type is faster
  // than 2G.
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_3G);
  // Limit will only trigger after the page has a body.

  InitializeScheduler();
  scheduler()->DeprecatedOnNavigate(kChildId, kRouteId);
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
  base::test::ScopedFeatureList scoped_feature_list;
  const double kNonDelayableWeight = 2.0;
  const int kDefaultMaxNumDelayableRequestsPerClient =
      8;  // Should be in sync with cc.
  // Initialize the experiment with |kNonDelayableWeight| as the weight of
  // non-delayable requests.
  InitializeThrottleDelayableExperiment(false, kNonDelayableWeight);
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  InitializeScheduler();
  // Limit will only trigger after the page has a body.
  scheduler()->DeprecatedOnNavigate(kChildId, kRouteId);
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

TEST_F(ResourceSchedulerTest, SchedulerEnabled) {
  SetMaxDelayableRequests(1);
  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/req", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::LOWEST));

  EXPECT_FALSE(request->started());
}

TEST_F(ResourceSchedulerTest, SchedulerDisabled) {
  InitializeScheduler(false);

  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/req", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::LOWEST));

  // Normally |request| wouldn't start immediately due to the |high| priority
  // request, but when the scheduler is disabled it starts immediately.
  EXPECT_TRUE(request->started());
}

TEST_F(ResourceSchedulerTest, MultipleInstances_1) {
  SetMaxDelayableRequests(1);
  // In some circumstances there may exist multiple instances.
  ResourceScheduler another_scheduler(false,
                                      base::DefaultTickClock::GetInstance());

  std::unique_ptr<TestRequest> high(
      NewRequest("http://host/high", net::HIGHEST));
  std::unique_ptr<TestRequest> low(NewRequest("http://host/req", net::LOWEST));

  std::unique_ptr<TestRequest> request(
      NewRequest("http://host/req", net::LOWEST));

  // Though |another_scheduler| is disabled, this request should be throttled
  // as it's handled by |scheduler_| which is active.
  EXPECT_FALSE(request->started());
}

TEST_F(ResourceSchedulerTest, MultipleInstances_2) {
  SetMaxDelayableRequests(1);
  ResourceScheduler another_scheduler(true,
                                      base::DefaultTickClock::GetInstance());
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
        "http://host/another", net::LOWEST, kChildId, kRouteId));
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
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  InitializeScheduler();
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

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

// Verify that when |delay_requests_on_multiplexed_connections| is false, spdy
// hosts are not subject to kMaxNumDelayableRequestsPerHostPerClient or
// kDefaultMaxNumDelayableRequestsPerClient limits.
TEST_F(ResourceSchedulerTest,
       MaxRequestsPerHostForSpdyWhenDelayableFastConnections) {
  ConfigureDelayRequestsOnMultiplexedConnectionsFieldTrial();
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_4G);

  InitializeScheduler();
  http_server_properties_.SetSupportsSpdy(
      url::SchemeHostPort("https", "spdyhost", 443), true);

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
  network_quality_estimator_.set_effective_connection_type(
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
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_2G);

  InitializeScheduler();
  // The limit will matter only once the page has a body, since delayable
  // requests are not loaded before that.
  scheduler()->DeprecatedOnNavigate(kChildId, kRouteId);

  // Throw in one high priority request to ensure that it does not matter once
  // a body exists.
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
    std::string url = "http://host" + base::IntToString(i) + "/low";
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
  base::TimeDelta max_queuing_time = base::TimeDelta::FromSeconds(15);
  InitializeMaxQueuingDelayExperiment(max_queuing_time);
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  InitializeScheduler();
  // The limit will matter only once the page has a body, since delayable
  // requests are not loaded before that.
  scheduler()->DeprecatedOnNavigate(kChildId, kRouteId);

  // Throw in one high priority request to ensure that it does not matter once
  // a body exists.
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
    std::string url = "http://host" + base::IntToString(i) + "/low";
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
}

// Verify that when |max_queuing_time| is not set, requests queued for too long
// duration are not dispatched to the network.
TEST_F(ResourceSchedulerTest, MaxQueuingDelayNotSet) {
  base::TimeDelta max_queuing_time = base::TimeDelta::FromSeconds(15);
  network_quality_estimator_.set_effective_connection_type(
      net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  InitializeScheduler();
  // The limit will matter only once the page has a body, since delayable
  // requests are not loaded before that.
  scheduler()->DeprecatedOnNavigate(kChildId, kRouteId);

  // Throw in one high priority request to ensure that it does not matter once
  // a body exists.
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
    std::string url = "http://host" + base::IntToString(i) + "/low";
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

}  // unnamed namespace

}  // namespace network
