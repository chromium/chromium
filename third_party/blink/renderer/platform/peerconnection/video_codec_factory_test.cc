// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/video_codec_factory.h"

#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "media/base/mock_filters.h"
#include "media/base/video_encoder_metrics_provider.h"
#include "media/mojo/clients/mock_mojo_video_encoder_metrics_provider_factory.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "media/video/video_encode_accelerator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/peerconnection/stats_collector.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_util.h"
#include "third_party/webrtc/api/environment/environment_factory.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/media/engine/internal_encoder_factory.h"
#include "third_party/webrtc/modules/video_coding/codecs/h264/include/h264.h"

using ::testing::Return;
using ::testing::ValuesIn;

namespace blink {

class VideoCodecFactoryTestWithSdpFormat
    : public testing::TestWithParam<webrtc::SdpVideoFormat> {
 public:
  VideoCodecFactoryTestWithSdpFormat()
      : mock_encoder_metrics_provider_factory_(
            base::MakeRefCounted<
                media::MockMojoVideoEncoderMetricsProviderFactory>(
                media::mojom::VideoEncoderUseCase::kWebRTC)) {
    ON_CALL(mock_gpu_factories_, GetTaskRunner())
        .WillByDefault(Return(base::SequencedTaskRunner::GetCurrentDefault()));
    ON_CALL(mock_gpu_factories_, IsEncoderSupportKnown())
        .WillByDefault(Return(true));
  }

  ~VideoCodecFactoryTestWithSdpFormat() override = default;

  void TearDown() override {
    // Wait until the tasks are completed that are posted to
    // base::SequencedTaskRunner::GetCurrentDefault().
    task_environment_.RunUntilIdle();
  }

 protected:
  bool CanCreateEncoder(const webrtc::SdpVideoFormat& sdp, bool sw) {
    std::optional<media::VideoCodecProfile> profile =
        WebRTCFormatToCodecProfile(sdp);
    if (!profile.has_value()) {
      return false;
    }
    if (sw) {
      webrtc::InternalEncoderFactory software_encoder_factory;
      return sdp.IsCodecInList(software_encoder_factory.GetSupportedFormats());
    }
    return true;
  }
  std::unique_ptr<webrtc::VideoEncoderFactory> CreateEncoderFactory() {
    return CreateWebrtcVideoEncoderFactory(
        &mock_gpu_factories_, mock_encoder_metrics_provider_factory_,
        base::NullCallback());
  }

  testing::NiceMock<media::MockGpuVideoAcceleratorFactories>
      mock_gpu_factories_{nullptr};
  scoped_refptr<media::MockMojoVideoEncoderMetricsProviderFactory>
      mock_encoder_metrics_provider_factory_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_P(VideoCodecFactoryTestWithSdpFormat, CreateHardwareEncoder) {
  std::unique_ptr<webrtc::VideoEncoderFactory> encoder_factory =
      CreateEncoderFactory();
  ASSERT_TRUE(encoder_factory);

  const media::VideoEncodeAccelerator::SupportedProfiles kSupportedProfiles = {
      {media::H264PROFILE_BASELINE, gfx::Size(3840, 2160)},
      {media::VP8PROFILE_ANY, gfx::Size(3840, 2160)},
      {media::VP9PROFILE_PROFILE0, gfx::Size(3840, 2160)},
      {media::AV1PROFILE_PROFILE_MAIN, gfx::Size(3840, 2160)},
  };
  EXPECT_CALL(mock_gpu_factories_, GetVideoEncodeAcceleratorSupportedProfiles())
      .WillRepeatedly(Return(kSupportedProfiles));
  webrtc::EnvironmentFactory environment_factory;
  auto encoder =
      encoder_factory->Create(environment_factory.Create(), GetParam());
  EXPECT_EQ(encoder != nullptr, CanCreateEncoder(GetParam(), false));
  if (encoder) {
    EXPECT_TRUE(encoder->GetEncoderInfo().is_hardware_accelerated);
  }
}

TEST_P(VideoCodecFactoryTestWithSdpFormat, CreateSoftwareEncoder) {
  std::unique_ptr<webrtc::VideoEncoderFactory> encoder_factory =
      CreateEncoderFactory();
  ASSERT_TRUE(encoder_factory);

  EXPECT_CALL(mock_gpu_factories_, GetVideoEncodeAcceleratorSupportedProfiles())
      .WillRepeatedly(Return(
          std::vector<media::VideoEncodeAccelerator::SupportedProfile>{}));
  webrtc::EnvironmentFactory environment_factory;
  auto encoder =
      encoder_factory->Create(environment_factory.Create(), GetParam());
  EXPECT_EQ(encoder != nullptr, CanCreateEncoder(GetParam(), true));
  // Don't check encoder->GetEncoderInfo().is_hardware_accelerated because
  // SimulcastEncoderAdapter doesn't set it and the default value on
  // is_hardware_accelerated is true.
}

INSTANTIATE_TEST_SUITE_P(
    ,
    VideoCodecFactoryTestWithSdpFormat,
    ValuesIn({
#if !BUILDFLAG(IS_ANDROID)
        webrtc::CreateH264Format(webrtc::H264Profile::kProfileBaseline,
                                 webrtc::H264Level::kLevel1,
                                 /*packetization_mode=*/"1"),
#endif
        webrtc::SdpVideoFormat::VP8(),
        webrtc::SdpVideoFormat::VP9Profile0(),
        webrtc::SdpVideoFormat::AV1Profile0(),
        // no supported profile.
        webrtc::SdpVideoFormat("bogus"),
    }));

}  // namespace blink
