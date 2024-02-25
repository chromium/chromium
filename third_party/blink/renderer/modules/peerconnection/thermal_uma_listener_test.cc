// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/thermal_uma_listener.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/peerconnection/peer_connection_tracker.mojom-blink.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"

namespace blink {

namespace {

const base::TimeDelta kStatsReportingPeriod = base::Minutes(1);

class ThermalUmaListenerTest : public ::testing::Test {
 public:
  void SetUp() override {
    task_runner_ = platform_->test_task_runner();
    thermal_uma_listener_ = ThermalUmaListener::Create(task_runner_);
  }

 protected:
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  // Tasks run on the test thread with fake time, use FastForwardBy() to
  // advance time and execute delayed tasks.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::HistogramTester histogram_;
  std::unique_ptr<ThermalUmaListener> thermal_uma_listener_;
};

}  // namespace

using base::Bucket;

TEST_F(ThermalUmaListenerTest, NoMeasurementsHasNoHistograms) {
  EXPECT_THAT(histogram_.GetTotalCountsForPrefix("WebRTC.PeerConnectio"),
              testing::IsEmpty());
  task_runner_->FastForwardBy(kStatsReportingPeriod);
  EXPECT_THAT(histogram_.GetTotalCountsForPrefix("WebRTC.PeerConnection"),
              testing::IsEmpty());
}

TEST_F(ThermalUmaListenerTest, HistogramAfterSignal) {
  thermal_uma_listener_->OnThermalMeasurement(
      mojom::blink::DeviceThermalState::kFair);
  task_runner_->FastForwardBy(kStatsReportingPeriod);

  EXPECT_THAT(histogram_.GetAllSamples("WebRTC.PeerConnection.ThermalState"),
              testing::ElementsAre(Bucket(1, 1)));
}

TEST_F(ThermalUmaListenerTest, DeletionCancelsListener) {
  thermal_uma_listener_->OnThermalMeasurement(
      mojom::blink::DeviceThermalState::kFair);
  task_runner_->FastForwardBy(2 * kStatsReportingPeriod);
  EXPECT_THAT(histogram_.GetAllSamples("WebRTC.PeerConnection.ThermalState"),
              testing::ElementsAre(Bucket(1, 2)));

  thermal_uma_listener_ = nullptr;
  task_runner_->FastForwardBy(kStatsReportingPeriod);
  EXPECT_THAT(histogram_.GetAllSamples("WebRTC.PeerConnection.ThermalState"),
              testing::ElementsAre(Bucket(1, 2)));
}

TEST_F(ThermalUmaListenerTest, RecordsMostRecentState) {
  thermal_uma_listener_->OnThermalMeasurement(
      mojom::blink::DeviceThermalState::kFair);
  task_runner_->FastForwardBy(kStatsReportingPeriod / 2);
  thermal_uma_listener_->OnThermalMeasurement(
      mojom::blink::DeviceThermalState::kSerious);
  task_runner_->FastForwardBy(kStatsReportingPeriod / 2);

  EXPECT_THAT(histogram_.GetAllSamples("WebRTC.PeerConnection.ThermalState"),
              testing::ElementsAre(Bucket(2, 1)));
}

TEST_F(ThermalUmaListenerTest, HistogramBucketsIncludesPreviousPeriod) {
  thermal_uma_listener_->OnThermalMeasurement(
      mojom::blink::DeviceThermalState::kNominal);
  task_runner_->FastForwardBy(kStatsReportingPeriod);
  thermal_uma_listener_->OnThermalMeasurement(
      mojom::blink::DeviceThermalState::kFair);
  task_runner_->FastForwardBy(kStatsReportingPeriod);
  thermal_uma_listener_->OnThermalMeasurement(
      mojom::blink::DeviceThermalState::kSerious);
  task_runner_->FastForwardBy(kStatsReportingPeriod);
  thermal_uma_listener_->OnThermalMeasurement(
      mojom::blink::DeviceThermalState::kCritical);
  task_runner_->FastForwardBy(kStatsReportingPeriod);

  EXPECT_THAT(histogram_.GetAllSamples("WebRTC.PeerConnection.ThermalState"),
              testing::ElementsAre(Bucket(0, 1), Bucket(1, 1), Bucket(2, 1),
                                   Bucket(3, 1)));
}

}  // namespace blink
