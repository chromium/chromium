// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/encoding/external_video_encoder.h"

#include <stdint.h>

#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/cpu.h"  // nogncheck
#endif

namespace media::cast {

namespace {

scoped_refptr<VideoFrame> CreateFrame(const uint8_t* y_plane_data,
                                      const gfx::Size& size) {
  scoped_refptr<VideoFrame> result = VideoFrame::CreateFrame(
      PIXEL_FORMAT_I420, size, gfx::Rect(size), size, base::TimeDelta());
  for (int y = 0, y_end = size.height(); y < y_end; ++y) {
    memcpy(result->GetWritableVisibleData(VideoFrame::kYPlane) +
               y * result->stride(VideoFrame::kYPlane),
           y_plane_data + y * size.width(), size.width());
  }
  return result;
}

static const std::vector<media::VideoEncodeAccelerator::SupportedProfile>
    kValidVeaProfiles{
        VideoEncodeAccelerator::SupportedProfile(media::VP8PROFILE_MIN,
                                                 gfx::Size(1920, 1080)),
        VideoEncodeAccelerator::SupportedProfile(media::H264PROFILE_MIN,
                                                 gfx::Size(1920, 1080)),
    };

constexpr std::array<const char*, 3> kFirstPartyModelNames{
    {"Chromecast", "Eureka Dongle", "Chromecast Ultra"}};

}  // namespace

TEST(QuantizerEstimatorTest, EstimatesForTrivialFrames) {
  QuantizerEstimator qe;

  const gfx::Size frame_size(320, 180);
  const auto black_frame_data =
      std::make_unique<uint8_t[]>(frame_size.GetArea());
  memset(black_frame_data.get(), 0, frame_size.GetArea());
  const scoped_refptr<VideoFrame> black_frame =
      CreateFrame(black_frame_data.get(), frame_size);

  // A solid color frame should always generate a minimum quantizer value (4.0)
  // as a key frame.  If it is provided repeatedly as delta frames, the minimum
  // quantizer value should be repeatedly generated since there is no difference
  // between frames.
  EXPECT_EQ(4.0, qe.EstimateForKeyFrame(*black_frame));
  for (int i = 0; i < 3; ++i)
    EXPECT_EQ(4.0, qe.EstimateForDeltaFrame(*black_frame));

  const auto checkerboard_frame_data =
      std::make_unique<uint8_t[]>(frame_size.GetArea());
  for (int i = 0, end = frame_size.GetArea(); i < end; ++i)
    checkerboard_frame_data.get()[i] = (((i % 2) == 0) ? 0 : 255);
  const scoped_refptr<VideoFrame> checkerboard_frame =
      CreateFrame(checkerboard_frame_data.get(), frame_size);

  // Now, introduce a frame with a checkerboard pattern.  Half of the pixels
  // will have a difference of 255, and half will have zero difference.
  // Therefore, the Shannon Entropy should be 1.0 and the resulting quantizer
  // estimate should be ~11.9.
  EXPECT_NEAR(11.9, qe.EstimateForDeltaFrame(*checkerboard_frame), 0.1);

  // Now, introduce a series of frames with "random snow" in them.  Expect this
  // results in high quantizer estimates.
  for (int i = 0; i < 3; ++i) {
    int rand_seed = 0xdeadbeef + i;
    const auto random_frame_data =
        std::make_unique<uint8_t[]>(frame_size.GetArea());
    for (int j = 0, end = frame_size.GetArea(); j < end; ++j) {
      rand_seed = (1103515245 * rand_seed + 12345) % (1 << 31);
      random_frame_data.get()[j] = static_cast<uint8_t>(rand_seed & 0xff);
    }
    const scoped_refptr<VideoFrame> random_frame =
        CreateFrame(random_frame_data.get(), frame_size);
    EXPECT_LE(50.0, qe.EstimateForDeltaFrame(*random_frame));
  }
}

// The decoder on Vizio TVs doesn't play well with Chrome OS hardware encoders.
// See https://crbug.com/1238774 for more context.
TEST(ExternalVideoEncoderTest,
     DoesntRecommendExternalVp8EncoderForVizioOnChromeOS) {
  constexpr std::array<const char*, 10> kVizioTvModelNames{
      {"e43u-d2", "e60-e3", "OLED55-H1", "M50-D1", "E65-F1", "E50-F2", "M55-D0",
       "Vizio P-Series Quantum 4K", "M55-E0", "V435-H1"}};

  for (const char* model_name : kVizioTvModelNames) {
    constexpr bool should_recommend =
#if BUILDFLAG(IS_CHROMEOS)
        false;
#else
        true;
#endif
    EXPECT_EQ(should_recommend,
              ExternalVideoEncoder::IsRecommended(
                  CODEC_VIDEO_VP8, std::string(model_name), kValidVeaProfiles))
        << model_name;
  }
}

TEST(ExternalVideoEncoderTest, RecommendsExternalVp8EncoderForChromecast) {
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_FALSE(ExternalVideoEncoder::IsRecommended(
      CODEC_VIDEO_VP8, "Eureka Dongle", kValidVeaProfiles));
  EXPECT_FALSE(ExternalVideoEncoder::IsRecommended(
      CODEC_VIDEO_VP8, "Chromecast", kValidVeaProfiles));
  EXPECT_FALSE(ExternalVideoEncoder::IsRecommended(
      CODEC_VIDEO_VP8, "Chromecast Ultra", kValidVeaProfiles));
  EXPECT_FALSE(ExternalVideoEncoder::IsRecommended(
      CODEC_VIDEO_VP8, "Google Home", kValidVeaProfiles));
#else
  for (const char* model_name : kFirstPartyModelNames) {
    EXPECT_TRUE(ExternalVideoEncoder::IsRecommended(
        CODEC_VIDEO_VP8, std::string(model_name), kValidVeaProfiles));
  }
#endif
}

TEST(ExternalVideoEncoderTest, RecommendsH264HardwareEncoderProperly) {
  for (const char* model_name : kFirstPartyModelNames) {
// On ChromeOS only, disable hardware encoder on AMD chipsets due to
// failure on Chromecast chipsets to decode.
#if BUILDFLAG(IS_CHROMEOS)
    if (base::CPU().vendor_name() == "AuthenticAMD") {
      EXPECT_FALSE(ExternalVideoEncoder::IsRecommended(
          CODEC_VIDEO_H264, std::string(model_name), kValidVeaProfiles));
      break;
    }
#endif

    constexpr bool should_recommend =
#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_WIN)
        true;
#else
        false;
#endif
    EXPECT_EQ(should_recommend, ExternalVideoEncoder::IsRecommended(
                                    CODEC_VIDEO_H264, std::string(model_name),
                                    kValidVeaProfiles));
  }
}

}  // namespace media::cast
