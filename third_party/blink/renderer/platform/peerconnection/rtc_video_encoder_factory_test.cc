// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/video_codecs.h"
#include "media/mojo/clients/mojo_video_encoder_metrics_provider.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder_factory.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/api/video_codecs/video_encoder_factory.h"

using ::testing::Return;

namespace blink {

namespace {
constexpr webrtc::VideoEncoderFactory::CodecSupport kSupportedPowerEfficient = {
    true, true};
constexpr webrtc::VideoEncoderFactory::CodecSupport kUnsupported = {false,
                                                                    false};
constexpr gfx::Size kMaxResolution = {1920, 1080};
constexpr uint32_t kMaxFramerateNumerator = 30;
constexpr uint32_t kMaxFramerateDenominator = 1;
const std::vector<media::SVCScalabilityMode> kScalabilityModes = {
    media::SVCScalabilityMode::kL1T1, media::SVCScalabilityMode::kL1T2,
    media::SVCScalabilityMode::kL1T3};

bool Equals(webrtc::VideoEncoderFactory::CodecSupport a,
            webrtc::VideoEncoderFactory::CodecSupport b) {
  return a.is_supported == b.is_supported &&
         a.is_power_efficient == b.is_power_efficient;
}

class MockGpuVideoEncodeAcceleratorFactories
    : public media::MockGpuVideoAcceleratorFactories {
 public:
  MockGpuVideoEncodeAcceleratorFactories()
      : MockGpuVideoAcceleratorFactories(nullptr) {}

  absl::optional<media::VideoEncodeAccelerator::SupportedProfiles>
  GetVideoEncodeAcceleratorSupportedProfiles() override {
    media::VideoEncodeAccelerator::SupportedProfiles profiles = {
        {media::H264PROFILE_BASELINE, kMaxResolution, kMaxFramerateNumerator,
         kMaxFramerateDenominator, media::VideoEncodeAccelerator::kConstantMode,
         kScalabilityModes},
        {media::H264PROFILE_BASELINE, kMaxResolution, kMaxFramerateNumerator,
         kMaxFramerateDenominator, media::VideoEncodeAccelerator::kConstantMode,
         kScalabilityModes},
        {media::VP8PROFILE_ANY, kMaxResolution, kMaxFramerateNumerator,
         kMaxFramerateDenominator, media::VideoEncodeAccelerator::kConstantMode,
         kScalabilityModes},
        {media::VP9PROFILE_PROFILE0, kMaxResolution, kMaxFramerateNumerator,
         kMaxFramerateDenominator, media::VideoEncodeAccelerator::kConstantMode,
         kScalabilityModes}};
    return profiles;
  }

  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() override {
    return base::SequencedTaskRunner::GetCurrentDefault();
  }
};

}  // anonymous namespace

typedef webrtc::SdpVideoFormat Sdp;
typedef webrtc::SdpVideoFormat::Parameters Params;

class RTCVideoEncoderFactoryTest : public ::testing::Test {
 public:
  RTCVideoEncoderFactoryTest()
      : encoder_factory_(&mock_gpu_factories_,
                         /*encoder_metrics_provider_factory=*/nullptr) {
    // Ensure all the profiles in our mock GPU factory are allowed.
    encoder_factory_.clear_disabled_profiles_for_testing();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  MockGpuVideoEncodeAcceleratorFactories mock_gpu_factories_;
  RTCVideoEncoderFactory encoder_factory_;
};

TEST_F(RTCVideoEncoderFactoryTest, QueryCodecSupportNoSvc) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine("MediaFoundationH264CbpEncoding", "");

  EXPECT_CALL(mock_gpu_factories_, IsEncoderSupportKnown())
      .WillRepeatedly(Return(true));
  // H.264 BP/CBP, VP8 and VP9 profile 0 are supported.
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(
                         Sdp("VP8"), /*scalability_mode=*/absl::nullopt),
                     kSupportedPowerEfficient));
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(
                         Sdp("VP9"), /*scalability_mode=*/absl::nullopt),
                     kSupportedPowerEfficient));
#if BUILDFLAG(RTC_USE_H264)
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(
                         Sdp("H264", Params{{"level-asymmetry-allowed", "1"},
                                            {"packetization-mode", "1"},
                                            {"profile-level-id", "42001f"}}),
                         /*scalability_mode=*/absl::nullopt),
                     kSupportedPowerEfficient));
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(
                         Sdp("H264", Params{{"level-asymmetry-allowed", "1"},
                                            {"packetization-mode", "1"},
                                            {"profile-level-id", "42c01f"}}),
                         /*scalability_mode=*/absl::nullopt),
                     kSupportedPowerEfficient));
#endif
#endif

  // H264 > BP, VP9 profile 2 and AV1 are unsupported.
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(
                         Sdp("H264", Params{{"level-asymmetry-allowed", "1"},
                                            {"packetization-mode", "1"},
                                            {"profile-level-id", "4d001f"}}),
                         /*scalability_mode=*/absl::nullopt),
                     kUnsupported));
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(
                         Sdp("VP9", Params{{"profile-id", "2"}}),
                         /*scalability_mode=*/absl::nullopt),
                     kUnsupported));
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(
                         Sdp("AV1"), /*scalability_mode=*/absl::nullopt),
                     kUnsupported));
}

TEST_F(RTCVideoEncoderFactoryTest, QueryCodecSupportSvc) {
  EXPECT_CALL(mock_gpu_factories_, IsEncoderSupportKnown())
      .WillRepeatedly(Return(true));
  // Test supported modes.
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(Sdp("VP8"), "L1T2"),
                     kSupportedPowerEfficient));
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(Sdp("VP9"), "L1T3"),
                     kSupportedPowerEfficient));

  // Test unsupported modes.
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(Sdp("AV1"), "L2T1"),
                     kUnsupported));
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(Sdp("H264"), "L2T2"),
                     kUnsupported));
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(Sdp("VP8"), "L3T3"),
                     kUnsupported));
}

}  // namespace blink
