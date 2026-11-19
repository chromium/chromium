// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/av1_video_encoder.h"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/video_codecs.h"
#include "media/base/video_encoder.h"
#include "media/base/video_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libgav1/src/src/buffer_pool.h"
#include "third_party/libgav1/src/src/decoder_state.h"
#include "third_party/libgav1/src/src/gav1/status_code.h"
#include "third_party/libgav1/src/src/obu_parser.h"
#include "third_party/libgav1/src/src/utils/types.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace {

using ::testing::AllOf;
using ::testing::Ge;
using ::testing::Le;
using ::testing::Ne;

class Av1HeaderParser {
 public:
  Av1HeaderParser()
      : buffer_pool_(/*on_frame_buffer_size_changed=*/nullptr,
                     /*get_frame_buffer=*/nullptr,
                     /*release_frame_buffer=*/nullptr,
                     /*callback_private_data=*/nullptr) {}

  std::optional<libgav1::ObuFrameHeader> ParseFrame(
      base::span<const uint8_t> data) {
    libgav1::RefCountedBufferPtr curr_frame;
    libgav1::ObuParser parser(data.data(), data.size(), /*operating_point=*/0,
                              &buffer_pool_, &decoder_state_);
    if (sequence_header_) {
      parser.set_sequence_header(*sequence_header_);
    }
    if (parser.ParseOneFrame(&curr_frame) != libgav1::kStatusOk) {
      return std::nullopt;
    }
    const libgav1::ObuFrameHeader& header = parser.frame_header();
    if (parser.sequence_header_changed()) {
      sequence_header_ = parser.sequence_header();
    }
    decoder_state_.UpdateReferenceFrames(curr_frame,
                                         header.refresh_frame_flags);
    return header;
  }

 private:
  libgav1::BufferPool buffer_pool_;
  libgav1::DecoderState decoder_state_;
  std::optional<libgav1::ObuSequenceHeader> sequence_header_;
};

scoped_refptr<VideoFrame> MakeDisjointRangeFrame(const gfx::Size& size,
                                                 uint8_t base_y,
                                                 base::TimeDelta timestamp) {
  auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_I420, size, gfx::Rect(size),
                                       size, timestamp);
  for (int by = 0; by < size.height() / 4; ++by) {
    for (int bx = 0; bx < size.width() / 4; ++bx) {
      int hash = (bx * 7 + by * 13 + bx * by * 11) ^ (bx * bx * 3 + by * 5);
      uint8_t y_val = base_y + ((hash & 0x7fff) % 9) * 4;
      libyuv::I420Rect(frame->GetWritableVisibleData(VideoFrame::Plane::kY),
                       frame->stride(VideoFrame::Plane::kY),
                       frame->GetWritableVisibleData(VideoFrame::Plane::kU),
                       frame->stride(VideoFrame::Plane::kU),
                       frame->GetWritableVisibleData(VideoFrame::Plane::kV),
                       frame->stride(VideoFrame::Plane::kV), bx * 4, by * 4, 4,
                       4, y_val, 128, 128);
    }
  }
  return frame;
}

scoped_refptr<VideoFrame> BlendFrames(const VideoFrame& a,
                                      const VideoFrame& b,
                                      base::TimeDelta timestamp,
                                      uint8_t bias_base_y = 0) {
  const gfx::Size size = a.visible_rect().size();
  auto out = VideoFrame::CreateFrame(PIXEL_FORMAT_I420, size, gfx::Rect(size),
                                     size, timestamp);
  libyuv::InterpolatePlane(
      a.visible_data(VideoFrame::Plane::kY), a.stride(VideoFrame::Plane::kY),
      b.visible_data(VideoFrame::Plane::kY), b.stride(VideoFrame::Plane::kY),
      out->GetWritableVisibleData(VideoFrame::Plane::kY),
      out->stride(VideoFrame::Plane::kY), size.width(), size.height(), 128);
  libyuv::InterpolatePlane(
      a.visible_data(VideoFrame::Plane::kU), a.stride(VideoFrame::Plane::kU),
      b.visible_data(VideoFrame::Plane::kU), b.stride(VideoFrame::Plane::kU),
      out->GetWritableVisibleData(VideoFrame::Plane::kU),
      out->stride(VideoFrame::Plane::kU), size.width() / 2, size.height() / 2,
      128);
  libyuv::InterpolatePlane(
      a.visible_data(VideoFrame::Plane::kV), a.stride(VideoFrame::Plane::kV),
      b.visible_data(VideoFrame::Plane::kV), b.stride(VideoFrame::Plane::kV),
      out->GetWritableVisibleData(VideoFrame::Plane::kV),
      out->stride(VideoFrame::Plane::kV), size.width() / 2, size.height() / 2,
      128);
  if (bias_base_y > 0) {
    for (int by = 0; by < size.height() / 4; ++by) {
      for (int bx = 0; bx < size.width() / 4; ++bx) {
        if (((bx / 8) + (by / 8)) % 2 == 1) {
          int hash = (bx * 7 + by * 13 + bx * by * 11) ^ (bx * bx * 3 + by * 5);
          uint8_t y_val = bias_base_y + ((hash & 0x7fff) % 9) * 4;
          libyuv::I420Rect(out->GetWritableVisibleData(VideoFrame::Plane::kY),
                           out->stride(VideoFrame::Plane::kY),
                           out->GetWritableVisibleData(VideoFrame::Plane::kU),
                           out->stride(VideoFrame::Plane::kU),
                           out->GetWritableVisibleData(VideoFrame::Plane::kV),
                           out->stride(VideoFrame::Plane::kV), bx * 4, by * 4,
                           4, 4, y_val, 128, 128);
        }
      }
    }
  }
  return out;
}

class Av1VideoEncoderTest : public ::testing::Test {
 protected:
  void InitializeManualCqpEncoder(Av1VideoEncoder& encoder,
                                  const gfx::Size& size,
                                  VideoEncoder::LatencyMode latency_mode =
                                      VideoEncoder::LatencyMode::Realtime) {
    encoder.DisablePostedCallbacks();
    VideoEncoder::Options options;
    options.frame_size = size;
    options.framerate = 30;
    options.bitrate = Bitrate::ExternalRateControl();
    options.manual_reference_buffer_control = true;
    options.latency_mode = latency_mode;

    std::optional<EncoderStatus> init_status;
    encoder.Initialize(AV1PROFILE_PROFILE_MAIN, options,
                       /*info_cb=*/base::DoNothing(),
                       base::BindLambdaForTesting(
                           [&](VideoEncoderOutput output,
                               std::optional<VideoEncoder::CodecDescription>) {
                             outputs_.push_back(std::move(output));
                           }),
                       base::BindLambdaForTesting([&](EncoderStatus status) {
                         init_status = std::move(status);
                       }));
    ASSERT_TRUE(init_status.has_value());
    ASSERT_TRUE(init_status->is_ok()) << init_status->message();
  }

  std::optional<libgav1::ObuFrameHeader> EncodeAndParse(
      Av1VideoEncoder& encoder,
      Av1HeaderParser& parser,
      scoped_refptr<VideoFrame> frame,
      bool key_frame,
      std::initializer_list<uint8_t> refs,
      std::optional<uint8_t> update_slot,
      int q_index) {
    VideoEncoder::EncodeOptions encode_options(key_frame);
    encode_options.quantizer = q_index;
    for (uint8_t ref : refs) {
      encode_options.reference_buffers.push_back(ref);
    }
    encode_options.update_buffer = update_slot;

    size_t prev_count = outputs_.size();
    std::optional<EncoderStatus> encode_status;
    encoder.Encode(std::move(frame), encode_options,
                   base::BindLambdaForTesting([&](EncoderStatus status) {
                     encode_status = std::move(status);
                   }));
    EXPECT_TRUE(encode_status.has_value());
    if (!encode_status || !encode_status->is_ok() ||
        outputs_.size() != prev_count + 1) {
      return std::nullopt;
    }
    return parser.ParseFrame(outputs_.back().data);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::vector<VideoEncoderOutput> outputs_;
};

TEST_F(Av1VideoEncoderTest, CqpEnablesBoundedCyclicRefreshSegmentation) {
  constexpr gfx::Size kSize(256, 256);
  Av1VideoEncoder encoder;
  InitializeManualCqpEncoder(encoder, kSize);

  Av1HeaderParser parser;
  scoped_refptr<VideoFrame> f0 =
      MakeDisjointRangeFrame(kSize, 64, base::Milliseconds(0));
  // Keep the background static with a small moving patch so cyclic refresh
  // continues sweeping static superblocks across frames.
  scoped_refptr<VideoFrame> f1 = BlendFrames(*f0, *f0, base::Milliseconds(33));
  libyuv::I420Rect(f1->GetWritableVisibleData(VideoFrame::Plane::kY),
                   f1->stride(VideoFrame::Plane::kY),
                   f1->GetWritableVisibleData(VideoFrame::Plane::kU),
                   f1->stride(VideoFrame::Plane::kU),
                   f1->GetWritableVisibleData(VideoFrame::Plane::kV),
                   f1->stride(VideoFrame::Plane::kV), 0, 0, 32, 32, 180, 128,
                   128);

  std::optional<libgav1::ObuFrameHeader> key_hdr =
      EncodeAndParse(encoder, parser, f0, /*key_frame=*/true, {}, 0, 120);
  ASSERT_TRUE(key_hdr.has_value());
  EXPECT_EQ(key_hdr->quantizer.base_index, 120);

  std::optional<libgav1::ObuFrameHeader> delta_hdr =
      EncodeAndParse(encoder, parser, f1, /*key_frame=*/false, {0}, 0, 120);
  ASSERT_TRUE(delta_hdr.has_value());
  EXPECT_EQ(delta_hdr->quantizer.base_index, 120);
  EXPECT_TRUE(delta_hdr->segmentation.enabled);
  EXPECT_TRUE(delta_hdr->segmentation.FeatureActive(
      1, libgav1::kSegmentFeatureQuantizer));
  const int16_t delta_q =
      delta_hdr->segmentation
          .feature_data[1][libgav1::kSegmentFeatureQuantizer];
  EXPECT_THAT(delta_q, AllOf(Ne(0), Ge(-16), Le(16)));

  // Dynamically change QP to 100 and verify the updated cq_level and bounded
  // cyclic refresh window take effect without reinitializing the encoder.
  scoped_refptr<VideoFrame> f2 = BlendFrames(*f0, *f0, base::Milliseconds(66));
  libyuv::I420Rect(f2->GetWritableVisibleData(VideoFrame::Plane::kY),
                   f2->stride(VideoFrame::Plane::kY),
                   f2->GetWritableVisibleData(VideoFrame::Plane::kU),
                   f2->stride(VideoFrame::Plane::kU),
                   f2->GetWritableVisibleData(VideoFrame::Plane::kV),
                   f2->stride(VideoFrame::Plane::kV), 32, 0, 32, 32, 180, 128,
                   128);
  std::optional<libgav1::ObuFrameHeader> delta_hdr_100 =
      EncodeAndParse(encoder, parser, f2, /*key_frame=*/false, {0}, 0, 100);
  ASSERT_TRUE(delta_hdr_100.has_value());
  EXPECT_EQ(delta_hdr_100->quantizer.base_index, 100);
  EXPECT_TRUE(delta_hdr_100->segmentation.enabled);
  EXPECT_TRUE(delta_hdr_100->segmentation.FeatureActive(
      1, libgav1::kSegmentFeatureQuantizer));
  const int16_t delta_q_100 =
      delta_hdr_100->segmentation
          .feature_data[1][libgav1::kSegmentFeatureQuantizer];
  EXPECT_THAT(delta_q_100, AllOf(Ne(0), Ge(-16), Le(16)));
}

TEST_F(Av1VideoEncoderTest, CqpZeroEnablesLossless) {
  constexpr gfx::Size kSize(256, 256);
  Av1VideoEncoder encoder;
  InitializeManualCqpEncoder(encoder, kSize);

  Av1HeaderParser parser;
  scoped_refptr<VideoFrame> f0 =
      MakeDisjointRangeFrame(kSize, 64, base::Milliseconds(0));
  scoped_refptr<VideoFrame> f1 =
      MakeDisjointRangeFrame(kSize, 96, base::Milliseconds(33));

  std::optional<libgav1::ObuFrameHeader> key_hdr =
      EncodeAndParse(encoder, parser, f0, /*key_frame=*/true, {}, 0, 0);
  ASSERT_TRUE(key_hdr.has_value());
  EXPECT_EQ(key_hdr->quantizer.base_index, 0);
  EXPECT_TRUE(key_hdr->coded_lossless);
  EXPECT_FALSE(key_hdr->segmentation.enabled);

  std::optional<libgav1::ObuFrameHeader> delta_hdr =
      EncodeAndParse(encoder, parser, f1, /*key_frame=*/false, {0}, 0, 0);
  ASSERT_TRUE(delta_hdr.has_value());
  EXPECT_EQ(delta_hdr->quantizer.base_index, 0);
  EXPECT_TRUE(delta_hdr->coded_lossless);
  EXPECT_FALSE(delta_hdr->segmentation.enabled);

  // Low non-zero quantizer (q_index = 4 -> qp = 1) exits lossless, keeps
  // min_q >= 1, and does not produce a lossless frame.
  auto make_small_motion_frame = [&](int rect_x, base::TimeDelta ts) {
    scoped_refptr<VideoFrame> frame = BlendFrames(*f1, *f1, ts);
    libyuv::I420Rect(frame->GetWritableVisibleData(VideoFrame::Plane::kY),
                     frame->stride(VideoFrame::Plane::kY),
                     frame->GetWritableVisibleData(VideoFrame::Plane::kU),
                     frame->stride(VideoFrame::Plane::kU),
                     frame->GetWritableVisibleData(VideoFrame::Plane::kV),
                     frame->stride(VideoFrame::Plane::kV), rect_x, 0, 32, 32,
                     180, 128, 128);
    return frame;
  };
  int64_t ts_ms = 66;
  std::optional<libgav1::ObuFrameHeader> low_lossy_hdr = EncodeAndParse(
      encoder, parser, make_small_motion_frame(0, base::Milliseconds(ts_ms)),
      /*key_frame=*/false, {0}, 0, 4);
  ts_ms += 33;
  ASSERT_TRUE(low_lossy_hdr.has_value());
  EXPECT_EQ(low_lossy_hdr->quantizer.base_index, 4);
  EXPECT_FALSE(low_lossy_hdr->coded_lossless);

  // Transition to lossy CQP at q_index = 120: lossless stays disabled and
  // cyclic refresh segmentation resumes once libaom's running average Q rises
  // from 0 past qp_thresh.
  bool saw_segmentation = false;
  for (int i = 0; i < 12; ++i) {
    scoped_refptr<VideoFrame> frame =
        make_small_motion_frame((i % 6) * 32, base::Milliseconds(ts_ms));
    ts_ms += 33;
    std::optional<libgav1::ObuFrameHeader> lossy_hdr = EncodeAndParse(
        encoder, parser, frame, /*key_frame=*/false, {0}, 0, 120);
    ASSERT_TRUE(lossy_hdr.has_value());
    EXPECT_EQ(lossy_hdr->quantizer.base_index, 120);
    EXPECT_FALSE(lossy_hdr->coded_lossless);
    saw_segmentation |= lossy_hdr->segmentation.enabled;
  }
  EXPECT_TRUE(saw_segmentation);

  // Transition from lossy (with active cyclic refresh) back to lossless
  // explicitly disables AQ and re-enables lossless coding.
  std::optional<libgav1::ObuFrameHeader> lossless_again_hdr = EncodeAndParse(
      encoder, parser, make_small_motion_frame(32, base::Milliseconds(ts_ms)),
      /*key_frame=*/false, {0}, 0, 0);
  ASSERT_TRUE(lossless_again_hdr.has_value());
  EXPECT_EQ(lossless_again_hdr->quantizer.base_index, 0);
  EXPECT_TRUE(lossless_again_hdr->coded_lossless);
  EXPECT_FALSE(lossless_again_hdr->segmentation.enabled);
}

TEST_F(Av1VideoEncoderTest, CqpOutOfBoundsQuantizerIsClamped) {
  constexpr gfx::Size kSize(256, 256);
  Av1VideoEncoder encoder;
  InitializeManualCqpEncoder(encoder, kSize);

  Av1HeaderParser parser;
  scoped_refptr<VideoFrame> f0 =
      MakeDisjointRangeFrame(kSize, 64, base::Milliseconds(0));
  scoped_refptr<VideoFrame> f1 =
      MakeDisjointRangeFrame(kSize, 96, base::Milliseconds(33));

  std::optional<libgav1::ObuFrameHeader> low_hdr =
      EncodeAndParse(encoder, parser, f0, /*key_frame=*/true, {}, 0, -1);
  ASSERT_TRUE(low_hdr.has_value());
  EXPECT_EQ(low_hdr->quantizer.base_index, 0);
  EXPECT_TRUE(low_hdr->coded_lossless);

  std::optional<libgav1::ObuFrameHeader> high_hdr =
      EncodeAndParse(encoder, parser, f1, /*key_frame=*/false, {0}, 0, 300);
  ASSERT_TRUE(high_hdr.has_value());
  EXPECT_EQ(high_hdr->quantizer.base_index, 255);
  EXPECT_FALSE(high_hdr->coded_lossless);
}

TEST_F(Av1VideoEncoderTest, QuantizerWithoutExternalRateControlFailsEncode) {
  constexpr gfx::Size kSize(256, 256);
  Av1VideoEncoder encoder;
  encoder.DisablePostedCallbacks();

  VideoEncoder::Options options;
  options.frame_size = kSize;
  options.framerate = 30;
  options.latency_mode = VideoEncoder::LatencyMode::Realtime;
  EncoderStatus init_status = EncoderStatus::Codes::kOk;
  encoder.Initialize(AV1PROFILE_PROFILE_MAIN, options,
                     /*info_cb=*/base::DoNothing(),
                     /*output_cb=*/base::DoNothing(),
                     base::BindLambdaForTesting([&](EncoderStatus status) {
                       init_status = std::move(status);
                     }));
  ASSERT_TRUE(init_status.is_ok()) << init_status.message();

  scoped_refptr<VideoFrame> frame =
      MakeDisjointRangeFrame(kSize, 64, base::Milliseconds(0));
  VideoEncoder::EncodeOptions opts(/*key_frame=*/true);
  opts.quantizer = 120;
  EncoderStatus encode_status = EncoderStatus::Codes::kOk;
  encoder.Encode(frame, opts,
                 base::BindLambdaForTesting([&](EncoderStatus status) {
                   encode_status = std::move(status);
                 }));
  EXPECT_EQ(encode_status.code(), EncoderStatus::Codes::kEncoderFailedEncode);
}

TEST_F(Av1VideoEncoderTest, CqpTemporalSvcDynamicQuantizerAndLossless) {
  constexpr gfx::Size kSize(256, 256);
  Av1VideoEncoder encoder;
  encoder.DisablePostedCallbacks();

  VideoEncoder::Options options;
  options.frame_size = kSize;
  options.framerate = 30;
  options.bitrate = Bitrate::ExternalRateControl();
  options.scalability_mode = SVCScalabilityMode::kL1T3;
  options.latency_mode = VideoEncoder::LatencyMode::Realtime;

  EncoderStatus init_status = EncoderStatus::Codes::kOk;
  encoder.Initialize(AV1PROFILE_PROFILE_MAIN, options,
                     /*info_cb=*/base::DoNothing(),
                     base::BindLambdaForTesting(
                         [&](VideoEncoderOutput output,
                             std::optional<VideoEncoder::CodecDescription>) {
                           outputs_.push_back(std::move(output));
                         }),
                     base::BindLambdaForTesting([&](EncoderStatus status) {
                       init_status = std::move(status);
                     }));
  ASSERT_TRUE(init_status.is_ok()) << init_status.message();

  Av1HeaderParser parser;
  constexpr std::array<int, 5> kQIndices = {120, 100, 0, 80, 120};
  constexpr std::array<int, 5> kExpectedTemporalIds = {0, 2, 1, 2, 0};

  for (size_t i = 0; i < kQIndices.size(); ++i) {
    scoped_refptr<VideoFrame> frame = MakeDisjointRangeFrame(
        kSize, static_cast<uint8_t>(48 + i * 24), base::Milliseconds(i * 33));
    std::optional<libgav1::ObuFrameHeader> hdr =
        EncodeAndParse(encoder, parser, frame, /*key_frame=*/i == 0, {},
                       std::nullopt, kQIndices[i]);
    ASSERT_TRUE(hdr.has_value()) << "frame " << i;
    EXPECT_EQ(outputs_.back().temporal_id, kExpectedTemporalIds[i]);
    EXPECT_EQ(hdr->quantizer.base_index, kQIndices[i]);
    EXPECT_EQ(hdr->coded_lossless, kQIndices[i] == 0);
  }
}

TEST_F(Av1VideoEncoderTest, ChangeOptionsUpscaleAfterLossless) {
  constexpr gfx::Size kLargeSize(640, 480);
  constexpr gfx::Size kSmallSize(320, 240);
  Av1VideoEncoder encoder;
  InitializeManualCqpEncoder(encoder, kLargeSize);

  Av1HeaderParser parser;
  auto change_options = [&](const gfx::Size& size) {
    VideoEncoder::Options options;
    options.frame_size = size;
    options.framerate = 30;
    options.bitrate = Bitrate::ExternalRateControl();
    options.manual_reference_buffer_control = true;
    options.latency_mode = VideoEncoder::LatencyMode::Realtime;
    std::optional<EncoderStatus> change_status;
    encoder.ChangeOptions(options, VideoEncoder::OutputCB(),
                          base::BindLambdaForTesting([&](EncoderStatus status) {
                            change_status = std::move(status);
                          }));
    ASSERT_TRUE(change_status.has_value());
    ASSERT_TRUE(change_status->is_ok()) << change_status->message();
  };

  // Downscale to 320x240 and encode a lossless frame.
  change_options(kSmallSize);
  scoped_refptr<VideoFrame> small_frame =
      MakeDisjointRangeFrame(kSmallSize, 64, base::Milliseconds(0));
  std::optional<libgav1::ObuFrameHeader> lossless_hdr = EncodeAndParse(
      encoder, parser, small_frame, /*key_frame=*/true, {}, 0, 0);
  ASSERT_TRUE(lossless_hdr.has_value());
  EXPECT_TRUE(lossless_hdr->coded_lossless);

  // Upscale back to 640x480 and verify lossy frames with cyclic refresh
  // segmentation succeed.
  change_options(kLargeSize);
  scoped_refptr<VideoFrame> large_f0 =
      MakeDisjointRangeFrame(kLargeSize, 64, base::Milliseconds(33));
  std::optional<libgav1::ObuFrameHeader> key_hdr =
      EncodeAndParse(encoder, parser, large_f0, /*key_frame=*/true, {}, 0, 120);
  ASSERT_TRUE(key_hdr.has_value());
  EXPECT_FALSE(key_hdr->coded_lossless);

  bool saw_segmentation = false;
  for (int i = 0; i < 12; ++i) {
    scoped_refptr<VideoFrame> delta =
        BlendFrames(*large_f0, *large_f0, base::Milliseconds(66 + i * 33));
    std::optional<libgav1::ObuFrameHeader> delta_hdr = EncodeAndParse(
        encoder, parser, delta, /*key_frame=*/false, {0}, 0, 120);
    ASSERT_TRUE(delta_hdr.has_value());
    EXPECT_EQ(delta_hdr->quantizer.base_index, 120);
    EXPECT_FALSE(delta_hdr->coded_lossless);
    saw_segmentation |= delta_hdr->segmentation.enabled;
  }
  EXPECT_TRUE(saw_segmentation);
}

TEST_F(Av1VideoEncoderTest, ManualReferencesAliasMostRecentToLast) {
  constexpr gfx::Size kSize(256, 256);
  Av1VideoEncoder encoder;
  InitializeManualCqpEncoder(encoder, kSize);

  Av1HeaderParser parser;
  auto f0 = MakeDisjointRangeFrame(kSize, 40, base::Milliseconds(0));
  auto f1 = MakeDisjointRangeFrame(kSize, 80, base::Milliseconds(33));
  auto f2 = MakeDisjointRangeFrame(kSize, 120, base::Milliseconds(66));
  auto f3 = MakeDisjointRangeFrame(kSize, 160, base::Milliseconds(99));

  // Keyframe populates all buffers (seq 1).
  auto hdr0 =
      EncodeAndParse(encoder, parser, f0, /*key_frame=*/true, {}, 0, 80);
  ASSERT_TRUE(hdr0.has_value());
  EXPECT_EQ(hdr0->refresh_frame_flags, 0xFF);

  // Delta frame 1 references buffer 0 and updates buffer 1 (seq 2).
  auto hdr1 =
      EncodeAndParse(encoder, parser, f1, /*key_frame=*/false, {0}, 1, 80);
  ASSERT_TRUE(hdr1.has_value());
  EXPECT_EQ(hdr1->reference_frame_index[0], 0);  // LAST -> buffer 0
  EXPECT_EQ(hdr1->refresh_frame_flags, 1 << 1);

  // Delta frame 2 references ONLY buffer 1 (and updates buffer 2, seq 3).
  // Buffer 1 must be aliased as LAST (index 0), not left only on GOLDEN with
  // LAST disabled.
  auto hdr2 =
      EncodeAndParse(encoder, parser, f2, /*key_frame=*/false, {1}, 2, 80);
  ASSERT_TRUE(hdr2.has_value());
  EXPECT_EQ(hdr2->reference_frame_index[0], 1);  // LAST -> buffer 1
  EXPECT_EQ(hdr2->refresh_frame_flags, 1 << 2);

  // Delta frame 3 passes references in oldest-to-newest order {0, 1, 2}.
  // Encoder must sort them by recency so LAST (0) -> 2, GOLDEN (3) -> 1,
  // ALTREF (6) -> 0.
  auto hdr3 = EncodeAndParse(encoder, parser, f3, /*key_frame=*/false,
                             {0, 1, 2}, std::nullopt, 80);
  ASSERT_TRUE(hdr3.has_value());
  EXPECT_EQ(hdr3->reference_frame_index[0], 2);
  EXPECT_EQ(hdr3->reference_frame_index[3], 1);
  EXPECT_EQ(hdr3->reference_frame_index[6], 0);
}

TEST_F(Av1VideoEncoderTest, CompoundPredictionUsedForTwoAndThreeReferences) {
  constexpr gfx::Size kSize(256, 256);
  Av1VideoEncoder encoder;
  InitializeManualCqpEncoder(encoder, kSize,
                             VideoEncoder::LatencyMode::Quality);

  Av1HeaderParser parser;
  int64_t ts_ms = 0;
  auto encode_with_ts = [&](const scoped_refptr<VideoFrame>& src, bool key,
                            std::initializer_list<uint8_t> refs,
                            std::optional<uint8_t> update_slot, int qp) {
    auto wrapped = VideoFrame::WrapVideoFrame(
        src, src->format(), src->visible_rect(), src->natural_size());
    wrapped->set_timestamp(base::Milliseconds(ts_ms));
    ts_ms += 33;
    return EncodeAndParse(encoder, parser, std::move(wrapped), key, refs,
                          update_slot, qp);
  };

  auto p0 = MakeDisjointRangeFrame(kSize, 188, base::TimeDelta());
  auto p1 = MakeDisjointRangeFrame(kSize, 36, base::TimeDelta());
  auto p2 = MakeDisjointRangeFrame(kSize, 112, base::TimeDelta());
  auto m12 = BlendFrames(*p1, *p2, base::TimeDelta());
  auto m02 = BlendFrames(*p0, *p2, base::TimeDelta(), 188);

  ASSERT_TRUE(encode_with_ts(p0, true, {}, 0, 20).has_value());
  ASSERT_TRUE(encode_with_ts(p1, false, {0}, 1, 20).has_value());

  auto encode_after_p2 = [&](const scoped_refptr<VideoFrame>& frame,
                             std::initializer_list<uint8_t> refs) {
    EXPECT_TRUE(encode_with_ts(p2, false, {1}, 2, 20).has_value());
    return encode_with_ts(frame, false, refs, std::nullopt, 180);
  };

  // Single reference: compound prediction must not be signaled.
  auto single_hdr = encode_after_p2(m12, {2});
  ASSERT_TRUE(single_hdr.has_value());
  EXPECT_FALSE(single_hdr->reference_mode_select);

  // Two references (passed as {1, 2}, sorted by recency to LAST=2, GOLDEN=1) on
  // m12 = (p1 + p2) / 2: exercises GOLDEN_LAST compound prediction.
  auto comp2_hdr = encode_after_p2(m12, {1, 2});
  ASSERT_TRUE(comp2_hdr.has_value());
  EXPECT_TRUE(comp2_hdr->reference_mode_select);

  // Three references (passed as {0, 1, 2}, sorted by recency to LAST=2,
  // GOLDEN=1, ALTREF=0) on m02 = (p0 + p2) / 2: exercises ALTREF_LAST compound
  // prediction.
  auto comp3_hdr = encode_after_p2(m02, {0, 1, 2});
  ASSERT_TRUE(comp3_hdr.has_value());
  EXPECT_TRUE(comp3_hdr->reference_mode_select);
}

}  // namespace
}  // namespace media
