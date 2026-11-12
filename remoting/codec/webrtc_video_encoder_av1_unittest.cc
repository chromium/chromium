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

// 320x320 corresponds to 20x20 = 400 16x16 macroblocks, so the [2.5%, 25%]
// gating thresholds land on exact integer boundaries (10 and 100 macroblocks).
constexpr int kFrameWidth = 320;
constexpr int kFrameHeight = 320;

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

void EncodeAndExpectSuccess(WebrtcVideoEncoderAV1& encoder,
                            const webrtc::DesktopRegion& updated_region,
                            bool key_frame,
                            bool clear_active_map) {
  base::test::TestFuture<WebrtcVideoEncoder::EncodeResult,
                         std::unique_ptr<WebrtcVideoEncoder::EncodedFrame>>
      future;
  auto frame = CreateFrame(kFrameWidth, kFrameHeight, updated_region);
  if (updated_region.is_empty()) {
    EXPECT_TRUE(frame->updated_region().is_empty());
  }
  encoder.Encode(
      std::move(frame),
      MakeFrameParams(/*bitrate_kbps=*/2000, key_frame, clear_active_map),
      future.GetCallback());
  auto [result, encoded_frame] = future.Take();
  EXPECT_EQ(result, WebrtcVideoEncoder::EncodeResult::SUCCEEDED);
  ASSERT_NE(encoded_frame, nullptr);
  EXPECT_EQ(encoded_frame->key_frame, key_frame);
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

TEST_F(WebrtcVideoEncoderAV1Test, ActiveMapDisabledOnKeyframes) {
  WebrtcVideoEncoderAV1 encoder;
  const webrtc::DesktopRegion full_region(
      webrtc::DesktopRect::MakeWH(kFrameWidth, kFrameHeight));

  // Initial keyframe keeps the codec active map disabled while marking the
  // internal map full for subsequent top-off frames.
  EncodeAndExpectSuccess(encoder, full_region, /*key_frame=*/true,
                         /*clear_active_map=*/true);
  EXPECT_FALSE(encoder.codec_active_map_enabled_for_testing());

  // Extrapolated top-off delta frame immediately after a keyframe
  // (`clear_active_map = false`, empty damage) retains all 400/400 active
  // macroblocks, so the codec active map stays disabled.
  EncodeAndExpectSuccess(encoder, webrtc::DesktopRegion(), /*key_frame=*/false,
                         /*clear_active_map=*/false);
  EXPECT_FALSE(encoder.codec_active_map_enabled_for_testing());

  // Enable the codec active map on a 10% partial update (40 / 400 macroblocks,
  // 320x32), then verify forcing a keyframe disables it again.
  EncodeAndExpectSuccess(
      encoder,
      webrtc::DesktopRegion(webrtc::DesktopRect::MakeXYWH(0, 0, 320, 32)),
      /*key_frame=*/false, /*clear_active_map=*/true);
  EXPECT_TRUE(encoder.codec_active_map_enabled_for_testing());

  EncodeAndExpectSuccess(encoder, full_region, /*key_frame=*/true,
                         /*clear_active_map=*/false);
  EXPECT_FALSE(encoder.codec_active_map_enabled_for_testing());
}

TEST_F(WebrtcVideoEncoderAV1Test, ActiveMapLowerCoverageBoundary) {
  WebrtcVideoEncoderAV1 encoder;
  EncodeAndExpectSuccess(encoder,
                         webrtc::DesktopRegion(webrtc::DesktopRect::MakeWH(
                             kFrameWidth, kFrameHeight)),
                         /*key_frame=*/true, /*clear_active_map=*/true);

  // Just below the 2.5% cutoff: 9 / 400 macroblocks (2.25%, 144x16).
  EncodeAndExpectSuccess(
      encoder,
      webrtc::DesktopRegion(webrtc::DesktopRect::MakeXYWH(0, 0, 144, 16)),
      /*key_frame=*/false, /*clear_active_map=*/true);
  EXPECT_FALSE(encoder.codec_active_map_enabled_for_testing());

  // Exactly at the 2.5% lower bound: 10 / 400 macroblocks (2.50%, 160x16).
  EncodeAndExpectSuccess(
      encoder,
      webrtc::DesktopRegion(webrtc::DesktopRect::MakeXYWH(0, 0, 160, 16)),
      /*key_frame=*/false, /*clear_active_map=*/true);
  EXPECT_TRUE(encoder.codec_active_map_enabled_for_testing());

  // Dropping back below 2.5% disables the previously applied codec active map.
  EncodeAndExpectSuccess(
      encoder,
      webrtc::DesktopRegion(webrtc::DesktopRect::MakeXYWH(0, 0, 144, 16)),
      /*key_frame=*/false, /*clear_active_map=*/true);
  EXPECT_FALSE(encoder.codec_active_map_enabled_for_testing());
}

TEST_F(WebrtcVideoEncoderAV1Test, ActiveMapUpperCoverageBoundary) {
  WebrtcVideoEncoderAV1 encoder;
  const webrtc::DesktopRegion full_region(
      webrtc::DesktopRect::MakeWH(kFrameWidth, kFrameHeight));
  EncodeAndExpectSuccess(encoder, full_region, /*key_frame=*/true,
                         /*clear_active_map=*/true);

  // Exactly at the 25% upper bound: 100 / 400 macroblocks (25.00%, 320x80).
  EncodeAndExpectSuccess(
      encoder,
      webrtc::DesktopRegion(webrtc::DesktopRect::MakeXYWH(0, 0, 320, 80)),
      /*key_frame=*/false, /*clear_active_map=*/true);
  EXPECT_TRUE(encoder.codec_active_map_enabled_for_testing());

  // Just above the 25% upper bound: 101 / 400 macroblocks (25.25%) disables the
  // previously applied codec active map.
  webrtc::DesktopRegion over_upper_bound_region;
  over_upper_bound_region.AddRect(webrtc::DesktopRect::MakeXYWH(0, 0, 320, 80));
  over_upper_bound_region.AddRect(webrtc::DesktopRect::MakeXYWH(0, 80, 16, 16));
  EncodeAndExpectSuccess(encoder, over_upper_bound_region, /*key_frame=*/false,
                         /*clear_active_map=*/true);
  EXPECT_FALSE(encoder.codec_active_map_enabled_for_testing());

  // Re-enable at 25%, then verify full-frame delta damage (400 / 400
  // macroblocks, 100%) also disables the codec active map.
  EncodeAndExpectSuccess(
      encoder,
      webrtc::DesktopRegion(webrtc::DesktopRect::MakeXYWH(0, 0, 320, 80)),
      /*key_frame=*/false, /*clear_active_map=*/true);
  EXPECT_TRUE(encoder.codec_active_map_enabled_for_testing());

  EncodeAndExpectSuccess(encoder, full_region, /*key_frame=*/false,
                         /*clear_active_map=*/true);
  EXPECT_FALSE(encoder.codec_active_map_enabled_for_testing());
}

TEST_F(WebrtcVideoEncoderAV1Test, ActiveMapPreservedOnEmptyTopOffDeltaFrame) {
  WebrtcVideoEncoderAV1 encoder;
  EncodeAndExpectSuccess(encoder,
                         webrtc::DesktopRegion(webrtc::DesktopRect::MakeWH(
                             kFrameWidth, kFrameHeight)),
                         /*key_frame=*/true, /*clear_active_map=*/true);

  EncodeAndExpectSuccess(
      encoder,
      webrtc::DesktopRegion(webrtc::DesktopRect::MakeXYWH(0, 0, 160, 16)),
      /*key_frame=*/false, /*clear_active_map=*/true);
  EXPECT_TRUE(encoder.codec_active_map_enabled_for_testing());

  // Empty-damage top-off frame (`clear_active_map = false`) preserves the
  // existing 10 / 400 active macroblocks rather than clearing to 0 or expanding
  // to full-frame.
  EncodeAndExpectSuccess(encoder, webrtc::DesktopRegion(), /*key_frame=*/false,
                         /*clear_active_map=*/false);
  EXPECT_TRUE(encoder.codec_active_map_enabled_for_testing());
}

TEST_F(WebrtcVideoEncoderAV1Test, SetUseActiveMapFalseKeepsMapDisabled) {
  WebrtcVideoEncoderAV1 encoder;
  encoder.SetUseActiveMap(false);

  EncodeAndExpectSuccess(encoder,
                         webrtc::DesktopRegion(webrtc::DesktopRect::MakeWH(
                             kFrameWidth, kFrameHeight)),
                         /*key_frame=*/true, /*clear_active_map=*/true);
  EXPECT_FALSE(encoder.codec_active_map_enabled_for_testing());

  // 10% dirty region (40 / 400 macroblocks) would normally enable the active
  // map, but `use_active_map` was explicitly disabled.
  EncodeAndExpectSuccess(
      encoder,
      webrtc::DesktopRegion(webrtc::DesktopRect::MakeXYWH(0, 0, 320, 32)),
      /*key_frame=*/false, /*clear_active_map=*/true);
  EXPECT_FALSE(encoder.codec_active_map_enabled_for_testing());
}

}  // namespace remoting
