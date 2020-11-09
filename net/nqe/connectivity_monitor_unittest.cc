// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/connectivity_monitor.h"

#include <memory>

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/base/features.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

constexpr base::TimeDelta kInactivityThreshold{base::TimeDelta::FromSeconds(1)};
constexpr base::TimeDelta kUpdateInterval{
    base::TimeDelta::FromMilliseconds(100)};
constexpr base::TimeDelta kMinFailureLoggingInterval{
    base::TimeDelta::FromSeconds(45)};

class ConnectivityMonitorTest
    : public testing::Test,
      public testing::WithParamInterface<bool>,
      public NetworkChangeNotifier::NetworkChangeObserver {
 public:
  ConnectivityMonitorTest() {
    if (ShouldReportFailureToOS()) {
      feature_overrides_.InitAndEnableFeature(
          features::kReportPoorConnectivity);
    } else {
      feature_overrides_.InitAndDisableFeature(
          features::kReportPoorConnectivity);
    }

    NetworkChangeNotifier::AddNetworkChangeObserver(this);

    test_server_.ServeFilesFromDirectory(GetTestNetDataDirectory());
    CHECK(test_server_.Start());
  }

  ~ConnectivityMonitorTest() override {
    NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  }

  ConnectivityMonitor& monitor() { return connectivity_monitor_; }

  bool ShouldReportFailureToOS() const { return GetParam(); }

  void SimulateSwitchToWiFiNetwork() {
    SimulateNetworkChange(NetworkChangeNotifier::CONNECTION_WIFI);
  }

  void SimulateSwitchToMobileNetwork() {
    SimulateNetworkChange(NetworkChangeNotifier::CONNECTION_4G);
  }

  std::unique_ptr<URLRequest> CreateTestRequest() {
    std::unique_ptr<URLRequest> request = test_request_context_.CreateRequest(
        test_server_.GetURL("/test.html"), DEFAULT_PRIORITY,
        &test_url_request_delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
    connectivity_monitor_.TrackNewRequest(*request);
    return request;
  }

  void FastForwardTimeBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  void SimulateNetworkChange(NetworkChangeNotifier::ConnectionType type) {
    network_change_wait_loop_.emplace(
        base::RunLoop::Type::kNestableTasksAllowed);
    network_change_notifier_.mock_network_change_notifier()->SetConnectionType(
        type);
    NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(type);
    network_change_wait_loop_->Run();
    network_change_wait_loop_.reset();
  }

  // NetworkChangeNotifier::NetworkChangeObserver:
  void OnNetworkChanged(NetworkChangeNotifier::ConnectionType type) override {
    connectivity_monitor_.NotifyConnectionTypeChanged(type);
    if (network_change_wait_loop_)
      network_change_wait_loop_->Quit();
  }

  base::test::ScopedFeatureList feature_overrides_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::IO};
  EmbeddedTestServer test_server_;
  TestURLRequestContext test_request_context_;
  test::ScopedMockNetworkChangeNotifier network_change_notifier_;
  ConnectivityMonitor connectivity_monitor_{kInactivityThreshold,
                                            kMinFailureLoggingInterval};
  TestDelegate test_url_request_delegate_;
  base::Optional<base::RunLoop> network_change_wait_loop_;
};

INSTANTIATE_TEST_SUITE_P(, ConnectivityMonitorTest, testing::Bool());

TEST_P(ConnectivityMonitorTest, TrackWiFiRequests) {
  SimulateSwitchToWiFiNetwork();

  std::unique_ptr<URLRequest> request = CreateTestRequest();
  EXPECT_EQ(1u, monitor().num_active_requests_for_testing());
  monitor().NotifyRequestCompleted(*request);
  EXPECT_EQ(0u, monitor().num_active_requests_for_testing());
}

TEST_P(ConnectivityMonitorTest, TrackMobileRequests) {
  SimulateSwitchToMobileNetwork();

  std::unique_ptr<URLRequest> request = CreateTestRequest();
  EXPECT_EQ(1u, monitor().num_active_requests_for_testing());
  monitor().NotifyRequestCompleted(*request);
  EXPECT_EQ(0u, monitor().num_active_requests_for_testing());
}

TEST_P(ConnectivityMonitorTest, NetworkChangeResetsState) {
  SimulateSwitchToWiFiNetwork();

  std::unique_ptr<URLRequest> request = CreateTestRequest();
  EXPECT_EQ(1u, monitor().num_active_requests_for_testing());
  FastForwardTimeBy(kInactivityThreshold);
  EXPECT_EQ(base::TimeDelta(), monitor().GetTimeSinceLastFailureForTesting());

  FastForwardTimeBy(base::TimeDelta::FromDays(42));
  SimulateSwitchToMobileNetwork();
  EXPECT_EQ(0u, monitor().num_active_requests_for_testing());
  EXPECT_EQ(base::nullopt, monitor().GetTimeSinceLastFailureForTesting());
}

TEST_P(ConnectivityMonitorTest, BasicStalledRequest) {
  SimulateSwitchToWiFiNetwork();

  std::unique_ptr<URLRequest> request = CreateTestRequest();
  EXPECT_EQ(1u, monitor().num_active_requests_for_testing());

  bool deadline_reached = false;
  monitor().SetNextDeadlineCallbackForTesting(
      base::BindLambdaForTesting([&] { deadline_reached = true; }));

  // Pass some time, but not enough to suspect connectivity issues.
  FastForwardTimeBy(kUpdateInterval);
  EXPECT_FALSE(deadline_reached);
  EXPECT_EQ(base::nullopt, monitor().GetTimeSinceLastFailureForTesting());

  // Simulate additional passage of time to trigger connectivity failure
  // observation.
  FastForwardTimeBy(kInactivityThreshold);
  EXPECT_TRUE(deadline_reached);
  EXPECT_EQ(kUpdateInterval, monitor().GetTimeSinceLastFailureForTesting());

  deadline_reached = false;
  monitor().SetNextDeadlineCallbackForTesting(
      base::BindLambdaForTesting([&] { deadline_reached = true; }));

  // Pass a little more time and verify that the current failure duration has
  // grown accordingly. Another deadline will not be reached yet because
  // kMinFailureLoggingInterval hasn't elapsed.
  FastForwardTimeBy(kInactivityThreshold);
  EXPECT_FALSE(deadline_reached);
  EXPECT_EQ(kUpdateInterval + kInactivityThreshold,
            monitor().GetTimeSinceLastFailureForTesting());

  monitor().NotifyRequestCompleted(*request);
  EXPECT_EQ(0u, monitor().num_active_requests_for_testing());
}

TEST_P(ConnectivityMonitorTest, MultipleRequests) {
  SimulateSwitchToWiFiNetwork();

  std::unique_ptr<URLRequest> request1 = CreateTestRequest();
  std::unique_ptr<URLRequest> request2 = CreateTestRequest();
  EXPECT_EQ(2u, monitor().num_active_requests_for_testing());

  // Pass some time, but not enough to suspect connectivity issues.
  FastForwardTimeBy(kUpdateInterval);
  EXPECT_EQ(base::nullopt, monitor().GetTimeSinceLastFailureForTesting());

  // Simulate progress on one but not both requests. Connectivity failure should
  // still not be detected due to the first request's progress.
  monitor().NotifyRequestProgress(*request1);
  FastForwardTimeBy(kInactivityThreshold - kUpdateInterval);
  EXPECT_EQ(base::nullopt, monitor().GetTimeSinceLastFailureForTesting());

  // Pass enough time to trigger a failure.
  FastForwardTimeBy(kUpdateInterval);
  EXPECT_EQ(base::TimeDelta(), monitor().GetTimeSinceLastFailureForTesting());
}

TEST_P(ConnectivityMonitorTest, HistogramLogging) {
  const char kHistogramName[] = "NQE.ConnectivityMonitor.TimeToSwitchNetworks";

  SimulateSwitchToWiFiNetwork();

  base::HistogramTester histograms;

  std::unique_ptr<URLRequest> request = CreateTestRequest();
  FastForwardTimeBy(kInactivityThreshold + kUpdateInterval);

  // The monitor should have logged a failure by now, but no recorded
  // histograms.
  EXPECT_EQ(kUpdateInterval, monitor().GetTimeSinceLastFailureForTesting());
  histograms.ExpectTotalCount(kHistogramName, 0);

  // Now trigger a network change after a long delay. This should log a
  // histogram sample conveying the time since the failure was first detected.
  constexpr base::TimeDelta kArbitraryDelay{base::TimeDelta::FromSeconds(60)};
  FastForwardTimeBy(kArbitraryDelay);
  SimulateSwitchToMobileNetwork();
  histograms.ExpectTotalCount(kHistogramName, 1);
  histograms.ExpectUniqueTimeSample(kHistogramName,
                                    kUpdateInterval + kArbitraryDelay, 1);
}

TEST_P(ConnectivityMonitorTest, OnlyReportToOSWithFeatureEnabled) {
  SimulateSwitchToWiFiNetwork();

  std::unique_ptr<URLRequest> request = CreateTestRequest();

  bool reported_to_os = false;
  monitor().SetReportCallbackForTesting(
      base::BindLambdaForTesting([&reported_to_os] { reported_to_os = true; }));

  // Pass some time, but not enough to suspect connectivity issues.
  FastForwardTimeBy(kUpdateInterval);
  EXPECT_FALSE(reported_to_os);
  EXPECT_EQ(base::nullopt, monitor().GetTimeSinceLastFailureForTesting());

  // Simulate additional passage of time to trigger connectivity failure
  // observation. If the ReportPoorConnectivity feature is enabled, this should
  // have invoked the callback above; otherwise it should not.
  FastForwardTimeBy(kInactivityThreshold);
  EXPECT_EQ(ShouldReportFailureToOS(), reported_to_os);
  EXPECT_EQ(kUpdateInterval, monitor().GetTimeSinceLastFailureForTesting());
}

}  // namespace
}  // namespace net
