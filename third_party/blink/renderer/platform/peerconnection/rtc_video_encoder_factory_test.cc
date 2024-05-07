// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder_factory.h"

#include <stdint.h>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "media/base/media_switches.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/video_codecs.h"
#include "media/mojo/clients/mojo_video_encoder_metrics_provider.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "media/webrtc/webrtc_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder.h"
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

#if BUILDFLAG(RTC_USE_H265)
// Settings from video toolbox encoder.
constexpr gfx::Size kHEVCMaxResolution = {4096, 2304};
constexpr uint32_t kHEVCMaxFramerateNumerator = 120;
#endif  // BUILDFLAG(RTC_USE_H265)

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

  std::optional<media::VideoEncodeAccelerator::SupportedProfiles>
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
         kScalabilityModes},
#if BUILDFLAG(RTC_USE_H265)
        {media::HEVCPROFILE_MAIN, kHEVCMaxResolution,
         kHEVCMaxFramerateNumerator, kMaxFramerateDenominator,
         media::VideoEncodeAccelerator::kConstantMode, kScalabilityModes}
#endif  //  BUILDFLAG(RTC_USE_H265)
    };
    return profiles;
  }

  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() override {
    return base::SequencedTaskRunner::GetCurrentDefault();
  }
};

}  // anonymous namespace

class RTCVideoEncoderFactoryTest : public ::testing::Test {
 public:
  RTCVideoEncoderFactoryTest()
      : encoder_factory_(&mock_gpu_factories_,
                         /*encoder_metrics_provider_factory=*/nullptr) {}
  // Ensure all the profiles in our mock GPU factory are allowed.
  void ClearDisabledProfilesForTesting() {
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

  ClearDisabledProfilesForTesting();
  EXPECT_CALL(mock_gpu_factories_, IsEncoderSupportKnown())
      .WillRepeatedly(Return(true));
  // H.264 BP/CBP, VP8 and VP9 profile 0 are supported.
  EXPECT_TRUE(Equals(
      encoder_factory_.QueryCodecSupport(webrtc::SdpVideoFormat("VP8"),
                                         /*scalability_mode=*/std::nullopt),
      kSupportedPowerEfficient));
  EXPECT_TRUE(Equals(
      encoder_factory_.QueryCodecSupport(webrtc::SdpVideoFormat("VP9"),
                                         /*scalability_mode=*/std::nullopt),
      kSupportedPowerEfficient));
#if BUILDFLAG(RTC_USE_H264)
  EXPECT_TRUE(Equals(
      encoder_factory_.QueryCodecSupport(
          webrtc::SdpVideoFormat("H264", {{"level-asymmetry-allowed", "1"},
                                          {"packetization-mode", "1"},
                                          {"profile-level-id", "42001f"}}),
          /*scalability_mode=*/std::nullopt),
      kSupportedPowerEfficient));
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  EXPECT_TRUE(Equals(
      encoder_factory_.QueryCodecSupport(
          webrtc::SdpVideoFormat("H264", {{"level-asymmetry-allowed", "1"},
                                          {"packetization-mode", "1"},
                                          {"profile-level-id", "42c01f"}}),
          /*scalability_mode=*/std::nullopt),
      kSupportedPowerEfficient));
#endif
#endif

  // H264 > BP, VP9 profile 2 and AV1 are unsupported.
  EXPECT_TRUE(Equals(
      encoder_factory_.QueryCodecSupport(
          webrtc::SdpVideoFormat("H264", {{"level-asymmetry-allowed", "1"},
                                          {"packetization-mode", "1"},
                                          {"profile-level-id", "4d001f"}}),
          /*scalability_mode=*/std::nullopt),
      kUnsupported));
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(
                         webrtc::SdpVideoFormat("VP9", {{"profile-id", "2"}}),
                         /*scalability_mode=*/std::nullopt),
                     kUnsupported));
  EXPECT_TRUE(Equals(
      encoder_factory_.QueryCodecSupport(webrtc::SdpVideoFormat("AV1"),
                                         /*scalability_mode=*/std::nullopt),
      kUnsupported));
}

TEST_F(RTCVideoEncoderFactoryTest, QueryCodecSupportSvc) {
  ClearDisabledProfilesForTesting();
  EXPECT_CALL(mock_gpu_factories_, IsEncoderSupportKnown())
      .WillRepeatedly(Return(true));
  // Test supported modes.
  EXPECT_TRUE(Equals(
      encoder_factory_.QueryCodecSupport(webrtc::SdpVideoFormat("VP8"), "L1T2"),
      kSupportedPowerEfficient));
  EXPECT_TRUE(Equals(
      encoder_factory_.QueryCodecSupport(webrtc::SdpVideoFormat("VP9"), "L1T3"),
      kSupportedPowerEfficient));

  // Test unsupported modes.
  EXPECT_TRUE(Equals(
      encoder_factory_.QueryCodecSupport(webrtc::SdpVideoFormat("AV1"), "L2T1"),
      kUnsupported));
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(
                         webrtc::SdpVideoFormat("H264"), "L2T2"),
                     kUnsupported));
  EXPECT_TRUE(Equals(
      encoder_factory_.QueryCodecSupport(webrtc::SdpVideoFormat("VP8"), "L3T3"),
      kUnsupported));
}

#if BUILDFLAG(RTC_USE_H265)
TEST_F(RTCVideoEncoderFactoryTest,
       QueryCodecSupportForH265WithoutNeccessaryFeatures) {
  base::test::ScopedFeatureList scoped_feature_list;
  EXPECT_CALL(mock_gpu_factories_, IsEncoderSupportKnown())
      .WillRepeatedly(Return(true));

  // H.256 is not supported when WebRtcAllowH265Send is not enabled.
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(
                         webrtc::SdpVideoFormat("H265", {{"profile-id", "1"}}),
                         /*scalability_mode=*/std::nullopt),
                     kUnsupported));

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)
  // H.265 is not supported when WebRtcAllowH265Send is enabled but
  // PlatformHEVCEncoderSupport is disabled.
  scoped_feature_list.InitWithFeatures({::features::kWebRtcAllowH265Send},
                                       {media::kPlatformHEVCEncoderSupport});
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(
                         webrtc::SdpVideoFormat("H265", {{"profile-id", "1"}}),
                         /*scalability_mode=*/std::nullopt),
                     kUnsupported));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)
}

TEST_F(RTCVideoEncoderFactoryTest,
       QueryCodecSupportForH265WithNeccessaryFeatures) {
  ClearDisabledProfilesForTesting();
  base::test::ScopedFeatureList scoped_feature_list;
  std::vector<base::test::FeatureRef> enabled_features;
  enabled_features.emplace_back(::features::kWebRtcAllowH265Send);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)
  enabled_features.emplace_back(media::kPlatformHEVCEncoderSupport);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)

  scoped_feature_list.InitWithFeatures(enabled_features, {});

  EXPECT_CALL(mock_gpu_factories_, IsEncoderSupportKnown())
      .WillRepeatedly(Return(true));

  // H.265 main profile is supported when both WebRtcAllowH265Send and
  // PlatformHEVCEncoderSupport are enabled. level-id, when not specified,
  // implies level 93, and tier-flag defaults to main tier.
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(
                         webrtc::SdpVideoFormat("H265", {{"profile-id", "1"}}),
                         /*scalability_mode=*/std::nullopt),
                     kSupportedPowerEfficient));

  // GPU factory reports maximum supported level to be 5.2, which is higher than
  // 3.1. As a result, RTC encoder factory reports level 3.1 to be supported as
  // well.
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(
                         webrtc::SdpVideoFormat(
                             "H265", {{"profile-id", "1"}, {"level-id", "93"}}),
                         /*scalability_mode=*/std::nullopt),
                     kSupportedPowerEfficient));

  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(
                         webrtc::SdpVideoFormat("H265", {{"profile-id", "1"},
                                                         {"level-id", "156"}}),
                         /*scalability_mode=*/std::nullopt),
                     kSupportedPowerEfficient));

  // Main10 profile is not supported by mock factory here.
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(
                         webrtc::SdpVideoFormat("H265", {{"profile-id", "2"}}),
                         /*scalability_mode=*/std::nullopt),
                     kUnsupported));
}
#endif  // BUILDFLAG(RTC_USE_H265)

}  // namespace blink
