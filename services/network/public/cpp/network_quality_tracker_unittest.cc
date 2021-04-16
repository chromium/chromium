// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_quality_tracker.h"

#include <limits>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

class TestEffectiveConnectionTypeObserver
    : public NetworkQualityTracker::EffectiveConnectionTypeObserver {
 public:
  explicit TestEffectiveConnectionTypeObserver(NetworkQualityTracker* tracker)
      : num_notifications_(0),
        tracker_(tracker),
        expected_effective_connection_type_(
            net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
        effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
    tracker_->AddEffectiveConnectionTypeObserver(this);
  }

  ~TestEffectiveConnectionTypeObserver() override {
    tracker_->RemoveEffectiveConnectionTypeObserver(this);
  }

  // Helper to synchronously get effective connection type from
  // NetworkQualityTracker.
  net::EffectiveConnectionType GetEffectiveConnectionTypeSync() {
    return tracker_->GetEffectiveConnectionType();
  }

  // NetworkConnectionObserver implementation:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override {
    EXPECT_EQ(type, GetEffectiveConnectionTypeSync());
    num_notifications_++;
    effective_connection_type_ = type;
    if (run_loop_ && type == expected_effective_connection_type_)
      run_loop_->Quit();
  }

  size_t num_notifications() const { return num_notifications_; }

  void WaitForNotification(
      net::EffectiveConnectionType expected_effective_connection_type) {
    if (expected_effective_connection_type == effective_connection_type_)
      return;

    expected_effective_connection_type_ = expected_effective_connection_type;
    // WaitForNotification should not be called twice.
    EXPECT_EQ(nullptr, run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  net::EffectiveConnectionType effective_connection_type() const {
    return effective_connection_type_;
  }

 private:
  size_t num_notifications_;
  NetworkQualityTracker* tracker_;
  // May be null.
  std::unique_ptr<base::RunLoop> run_loop_;
  net::EffectiveConnectionType expected_effective_connection_type_;
  net::EffectiveConnectionType effective_connection_type_;

  DISALLOW_COPY_AND_ASSIGN(TestEffectiveConnectionTypeObserver);
};

class TestRTTAndThroughputEstimatesObserver
    : public NetworkQualityTracker::RTTAndThroughputEstimatesObserver {
 public:
  explicit TestRTTAndThroughputEstimatesObserver(NetworkQualityTracker* tracker)
      : num_notifications_(0),
        tracker_(tracker),
        downstream_throughput_kbps_(std::numeric_limits<int32_t>::max()) {
    tracker_->AddRTTAndThroughputEstimatesObserver(this);
  }

  ~TestRTTAndThroughputEstimatesObserver() override {
    tracker_->RemoveRTTAndThroughputEstimatesObserver(this);
  }

  // RTTAndThroughputEstimatesObserver implementation:
  void OnRTTOrThroughputEstimatesComputed(
      base::TimeDelta http_rtt,
      base::TimeDelta transport_rtt,
      int32_t downstream_throughput_kbps) override {
    EXPECT_EQ(http_rtt, tracker_->GetHttpRTT());
    EXPECT_EQ(transport_rtt, tracker_->GetTransportRTT());
    EXPECT_EQ(downstream_throughput_kbps,
              tracker_->GetDownstreamThroughputKbps());

    num_notifications_++;
    http_rtt_ = http_rtt;
    transport_rtt_ = transport_rtt;
    downstream_throughput_kbps_ = downstream_throughput_kbps;

    if (run_loop_ && http_rtt == http_rtt_notification_wait_)
      run_loop_->Quit();
  }

  size_t num_notifications() const { return num_notifications_; }

  void WaitForNotification(base::TimeDelta expected_http_rtt) {
    // It's not meaningful to wait for notification with RTT set to
    // base::TimeDelta() since that value implies that the network quality
    // estimate was unavailable.
    EXPECT_NE(base::TimeDelta(), expected_http_rtt);
    http_rtt_notification_wait_ = expected_http_rtt;
    if (http_rtt_notification_wait_ == http_rtt_)
      return;

    // WaitForNotification should not be called twice.
    EXPECT_EQ(nullptr, run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    EXPECT_EQ(expected_http_rtt, http_rtt_);
    run_loop_.reset();
  }

  void VerifyNetworkQualityMatchesWithTracker() const {
    EXPECT_EQ(tracker_->GetHttpRTT(), http_rtt_);
    EXPECT_EQ(tracker_->GetTransportRTT(), transport_rtt_);
    EXPECT_EQ(tracker_->GetDownstreamThroughputKbps(),
              downstream_throughput_kbps_);
  }

  base::TimeDelta http_rtt() const { return http_rtt_; }
  base::TimeDelta transport_rtt() const { return transport_rtt_; }
  int32_t downstream_throughput_kbps() const {
    return downstream_throughput_kbps_;
  }

 private:
  size_t num_notifications_;
  NetworkQualityTracker* tracker_;
  // May be null.
  std::unique_ptr<base::RunLoop> run_loop_;
  base::TimeDelta http_rtt_;
  base::TimeDelta transport_rtt_;
  int32_t downstream_throughput_kbps_;
  base::TimeDelta http_rtt_notification_wait_;

  DISALLOW_COPY_AND_ASSIGN(TestRTTAndThroughputEstimatesObserver);
};

}  // namespace

class NetworkQualityTrackerTest : public testing::Test {
 public:
  NetworkQualityTrackerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
    mojo::PendingRemote<network::mojom::NetworkService> network_service_remote;
    network_service_ = network::NetworkService::Create(
        network_service_remote.InitWithNewPipeAndPassReceiver());
    tracker_ = std::make_unique<NetworkQualityTracker>(
        base::BindRepeating(&NetworkQualityTrackerTest::mojom_network_service,
                            base::Unretained(this)));
    ect_observer_ =
        std::make_unique<TestEffectiveConnectionTypeObserver>(tracker_.get());
    rtt_throughput_observer_ =
        std::make_unique<TestRTTAndThroughputEstimatesObserver>(tracker_.get());
  }

  ~NetworkQualityTrackerTest() override {}

  network::NetworkService* network_service() { return network_service_.get(); }

  NetworkQualityTracker* network_quality_tracker() { return tracker_.get(); }

  TestEffectiveConnectionTypeObserver* effective_connection_type_observer() {
    return ect_observer_.get();
  }

  TestRTTAndThroughputEstimatesObserver* rtt_throughput_observer() {
    return rtt_throughput_observer_.get();
  }

  // Simulates a connection type change and broadcast it to observers.
  void SimulateEffectiveConnectionTypeChange(
      net::EffectiveConnectionType type) {
    network_service()
        ->network_quality_estimator()
        ->SimulateNetworkQualityChangeForTesting(type);
  }

 private:
  network::mojom::NetworkService* mojom_network_service() const {
    return network_service_.get();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<network::NetworkService> network_service_;
  std::unique_ptr<NetworkQualityTracker> tracker_;
  std::unique_ptr<TestEffectiveConnectionTypeObserver> ect_observer_;
  std::unique_ptr<TestRTTAndThroughputEstimatesObserver>
      rtt_throughput_observer_;

  DISALLOW_COPY_AND_ASSIGN(NetworkQualityTrackerTest);
};

TEST_F(NetworkQualityTrackerTest, ECTObserverNotified) {
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            effective_connection_type_observer()->effective_connection_type());

  SimulateEffectiveConnectionTypeChange(net::EFFECTIVE_CONNECTION_TYPE_3G);
  effective_connection_type_observer()->WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_3G);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_3G,
            effective_connection_type_observer()->effective_connection_type());
  // Typical RTT and downlink values when effective connection type is 3G. Taken
  // from net::NetworkQualityEstimatorParams.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(450),
            network_quality_tracker()->GetHttpRTT());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(400),
            network_quality_tracker()->GetTransportRTT());
  EXPECT_EQ(400, network_quality_tracker()->GetDownstreamThroughputKbps());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, effective_connection_type_observer()->num_notifications());

  SimulateEffectiveConnectionTypeChange(net::EFFECTIVE_CONNECTION_TYPE_2G);
  effective_connection_type_observer()->WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_2G);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            effective_connection_type_observer()->effective_connection_type());
  // Typical RTT and downlink values when effective connection type is 3G. Taken
  // from net::NetworkQualityEstimatorParams.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1800),
            network_quality_tracker()->GetHttpRTT());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1500),
            network_quality_tracker()->GetTransportRTT());
  EXPECT_EQ(75, network_quality_tracker()->GetDownstreamThroughputKbps());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, effective_connection_type_observer()->num_notifications());
}

// Test that network quality tracker sets the network quality values correctly
// when the network quality is unavailable.
TEST_F(NetworkQualityTrackerTest, ECTObserverNotifiedUnknown) {
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            effective_connection_type_observer()->effective_connection_type());

  SimulateEffectiveConnectionTypeChange(net::EFFECTIVE_CONNECTION_TYPE_3G);
  effective_connection_type_observer()->WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_3G);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_3G,
            effective_connection_type_observer()->effective_connection_type());
  // Typical RTT and downlink values when effective connection type is 3G. Taken
  // from net::NetworkQualityEstimatorParams.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(450),
            network_quality_tracker()->GetHttpRTT());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(400),
            network_quality_tracker()->GetTransportRTT());
  EXPECT_EQ(400, network_quality_tracker()->GetDownstreamThroughputKbps());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, effective_connection_type_observer()->num_notifications());

  SimulateEffectiveConnectionTypeChange(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  effective_connection_type_observer()->WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            effective_connection_type_observer()->effective_connection_type());
  // RTT and downlink values when effective connection type is UNKNOWN.
  EXPECT_EQ(base::TimeDelta(), network_quality_tracker()->GetHttpRTT());
  EXPECT_EQ(base::TimeDelta(), network_quality_tracker()->GetTransportRTT());
  EXPECT_EQ(std::numeric_limits<int32_t>::max(),
            network_quality_tracker()->GetDownstreamThroughputKbps());
}

TEST_F(NetworkQualityTrackerTest, RttThroughputObserverNotified) {
  // One notification must be received by rtt_throughput_observer() as soon as
  // it is registered as an observer.
  EXPECT_EQ(1u, rtt_throughput_observer()->num_notifications());

  SimulateEffectiveConnectionTypeChange(net::EFFECTIVE_CONNECTION_TYPE_3G);
  rtt_throughput_observer()->WaitForNotification(
      base::TimeDelta::FromMilliseconds(450));
  // Typical RTT and downlink values when effective connection type is 3G. Taken
  // from net::NetworkQualityEstimatorParams.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(450),
            network_quality_tracker()->GetHttpRTT());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(400),
            network_quality_tracker()->GetTransportRTT());
  EXPECT_EQ(400, network_quality_tracker()->GetDownstreamThroughputKbps());
  rtt_throughput_observer()->VerifyNetworkQualityMatchesWithTracker();
  EXPECT_EQ(2u, rtt_throughput_observer()->num_notifications());

  SimulateEffectiveConnectionTypeChange(net::EFFECTIVE_CONNECTION_TYPE_2G);
  rtt_throughput_observer()->VerifyNetworkQualityMatchesWithTracker();
  rtt_throughput_observer()->WaitForNotification(
      base::TimeDelta::FromMilliseconds(1800));
  // Typical RTT and downlink values when effective connection type is 3G. Taken
  // from net::NetworkQualityEstimatorParams.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1800),
            network_quality_tracker()->GetHttpRTT());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1500),
            network_quality_tracker()->GetTransportRTT());
  EXPECT_EQ(75, network_quality_tracker()->GetDownstreamThroughputKbps());
  EXPECT_EQ(3u, rtt_throughput_observer()->num_notifications());
}

TEST_F(NetworkQualityTrackerTest, ECTObserverNotifiedOnAddition) {
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            effective_connection_type_observer()->effective_connection_type());

  SimulateEffectiveConnectionTypeChange(net::EFFECTIVE_CONNECTION_TYPE_3G);
  effective_connection_type_observer()->WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_3G);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_3G,
            effective_connection_type_observer()->effective_connection_type());
  // Typical RTT and downlink values when effective connection type is 3G. Taken
  // from net::NetworkQualityEstimatorParams.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(450),
            network_quality_tracker()->GetHttpRTT());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(400),
            network_quality_tracker()->GetTransportRTT());
  EXPECT_EQ(400, network_quality_tracker()->GetDownstreamThroughputKbps());
  EXPECT_EQ(1u, effective_connection_type_observer()->num_notifications());

  auto ect_observer_2 = std::make_unique<TestEffectiveConnectionTypeObserver>(
      network_quality_tracker());
  ect_observer_2->WaitForNotification(net::EFFECTIVE_CONNECTION_TYPE_3G);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_3G,
            ect_observer_2->effective_connection_type());
}

TEST_F(NetworkQualityTrackerTest, RttThroughputObserverNotifiedOnAddition) {
  SimulateEffectiveConnectionTypeChange(net::EFFECTIVE_CONNECTION_TYPE_3G);
  rtt_throughput_observer()->WaitForNotification(
      base::TimeDelta::FromMilliseconds(450));
  // Typical RTT and downlink values when effective connection type is 3G. Taken
  // from net::NetworkQualityEstimatorParams.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(450),
            network_quality_tracker()->GetHttpRTT());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(400),
            network_quality_tracker()->GetTransportRTT());
  EXPECT_EQ(400, network_quality_tracker()->GetDownstreamThroughputKbps());
  rtt_throughput_observer()->VerifyNetworkQualityMatchesWithTracker();
  EXPECT_EQ(2u, rtt_throughput_observer()->num_notifications());

  auto rtt_throughput_observer_2 =
      std::make_unique<TestRTTAndThroughputEstimatesObserver>(
          network_quality_tracker());
  rtt_throughput_observer_2->VerifyNetworkQualityMatchesWithTracker();
  EXPECT_EQ(1u, rtt_throughput_observer_2->num_notifications());

  // Simulate a network quality change.
  SimulateEffectiveConnectionTypeChange(net::EFFECTIVE_CONNECTION_TYPE_2G);

  rtt_throughput_observer()->WaitForNotification(
      base::TimeDelta::FromMilliseconds(1800));
  // Typical RTT and downlink values when effective connection type is 2G. Taken
  // from net::NetworkQualityEstimatorParams.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1800),
            network_quality_tracker()->GetHttpRTT());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1500),
            network_quality_tracker()->GetTransportRTT());
  EXPECT_EQ(75, network_quality_tracker()->GetDownstreamThroughputKbps());

  rtt_throughput_observer()->VerifyNetworkQualityMatchesWithTracker();
  EXPECT_EQ(3u, rtt_throughput_observer()->num_notifications());
  rtt_throughput_observer_2->VerifyNetworkQualityMatchesWithTracker();
  EXPECT_EQ(2u, rtt_throughput_observer_2->num_notifications());
}

TEST_F(NetworkQualityTrackerTest, UnregisteredECTObserverNotNotified) {
  auto ect_observer_2 = std::make_unique<TestEffectiveConnectionTypeObserver>(
      network_quality_tracker());

  // Simulate a network quality change.
  SimulateEffectiveConnectionTypeChange(net::EFFECTIVE_CONNECTION_TYPE_3G);

  ect_observer_2->WaitForNotification(net::EFFECTIVE_CONNECTION_TYPE_3G);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_3G,
            ect_observer_2->effective_connection_type());
  effective_connection_type_observer()->WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_3G);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_3G,
            effective_connection_type_observer()->effective_connection_type());
  // Typical RTT and downlink values when effective connection type is 3G. Taken
  // from net::NetworkQualityEstimatorParams.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(450),
            network_quality_tracker()->GetHttpRTT());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(400),
            network_quality_tracker()->GetTransportRTT());
  EXPECT_EQ(400, network_quality_tracker()->GetDownstreamThroughputKbps());

  ect_observer_2.reset();

  // Simulate an another network quality change.
  SimulateEffectiveConnectionTypeChange(net::EFFECTIVE_CONNECTION_TYPE_2G);
  effective_connection_type_observer()->WaitForNotification(
      net::EFFECTIVE_CONNECTION_TYPE_2G);
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G,
            effective_connection_type_observer()->effective_connection_type());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1800),
            network_quality_tracker()->GetHttpRTT());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1500),
            network_quality_tracker()->GetTransportRTT());
  EXPECT_EQ(75, network_quality_tracker()->GetDownstreamThroughputKbps());
  EXPECT_EQ(2u, effective_connection_type_observer()->num_notifications());
}

TEST_F(NetworkQualityTrackerTest, UnregisteredRttThroughputbserverNotNotified) {
  auto rtt_throughput_observer_2 =
      std::make_unique<TestRTTAndThroughputEstimatesObserver>(
          network_quality_tracker());

  // Simulate a network quality change.
  SimulateEffectiveConnectionTypeChange(net::EFFECTIVE_CONNECTION_TYPE_3G);

  rtt_throughput_observer()->WaitForNotification(
      base::TimeDelta::FromMilliseconds(450));
  // Typical RTT and downlink values when effective connection type is 3G. Taken
  // from net::NetworkQualityEstimatorParams.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(450),
            network_quality_tracker()->GetHttpRTT());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(400),
            network_quality_tracker()->GetTransportRTT());
  EXPECT_EQ(400, network_quality_tracker()->GetDownstreamThroughputKbps());
  rtt_throughput_observer()->VerifyNetworkQualityMatchesWithTracker();
  rtt_throughput_observer_2->VerifyNetworkQualityMatchesWithTracker();

  // Destroying |rtt_throughput_observer_2| should unregister it as an observer.
  // Verify that doing this causes network quality tracker to remove it as an
  // observer from the list of registered observers.
  rtt_throughput_observer_2.reset();

  // Simulate an another network quality change.
  SimulateEffectiveConnectionTypeChange(net::EFFECTIVE_CONNECTION_TYPE_2G);
  rtt_throughput_observer()->WaitForNotification(
      base::TimeDelta::FromMilliseconds(1800));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1800),
            network_quality_tracker()->GetHttpRTT());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1500),
            network_quality_tracker()->GetTransportRTT());
  EXPECT_EQ(75, network_quality_tracker()->GetDownstreamThroughputKbps());
  rtt_throughput_observer()->VerifyNetworkQualityMatchesWithTracker();
  EXPECT_EQ(3u, rtt_throughput_observer()->num_notifications());
}

}  // namespace network
