// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/webrtc_video_perf_reporter.h"

#include <memory>

#include "base/run_loop.h"
#include "media/mojo/mojom/webrtc_video_perf.mojom-blink.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/peerconnection/stats_collector.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

using ::testing::_;

namespace blink {
namespace {

constexpr media::VideoCodecProfile kCodecProfile =
    media::VideoCodecProfile::VP9PROFILE_PROFILE0;

class MockWebrtcVideoPerfRecorder
    : public media::mojom::blink::WebrtcVideoPerfRecorder {
 public:
  MockWebrtcVideoPerfRecorder() = default;
  MOCK_METHOD2(UpdateRecord,
               void(media::mojom::blink::WebrtcPredictionFeaturesPtr,
                    media::mojom::blink::WebrtcVideoStatsPtr));

  mojo::PendingRemote<media::mojom::blink::WebrtcVideoPerfRecorder>
  CreatePendingRemote() {
    receiver_.reset();
    return receiver_.BindNewPipeAndPassRemote(
        blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  }

  mojo::Receiver<media::mojom::blink::WebrtcVideoPerfRecorder> receiver_{this};
};

class WebrtcVideoPerfReporterTest : public ::testing::Test {
 public:
  WebrtcVideoPerfReporterTest() {
    mock_recorder_ = std::make_unique<MockWebrtcVideoPerfRecorder>();
    reporter_ = MakeGarbageCollected<WebrtcVideoPerfReporter>(
        blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
        /* notifier */ nullptr, mock_recorder_->CreatePendingRemote());
  }

 protected:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<MockWebrtcVideoPerfRecorder> mock_recorder_;
  Persistent<WebrtcVideoPerfReporter> reporter_;
};

TEST_F(WebrtcVideoPerfReporterTest, StoreWebrtcVideoStats) {
  const StatsCollector::StatsKey kStatsKeyA = {/*is_decode=*/true,
                                               kCodecProfile, 1920 * 1080,
                                               /*hw_accelerated=*/false};
  const auto kExpectedFeaturesA = media::mojom::blink::WebrtcPredictionFeatures(
      /*is_decode_stats=*/true,
      static_cast<media::mojom::blink::VideoCodecProfile>(kCodecProfile),
      /*video_pixels=*/1920 * 1080, /*hardware_accelerated=*/false);

  const StatsCollector::VideoStats kVideoStats = {123, 4, 5.6};
  const auto kExpectedVideoStats = media::mojom::blink::WebrtcVideoStats(
      /*frames_processed=*/123, /*key_frames_processed=*/4,
      /*p99_processing_time_ms=*/5.6);

  EXPECT_CALL(*mock_recorder_, UpdateRecord)
      .WillOnce([&kExpectedFeaturesA, &kExpectedVideoStats](
                    media::mojom::blink::WebrtcPredictionFeaturesPtr features,
                    media::mojom::blink::WebrtcVideoStatsPtr video_stats) {
        EXPECT_EQ(kExpectedFeaturesA, *features);
        EXPECT_EQ(kExpectedVideoStats, *video_stats);
      });
  reporter_->StoreWebrtcVideoStats(kStatsKeyA, kVideoStats);
  base::RunLoop().RunUntilIdle();

  // Toggle the booleans.
  const StatsCollector::StatsKey kStatsKeyB = {/*is_decode=*/false,
                                               kCodecProfile, 1920 * 1080,
                                               /*hw_accelerated=*/true};
  const auto kExpectedFeaturesB = media::mojom::blink::WebrtcPredictionFeatures(
      /*is_decode_stats=*/false,
      static_cast<media::mojom::blink::VideoCodecProfile>(kCodecProfile),
      /*video_pixels=*/1920 * 1080, /*hardware_accelerated=*/true);

  EXPECT_CALL(*mock_recorder_, UpdateRecord)
      .WillOnce([&kExpectedFeaturesB, &kExpectedVideoStats](
                    media::mojom::blink::WebrtcPredictionFeaturesPtr features,
                    media::mojom::blink::WebrtcVideoStatsPtr video_stats) {
        EXPECT_EQ(kExpectedFeaturesB, *features);
        EXPECT_EQ(kExpectedVideoStats, *video_stats);
      });
  reporter_->StoreWebrtcVideoStats(kStatsKeyB, kVideoStats);
  base::RunLoop().RunUntilIdle();
}

}  // namespace

}  // namespace blink
