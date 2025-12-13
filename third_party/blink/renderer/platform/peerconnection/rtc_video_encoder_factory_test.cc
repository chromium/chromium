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
using ::testing::UnorderedElementsAre;

namespace blink {

namespace {

constexpr webrtc::VideoEncoderFactory::CodecSupport kSupportedPowerEfficient = {
    true, true};
constexpr webrtc::VideoEncoderFactory::CodecSupport kUnsupported = {false,
                                                                    false};
constexpr gfx::Size kMaxResolution = {1920, 1080};
constexpr uint32_t kMaxFramerateNumerator = 30;
constexpr gfx::Size kLowResolution = {320, 240};

#if BUILDFLAG(RTC_USE_H265)
// Settings from video toolbox encoder.
constexpr gfx::Size kHEVCMaxResolution = {4096, 2304};
constexpr uint32_t kHEVCMaxFramerateNumerator = 120;
#endif  // BUILDFLAG(RTC_USE_H265)

constexpr uint32_t kMaxFramerateDenominator = 1;
const std::vector<media::SVCScalabilityMode> kSVCScalabilityModes = {
    media::SVCScalabilityMode::kL1T1, media::SVCScalabilityMode::kL1T2,
    media::SVCScalabilityMode::kL1T3};
using ScalbilityModeMap ALLOW_DISCOURAGED_TYPE("Match WebRTC type") =
    absl::InlinedVector<webrtc::ScalabilityMode, webrtc::kScalabilityModeCount>;
const ScalbilityModeMap kScalabilityModes = {webrtc::ScalabilityMode::kL1T1,
                                             webrtc::ScalabilityMode::kL1T2,
                                             webrtc::ScalabilityMode::kL1T3};
const ScalbilityModeMap kReducedScalabilityModes = {
    webrtc::ScalabilityMode::kL1T1, webrtc::ScalabilityMode::kL1T2};
const ScalbilityModeMap kNoLayeringScalabilityModes = {
    webrtc::ScalabilityMode::kL1T1};

const webrtc::SdpVideoFormat kVp8Sdp("VP8", {}, kScalabilityModes);
const webrtc::SdpVideoFormat kVp9Profile0Sdp("VP9",
                                             {{"profile-id", "0"}},
                                             kScalabilityModes);
// TODO(http://crbugs.com/376306259): Ensure hardware encoder factory include
// profile-id/tier/level-idx in AV1 SDP.
const webrtc::SdpVideoFormat kAv1Profile0Sdp("AV1", {}, kScalabilityModes);
const webrtc::SdpVideoFormat kH264BaselinePacketizatonMode1Sdp(
    "H264",
    {{"level-asymmetry-allowed", "1"},
     {"packetization-mode", "1"},
     {"profile-level-id", "42001f"}},
    kScalabilityModes);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
const webrtc::SdpVideoFormat kH264ConstrainedBaselinePacketizatonMode1Sdp(
    "H264",
    {{"level-asymmetry-allowed", "1"},
     {"packetization-mode", "1"},
     {"profile-level-id", "42e01f"}},
    kScalabilityModes);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(RTC_USE_H265)
const webrtc::SdpVideoFormat kH265MainProfileLevel31Sdp("H265",
                                                        {{"profile-id", "1"},
                                                         {"tier-flag", "0"},
                                                         {"level-id", "93"},
                                                         {"tx-mode", "SRST"}},
                                                        kScalabilityModes);
const webrtc::SdpVideoFormat kH265MainProfileLevel52Sdp("H265",
                                                        {{"profile-id", "1"},
                                                         {"tier-flag", "0"},
                                                         {"level-id", "156"},
                                                         {"tx-mode", "SRST"}},
                                                        kScalabilityModes);
const webrtc::SdpVideoFormat kH265Main10ProfileLevel31Sdp("H265",
                                                          {{"profile-id", "2"},
                                                           {"tier-flag", "0"},
                                                           {"level-id", "93"},
                                                           {"tx-mode", "SRST"}},
                                                          kScalabilityModes);
const webrtc::SdpVideoFormat kH265MainProfileLevel52SdpL1T2(
    "H265",
    {{"profile-id", "1"},
     {"tier-flag", "0"},
     {"level-id", "156"},
     {"tx-mode", "SRST"}},
    kReducedScalabilityModes);
const webrtc::SdpVideoFormat kH265Main10ProfileLevel31SdpL1T2(
    "H265",
    {{"profile-id", "2"},
     {"tier-flag", "0"},
     {"level-id", "93"},
     {"tx-mode", "SRST"}},
    kReducedScalabilityModes);
const webrtc::SdpVideoFormat kH265MainProfileLevel52SdpL1T1(
    "H265",
    {{"profile-id", "1"},
     {"tier-flag", "0"},
     {"level-id", "156"},
     {"tx-mode", "SRST"}},
    kNoLayeringScalabilityModes);
const webrtc::SdpVideoFormat kH265Main10ProfileLevel31SdpL1T1(
    "H265",
    {{"profile-id", "2"},
     {"tier-flag", "0"},
     {"level-id", "93"},
     {"tx-mode", "SRST"}},
    kNoLayeringScalabilityModes);
#endif  // BUILDFLAG(RTC_USE_H265)

bool Equals(webrtc::VideoEncoderFactory::CodecSupport a,
            webrtc::VideoEncoderFactory::CodecSupport b) {
  return a.is_supported == b.is_supported &&
         a.is_power_efficient == b.is_power_efficient;
}

#if BUILDFLAG(RTC_USE_H265)
void MaybeEnableOpenH264SoftwareEncoder(
    std::vector<base::test::FeatureRef>& enabled_features) {
#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS) && BUILDFLAG(ENABLE_OPENH264)
  enabled_features.push_back(media::kOpenH264SoftwareEncoder);
#endif
}
#endif  //  BUILDFLAG(RTC_USE_H265)

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
         kSVCScalabilityModes},
        {media::H264PROFILE_BASELINE, kMaxResolution, kMaxFramerateNumerator,
         kMaxFramerateDenominator, media::VideoEncodeAccelerator::kConstantMode,
         kSVCScalabilityModes},
        // H264 with mismatch between profile and resolution should be ignored.
        {media::H264PROFILE_HIGH, kLowResolution, kMaxFramerateNumerator,
         kMaxFramerateDenominator, media::VideoEncodeAccelerator::kConstantMode,
         kSVCScalabilityModes},
        {media::VP8PROFILE_ANY, kMaxResolution, kMaxFramerateNumerator,
         kMaxFramerateDenominator, media::VideoEncodeAccelerator::kConstantMode,
         kSVCScalabilityModes},
        {media::VP9PROFILE_PROFILE0, kMaxResolution, kMaxFramerateNumerator,
         kMaxFramerateDenominator, media::VideoEncodeAccelerator::kConstantMode,
         kSVCScalabilityModes},
        {media::AV1PROFILE_PROFILE_MAIN, kMaxResolution, kMaxFramerateNumerator,
         kMaxFramerateDenominator, media::VideoEncodeAccelerator::kConstantMode,
         kSVCScalabilityModes},
#if BUILDFLAG(RTC_USE_H265)
        {media::HEVCPROFILE_MAIN, kHEVCMaxResolution,
         kHEVCMaxFramerateNumerator, kMaxFramerateDenominator,
         media::VideoEncodeAccelerator::kConstantMode, kSVCScalabilityModes},
        // The profile below will produce HEVC level 3.1, which we expect not to
        // be reported as the supported level, since the profile above
        // produces HEVC level 5.2, which we will report as supported level.
        {media::HEVCPROFILE_MAIN, kMaxResolution, kHEVCMaxFramerateNumerator,
         kMaxFramerateDenominator, media::VideoEncodeAccelerator::kConstantMode,
         kSVCScalabilityModes},
        {media::HEVCPROFILE_MAIN10, kMaxResolution, kHEVCMaxFramerateNumerator,
         kMaxFramerateDenominator, media::VideoEncodeAccelerator::kConstantMode,
         kSVCScalabilityModes}
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
  scoped_feature_list.InitFromCommandLine("PlatformH264CbpEncoding", "");

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
  EXPECT_TRUE(Equals(
      encoder_factory_.QueryCodecSupport(webrtc::SdpVideoFormat("AV1"),
                                         /*scalability_mode=*/std::nullopt),
      kSupportedPowerEfficient));

  // H264 > BP and VP9 profile 2 are unsupported.
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
       QueryCodecSupportH265WithWebRtcAllowH265SendDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({}, {::features::kWebRtcAllowH265Send});

  EXPECT_CALL(mock_gpu_factories_, IsEncoderSupportKnown())
      .WillRepeatedly(Return(true));

  // The `disabled_profiles_` is set at construction time so we must create the
  // encoder factory *after* InitWithFeatures in order for QueryCodecSupport()
  // to say kUnsupported.
  RTCVideoEncoderFactory encoder_factory(&mock_gpu_factories_, nullptr);
  EXPECT_TRUE(Equals(encoder_factory.QueryCodecSupport(
                         webrtc::SdpVideoFormat("H265", {{"profile-id", "1"}}),
                         /*scalability_mode=*/std::nullopt),
                     kUnsupported));
}

TEST_F(RTCVideoEncoderFactoryTest, QueryCodecSupportForH265) {
  ClearDisabledProfilesForTesting();
  EXPECT_CALL(mock_gpu_factories_, IsEncoderSupportKnown())
      .WillRepeatedly(Return(true));

  // H.265 main profile is supported by default. level-id, when not specified,
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

  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(
                         webrtc::SdpVideoFormat("H265", {{"profile-id", "1"},
                                                         {"level-id", "180"}}),
                         /*scalability_mode=*/std::nullopt),
                     kUnsupported));

  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(
                         webrtc::SdpVideoFormat("H265", {{"profile-id", "2"}}),
                         /*scalability_mode=*/std::nullopt),
                     kSupportedPowerEfficient));
}

TEST_F(RTCVideoEncoderFactoryTest, GetSupportedFormatsReturnsAllExpectedModes) {
  ClearDisabledProfilesForTesting();
  base::test::ScopedFeatureList scoped_feature_list;
  std::vector<base::test::FeatureRef> enabled_features;
  enabled_features.emplace_back(::features::kWebRtcH265L1T2);
  enabled_features.emplace_back(::features::kWebRtcH265L1T3);
  MaybeEnableOpenH264SoftwareEncoder(enabled_features);

  scoped_feature_list.InitWithFeatures(enabled_features, {});
  EXPECT_CALL(mock_gpu_factories_, IsEncoderSupportKnown())
      .WillRepeatedly(Return(true));

  EXPECT_THAT(encoder_factory_.GetSupportedFormats(),
              UnorderedElementsAre(
                  kH264BaselinePacketizatonMode1Sdp,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
                  kH264ConstrainedBaselinePacketizatonMode1Sdp,
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
                  kVp8Sdp, kVp9Profile0Sdp, kH265MainProfileLevel52Sdp,
                  kH265Main10ProfileLevel31Sdp, kAv1Profile0Sdp));
}

// When WebRtcH265L1T3 flag is not enabled, GetSupportedFormats should exclude
// L1T3 from supported H.265 scalability modes.
TEST_F(RTCVideoEncoderFactoryTest,
       GetSupportedFormatsReturnsAllModesExceptH265L1T3) {
  ClearDisabledProfilesForTesting();
  base::test::ScopedFeatureList scoped_feature_list;
  std::vector<base::test::FeatureRef> enabled_features;
  enabled_features.emplace_back(::features::kWebRtcH265L1T2);
  MaybeEnableOpenH264SoftwareEncoder(enabled_features);

  scoped_feature_list.InitWithFeatures(enabled_features, {});
  EXPECT_CALL(mock_gpu_factories_, IsEncoderSupportKnown())
      .WillRepeatedly(Return(true));

  EXPECT_THAT(encoder_factory_.GetSupportedFormats(),
              UnorderedElementsAre(
                  kH264BaselinePacketizatonMode1Sdp,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
                  kH264ConstrainedBaselinePacketizatonMode1Sdp,
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
                  kVp8Sdp, kVp9Profile0Sdp, kH265MainProfileLevel52SdpL1T2,
                  kH265Main10ProfileLevel31SdpL1T2, kAv1Profile0Sdp));
}

// When both WebRtcH265L1T2 and WebRtcH265L1T2 flags are disabled,
// GetSupportedFormats should exclude both L1T2 and L1T3 from supported H.265
// scalability modes.
TEST_F(RTCVideoEncoderFactoryTest,
       GetSupportedFormatsReturnsAllModesExceptH265L1T2AndL1T3) {
  ClearDisabledProfilesForTesting();
  base::test::ScopedFeatureList scoped_feature_list;
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
  disabled_features.emplace_back(::features::kWebRtcH265L1T2);
  MaybeEnableOpenH264SoftwareEncoder(enabled_features);

  scoped_feature_list.InitWithFeatures(enabled_features, disabled_features);
  EXPECT_CALL(mock_gpu_factories_, IsEncoderSupportKnown())
      .WillRepeatedly(Return(true));

  EXPECT_THAT(encoder_factory_.GetSupportedFormats(),
              UnorderedElementsAre(
                  kH264BaselinePacketizatonMode1Sdp,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
                  kH264ConstrainedBaselinePacketizatonMode1Sdp,
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
                  kVp8Sdp, kVp9Profile0Sdp, kH265MainProfileLevel52SdpL1T1,
                  kH265Main10ProfileLevel31SdpL1T1, kAv1Profile0Sdp));
}
#endif  // BUILDFLAG(RTC_USE_H265)

TEST_F(RTCVideoEncoderFactoryTest, SupportedFormatsHaveScalabilityModes) {
  base::test::ScopedFeatureList scoped_feature_list;
  std::vector<base::test::FeatureRef> enabled_features;
  enabled_features.emplace_back(::features::kWebRtcH265L1T2);
  enabled_features.emplace_back(::features::kWebRtcH265L1T3);
  scoped_feature_list.InitWithFeatures(enabled_features, {});

  ClearDisabledProfilesForTesting();
  EXPECT_CALL(mock_gpu_factories_, IsEncoderSupportKnown())
      .WillRepeatedly(Return(true));

  auto supported_formats = encoder_factory_.GetSupportedFormats();
  for (const auto& format : supported_formats) {
    EXPECT_THAT(format.scalability_modes,
                testing::UnorderedElementsAreArray(kScalabilityModes));
  }
}

}  // namespace blink
