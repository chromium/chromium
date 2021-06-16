// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/test/task_environment.h"
#include "media/base/video_codecs.h"
#include "media/video/mock_gpu_video_accelerator_factories.h"
#include "testing/gtest/include/gtest/gtest.h"
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
constexpr gfx::Size max_resolution = {1920, 1080};

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
        {media::VP8PROFILE_ANY, max_resolution, 30, 1},
        {media::VP9PROFILE_PROFILE0, max_resolution, 30, 1}};
    return profiles;
  }
};

}  // anonymous namespace

typedef webrtc::SdpVideoFormat Sdp;
typedef webrtc::SdpVideoFormat::Parameters Params;

class RTCVideoEncoderFactoryTest : public ::testing::Test {
 public:
  RTCVideoEncoderFactoryTest() : encoder_factory_(&mock_gpu_factories_) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  MockGpuVideoEncodeAcceleratorFactories mock_gpu_factories_;
  RTCVideoEncoderFactory encoder_factory_;
};

TEST_F(RTCVideoEncoderFactoryTest, QueryCodecSupportNoSvc) {
  EXPECT_CALL(mock_gpu_factories_, IsEncoderSupportKnown())
      .WillRepeatedly(Return(true));
  // VP8, H264, and VP9 profile 0 are supported.
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(
                         Sdp("VP8"), /*scalability_mode=*/absl::nullopt),
                     kSupportedPowerEfficient));
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(
                         Sdp("VP9"), /*scalability_mode=*/absl::nullopt),
                     kSupportedPowerEfficient));

  // H264, VP9 profile 2 and AV1 are unsupported.
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(
                         Sdp("H264", Params{{"level-asymmetry-allowed", "1"},
                                            {"packetization-mode", "1"},
                                            {"profile-level-id", "42001f"}}),
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
  // VP8 and VP9 supported for singles spatial layers.
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(Sdp("VP8"), "L1T2"),
                     kSupportedPowerEfficient));
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(Sdp("VP9"), "L1T3"),
                     kSupportedPowerEfficient));

  // VP9 support for spatial layers is conditionally supported/unsupported.
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(Sdp("VP9"), "L3T3"),
                     RTCVideoEncoder::Vp9HwSupportForSpatialLayers()
                         ? kSupportedPowerEfficient
                         : kUnsupported));

  // Valid SVC config but AV1 is not supported.
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(Sdp("AV1"), "L2T1"),
                     kUnsupported));

  // Invalid SVC config even though VP8 is supported.
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(Sdp("H264"), "L1T2"),
                     kUnsupported));
  EXPECT_TRUE(Equals(encoder_factory_.QueryCodecSupport(Sdp("VP8"), "L3T3"),
                     kUnsupported));
}

}  // namespace blink
