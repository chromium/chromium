// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/instrumented_simulcast_adapter.h"

#include <queue>

#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "media/base/mock_filters.h"
#include "media/mojo/clients/mock_mojo_video_encoder_metrics_provider_factory.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder_factory.h"
#include "third_party/blink/renderer/platform/peerconnection/stats_collector.h"
#include "third_party/blink/renderer/platform/peerconnection/video_encoder_state_observer_impl.h"
#include "third_party/webrtc/api/environment/environment_factory.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/media/engine/internal_encoder_factory.h"

using ::testing::ByMove;
using ::testing::Return;

namespace blink {

class InstrumentedSimulcastAdapterTest : public ::testing::Test {
 public:
  InstrumentedSimulcastAdapterTest()
      : mock_gpu_factories_(nullptr),
        mock_encoder_metrics_provider_factory_(
            base::MakeRefCounted<
                media::MockMojoVideoEncoderMetricsProviderFactory>(
                media::mojom::VideoEncoderUseCase::kWebRTC)) {
    ON_CALL(mock_gpu_factories_, GetTaskRunner())
        .WillByDefault(Return(base::SequencedTaskRunner::GetCurrentDefault()));
    ON_CALL(mock_gpu_factories_, IsEncoderSupportKnown())
        .WillByDefault(Return(true));
  }
  ~InstrumentedSimulcastAdapterTest() override = default;

  void UseHwEncoder() {
    rtc_video_encoder_factory_ = std::make_unique<RTCVideoEncoderFactory>(
        &mock_gpu_factories_, mock_encoder_metrics_provider_factory_);

    primary_encoder_factory_ = static_cast<webrtc::VideoEncoderFactory*>(
        rtc_video_encoder_factory_.get());
    secondate_encoder_factory_ =
        static_cast<webrtc::VideoEncoderFactory*>(&software_encoder_factory_);
  }
  void SetUp() override {
    primary_encoder_factory_ =
        static_cast<webrtc::VideoEncoderFactory*>(&software_encoder_factory_);
    secondate_encoder_factory_ = nullptr;
  }
  void TearDown() override {
    primary_encoder_factory_ = nullptr;
    secondate_encoder_factory_ = nullptr;

    rtc_video_encoder_factory_.reset();
    mock_encoder_metrics_provider_factory_.reset();

    // Wait until the tasks are completed that are posted to
    // base::SequencedTaskRunner::GetCurrentDefault().
    task_environment_.RunUntilIdle();
  }

 protected:
  using StatsKey = StatsCollector::StatsKey;
  using VideoStats = StatsCollector::VideoStats;

  std::unique_ptr<InstrumentedSimulcastAdapter>
  CreateInstrumentedSimulcastAdapter() {
    return InstrumentedSimulcastAdapter::Create(
        webrtc::EnvironmentFactory().Create(), primary_encoder_factory_.get(),
        secondate_encoder_factory_.get(),
        std::make_unique<VideoEncoderStateObserverImpl>(
            media::VideoCodecProfile::VP8PROFILE_ANY, base::NullCallback()),
        webrtc::SdpVideoFormat::VP8());
  }

  base::test::TaskEnvironment task_environment_;
  media::MockGpuVideoAcceleratorFactories mock_gpu_factories_;
  scoped_refptr<media::MockMojoVideoEncoderMetricsProviderFactory>
      mock_encoder_metrics_provider_factory_;
  std::unique_ptr<RTCVideoEncoderFactory> rtc_video_encoder_factory_;
  webrtc::InternalEncoderFactory software_encoder_factory_;

  raw_ptr<webrtc::VideoEncoderFactory> primary_encoder_factory_;
  raw_ptr<webrtc::VideoEncoderFactory> secondate_encoder_factory_;

  std::queue<std::pair<StatsKey, VideoStats>> processing_stats_;

 private:
  void StoreProcessingStats(const StatsKey& stats_key,
                            const VideoStats& video_stats) {
    processing_stats_.emplace(stats_key, video_stats);
  }
};

TEST_F(InstrumentedSimulcastAdapterTest,
       CreateAndDestroyWithoutHardwareAcceleration) {
  EXPECT_TRUE(CreateInstrumentedSimulcastAdapter());
}

TEST_F(InstrumentedSimulcastAdapterTest,
       CreateAndDestroyWithHardwareAcceleration) {
  UseHwEncoder();
  EXPECT_TRUE(CreateInstrumentedSimulcastAdapter());
}
}  // namespace blink
