// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/video_encode_accelerator_adapter.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_switches.h"
#include "media/base/mock_media_log.h"
#include "media/base/video_decoder.h"
#include "media/base/video_encoder.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/video/alpha_video_encoder_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"

#if BUILDFLAG(ENABLE_LIBVPX)
#include "media/filters/vpx_video_decoder.h"
#include "media/video/vpx_video_encoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8cx.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_codec.h"
#endif

namespace media {

class AlphaVideoEncoderWrapperTest
    : public ::testing::TestWithParam<VideoCodecProfile> {
 public:
  AlphaVideoEncoderWrapperTest() = default;

  void SetUp() override {
    profile_ = GetParam();
    codec_ = VideoCodecProfileToVideoCodec(profile_);
    encoder_ = CreateEncoder();
    if (!encoder_) {
      GTEST_SKIP()
          << "Couldn't create encoder for this configuration - skipping test";
    }
  }

  void TearDown() override {
    encoder_.reset();
    decoder_.reset();
    RunUntilIdle();
  }

  void PrepareDecoder(
      gfx::Size size,
      VideoDecoder::OutputCB output_cb,
      std::vector<uint8_t> extra_data = std::vector<uint8_t>()) {
    VideoDecoderConfig config(
        codec_, profile_, VideoDecoderConfig::AlphaMode::kHasAlpha,
        VideoColorSpace::JPEG(), VideoTransformation(), size, gfx::Rect(size),
        size, extra_data, EncryptionScheme::kUnencrypted);
#if BUILDFLAG(ENABLE_LIBVPX)
    decoder_ = std::make_unique<VpxVideoDecoder>();
    decoder_->Initialize(config, false, nullptr, base::DoNothing(),
                         std::move(output_cb), base::DoNothing());
#endif
    ASSERT_NE(decoder_, nullptr);
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }
  void RunUntilQuit() { task_environment_.RunUntilQuit(); }

  scoped_refptr<VideoFrame> CreateFrame(gfx::Size size,
                                        base::TimeDelta timestamp,
                                        uint32_t color = 0x964050) {
    auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_I420A, size,
                                         gfx::Rect(size), size, timestamp);
    uint32_t y = color & 0xFF;
    uint32_t u = (color >> 8) & 0xFF;
    uint32_t v = (color >> 16) & 0xFF;
    libyuv::I420Rect(frame->writable_data(VideoFrame::Plane::kY),
                     frame->stride(VideoFrame::Plane::kY),
                     frame->writable_data(VideoFrame::Plane::kU),
                     frame->stride(VideoFrame::Plane::kU),
                     frame->writable_data(VideoFrame::Plane::kV),
                     frame->stride(VideoFrame::Plane::kV),
                     frame->visible_rect().x(),       // x
                     frame->visible_rect().y(),       // y
                     frame->visible_rect().width(),   // width
                     frame->visible_rect().height(),  // height
                     y,                               // Y color
                     u,                               // U color
                     v);                              // V color
    libyuv::SetPlane(frame->writable_data(VideoFrame::Plane::kA),
                     frame->stride(VideoFrame::Plane::kA),
                     frame->visible_rect().width(),   // width
                     frame->visible_rect().height(),  // height
                     color);
    return frame;
  }

  std::unique_ptr<VideoEncoder> CreateEncoder() {
#if BUILDFLAG(ENABLE_LIBVPX)
    auto yuv_encoder = std::make_unique<VpxVideoEncoder>();
    auto alpha_encoder = std::make_unique<VpxVideoEncoder>();
    return std::make_unique<AlphaVideoEncoderWrapper>(std::move(yuv_encoder),
                                                      std::move(alpha_encoder));
#else
    return nullptr;
#endif  // ENABLE_LIBVPX
  }

  VideoEncoder::EncoderStatusCB ValidatingStatusCB(
      bool quit_run_loop_on_call = false,
      base::Location loc = FROM_HERE) {
    struct CallEnforcer {
      bool called = false;
      std::string location;
      ~CallEnforcer() {
        EXPECT_TRUE(called) << "Callback created: " << location;
      }
    };
    auto enforcer = std::make_unique<CallEnforcer>();
    enforcer->location = loc.ToString();
    auto check_callback = base::BindLambdaForTesting(
        [enforcer{std::move(enforcer)}](EncoderStatus s) {
          EXPECT_TRUE(s.is_ok())
              << " Callback created: " << enforcer->location
              << " Code: " << std::hex << static_cast<StatusCodeType>(s.code())
              << " Error: " << s.message();
          enforcer->called = true;
        });

    if (quit_run_loop_on_call) {
      return std::move(check_callback).Then(task_environment_.QuitClosure());
    } else {
      return check_callback;
    }
  }

 protected:
  VideoCodec codec_;
  VideoCodecProfile profile_;

  MockMediaLog media_log_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<VideoEncoder> encoder_;
  std::unique_ptr<VideoDecoder> decoder_;
};

TEST_P(AlphaVideoEncoderWrapperTest, InitializeAndFlush) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  bool output_called = false;
  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput, std::optional<VideoEncoder::CodecDescription>) {
        output_called = true;
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(output_cb),
                       ValidatingStatusCB(
                           /* quit_run_loop_on_call */ true));
  RunUntilQuit();
  encoder_->Flush(ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();
  EXPECT_FALSE(output_called) << "Output callback shouldn't be called";
}

TEST_P(AlphaVideoEncoderWrapperTest, ForceAllKeyFrames) {
  int outputs_count = 0;
  int frames = 10;
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  auto frame_duration = base::Seconds(1.0 / 60);

  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription> desc) {
        EXPECT_TRUE(output.key_frame);
        outputs_count++;
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(output_cb),
                       ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();

  for (int i = 0; i < frames; i++) {
    auto timestamp = i * frame_duration;
    auto frame = CreateFrame(options.frame_size, timestamp);
    encoder_->Encode(frame, VideoEncoder::EncodeOptions(true),
                     ValidatingStatusCB(/* quit_run_loop_on_call */ true));
    RunUntilQuit();
  }

  encoder_->Flush(ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();
  EXPECT_EQ(outputs_count, frames);
}

TEST_P(AlphaVideoEncoderWrapperTest, OutputCountEqualsFrameCount) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(320, 200);
  options.bitrate = Bitrate::VariableBitrate(1000000u, 2000000u);
  options.framerate = 25;
  options.keyframe_interval = options.framerate.value() * 3;  // every 3s
  int total_frames_count =
      options.framerate.value() * 10;  // total duration 20s
  int outputs_count = 0;

  auto frame_duration = base::Seconds(1.0 / options.framerate.value());

  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription> desc) {
        EXPECT_FALSE(output.data.empty());
        EXPECT_FALSE(output.alpha_data.empty());
        EXPECT_EQ(output.timestamp, frame_duration * outputs_count);
        outputs_count++;
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(output_cb),
                       ValidatingStatusCB(/* quit_run_loop_on_call */ true));

  RunUntilQuit();
  uint32_t color = 0x964050;
  for (int frame_index = 0; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * frame_duration;
    auto frame = CreateFrame(options.frame_size, timestamp, color);
    color = (color << 1) + frame_index;
    encoder_->Encode(frame, VideoEncoder::EncodeOptions(false),
                     ValidatingStatusCB());
  }

  encoder_->Flush(ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();
  EXPECT_EQ(outputs_count, total_frames_count);
}

TEST_P(AlphaVideoEncoderWrapperTest, EncodeAndDecode) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(320, 200);
  options.bitrate = Bitrate::ConstantBitrate(1000000u);  // 1Mbps
  options.framerate = 20;
  options.keyframe_interval = options.framerate.value() * 3;  // every 3s
  std::vector<scoped_refptr<VideoFrame>> frames_to_encode;
  std::vector<scoped_refptr<VideoFrame>> decoded_frames;
  int total_frames_count = options.framerate.value();

  auto frame_duration = base::Seconds(1.0 / options.framerate.value());

  VideoEncoder::OutputCB encoder_output_cb = base::BindLambdaForTesting(
      [&, this](VideoEncoderOutput output,
                std::optional<VideoEncoder::CodecDescription> desc) {
        auto buffer = DecoderBuffer::FromArray(std::move(output.data));
        buffer->set_timestamp(output.timestamp);
        buffer->set_is_key_frame(output.key_frame);
        EXPECT_FALSE(output.alpha_data.empty());
        std::vector<uint8_t>& buf_alpha = buffer->WritableSideData().alpha_data;
        // Side data id for alpha. Big endian one.
        buf_alpha.assign({0, 0, 0, 0, 0, 0, 0, 1});
        buf_alpha.insert(buf_alpha.end(), output.alpha_data.begin(),
                         output.alpha_data.end());
        decoder_->Decode(std::move(buffer), base::DoNothing());
      });

  VideoDecoder::OutputCB decoder_output_cb =
      base::BindLambdaForTesting([&](scoped_refptr<VideoFrame> frame) {
        decoded_frames.push_back(frame);
      });

  PrepareDecoder(options.frame_size, std::move(decoder_output_cb));

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(encoder_output_cb),
                       ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();

  uint32_t color = 0x964050;
  for (int frame_index = 0; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * frame_duration;
    auto frame = CreateFrame(options.frame_size, timestamp, color);
    frames_to_encode.push_back(frame);
    color = (color << 1) + frame_index;
    encoder_->Encode(frame, VideoEncoder::EncodeOptions(false),
                     ValidatingStatusCB());
  }

  encoder_->Flush(ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();

  auto quit = task_environment_.QuitClosure();
  decoder_->Decode(DecoderBuffer::CreateEOSBuffer(),
                   base::BindLambdaForTesting([&](DecoderStatus status) {
                     EXPECT_TRUE(status.is_ok());
                     quit.Run();
                   }));
  RunUntilQuit();
  EXPECT_EQ(decoded_frames.size(), frames_to_encode.size());
  for (auto i = 0u; i < decoded_frames.size(); i++) {
    auto original_frame = frames_to_encode[i];
    auto decoded_frame = decoded_frames[i];
    EXPECT_EQ(decoded_frame->format(), PIXEL_FORMAT_I420A);
    EXPECT_EQ(decoded_frame->timestamp(), original_frame->timestamp());
    EXPECT_EQ(decoded_frame->visible_rect().size(),
              original_frame->visible_rect().size());
  }
}

std::string PrintTestParams(
    const testing::TestParamInfo<VideoCodecProfile>& info) {
  auto result = GetProfileName(info.param);

  // GTest doesn't like spaces, but profile names have spaces, so we need
  // to replace them with underscores.
  for (auto& c : result) {
    if (c == ' ') {
      c = '_';
    }
  }
  return result;
}

#if BUILDFLAG(ENABLE_LIBVPX)
INSTANTIATE_TEST_SUITE_P(AlphaVideoEncoderWrapperTest,
                         AlphaVideoEncoderWrapperTest,
                         ::testing::Values(VP9PROFILE_PROFILE0, VP8PROFILE_ANY),
                         PrintTestParams);

#endif  // ENABLE_LIBVPX

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(AlphaVideoEncoderWrapperTest);

}  // namespace media
