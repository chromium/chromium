// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/peerconnection/speed_limit_uma_listener.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/peerconnection/peer_connection_tracker.mojom-blink.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"

namespace blink {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

class SpeedLimitUmaListenerTest : public ::testing::Test {
 public:
  void SetUp() override {
    task_runner_ = platform_->test_task_runner();
    listener_ = std::make_unique<SpeedLimitUmaListener>(task_runner_);
  }

 protected:
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  // Tasks run on the test thread with fake time, use FastForwardBy() to
  // advance time and execute delayed tasks.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::HistogramTester histogram_;
  std::unique_ptr<SpeedLimitUmaListener> listener_;
};
}  // namespace

using base::Bucket;

TEST_F(SpeedLimitUmaListenerTest, HasOneBucketWithoutMeasurements) {
  EXPECT_THAT(histogram_.GetTotalCountsForPrefix("WebRTC.PeerConnectio"),
              IsEmpty());
  task_runner_->FastForwardBy(SpeedLimitUmaListener::kStatsReportingPeriod);
  EXPECT_THAT(histogram_.GetAllSamples("WebRTC.PeerConnection.SpeedLimit"),
              IsEmpty());
  EXPECT_THAT(
      histogram_.GetAllSamples("WebRTC.PeerConnection.ThermalThrottling"),
      ElementsAre(Bucket(false, 1)));
}

TEST_F(SpeedLimitUmaListenerTest, HistogramAfterThrottledSignal) {
  listener_->OnSpeedLimitChange(55);
  task_runner_->FastForwardBy(SpeedLimitUmaListener::kStatsReportingPeriod);

  EXPECT_THAT(histogram_.GetAllSamples("WebRTC.PeerConnection.SpeedLimit"),
              ElementsAre(Bucket(55, 1)));
  EXPECT_THAT(
      histogram_.GetAllSamples("WebRTC.PeerConnection.ThermalThrottling"),
      ElementsAre(Bucket(true, 1)));
}

TEST_F(SpeedLimitUmaListenerTest, DeletionCancelsListener) {
  listener_->OnSpeedLimitChange(33);
  task_runner_->FastForwardBy(2 * SpeedLimitUmaListener::kStatsReportingPeriod);
  EXPECT_THAT(histogram_.GetAllSamples("WebRTC.PeerConnection.SpeedLimit"),
              ElementsAre(Bucket(33, 2)));

  listener_ = nullptr;
  task_runner_->FastForwardBy(SpeedLimitUmaListener::kStatsReportingPeriod);
  EXPECT_THAT(histogram_.GetAllSamples("WebRTC.PeerConnection.SpeedLimit"),
              ElementsAre(Bucket(33, 2)));
  EXPECT_THAT(
      histogram_.GetAllSamples("WebRTC.PeerConnection.ThermalThrottling"),
      ElementsAre(Bucket(true, 2)));
}

TEST_F(SpeedLimitUmaListenerTest, RecordsMostRecentState) {
  listener_->OnSpeedLimitChange(33);
  task_runner_->FastForwardBy(SpeedLimitUmaListener::kStatsReportingPeriod / 2);
  listener_->OnSpeedLimitChange(44);
  task_runner_->FastForwardBy(SpeedLimitUmaListener::kStatsReportingPeriod / 2);

  EXPECT_THAT(histogram_.GetAllSamples("WebRTC.PeerConnection.SpeedLimit"),
              ElementsAre(Bucket(44, 1)));
}

TEST_F(SpeedLimitUmaListenerTest, HistogramBucketsIncludesPreviousPeriod) {
  listener_->OnSpeedLimitChange(1);
  task_runner_->FastForwardBy(SpeedLimitUmaListener::kStatsReportingPeriod);
  listener_->OnSpeedLimitChange(2);
  task_runner_->FastForwardBy(SpeedLimitUmaListener::kStatsReportingPeriod);
  listener_->OnSpeedLimitChange(3);
  task_runner_->FastForwardBy(SpeedLimitUmaListener::kStatsReportingPeriod);
  listener_->OnSpeedLimitChange(mojom::blink::kSpeedLimitMax);
  task_runner_->FastForwardBy(SpeedLimitUmaListener::kStatsReportingPeriod);

  EXPECT_THAT(histogram_.GetAllSamples("WebRTC.PeerConnection.SpeedLimit"),
              ElementsAre(Bucket(1, 1), Bucket(2, 1), Bucket(3, 1)));
  EXPECT_THAT(
      histogram_.GetAllSamples("WebRTC.PeerConnection.ThermalThrottling"),
      ElementsAre(Bucket(false, 1), Bucket(true, 3)));
}

TEST_F(SpeedLimitUmaListenerTest, NoThrottlingEpisodesIfNothingReported) {
  listener_ = nullptr;
  EXPECT_THAT(histogram_.GetAllSamples(
                  "WebRTC.PeerConnection.ThermalThrottlingEpisodes"),
              ElementsAre(Bucket(0, 1)));
}

TEST_F(SpeedLimitUmaListenerTest, NoThrottlingEpisodesIfNominalSpeedReported) {
  listener_->OnSpeedLimitChange(mojom::blink::kSpeedLimitMax);
  listener_->OnSpeedLimitChange(mojom::blink::kSpeedLimitMax);
  listener_->OnSpeedLimitChange(mojom::blink::kSpeedLimitMax);
  listener_ = nullptr;
  EXPECT_THAT(histogram_.GetAllSamples(
                  "WebRTC.PeerConnection.ThermalThrottlingEpisodes"),
              ElementsAre(Bucket(0, 1)));
}

TEST_F(SpeedLimitUmaListenerTest, CountsOneEpisode) {
  listener_->OnSpeedLimitChange(55);
  listener_ = nullptr;
  EXPECT_THAT(histogram_.GetAllSamples(
                  "WebRTC.PeerConnection.ThermalThrottlingEpisodes"),
              ElementsAre(Bucket(1, 1)));
}

TEST_F(SpeedLimitUmaListenerTest, CountsTwoEpisodes) {
  listener_->OnSpeedLimitChange(55);
  listener_->OnSpeedLimitChange(100);
  listener_->OnSpeedLimitChange(99);
  listener_->OnSpeedLimitChange(100);
  listener_ = nullptr;
  EXPECT_THAT(histogram_.GetAllSamples(
                  "WebRTC.PeerConnection.ThermalThrottlingEpisodes"),
              ElementsAre(Bucket(2, 1)));
}

}  // namespace blink
