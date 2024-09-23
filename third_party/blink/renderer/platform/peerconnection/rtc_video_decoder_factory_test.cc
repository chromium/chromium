// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_factory.h"

#include <stdint.h>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "media/base/platform_features.h"
#include "media/base/video_codecs.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "media/video/video_decode_accelerator.h"
#include "media/webrtc/webrtc_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/api/video_codecs/video_decoder_factory.h"

using ::testing::Return;
using ::testing::UnorderedElementsAre;

namespace blink {

namespace {

const webrtc::SdpVideoFormat kVp9Profile0Sdp("VP9", {{"profile-id", "0"}});
const webrtc::SdpVideoFormat kVp9Profile1Sdp("VP9", {{"profile-id", "1"}});
const webrtc::SdpVideoFormat kVp9Profile2Sdp("VP9", {{"profile-id", "2"}});
const webrtc::SdpVideoFormat kAv1Sdp("AV1", {});
const webrtc::SdpVideoFormat kH264CbPacketizatonMode0Sdp(
    "H264",
    {{"level-asymmetry-allowed", "1"},
     {"packetization-mode", "0"},
     {"profile-level-id", "42e01f"}});
const webrtc::SdpVideoFormat kH264CbPacketizatonMode1Sdp(
    "H264",
    {{"level-asymmetry-allowed", "1"},
     {"packetization-mode", "1"},
     {"profile-level-id", "42e01f"}});
const webrtc::SdpVideoFormat kH264BaselinePacketizatonMode0Sdp(
    "H264",
    {{"level-asymmetry-allowed", "1"},
     {"packetization-mode", "0"},
     {"profile-level-id", "42001f"}});
const webrtc::SdpVideoFormat kH264BaselinePacketizatonMode1Sdp(
    "H264",
    {{"level-asymmetry-allowed", "1"},
     {"packetization-mode", "1"},
     {"profile-level-id", "42001f"}});
const webrtc::SdpVideoFormat kH264MainPacketizatonMode0Sdp(
    "H264",
    {{"level-asymmetry-allowed", "1"},
     {"packetization-mode", "0"},
     {"profile-level-id", "4d001f"}});
const webrtc::SdpVideoFormat kH264MainPacketizatonMode1Sdp(
    "H264",
    {{"level-asymmetry-allowed", "1"},
     {"packetization-mode", "1"},
     {"profile-level-id", "4d001f"}});
#if BUILDFLAG(RTC_USE_H265)
const webrtc::SdpVideoFormat kH265MainProfileSdp("H265",
                                                 {{"profile-id", "1"},
                                                  {"tier-flag", "0"},
                                                  {"level-id", "93"},
                                                  {"tx-mode", "SRST"}});
const webrtc::SdpVideoFormat kH265Main10ProfileSdp("H265",
                                                   {{"profile-id", "2"},
                                                    {"tier-flag", "0"},
                                                    {"level-id", "93"},
                                                    {"tx-mode", "SRST"}});
#endif  // BUILDFLAG(RTC_USE_H265)

bool Equals(webrtc::VideoDecoderFactory::CodecSupport a,
            webrtc::VideoDecoderFactory::CodecSupport b) {
  return a.is_supported == b.is_supported &&
         a.is_power_efficient == b.is_power_efficient;
}

constexpr webrtc::VideoDecoderFactory::CodecSupport kSupportedPowerEfficient = {
    true, true};
constexpr webrtc::VideoDecoderFactory::CodecSupport kUnsupported = {false,
                                                                    false};
class MockGpuVideoDecodeAcceleratorFactories
    : public media::MockGpuVideoAcceleratorFactories {
 public:
  MockGpuVideoDecodeAcceleratorFactories()
      : MockGpuVideoAcceleratorFactories(nullptr) {}

  Supported IsDecoderConfigSupported(
      const media::VideoDecoderConfig& config) override {
    if (config.codec() == media::VideoCodec::kVP9 ||
        config.codec() == media::VideoCodec::kAV1) {
      return Supported::kTrue;
    } else if (config.codec() == media::VideoCodec::kH264) {
      if (config.profile() == media::VideoCodecProfile::H264PROFILE_BASELINE ||
          config.profile() == media::VideoCodecProfile::H264PROFILE_MAIN) {
        return Supported::kTrue;
      } else {
        return Supported::kFalse;
      }
    }
#if BUILDFLAG(RTC_USE_H265)
    else if (config.codec() == media::VideoCodec::kHEVC) {
      if (config.profile() == media::VideoCodecProfile::HEVCPROFILE_MAIN) {
        return Supported::kTrue;
      } else {
        return Supported::kFalse;
      }
    }
#endif  // BUILDFLAG(RTC_USE_H265)
    else {
      return Supported::kFalse;
    }
  }
};

}  // anonymous namespace

class RTCVideoDecoderFactoryTest : public ::testing::Test {
 public:
  RTCVideoDecoderFactoryTest() : decoder_factory_(&mock_gpu_factories_, {}) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  MockGpuVideoDecodeAcceleratorFactories mock_gpu_factories_;
  RTCVideoDecoderFactory decoder_factory_;
};

TEST_F(RTCVideoDecoderFactoryTest, QueryCodecSupportReturnsExpectedResults) {
  EXPECT_CALL(mock_gpu_factories_, IsDecoderSupportKnown())
      .WillRepeatedly(Return(true));

  // VP8 is not supported
  EXPECT_TRUE(
      Equals(decoder_factory_.QueryCodecSupport(webrtc::SdpVideoFormat("VP8"),
                                                false /*reference_scaling*/),
             kUnsupported));

  // H264 high profile is not supported
  EXPECT_TRUE(Equals(
      decoder_factory_.QueryCodecSupport(
          webrtc::SdpVideoFormat("H264", {{"level-asymmetry-allowed", "1"},
                                          {"packetization-mode", "1"},
                                          {"profile-level-id", "64001f"}}),
          false /*reference_scaling*/),
      kUnsupported));

  // VP9, H264 & AV1 decode should be supported without reference scaling.
  EXPECT_TRUE(
      Equals(decoder_factory_.QueryCodecSupport(webrtc::SdpVideoFormat("VP9"),
                                                false /*reference_scaling*/),
             kSupportedPowerEfficient));
  EXPECT_TRUE(
      Equals(decoder_factory_.QueryCodecSupport(webrtc::SdpVideoFormat("AV1"),
                                                false /*reference_scaling*/),
             kSupportedPowerEfficient));
  EXPECT_TRUE(Equals(
      decoder_factory_.QueryCodecSupport(
          webrtc::SdpVideoFormat("H264", {{"level-asymmetry-allowed", "1"},
                                          {"packetization-mode", "1"},
                                          {"profile-level-id", "42001f"}}),
          false /*reference_scaling*/),
      kSupportedPowerEfficient));

  // AV1 decode should be supported with reference scaling.
  EXPECT_TRUE(
      Equals(decoder_factory_.QueryCodecSupport(webrtc::SdpVideoFormat("AV1"),
                                                true /*reference_scaling*/),
             kSupportedPowerEfficient));

  // VP9 decode supported depending on platform.
  EXPECT_TRUE(
      Equals(decoder_factory_.QueryCodecSupport(webrtc::SdpVideoFormat("VP9"),
                                                true /*reference_scaling*/),
             media::IsVp9kSVCHWDecodingEnabled() ? kSupportedPowerEfficient
                                                 : kUnsupported));

  // H264 decode not supported with reference scaling.
  EXPECT_TRUE(Equals(
      decoder_factory_.QueryCodecSupport(
          webrtc::SdpVideoFormat("H264", {{"level-asymmetry-allowed", "1"},
                                          {"packetization-mode", "1"},
                                          {"profile-level-id", "42001f"}}),
          true /*reference_scaling*/),
      kUnsupported));

  // If WebRTCAllowH265Receive is not enabled, H.265 decode should not be
  // supported.
  EXPECT_TRUE(
      Equals(decoder_factory_.QueryCodecSupport(webrtc::SdpVideoFormat("H265"),
                                                false /*reference_scaling*/),
             kUnsupported));
}

TEST_F(RTCVideoDecoderFactoryTest, GetSupportedFormatsReturnsAllExpectedModes) {
  EXPECT_CALL(mock_gpu_factories_, IsDecoderSupportKnown())
      .WillRepeatedly(Return(true));

  EXPECT_THAT(
      decoder_factory_.GetSupportedFormats(),
      UnorderedElementsAre(
          kH264CbPacketizatonMode0Sdp, kH264CbPacketizatonMode1Sdp,
          kH264BaselinePacketizatonMode0Sdp, kH264BaselinePacketizatonMode1Sdp,
          kH264MainPacketizatonMode0Sdp, kH264MainPacketizatonMode1Sdp,
          kVp9Profile0Sdp, kVp9Profile1Sdp, kVp9Profile2Sdp, kAv1Sdp));
}

#if BUILDFLAG(RTC_USE_H265)
TEST_F(RTCVideoDecoderFactoryTest,
       QueryCodecSupportH265WithWebRtcAllowH265ReceiveEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({::features::kWebRtcAllowH265Receive},
                                       {});
  EXPECT_CALL(mock_gpu_factories_, IsDecoderSupportKnown())
      .WillRepeatedly(Return(true));

  // H265 decode should be supported without reference scaling.
  EXPECT_TRUE(
      Equals(decoder_factory_.QueryCodecSupport(webrtc::SdpVideoFormat("H265"),
                                                false /*reference_scaling*/),
             kSupportedPowerEfficient));

  // H265 decode should not be supported with reference scaling.
  EXPECT_TRUE(
      Equals(decoder_factory_.QueryCodecSupport(webrtc::SdpVideoFormat("H265"),
                                                true /*reference_scaling*/),
             kUnsupported));

  // H265 decode should be supported with main profile explicitly configured.
  EXPECT_TRUE(Equals(decoder_factory_.QueryCodecSupport(
                         webrtc::SdpVideoFormat("H265", {{"profile-id", "1"}}),
                         false /*reference_scaling*/),
                     kSupportedPowerEfficient));

  // H265 main10 profile is not supported.
  EXPECT_TRUE(Equals(decoder_factory_.QueryCodecSupport(
                         webrtc::SdpVideoFormat("H265", {{"profile-id", "2"}}),
                         false /*reference_scaling*/),
                     kUnsupported));

  EXPECT_THAT(
      decoder_factory_.GetSupportedFormats(),
      UnorderedElementsAre(
          kH264CbPacketizatonMode0Sdp, kH264CbPacketizatonMode1Sdp,
          kH264BaselinePacketizatonMode0Sdp, kH264BaselinePacketizatonMode1Sdp,
          kH264MainPacketizatonMode0Sdp, kH264MainPacketizatonMode1Sdp,
          kVp9Profile0Sdp, kVp9Profile1Sdp, kVp9Profile2Sdp, kAv1Sdp,
          kH265MainProfileSdp));
}
#endif  // BUILDFLAG(RTC_USE_H265)
}  // namespace blink
