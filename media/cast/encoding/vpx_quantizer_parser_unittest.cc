// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/cast/encoding/vpx_quantizer_parser.h"

#include <stdint.h>

#include <cstdlib>
#include <memory>

#include "base/time/time.h"
#include "media/base/mock_filters.h"
#include "media/base/video_codecs.h"
#include "media/cast/cast_config.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/cast/encoding/vpx_encoder.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/video_utility.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/public/encoded_frame.h"

namespace media::cast {

namespace {
constexpr int kWidth = 32;
constexpr int kHeight = 32;
constexpr int kFrameRate = 10;
constexpr int kQp = 20;

FrameSenderConfig GetVideoConfigForTest() {
  FrameSenderConfig config = GetDefaultVideoSenderConfig();
  config.use_hardware_encoder = false;
  config.max_frame_rate = kFrameRate;

  VideoCodecParams& codec_params = config.video_codec_params.value();
  codec_params.codec = VideoCodec::kVP8;
  codec_params.min_qp = kQp;
  codec_params.max_qp = kQp;
  codec_params.max_cpu_saver_qp = kQp;
  return config;
}
}  // unnamed namespace

class VpxQuantizerParserTest : public ::testing::Test {
 public:
  VpxQuantizerParserTest() : video_config_(GetVideoConfigForTest()) {}

  // Call vp8 software encoder to encode one randomly generated frame.
  void EncodeOneFrame(SenderEncodedFrame* frame) {
    const gfx::Size frame_size = gfx::Size(kWidth, kHeight);
    const scoped_refptr<VideoFrame> video_frame = VideoFrame::CreateFrame(
        PIXEL_FORMAT_I420, frame_size, gfx::Rect(frame_size), frame_size,
        next_frame_timestamp_);
    const base::TimeTicks reference_time =
        base::TimeTicks::UnixEpoch() + next_frame_timestamp_;
    next_frame_timestamp_ += base::Seconds(1) / kFrameRate;
    PopulateVideoFrameWithNoise(video_frame.get());
    vp8_encoder_->Encode(video_frame, reference_time, frame);
  }

  // Update the vp8 encoder with the new quantizer.
  void UpdateQuantizer(int qp) {
    DCHECK((qp > 3) && (qp < 64));
    VideoCodecParams& codec_params = video_config_.video_codec_params.value();
    codec_params.min_qp = qp;
    codec_params.max_qp = qp;
    codec_params.max_cpu_saver_qp = qp;
    RecreateVp8Encoder();
  }

 protected:
  void SetUp() final {
    next_frame_timestamp_ = base::TimeDelta();
    RecreateVp8Encoder();
  }

 private:
  // Reconstruct a vp8 encoder with new config since the Vp8Encoder
  // class has no interface to update the config.
  void RecreateVp8Encoder() {
    vp8_encoder_ = std::make_unique<VpxEncoder>(
        video_config_,
        std::make_unique<media::MockVideoEncoderMetricsProvider>());
    vp8_encoder_->Initialize();
  }

  base::TimeDelta next_frame_timestamp_;
  FrameSenderConfig video_config_;
  std::unique_ptr<VpxEncoder> vp8_encoder_;
};

// Encode 3 frames to test the cases with insufficient data input.
TEST_F(VpxQuantizerParserTest, InsufficientData) {
  for (int i = 0; i < 3; ++i) {
    auto frame = std::make_unique<SenderEncodedFrame>();

    // Null input.
    EXPECT_EQ(-1, ParseVpxHeaderQuantizer(frame->data));
    EncodeOneFrame(frame.get());

    // Zero bytes should not be enough to decode the quantizer value.
    EXPECT_EQ(-1, ParseVpxHeaderQuantizer(frame->data.first(0)));

    // Three bytes should not be enough to decode the quantizer value..
    EXPECT_EQ(-1, ParseVpxHeaderQuantizer(frame->data.first(3)));

    const unsigned int first_partition_size =
        (frame->data[0] | (frame->data[1] << 8) | (frame->data[2] << 16)) >> 5;
    if (frame->dependency ==
        openscreen::cast::EncodedFrame::Dependency::kKeyFrame) {
      // Ten bytes should not be enough to decode the quantizer value
      // for a Key frame.
      EXPECT_EQ(-1, ParseVpxHeaderQuantizer(frame->data.first(10)));

      // One byte less than needed to decode the quantizer value.
      EXPECT_EQ(-1, ParseVpxHeaderQuantizer(
                        frame->data.first(10 + first_partition_size - 1)));

      // Minimum number of bytes to decode the quantizer value.
      EXPECT_EQ(kQp, ParseVpxHeaderQuantizer(
                         frame->data.first(10 + first_partition_size)));
    } else {
      // One byte less than needed to decode the quantizer value.
      EXPECT_EQ(-1, ParseVpxHeaderQuantizer(
                        frame->data.first(3 + first_partition_size - 1)));

      // Minimum number of bytes to decode the quantizer value.
      EXPECT_EQ(kQp, ParseVpxHeaderQuantizer(
                         frame->data.first(3 + first_partition_size)));
    }
  }
}

// Encode 3 fames for every quantizer value in the range of [4,63].
TEST_F(VpxQuantizerParserTest, VariedQuantizer) {
  int decoded_quantizer = -1;
  for (int qp = 4; qp <= 63; qp += 10) {
    UpdateQuantizer(qp);
    for (int i = 0; i < 3; ++i) {
      auto frame = std::make_unique<SenderEncodedFrame>();
      EncodeOneFrame(frame.get());
      decoded_quantizer = ParseVpxHeaderQuantizer(frame->data);
      EXPECT_EQ(qp, decoded_quantizer);
    }
  }
}

}  // namespace media::cast
