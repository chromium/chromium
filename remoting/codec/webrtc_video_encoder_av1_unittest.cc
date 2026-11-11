// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/webrtc_video_encoder_av1.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

namespace {

std::unique_ptr<webrtc::DesktopFrame> CreateFrame(
    int width,
    int height,
    const webrtc::DesktopRegion& updated_region) {
  auto frame = std::make_unique<webrtc::BasicDesktopFrame>(
      webrtc::DesktopSize(width, height));
  *frame->mutable_updated_region() = updated_region;
  return frame;
}

WebrtcVideoEncoder::FrameParams MakeFrameParams(int bitrate_kbps,
                                                bool key_frame,
                                                bool clear_active_map) {
  WebrtcVideoEncoder::FrameParams params;
  params.bitrate_kbps = bitrate_kbps;
  params.duration = base::Milliseconds(33);
  params.fps = 30;
  params.vpx_min_quantizer = WebrtcVideoEncoder::kMinQuantizer;
  params.vpx_max_quantizer = WebrtcVideoEncoder::kMaxQuantizer;
  params.key_frame = key_frame;
  params.clear_active_map = clear_active_map;
  return params;
}

}  // namespace

class WebrtcVideoEncoderAV1Test : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebrtcVideoEncoderAV1Test, AppliesTargetBitrateOnInitialEncode) {
  WebrtcVideoEncoderAV1 encoder;

  // Initial encode applies `params.bitrate_kbps` before `InitializeCodec()` so
  // `aom_codec_enc_init()` sees 4000 kbps rather than the default floor.
  {
    base::test::TestFuture<WebrtcVideoEncoder::EncodeResult,
                           std::unique_ptr<WebrtcVideoEncoder::EncodedFrame>>
        future;
    encoder.Encode(CreateFrame(320, 320,
                               webrtc::DesktopRegion(
                                   webrtc::DesktopRect::MakeWH(320, 320))),
                   MakeFrameParams(/*bitrate_kbps=*/4000, /*key_frame=*/true,
                                   /*clear_active_map=*/true),
                   future.GetCallback());
    auto [result, encoded_frame] = future.Take();
    EXPECT_EQ(result, WebrtcVideoEncoder::EncodeResult::SUCCEEDED);
    ASSERT_NE(encoded_frame, nullptr);
    EXPECT_EQ(encoder.last_init_target_bitrate_kbps_for_testing(), 4000u);
  }

  // Subsequent same-size frame with a >33% weighted-average jump (10000 kbps ->
  // ~7076 kbps) updates `config_` in place via `aom_codec_enc_config_set()`
  // without re-running `InitializeCodec()`, so the recorded init bitrate
  // remains 4000 kbps.
  {
    base::test::TestFuture<WebrtcVideoEncoder::EncodeResult,
                           std::unique_ptr<WebrtcVideoEncoder::EncodedFrame>>
        future;
    encoder.Encode(CreateFrame(320, 320,
                               webrtc::DesktopRegion(
                                   webrtc::DesktopRect::MakeWH(320, 320))),
                   MakeFrameParams(/*bitrate_kbps=*/10000, /*key_frame=*/false,
                                   /*clear_active_map=*/true),
                   future.GetCallback());
    auto [result, encoded_frame] = future.Take();
    EXPECT_EQ(result, WebrtcVideoEncoder::EncodeResult::SUCCEEDED);
    ASSERT_NE(encoded_frame, nullptr);
    EXPECT_EQ(encoder.last_init_target_bitrate_kbps_for_testing(), 4000u);
  }
}

TEST_F(WebrtcVideoEncoderAV1Test, AppliesMinimumBitrateOnResize) {
  WebrtcVideoEncoderAV1 encoder;

  {
    base::test::TestFuture<WebrtcVideoEncoder::EncodeResult,
                           std::unique_ptr<WebrtcVideoEncoder::EncodedFrame>>
        future;
    encoder.Encode(CreateFrame(320, 320,
                               webrtc::DesktopRegion(
                                   webrtc::DesktopRect::MakeWH(320, 320))),
                   MakeFrameParams(/*bitrate_kbps=*/1000, /*key_frame=*/true,
                                   /*clear_active_map=*/true),
                   future.GetCallback());
    auto [result, encoded_frame] = future.Take();
    EXPECT_EQ(result, WebrtcVideoEncoder::EncodeResult::SUCCEEDED);
    ASSERT_NE(encoded_frame, nullptr);
    EXPECT_EQ(encoder.last_init_target_bitrate_kbps_for_testing(), 1000u);
  }

  // Resizing to 1000x1000 (1 MP -> 2500 kbps minimum from `SetFrameSize()`)
  // clamps `rc_target_bitrate` up to the new resolution's floor before
  // `InitializeCodec()` calls `aom_codec_enc_init()`.
  {
    base::test::TestFuture<WebrtcVideoEncoder::EncodeResult,
                           std::unique_ptr<WebrtcVideoEncoder::EncodedFrame>>
        future;
    encoder.Encode(CreateFrame(1000, 1000,
                               webrtc::DesktopRegion(
                                   webrtc::DesktopRect::MakeWH(1000, 1000))),
                   MakeFrameParams(/*bitrate_kbps=*/1000, /*key_frame=*/true,
                                   /*clear_active_map=*/true),
                   future.GetCallback());
    auto [result, encoded_frame] = future.Take();
    EXPECT_EQ(result, WebrtcVideoEncoder::EncodeResult::SUCCEEDED);
    ASSERT_NE(encoded_frame, nullptr);
    EXPECT_EQ(encoded_frame->dimensions.width(), 1000);
    EXPECT_EQ(encoded_frame->dimensions.height(), 1000);
    EXPECT_EQ(encoder.last_init_target_bitrate_kbps_for_testing(), 2500u);
  }
}

}  // namespace remoting
