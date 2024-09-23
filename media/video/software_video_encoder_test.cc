// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
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
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/test_helpers.h"
#include "media/base/test_random.h"
#include "media/base/video_decoder.h"
#include "media/base/video_encoder.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/video/video_encode_accelerator_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/filters/h264_to_annex_b_bitstream_converter.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/parsers/h264_parser.h"
#endif

#if BUILDFLAG(ENABLE_OPENH264)
#include "media/video/openh264_video_encoder.h"
#endif

#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
#include "media/filters/ffmpeg_video_decoder.h"
#endif

#if BUILDFLAG(ENABLE_LIBVPX)
#include "media/filters/vpx_video_decoder.h"
#include "media/video/vpx_video_encoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8cx.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_codec.h"
#endif
#if BUILDFLAG(ENABLE_LIBAOM)
#include "media/video/av1_video_encoder.h"
#endif

#if BUILDFLAG(ENABLE_DAV1D_DECODER)
#include "media/filters/dav1d_video_decoder.h"
#endif

namespace media {

struct SwVideoTestParams {
  VideoCodec codec;
  VideoCodecProfile profile;
  VideoPixelFormat pixel_format;
  std::optional<SVCScalabilityMode> scalability_mode;
};

class SoftwareVideoEncoderTest
    : public ::testing::TestWithParam<SwVideoTestParams> {
 public:
  SoftwareVideoEncoderTest() = default;

  void SetUp() override {
    auto args = GetParam();
    profile_ = args.profile;
    pixel_format_ = args.pixel_format;
    codec_ = args.codec;
    encoder_ = CreateEncoder(codec_);
    if (!encoder_) {
      GTEST_SKIP() << "Encoder is not supported on the platform";
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
        codec_, profile_, VideoDecoderConfig::AlphaMode::kIsOpaque,
        VideoColorSpace::JPEG(), VideoTransformation(), size, gfx::Rect(size),
        size, extra_data, EncryptionScheme::kUnencrypted);

    if (codec_ == VideoCodec::kH264) {
#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
      decoder_ = std::make_unique<FFmpegVideoDecoder>(&media_log_);
#endif
    } else if (codec_ == VideoCodec::kVP8 || codec_ == VideoCodec::kVP9) {
#if BUILDFLAG(ENABLE_LIBVPX)
      decoder_ = std::make_unique<VpxVideoDecoder>();
#endif
    } else if (codec_ == VideoCodec::kAV1) {
#if BUILDFLAG(ENABLE_DAV1D_DECODER)
      decoder_ = std::make_unique<Dav1dVideoDecoder>(media_log_.Clone());
#endif
    }

    EXPECT_NE(decoder_, nullptr);
    decoder_->Initialize(config, false, nullptr, DecoderStatusCB(),
                         std::move(output_cb), base::NullCallback());
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }
  void RunUntilQuit() { task_environment_.RunUntilQuit(); }

  scoped_refptr<VideoFrame> CreateFrame(
      gfx::Size size,
      VideoPixelFormat format,
      base::TimeDelta timestamp,
      std::optional<uint32_t> xor_mask = std::nullopt) {
    if (xor_mask) {
      auto frame = frame_pool_.CreateFrame(format, size, gfx::Rect(size), size,
                                           timestamp);
      FillFourColors(*frame, xor_mask);
      return frame;
    }

    if (!four_colors_frame_ || format != four_colors_frame_->format() ||
        size != four_colors_frame_->coded_size()) {
      four_colors_frame_.reset();
      four_colors_frame_ = frame_pool_.CreateFrame(
          format, size, gfx::Rect(size), size, timestamp);
      FillFourColors(*four_colors_frame_);
    }

    auto wrapped_frame = VideoFrame::WrapVideoFrame(four_colors_frame_, format,
                                                    gfx::Rect(size), size);
    wrapped_frame->set_timestamp(timestamp);
    return wrapped_frame;
  }

  std::unique_ptr<VideoEncoder> CreateEncoder(VideoCodec codec) {
    switch (codec) {
      case media::VideoCodec::kAV1:
#if BUILDFLAG(ENABLE_LIBAOM)
        return std::make_unique<media::Av1VideoEncoder>();
#else
        return nullptr;
#endif
      case media::VideoCodec::kVP8:
      case media::VideoCodec::kVP9:
#if BUILDFLAG(ENABLE_LIBVPX)
        if (profile_ == VP9PROFILE_PROFILE2 ||
            profile_ == VP9PROFILE_PROFILE3) {
          vpx_codec_caps_t codec_caps = vpx_codec_get_caps(vpx_codec_vp9_cx());
          if ((codec_caps & VPX_CODEC_CAP_HIGHBITDEPTH) == 0) {
            return nullptr;
          }
        }
        return std::make_unique<media::VpxVideoEncoder>();
#else
        return nullptr;
#endif
      case media::VideoCodec::kH264:
#if BUILDFLAG(ENABLE_OPENH264)
        return std::make_unique<OpenH264VideoEncoder>();
#else
        return nullptr;
#endif
      default:
        return nullptr;
    }
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
          ASSERT_TRUE(s.is_ok())
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

  VideoEncoder::EncoderStatusCB ValidateStatusThenQuitCB(
      base::Location loc = FROM_HERE) {
    return ValidatingStatusCB(/*quit_run_loop_on_call=*/true, loc);
  }

  VideoDecoder::DecodeCB DecoderStatusCB(base::Location loc = FROM_HERE) {
    struct CallEnforcer {
      bool called = false;
      std::string location;
      ~CallEnforcer() {
        EXPECT_TRUE(called) << "Callback created: " << location;
      }
    };
    auto enforcer = std::make_unique<CallEnforcer>();
    enforcer->location = loc.ToString();
    return base::BindLambdaForTesting(
        [enforcer{std::move(enforcer)}](DecoderStatus s) {
          EXPECT_TRUE(s.is_ok()) << " Callback created: " << enforcer->location
                                 << " Code: " << static_cast<int>(s.code())
                                 << " Error: " << s.message();
          enforcer->called = true;
        });
  }

  void DecodeAndWaitForStatus(
      scoped_refptr<DecoderBuffer> buffer,
      const base::Location& location = base::Location::Current()) {
    base::RunLoop run_loop;
    decoder_->Decode(std::move(buffer),
                     base::BindLambdaForTesting([&](DecoderStatus status) {
                       EXPECT_TRUE(status.is_ok())
                           << " Callback created: " << location.ToString()
                           << " Code: " << static_cast<int>(status.code())
                           << " Error: " << status.message();
                       run_loop.Quit();
                     }));
    run_loop.Run(location);
  }

  int CountDifferentPixels(VideoFrame& frame1, VideoFrame& frame2) {
    int diff_cnt = 0;
    uint8_t tolerance = 10;

    if (frame1.format() != frame2.format() ||
        frame1.visible_rect().size() != frame2.visible_rect().size()) {
      return frame1.coded_size().GetArea();
    }

    VideoPixelFormat format = frame1.format();
    size_t num_planes = VideoFrame::NumPlanes(format);
    gfx::Size visible_size = frame1.visible_rect().size();
    for (size_t plane = 0; plane < num_planes; ++plane) {
      int stride1 = frame1.stride(plane);
      int stride2 = frame2.stride(plane);
      size_t rows = VideoFrame::Rows(plane, format, visible_size.height());
      int row_bytes = VideoFrame::RowBytes(plane, format, visible_size.width());
      // SAFETY: `VideoFrame` `visible_data` has enough bytes to fit so many
      // rows each row has `stride` bytes.
      auto data1 = UNSAFE_BUFFERS(
          base::span(frame1.visible_data(plane), stride1 * rows));
      auto data2 = UNSAFE_BUFFERS(
          base::span(frame2.visible_data(plane), stride2 * rows));

      for (size_t r = 0; r < rows; ++r) {
        auto row1 = data1.subspan(stride1 * r);
        auto row2 = data2.subspan(stride2 * r);
        for (int c = 0; c < row_bytes; ++c) {
          uint8_t b1 = row1[c];
          uint8_t b2 = row2[c];
          uint8_t diff = std::max(b1, b2) - std::min(b1, b2);
          if (diff > tolerance)
            diff_cnt++;
        }
      }
    }
    return diff_cnt;
  }

  VideoPixelFormat GetExpectedOutputPixelFormat(VideoCodecProfile profile) {
    switch (profile) {
      case VP9PROFILE_PROFILE1:
      case AV1PROFILE_PROFILE_HIGH:
        return PIXEL_FORMAT_I444;
      case VP9PROFILE_PROFILE2:
        return PIXEL_FORMAT_YUV420P10;
      case VP9PROFILE_PROFILE3:
        return PIXEL_FORMAT_YUV444P10;
      default:
        return PIXEL_FORMAT_I420;
    }
  }

  std::pair<int, int> GetQpRange(VideoCodec codec) {
    switch (codec) {
      case media::VideoCodec::kAV1:
      case media::VideoCodec::kVP9:
        return {0, 63};
      default:
        return {0, 0};
    }
  }

  VideoEncoder::Options CreateDefaultOptions() {
    VideoEncoder::Options default_options;
    if (profile_ == VP9PROFILE_PROFILE1 || profile_ == VP9PROFILE_PROFILE3 ||
        profile_ == AV1PROFILE_PROFILE_HIGH) {
      default_options.subsampling = VideoChromaSampling::k444;
    }
    return default_options;
  }

  int AssignNextTemporalId(int frame_index, int number_temporal_layers) {
    if (number_temporal_layers <= 1) {
      return 0;
    }
    switch (number_temporal_layers) {
      case 2: {
        const static std::array<int, 2> kTwoTemporalLayers = {0, 1};
        return kTwoTemporalLayers[frame_index % kTwoTemporalLayers.size()];
      }
      case 3: {
        const static std::array<int, 4> kThreeTemporalLayers = {0, 2, 1, 2};
        return kThreeTemporalLayers[frame_index % kThreeTemporalLayers.size()];
      }
    }
    return 0;
  }

 protected:
  VideoCodec codec_;
  VideoCodecProfile profile_;
  VideoPixelFormat pixel_format_;
  VideoFrameConverter frame_converter_;

  VideoFramePool frame_pool_;
  scoped_refptr<VideoFrame> four_colors_frame_;

  NullMediaLog media_log_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<VideoEncoder> encoder_;
  std::unique_ptr<VideoDecoder> decoder_;
};

class H264VideoEncoderTest : public SoftwareVideoEncoderTest {};
class SVCVideoEncoderTest : public SoftwareVideoEncoderTest {};
class ManualSVCVideoEncoderTest : public SoftwareVideoEncoderTest {};

TEST_P(SoftwareVideoEncoderTest, StopCallbackWrapping) {
  VideoEncoder::Options options = CreateDefaultOptions();
  options.frame_size = gfx::Size(640, 480);
  bool init_called = false;
  bool flush_called = false;
  encoder_->DisablePostedCallbacks();

  VideoEncoder::EncoderStatusCB init_cb = base::BindLambdaForTesting(
      [&](EncoderStatus error) { init_called = true; });

  VideoEncoder::EncoderStatusCB flush_cb = base::BindLambdaForTesting(
      [&](EncoderStatus error) { flush_called = true; });
  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       /*output_cb=*/base::DoNothing(), std::move(init_cb));
  encoder_->Flush(std::move(flush_cb));
  EXPECT_TRUE(init_called);
  EXPECT_TRUE(flush_called);
}

TEST_P(SoftwareVideoEncoderTest, InitializeAndFlush) {
  VideoEncoder::Options options = CreateDefaultOptions();
  options.frame_size = gfx::Size(640, 480);
  bool output_called = false;
  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput, std::optional<VideoEncoder::CodecDescription>) {
        output_called = true;
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(output_cb), ValidateStatusThenQuitCB());
  RunUntilQuit();
  encoder_->Flush(ValidateStatusThenQuitCB());
  RunUntilQuit();
  EXPECT_FALSE(output_called) << "Output callback shouldn't be called";
}

TEST_P(SoftwareVideoEncoderTest, ForceAllKeyFrames) {
  int outputs_count = 0;
  int frames = 10;
  VideoEncoder::Options options = CreateDefaultOptions();
  options.frame_size = gfx::Size(640, 480);
  auto frame_duration = base::Seconds(1.0 / 60);

  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription> desc) {
        EXPECT_TRUE(output.key_frame);
        outputs_count++;
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(output_cb), ValidateStatusThenQuitCB());
  RunUntilQuit();

  for (int i = 0; i < frames; i++) {
    auto timestamp = i * frame_duration;
    auto frame = CreateFrame(options.frame_size, pixel_format_, timestamp);
    encoder_->Encode(std::move(frame), VideoEncoder::EncodeOptions(true),
                     ValidateStatusThenQuitCB());
    RunUntilQuit();
  }

  encoder_->Flush(ValidateStatusThenQuitCB());
  RunUntilQuit();
  EXPECT_EQ(outputs_count, frames);
}

TEST_P(SoftwareVideoEncoderTest, ResizeFrames) {
  int outputs_count = 0;
  VideoEncoder::Options options = CreateDefaultOptions();
  options.frame_size = gfx::Size(640, 480);
  auto sec = base::Seconds(1);

  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription> desc) {
        outputs_count++;
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(output_cb), ValidateStatusThenQuitCB());

  RunUntilQuit();
  auto frame1 = CreateFrame(gfx::Size(320, 200), pixel_format_, 0 * sec);
  encoder_->Encode(std::move(frame1), VideoEncoder::EncodeOptions(false),
                   ValidatingStatusCB());
  auto frame2 = CreateFrame(gfx::Size(800, 600), pixel_format_, 1 * sec);
  encoder_->Encode(std::move(frame2), VideoEncoder::EncodeOptions(false),
                   ValidatingStatusCB());
  auto frame3 = CreateFrame(gfx::Size(720, 1280), pixel_format_, 2 * sec);
  encoder_->Encode(std::move(frame3), VideoEncoder::EncodeOptions(false),
                   ValidatingStatusCB());

  encoder_->Flush(ValidateStatusThenQuitCB());
  RunUntilQuit();
  EXPECT_EQ(outputs_count, 3);
}

TEST_P(SoftwareVideoEncoderTest, OutputCountEqualsFrameCount) {
  VideoEncoder::Options options = CreateDefaultOptions();
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
        EXPECT_EQ(output.timestamp, frame_duration * outputs_count);
        outputs_count++;
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(output_cb), ValidateStatusThenQuitCB());

  RunUntilQuit();
  for (int frame_index = 0; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * frame_duration;
    auto frame = CreateFrame(options.frame_size, pixel_format_, timestamp);
    encoder_->Encode(std::move(frame), VideoEncoder::EncodeOptions(false),
                     ValidatingStatusCB());
  }

  encoder_->Flush(ValidateStatusThenQuitCB());
  RunUntilQuit();
  EXPECT_EQ(outputs_count, total_frames_count);
}

TEST_P(SoftwareVideoEncoderTest, PerFrameQpEncoding) {
  VideoEncoder::Options options = CreateDefaultOptions();
  options.frame_size = gfx::Size(320, 200);
  options.bitrate = Bitrate::ExternalRateControl();
  options.framerate = 25;
  auto qp_range = GetQpRange(codec_);
  if (qp_range.first == qp_range.second) {
    GTEST_SKIP() << "Per frame QP control is not supported.";
  }
  int total_frames_count = qp_range.second - qp_range.first + 1;
  int outputs_count = 0;

  auto frame_duration = base::Seconds(1.0 / options.framerate.value());

  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription> desc) {
        EXPECT_FALSE(output.data.empty());
        outputs_count++;
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(output_cb), ValidateStatusThenQuitCB());

  RunUntilQuit();
  int qp = qp_range.first;
  for (int frame_index = 0; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * frame_duration;
    auto frame = CreateFrame(options.frame_size, pixel_format_, timestamp);
    VideoEncoder::EncodeOptions encode_options(false);
    encode_options.quantizer = qp;
    qp++;
    encoder_->Encode(std::move(frame), encode_options, ValidatingStatusCB());
  }

  encoder_->Flush(ValidateStatusThenQuitCB());
  RunUntilQuit();
  EXPECT_EQ(outputs_count, total_frames_count);
}

#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
TEST_P(SoftwareVideoEncoderTest, EncodeAndDecode) {
  VideoEncoder::Options options = CreateDefaultOptions();
  options.frame_size = gfx::Size(320, 200);
  options.bitrate = Bitrate::ConstantBitrate(1000000u);  // 1Mbps
  options.framerate = 25;
  if (codec_ == VideoCodec::kH264)
    options.avc.produce_annexb = true;
  options.keyframe_interval = options.framerate.value() * 3;  // every 3s
  base::circular_deque<scoped_refptr<VideoFrame>> frames_to_encode;
  int total_frames_count =
      options.framerate.value() * 10;  // total duration 10s
  int total_decoded_frames = 0;

  auto frame_duration = base::Seconds(1.0 / options.framerate.value());

  VideoEncoder::OutputCB encoder_output_cb = base::BindLambdaForTesting(
      [&, this](VideoEncoderOutput output,
                std::optional<VideoEncoder::CodecDescription> desc) {
        auto buffer = DecoderBuffer::FromArray(std::move(output.data));
        buffer->set_timestamp(output.timestamp);
        buffer->set_is_key_frame(output.key_frame);
        decoder_->Decode(std::move(buffer), DecoderStatusCB());
      });

  VideoDecoder::OutputCB decoder_output_cb =
      base::BindLambdaForTesting([&](scoped_refptr<VideoFrame> decoded_frame) {
        ASSERT_FALSE(frames_to_encode.empty());
        auto original_frame = std::move(frames_to_encode.front());
        frames_to_encode.pop_front();

        EXPECT_EQ(decoded_frame->timestamp(), original_frame->timestamp());
        EXPECT_EQ(decoded_frame->visible_rect().size(),
                  original_frame->visible_rect().size());
        EXPECT_EQ(decoded_frame->format(),
                  GetExpectedOutputPixelFormat(profile_));
        if (decoded_frame->format() == original_frame->format()) {
          EXPECT_LE(CountDifferentPixels(*decoded_frame, *original_frame),
                    original_frame->visible_rect().width());
        }
        ++total_decoded_frames;
      });

  PrepareDecoder(options.frame_size, std::move(decoder_output_cb));

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(encoder_output_cb),
                       ValidateStatusThenQuitCB());
  RunUntilQuit();

  TestRandom random_color(0x964050);
  for (int frame_index = 0; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * frame_duration;
    auto frame = CreateFrame(options.frame_size, pixel_format_, timestamp,
                             random_color.Rand() & 0x00FFFFFF);
    frames_to_encode.push_back(frame);
    encoder_->Encode(std::move(frame), VideoEncoder::EncodeOptions(false),
                     ValidateStatusThenQuitCB());
    ASSERT_NO_FATAL_FAILURE(RunUntilQuit());
  }

  encoder_->Flush(ValidateStatusThenQuitCB());
  RunUntilQuit();
  DecodeAndWaitForStatus(DecoderBuffer::CreateEOSBuffer());
  EXPECT_EQ(total_decoded_frames, total_frames_count);
}

TEST_P(SoftwareVideoEncoderTest, EncodeAndDecodeWithEnablingDrop) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kWebCodecsVideoEncoderFrameDrop);
  ASSERT_GT(GetDefaultVideoEncoderDropFrameThreshold(), 0u);

  VideoEncoder::Options options = CreateDefaultOptions();
  options.frame_size = gfx::Size(320, 200);
  options.bitrate = Bitrate::ConstantBitrate(1000000u);  // 1Mbps
  options.framerate = 25;
  if (codec_ == VideoCodec::kH264) {
    options.avc.produce_annexb = true;
  }
  options.keyframe_interval = options.framerate.value() * 3;  // every 3s
  options.latency_mode = VideoEncoder::LatencyMode::Realtime;

  base::circular_deque<scoped_refptr<VideoFrame>> frames_to_encode;
  const int total_frames_count =
      options.framerate.value() * 10;  // total duration 10s
  int total_decoded_frames = 0;
  int dropped_frames_count = 0;
  base::circular_deque<base::TimeDelta> dropped_frame_timestamps;

  auto frame_duration = base::Seconds(1.0 / options.framerate.value());
  VideoEncoder::OutputCB encoder_output_cb = base::BindLambdaForTesting(
      [&, this](VideoEncoderOutput output,
                std::optional<VideoEncoder::CodecDescription> desc) {
        if (output.data.empty()) {
          dropped_frames_count++;
          dropped_frame_timestamps.push_back(output.timestamp);
          return;
        }

        auto buffer = DecoderBuffer::FromArray(std::move(output.data));
        buffer->set_timestamp(output.timestamp);
        buffer->set_is_key_frame(output.key_frame);
        decoder_->Decode(std::move(buffer), DecoderStatusCB());
      });

  VideoDecoder::OutputCB decoder_output_cb =
      base::BindLambdaForTesting([&](scoped_refptr<VideoFrame> decoded_frame) {
        ASSERT_FALSE(frames_to_encode.empty());
        scoped_refptr<VideoFrame> original_frame;
        while (!frames_to_encode.empty()) {
          original_frame = std::move(frames_to_encode.front());
          frames_to_encode.pop_front();
          if (decoded_frame->timestamp() == original_frame->timestamp()) {
            break;
          }
          ASSERT_FALSE(dropped_frame_timestamps.empty());
          auto dropped_frame_timestamp = dropped_frame_timestamps.front();
          dropped_frame_timestamps.pop_front();
          EXPECT_EQ(original_frame->timestamp(), dropped_frame_timestamp);
          original_frame.reset();
        }

        ASSERT_TRUE(original_frame);
        EXPECT_EQ(decoded_frame->visible_rect().size(),
                  original_frame->visible_rect().size());
        EXPECT_EQ(decoded_frame->format(),
                  GetExpectedOutputPixelFormat(profile_));
        if (decoded_frame->format() == original_frame->format()) {
          EXPECT_LE(CountDifferentPixels(*decoded_frame, *original_frame),
                    original_frame->visible_rect().width());
        }
        ++total_decoded_frames;
      });

  PrepareDecoder(options.frame_size, std::move(decoder_output_cb));

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(encoder_output_cb),
                       ValidateStatusThenQuitCB());
  RunUntilQuit();

  TestRandom random_color(0x964050);
  for (int frame_index = 0; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * frame_duration;
    auto frame = CreateFrame(options.frame_size, pixel_format_, timestamp,
                             random_color.Rand() & 0x00FFFFFF);
    frames_to_encode.push_back(frame);
    encoder_->Encode(std::move(frame), VideoEncoder::EncodeOptions(false),
                     ValidateStatusThenQuitCB());
    ASSERT_NO_FATAL_FAILURE(RunUntilQuit());
  }

  encoder_->Flush(ValidateStatusThenQuitCB());
  RunUntilQuit();
  DecodeAndWaitForStatus(DecoderBuffer::CreateEOSBuffer());
  EXPECT_EQ(total_decoded_frames + dropped_frames_count, total_frames_count);
}

TEST_P(SVCVideoEncoderTest, EncodeClipTemporalSvc) {
  VideoEncoder::Options options = CreateDefaultOptions();
  options.frame_size = gfx::Size(320, 200);
  options.bitrate = Bitrate::ConstantBitrate(1000000u);  // 1Mbps
  options.framerate = 25;
  options.scalability_mode = GetParam().scalability_mode;
  if (codec_ == VideoCodec::kH264)
    options.avc.produce_annexb = true;
  std::vector<scoped_refptr<VideoFrame>> frames_to_encode;

  std::vector<VideoEncoderOutput> chunks;
  size_t total_frames_count = 80;

  // Encoder all frames with 3 temporal layers and put all outputs in |chunks|
  auto frame_duration = base::Seconds(1.0 / options.framerate.value());

  VideoEncoder::OutputCB encoder_output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription> desc) {
        chunks.push_back(std::move(output));
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(encoder_output_cb),
                       ValidateStatusThenQuitCB());
  RunUntilQuit();

  for (auto frame_index = 0u; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * frame_duration;
    auto frame = CreateFrame(options.frame_size, pixel_format_, timestamp);
    frames_to_encode.push_back(frame);
    encoder_->Encode(std::move(frame), VideoEncoder::EncodeOptions(false),
                     ValidateStatusThenQuitCB());
    RunUntilQuit();
  }

  encoder_->Flush(ValidateStatusThenQuitCB());
  RunUntilQuit();
  EXPECT_EQ(chunks.size(), total_frames_count);

  int num_temporal_layers = 1;
  if (options.scalability_mode) {
    switch (options.scalability_mode.value()) {
      case SVCScalabilityMode::kL1T1:
        // Nothing to do
        break;
      case SVCScalabilityMode::kL1T2:
        num_temporal_layers = 2;
        break;
      case SVCScalabilityMode::kL1T3:
        num_temporal_layers = 3;
        break;
      default:
        NOTREACHED_IN_MIGRATION()
            << "Unsupported SVC: "
            << GetScalabilityModeName(options.scalability_mode.value());
    }
  }
  // Try decoding saved outputs dropping varying number of layers
  // and check that decoded frames indeed match the pattern:
  // Layer Index 0: |0| | | |4| | | |8| |  |  |12|
  // Layer Index 1: | | |2| | | |6| | | |10|  |  |
  // Layer Index 2: | |1| |3| |5| |7| |9|  |11|  |
  for (int max_layer = 0; max_layer < num_temporal_layers; max_layer++) {
    std::vector<scoped_refptr<VideoFrame>> decoded_frames;
    VideoDecoder::OutputCB decoder_output_cb =
        base::BindLambdaForTesting([&](scoped_refptr<VideoFrame> frame) {
          decoded_frames.push_back(frame);
        });
    PrepareDecoder(options.frame_size, std::move(decoder_output_cb));

    for (auto& chunk : chunks) {
      if (chunk.temporal_id <= max_layer && !chunk.data.empty()) {
        auto buffer = DecoderBuffer::CopyFrom(chunk.data);
        buffer->set_timestamp(chunk.timestamp);
        buffer->set_is_key_frame(chunk.key_frame);
        DecodeAndWaitForStatus(std::move(buffer));
      }
    }
    DecodeAndWaitForStatus(DecoderBuffer::CreateEOSBuffer());

    int rate_decimator = (1 << (num_temporal_layers - 1)) / (1 << max_layer);
    ASSERT_EQ(decoded_frames.size(),
              size_t{total_frames_count / rate_decimator});
    for (auto i = 0u; i < decoded_frames.size(); i++) {
      auto decoded_frame = decoded_frames[i];
      auto original_frame = frames_to_encode[i * rate_decimator];
      EXPECT_EQ(decoded_frame->timestamp(), original_frame->timestamp());
    }
  }
}

TEST_P(SVCVideoEncoderTest, EncodeClipTemporalSvcWithEnablingDrop) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kWebCodecsVideoEncoderFrameDrop);
  ASSERT_GT(GetDefaultVideoEncoderDropFrameThreshold(), 0u);

  VideoEncoder::Options options = CreateDefaultOptions();
  options.frame_size = gfx::Size(320, 200);
  options.bitrate = Bitrate::ConstantBitrate(100000u);  // 100kbps
  options.framerate = 25;
  options.scalability_mode = GetParam().scalability_mode;
  if (codec_ == VideoCodec::kH264) {
    options.avc.produce_annexb = true;
  }
  options.latency_mode = VideoEncoder::LatencyMode::Realtime;

  std::vector<scoped_refptr<VideoFrame>> frames_to_encode;

  std::vector<VideoEncoderOutput> chunks;
  size_t total_frames_count = 80;
  std::vector<size_t> dropped_frame_indices;

  // Encoder all frames with 3 temporal layers and put all outputs in |chunks|
  auto frame_duration = base::Seconds(1.0 / options.framerate.value());

  VideoEncoder::OutputCB encoder_output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription> desc) {
        if (output.data.empty()) {
          dropped_frame_indices.push_back(chunks.size() +
                                          dropped_frame_indices.size());
          return;
        }
        chunks.push_back(std::move(output));
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(encoder_output_cb),
                       ValidateStatusThenQuitCB());
  RunUntilQuit();

  for (auto frame_index = 0u; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * frame_duration;
    auto frame = CreateFrame(options.frame_size, pixel_format_, timestamp);
    frames_to_encode.push_back(frame);
    encoder_->Encode(std::move(frame), VideoEncoder::EncodeOptions(false),
                     ValidateStatusThenQuitCB());
    RunUntilQuit();
  }

  encoder_->Flush(ValidateStatusThenQuitCB());
  RunUntilQuit();
  EXPECT_EQ(chunks.size() + dropped_frame_indices.size(), total_frames_count);

  int num_temporal_layers = 1;
  if (options.scalability_mode) {
    switch (options.scalability_mode.value()) {
      case SVCScalabilityMode::kL1T1:
        // Nothing to do
        break;
      case SVCScalabilityMode::kL1T2:
        num_temporal_layers = 2;
        break;
      case SVCScalabilityMode::kL1T3:
        num_temporal_layers = 3;
        break;
      default:
        NOTREACHED_IN_MIGRATION()
            << "Unsupported SVC: "
            << GetScalabilityModeName(options.scalability_mode.value());
    }
  }
  // Try decoding saved outputs dropping varying number of layers
  // and check that decoded frames indeed match the pattern:
  // Layer Index 0: |0| | | |4| | | |8| |  |  |12|
  // Layer Index 1: | | |2| | | |6| | | |10|  |  |
  // Layer Index 2: | |1| |3| |5| |7| |9|  |11|  |
  constexpr size_t kTemporalLayerCycle = 4;
  constexpr std::array<int, 4> kTemporalLayerTable[kTemporalLayerCycle] = {
      {0, 0, 0, 0},
      {0, 1, 0, 1},
      {0, 2, 1, 2},
  };
  auto last_layer =
      base::span(base::span(kTemporalLayerTable)[num_temporal_layers - 1]);
  for (size_t i = 0; i < chunks.size(); ++i) {
    ASSERT_FALSE(chunks[i].data.empty());
    EXPECT_EQ(chunks[i].temporal_id, last_layer[i % kTemporalLayerCycle]);
  }

  for (int max_layer = 0; max_layer < num_temporal_layers; max_layer++) {
    std::vector<scoped_refptr<VideoFrame>> decoded_frames;
    VideoDecoder::OutputCB decoder_output_cb =
        base::BindLambdaForTesting([&](scoped_refptr<VideoFrame> frame) {
          decoded_frames.push_back(frame);
        });
    PrepareDecoder(options.frame_size, std::move(decoder_output_cb));

    size_t num_chunks = 0;
    for (auto& chunk : chunks) {
      if (chunk.temporal_id <= max_layer) {
        num_chunks += 1;
        auto buffer = DecoderBuffer::CopyFrom(chunk.data);
        buffer->set_timestamp(chunk.timestamp);
        buffer->set_is_key_frame(chunk.key_frame);
        DecodeAndWaitForStatus(std::move(buffer));
      }
    }
    DecodeAndWaitForStatus(DecoderBuffer::CreateEOSBuffer());
    ASSERT_EQ(decoded_frames.size(), num_chunks);
    size_t encoded_frame_index = 0;
    size_t decoded_frame_index = 0;
    for (size_t i = 0; i < frames_to_encode.size(); ++i) {
      if (base::Contains(dropped_frame_indices, i)) {
        // Dropped
        continue;
      }
      const int temporal_id = base::span(
          kTemporalLayerTable)[num_temporal_layers - 1]
                              [encoded_frame_index % kTemporalLayerCycle];
      encoded_frame_index++;
      if (temporal_id > max_layer) {
        // Not decoded frame.
        continue;
      }

      auto decoded_frame = decoded_frames[decoded_frame_index];
      decoded_frame_index++;
      auto original_frame = frames_to_encode[i];
      EXPECT_EQ(decoded_frame->timestamp(), original_frame->timestamp());
    }
  }
}

TEST_P(ManualSVCVideoEncoderTest, EncodeClipTemporalSvc) {
  VideoEncoder::Options options = CreateDefaultOptions();
  options.frame_size = gfx::Size(320, 200);
  options.bitrate = Bitrate::ExternalRateControl();
  options.framerate = 25;
  options.manual_reference_buffer_control = true;
  auto scalability_mode = GetParam().scalability_mode;
  VideoEncoderInfo encoder_info;
  if (codec_ == VideoCodec::kH264) {
    options.avc.produce_annexb = true;
  }
  std::vector<scoped_refptr<VideoFrame>> frames_to_encode;

  std::vector<VideoEncoderOutput> chunks;
  size_t total_frames_count = 80;

  // Encoder all frames with 3 temporal layers and put all outputs in |chunks|
  auto frame_duration = base::Seconds(1.0 / options.framerate.value());

  VideoEncoder::OutputCB encoder_output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription> desc) {
        chunks.push_back(std::move(output));
      });

  VideoEncoder::EncoderInfoCB encoder_info_cb = base::BindLambdaForTesting(
      [&](const VideoEncoderInfo& info) { encoder_info = info; });

  encoder_->Initialize(profile_, options, std::move(encoder_info_cb),
                       std::move(encoder_output_cb),
                       ValidateStatusThenQuitCB());
  RunUntilQuit();
  ASSERT_EQ(encoder_info.number_of_manual_reference_buffers, 3ul);

  int num_temporal_layers = 0;
  if (scalability_mode) {
    switch (scalability_mode.value()) {
      case SVCScalabilityMode::kL1T1:
        num_temporal_layers = 1;
        break;
      case SVCScalabilityMode::kL1T2:
        num_temporal_layers = 2;
        break;
      case SVCScalabilityMode::kL1T3:
        num_temporal_layers = 3;
        break;
      default:
        NOTREACHED_IN_MIGRATION()
            << "Unsupported SVC: "
            << GetScalabilityModeName(scalability_mode.value());
    }
  }

  for (auto frame_index = 0u; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * frame_duration;
    auto frame = CreateFrame(options.frame_size, pixel_format_, timestamp);
    frames_to_encode.push_back(frame);
    VideoEncoder::EncodeOptions encode_options(false);
    switch (num_temporal_layers) {
      case 0:
        // No references, every frame is basically a key frame.
        encode_options.update_buffer = 0;
        break;
      case 1:
        // Each frame depends only on the previous one.
        encode_options.update_buffer = 0;
        encode_options.reference_buffers.push_back(0);
        break;
      case 2:
        // Emulate L1T2 scalability mode
        if (frame_index % 2 == 0) {
          encode_options.update_buffer = 0;
        }
        encode_options.reference_buffers.push_back(0);
        break;
      case 3:
        // Emulate L1T3 scalability mode
        int index_in_cycle = frame_index % 4;
        encode_options.reference_buffers.push_back(0);
        if (index_in_cycle == 0) {
          encode_options.update_buffer = 0;
        } else if (index_in_cycle == 1) {
          // Nothing to update.
        } else if (index_in_cycle == 2) {
          encode_options.update_buffer = 1;
        } else {
          encode_options.reference_buffers.push_back(1);
        }
        break;
    }
    encode_options.quantizer = 30;
    encoder_->Encode(std::move(frame), encode_options,
                     ValidateStatusThenQuitCB());
    RunUntilQuit();
  }

  encoder_->Flush(ValidateStatusThenQuitCB());
  RunUntilQuit();
  EXPECT_EQ(chunks.size(), total_frames_count);

  // Try decoding saved outputs dropping varying number of layers
  // and check that decoded frames indeed match the pattern:
  // Layer Index 0: |0| | | |4| | | |8| |  |  |12|
  // Layer Index 1: | | |2| | | |6| | | |10|  |  |
  // Layer Index 2: | |1| |3| |5| |7| |9|  |11|  |
  for (int max_layer = 0; max_layer < num_temporal_layers; max_layer++) {
    std::vector<scoped_refptr<VideoFrame>> decoded_frames;
    VideoDecoder::OutputCB decoder_output_cb =
        base::BindLambdaForTesting([&](scoped_refptr<VideoFrame> frame) {
          decoded_frames.push_back(frame);
        });
    PrepareDecoder(options.frame_size, std::move(decoder_output_cb));

    for (size_t frame_index = 0; frame_index < chunks.size(); frame_index++) {
      auto& chunk = chunks[frame_index];
      int temporal_id = AssignNextTemporalId(static_cast<int>(frame_index),
                                             num_temporal_layers);
      if (temporal_id <= max_layer && !chunk.data.empty()) {
        auto buffer = DecoderBuffer::CopyFrom(chunk.data);
        buffer->set_timestamp(chunk.timestamp);
        buffer->set_is_key_frame(chunk.key_frame);
        DecodeAndWaitForStatus(std::move(buffer));
      }
    }
    DecodeAndWaitForStatus(DecoderBuffer::CreateEOSBuffer());

    int rate_decimator = (1 << (num_temporal_layers - 1)) / (1 << max_layer);
    ASSERT_EQ(decoded_frames.size(),
              size_t{total_frames_count / rate_decimator});
    for (auto i = 0u; i < decoded_frames.size(); i++) {
      auto decoded_frame = decoded_frames[i];
      auto original_frame = frames_to_encode[i * rate_decimator];
      EXPECT_EQ(decoded_frame->timestamp(), original_frame->timestamp());
    }
  }
}

TEST_P(SVCVideoEncoderTest, ChangeLayers) {
  VideoEncoder::Options options = CreateDefaultOptions();
  options.frame_size = gfx::Size(640, 480);
  options.bitrate = Bitrate::ConstantBitrate(1000000u);  // 1Mbps
  options.framerate = 25;
  options.scalability_mode = GetParam().scalability_mode;
  std::vector<scoped_refptr<VideoFrame>> frames_to_encode;

  std::vector<VideoEncoderOutput> chunks;
  size_t total_frames_count = 80;

  // Encoder all frames with 3 temporal layers and put all outputs in |chunks|
  auto frame_duration = base::Seconds(1.0 / options.framerate.value());

  VideoEncoder::OutputCB encoder_output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription> desc) {
        chunks.push_back(std::move(output));
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(encoder_output_cb),
                       ValidateStatusThenQuitCB());
  RunUntilQuit();

  for (auto frame_index = 0u; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * frame_duration;

    const bool reconfigure = (frame_index == total_frames_count / 2);
    if (reconfigure) {
      encoder_->Flush(ValidateStatusThenQuitCB());
      RunUntilQuit();

      // Ask encoder to change SVC mode, empty output callback
      // means the encoder should keep the old one.
      options.scalability_mode = SVCScalabilityMode::kL1T1;
      encoder_->ChangeOptions(options, VideoEncoder::OutputCB(),
                              ValidateStatusThenQuitCB());
      RunUntilQuit();
    }

    auto frame = CreateFrame(options.frame_size, pixel_format_, timestamp);
    frames_to_encode.push_back(frame);
    encoder_->Encode(std::move(frame), VideoEncoder::EncodeOptions(false),
                     ValidateStatusThenQuitCB());
    RunUntilQuit();
  }

  encoder_->Flush(ValidateStatusThenQuitCB());
  RunUntilQuit();
  EXPECT_EQ(chunks.size(), total_frames_count);
}

TEST_P(SoftwareVideoEncoderTest, ReconfigureWithResizingNumberOfThreads) {
  int outputs_count = 0;
  VideoEncoder::Options options = CreateDefaultOptions();
  options.frame_size = gfx::Size(1024, 1024);

  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription> desc) {
        outputs_count++;
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(output_cb), ValidatingStatusCB());

  auto frame0 = CreateFrame(options.frame_size, pixel_format_, {});
  encoder_->Encode(std::move(frame0), VideoEncoder::EncodeOptions(false),
                   ValidatingStatusCB());

  options.frame_size = gfx::Size(1000, 608);
  encoder_->ChangeOptions(options, VideoEncoder::OutputCB(),
                          ValidatingStatusCB());

  auto frame1 = CreateFrame(options.frame_size, pixel_format_, {});
  encoder_->Encode(std::move(frame1), VideoEncoder::EncodeOptions(false),
                   ValidatingStatusCB());

  options.frame_size = gfx::Size(16, 720);
  encoder_->ChangeOptions(options, VideoEncoder::OutputCB(),
                          ValidatingStatusCB());

  auto frame2 = CreateFrame(options.frame_size, pixel_format_, {});
  encoder_->Encode(std::move(frame2), VideoEncoder::EncodeOptions(false),
                   ValidateStatusThenQuitCB());

  RunUntilQuit();
  EXPECT_EQ(outputs_count, 3);
}

TEST_P(H264VideoEncoderTest, ReconfigureWithResize) {
  VideoEncoder::Options options = CreateDefaultOptions();
  gfx::Size size1(320, 200), size2(400, 240);
  options.frame_size = size1;
  options.bitrate = Bitrate::ConstantBitrate(1000000u);  // 1Mbps
  options.framerate = 25;
  if (codec_ == VideoCodec::kH264)
    options.avc.produce_annexb = true;
  base::circular_deque<scoped_refptr<VideoFrame>> frames_to_encode;
  size_t total_frames_count = 8;
  size_t total_decoded_frames = 0;
  auto frame_duration = base::Seconds(1.0 / options.framerate.value());

  VideoDecoder::OutputCB decoder_output_cb =
      base::BindLambdaForTesting([&](scoped_refptr<VideoFrame> frame) {
        ASSERT_FALSE(frames_to_encode.empty());
        auto original_frame = std::move(frames_to_encode.front());
        frames_to_encode.pop_front();

        EXPECT_EQ(frame->timestamp(), original_frame->timestamp());
        EXPECT_EQ(frame->visible_rect().size(),
                  original_frame->visible_rect().size());
        if (frame->format() != original_frame->format()) {
          // The frame was converted from RGB to YUV, we can't easily compare to
          // the original frame, so we're going to compare with a new white
          // frame.
          EXPECT_EQ(frame->format(), PIXEL_FORMAT_I420);
          auto size = frame->visible_rect().size();
          auto i420_frame =
              frame_pool_.CreateFrame(PIXEL_FORMAT_I420, size, gfx::Rect(size),
                                      size, frame->timestamp());
          EXPECT_TRUE(
              frame_converter_.ConvertAndScale(*original_frame, *i420_frame)
                  .is_ok());
          original_frame = i420_frame;
        }
        EXPECT_LE(CountDifferentPixels(*frame, *original_frame),
                  original_frame->visible_rect().width());
        ++total_decoded_frames;
      });

  VideoEncoder::OutputCB encoder_output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription> desc) {
        auto buffer = DecoderBuffer::FromArray(std::move(output.data));
        buffer->set_timestamp(output.timestamp);
        buffer->set_is_key_frame(output.key_frame);
        decoder_->Decode(std::move(buffer), DecoderStatusCB());
      });

  PrepareDecoder(options.frame_size, std::move(decoder_output_cb));

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(encoder_output_cb),
                       ValidateStatusThenQuitCB());
  RunUntilQuit();

  TestRandom random_color(0x0080FF);
  for (auto frame_index = 0u; frame_index < total_frames_count; frame_index++) {
    const auto timestamp = frame_index * frame_duration;
    const bool reconfigure = (frame_index == total_frames_count / 2);

    if (reconfigure) {
      encoder_->Flush(ValidateStatusThenQuitCB());
      RunUntilQuit();

      // Ask encoder to change encoded resolution, empty output callback
      // means the encoder should keep the old one.
      options.frame_size = size2;
      encoder_->ChangeOptions(options, VideoEncoder::OutputCB(),
                              ValidateStatusThenQuitCB());
      RunUntilQuit();
    }

    auto frame = CreateFrame(options.frame_size, pixel_format_, timestamp,
                             random_color.Rand() & 0x00FFFFFF);
    frames_to_encode.push_back(frame);
    encoder_->Encode(std::move(frame), VideoEncoder::EncodeOptions(false),
                     ValidateStatusThenQuitCB());
    RunUntilQuit();
  }
  encoder_->Flush(ValidateStatusThenQuitCB());
  RunUntilQuit();
  DecodeAndWaitForStatus(DecoderBuffer::CreateEOSBuffer());
  EXPECT_EQ(total_decoded_frames, total_frames_count);
}
#endif  // ENABLE_FFMPEG_VIDEO_DECODERS

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
VideoCodecProfile ProfileIDToVideoCodecProfile(int profile) {
  switch (profile) {
    case H264SPS::kProfileIDCBaseline:
      return H264PROFILE_BASELINE;
    case H264SPS::kProfileIDCMain:
      return H264PROFILE_MAIN;
    case H264SPS::kProfileIDCHigh:
      return H264PROFILE_HIGH;
    default:
      return VIDEO_CODEC_PROFILE_UNKNOWN;
  }
}
#endif

TEST_P(H264VideoEncoderTest, AvcExtraData) {
  int outputs_count = 0;
  VideoEncoder::Options options = CreateDefaultOptions();
  options.frame_size = gfx::Size(640, 480);
  auto sec = base::Seconds(1);

  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription> desc) {
        switch (outputs_count) {
          case 0: {
            // First frame should have extra_data
            EXPECT_TRUE(desc.has_value());

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
            H264ToAnnexBBitstreamConverter converter;
            mp4::AVCDecoderConfigurationRecord avc_config;
            bool parse_ok = converter.ParseConfiguration(
                desc->data(), desc->size(), &avc_config);
            EXPECT_TRUE(parse_ok);
            EXPECT_EQ(profile_, ProfileIDToVideoCodecProfile(
                                    avc_config.profile_indication));
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
            break;
          }
          case 1:
            // Regular non-key frame shouldn't have extra_data
            EXPECT_FALSE(desc.has_value());
            break;
          case 2:
            // Forced Key frame should have extra_data
            EXPECT_TRUE(desc.has_value());
            break;
        }

        EXPECT_FALSE(output.data.empty());
        outputs_count++;
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(output_cb), ValidateStatusThenQuitCB());
  RunUntilQuit();

  auto frame1 = CreateFrame(options.frame_size, pixel_format_, 0 * sec);
  encoder_->Encode(std::move(frame1), VideoEncoder::EncodeOptions(false),
                   ValidateStatusThenQuitCB());
  RunUntilQuit();
  auto frame2 = CreateFrame(options.frame_size, pixel_format_, 1 * sec);
  encoder_->Encode(std::move(frame2), VideoEncoder::EncodeOptions(false),
                   ValidateStatusThenQuitCB());
  RunUntilQuit();
  auto frame3 = CreateFrame(options.frame_size, pixel_format_, 2 * sec);
  encoder_->Encode(std::move(frame3), VideoEncoder::EncodeOptions(true),
                   ValidateStatusThenQuitCB());
  RunUntilQuit();

  encoder_->Flush(ValidateStatusThenQuitCB());
  RunUntilQuit();
  EXPECT_EQ(outputs_count, 3);
}

TEST_P(H264VideoEncoderTest, AnnexB) {
  int outputs_count = 0;
  VideoEncoder::Options options = CreateDefaultOptions();
  options.frame_size = gfx::Size(640, 480);
  options.avc.produce_annexb = true;
  auto sec = base::Seconds(1);

  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription> desc) {
        EXPECT_FALSE(desc.has_value());
        EXPECT_FALSE(output.data.empty());

        // Check for a start code, it's either {0, 0, 1} or {0, 0, 0, 1}
        EXPECT_EQ(output.data[0], 0);
        EXPECT_EQ(output.data[1], 0);
        if (output.data[2] == 0)
          EXPECT_EQ(output.data[3], 1);
        else
          EXPECT_EQ(output.data[2], 1);
        outputs_count++;
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(output_cb), ValidateStatusThenQuitCB());
  RunUntilQuit();

  auto frame1 = CreateFrame(options.frame_size, pixel_format_, 0 * sec);
  encoder_->Encode(std::move(frame1), VideoEncoder::EncodeOptions(false),
                   ValidateStatusThenQuitCB());
  RunUntilQuit();
  auto frame2 = CreateFrame(options.frame_size, pixel_format_, 1 * sec);
  encoder_->Encode(std::move(frame2), VideoEncoder::EncodeOptions(false),
                   ValidateStatusThenQuitCB());
  RunUntilQuit();
  auto frame3 = CreateFrame(options.frame_size, pixel_format_, 2 * sec);
  encoder_->Encode(std::move(frame3), VideoEncoder::EncodeOptions(true),
                   ValidateStatusThenQuitCB());
  RunUntilQuit();

  encoder_->Flush(ValidateStatusThenQuitCB());
  RunUntilQuit();
  EXPECT_EQ(outputs_count, 3);
}

// This test is different from EncodeAndDecode:
// 1. It sets produce_annexb = false
// 2. It recreates the decoder each time there is new AVC extra data (SPS/PPS)
//    available.
TEST_P(H264VideoEncoderTest, EncodeAndDecodeWithConfig) {
  VideoEncoder::Options options = CreateDefaultOptions();
  options.frame_size = gfx::Size(320, 200);
  options.bitrate = Bitrate::ConstantBitrate(1000000u);  // 1Mbps
  options.framerate = 25;
  options.avc.produce_annexb = false;
  struct ChunkWithConfig {
    VideoEncoderOutput output;
    std::optional<VideoEncoder::CodecDescription> desc;
  };
  std::vector<scoped_refptr<VideoFrame>> frames_to_encode;
  std::vector<scoped_refptr<VideoFrame>> decoded_frames;
  std::vector<ChunkWithConfig> chunks;
  size_t total_frames_count = 30;
  auto frame_duration = base::Seconds(1.0 / options.framerate.value());

  VideoEncoder::OutputCB encoder_output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          std::optional<VideoEncoder::CodecDescription> desc) {
        chunks.push_back({std::move(output), std::move(desc)});
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(encoder_output_cb),
                       ValidateStatusThenQuitCB());
  RunUntilQuit();

  for (auto frame_index = 0u; frame_index < total_frames_count; frame_index++) {
    const auto timestamp = frame_index * frame_duration;
    const bool key_frame = (frame_index % 5) == 0;
    auto frame = CreateFrame(options.frame_size, pixel_format_, timestamp);
    frames_to_encode.push_back(frame);
    encoder_->Encode(std::move(frame), VideoEncoder::EncodeOptions(key_frame),
                     ValidateStatusThenQuitCB());
    RunUntilQuit();
  }
  encoder_->Flush(ValidateStatusThenQuitCB());
  RunUntilQuit();

  EXPECT_EQ(chunks.size(), total_frames_count);
  for (auto& chunk : chunks) {
    VideoDecoder::OutputCB decoder_output_cb =
        base::BindLambdaForTesting([&](scoped_refptr<VideoFrame> frame) {
          decoded_frames.push_back(frame);
        });

    if (chunk.desc.has_value()) {
      if (decoder_)
        DecodeAndWaitForStatus(DecoderBuffer::CreateEOSBuffer());
      PrepareDecoder(options.frame_size, std::move(decoder_output_cb),
                     chunk.desc.value());
    }
    auto& output = chunk.output;
    auto buffer = DecoderBuffer::FromArray(std::move(output.data));
    buffer->set_timestamp(output.timestamp);
    buffer->set_is_key_frame(output.key_frame);
    DecodeAndWaitForStatus(std::move(buffer));
  }
  DecodeAndWaitForStatus(DecoderBuffer::CreateEOSBuffer());
  EXPECT_EQ(decoded_frames.size(), total_frames_count);
}

std::string PrintTestParams(
    const testing::TestParamInfo<SwVideoTestParams>& info) {
  auto result =
      GetCodecName(info.param.codec) + "__" +
      GetProfileName(info.param.profile) + "__" +
      VideoPixelFormatToString(info.param.pixel_format) + "__" +
      (info.param.scalability_mode
           ? GetScalabilityModeName(info.param.scalability_mode.value())
           : "");

  // GTest doesn't like spaces, but profile names have spaces, so we need
  // to replace them with underscores.
  for (auto& c : result) {
    if (c == ' ')
      c = '_';
  }
  return result;
}

#if BUILDFLAG(ENABLE_OPENH264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
SwVideoTestParams kH264Params[] = {
    {VideoCodec::kH264, H264PROFILE_BASELINE, PIXEL_FORMAT_I420},
    {VideoCodec::kH264, H264PROFILE_BASELINE, PIXEL_FORMAT_XRGB},
    {VideoCodec::kH264, H264PROFILE_MAIN, PIXEL_FORMAT_I420},
    {VideoCodec::kH264, H264PROFILE_HIGH, PIXEL_FORMAT_I420},
};

INSTANTIATE_TEST_SUITE_P(H264Specific,
                         H264VideoEncoderTest,
                         ::testing::ValuesIn(kH264Params),
                         PrintTestParams);

INSTANTIATE_TEST_SUITE_P(H264Generic,
                         SoftwareVideoEncoderTest,
                         ::testing::ValuesIn(kH264Params),
                         PrintTestParams);

SwVideoTestParams kH264SVCParams[] = {
    {VideoCodec::kH264, H264PROFILE_BASELINE, PIXEL_FORMAT_I420, std::nullopt},
    {VideoCodec::kH264, H264PROFILE_BASELINE, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T1},
    {VideoCodec::kH264, H264PROFILE_BASELINE, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T2},
    {VideoCodec::kH264, H264PROFILE_BASELINE, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T3},
    {VideoCodec::kH264, H264PROFILE_MAIN, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T3},
    {VideoCodec::kH264, H264PROFILE_HIGH, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T3}};

INSTANTIATE_TEST_SUITE_P(H264TemporalSvc,
                         SVCVideoEncoderTest,
                         ::testing::ValuesIn(kH264SVCParams),
                         PrintTestParams);
#endif  // BUILDFLAG(ENABLE_OPENH264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)

#if BUILDFLAG(ENABLE_LIBVPX)
SwVideoTestParams kVpxParams[] = {
    {VideoCodec::kVP9, VP9PROFILE_PROFILE0, PIXEL_FORMAT_I420},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE0, PIXEL_FORMAT_NV12},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE0, PIXEL_FORMAT_XRGB},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE1, PIXEL_FORMAT_I444},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE1, PIXEL_FORMAT_NV12},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE1, PIXEL_FORMAT_XRGB},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE2, PIXEL_FORMAT_I420},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE2, PIXEL_FORMAT_NV12},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE2, PIXEL_FORMAT_XRGB},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE3, PIXEL_FORMAT_I444},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE3, PIXEL_FORMAT_NV12},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE3, PIXEL_FORMAT_XRGB},
    {VideoCodec::kVP8, VP8PROFILE_ANY, PIXEL_FORMAT_I420},
    {VideoCodec::kVP8, VP8PROFILE_ANY, PIXEL_FORMAT_XRGB}};

INSTANTIATE_TEST_SUITE_P(VpxGeneric,
                         SoftwareVideoEncoderTest,
                         ::testing::ValuesIn(kVpxParams),
                         PrintTestParams);

SwVideoTestParams kVpxSVCParams[] = {
    {VideoCodec::kVP9, VP9PROFILE_PROFILE0, PIXEL_FORMAT_I420, std::nullopt},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE0, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T1},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE0, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T2},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE0, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T3},
    {VideoCodec::kVP8, VP8PROFILE_ANY, PIXEL_FORMAT_I420, std::nullopt},
    {VideoCodec::kVP8, VP8PROFILE_ANY, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T1},
    {VideoCodec::kVP8, VP8PROFILE_ANY, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T2},
    {VideoCodec::kVP8, VP8PROFILE_ANY, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T3}};

INSTANTIATE_TEST_SUITE_P(VpxTemporalSvc,
                         SVCVideoEncoderTest,
                         ::testing::ValuesIn(kVpxSVCParams),
                         PrintTestParams);
#endif  // ENABLE_LIBVPX

#if BUILDFLAG(ENABLE_LIBAOM)
#if !BUILDFLAG(ENABLE_AV1_DECODER)
#error PrepareDecoder() requires an AV1 decoder.
#endif

SwVideoTestParams kAv1Params[] = {
    {VideoCodec::kAV1, AV1PROFILE_PROFILE_MAIN, PIXEL_FORMAT_I420},
    {VideoCodec::kAV1, AV1PROFILE_PROFILE_MAIN, PIXEL_FORMAT_NV12},
    {VideoCodec::kAV1, AV1PROFILE_PROFILE_MAIN, PIXEL_FORMAT_XRGB},
    {VideoCodec::kAV1, AV1PROFILE_PROFILE_HIGH, PIXEL_FORMAT_I444},
    {VideoCodec::kAV1, AV1PROFILE_PROFILE_HIGH, PIXEL_FORMAT_NV12},
    {VideoCodec::kAV1, AV1PROFILE_PROFILE_HIGH, PIXEL_FORMAT_XRGB}};

INSTANTIATE_TEST_SUITE_P(Av1Generic,
                         SoftwareVideoEncoderTest,
                         ::testing::ValuesIn(kAv1Params),
                         PrintTestParams);

SwVideoTestParams kAv1SVCParams[] = {
    {VideoCodec::kAV1, AV1PROFILE_PROFILE_MAIN, PIXEL_FORMAT_I420,
     std::nullopt},
    {VideoCodec::kAV1, AV1PROFILE_PROFILE_MAIN, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T1},
    {VideoCodec::kAV1, AV1PROFILE_PROFILE_MAIN, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T2},
    {VideoCodec::kAV1, AV1PROFILE_PROFILE_MAIN, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T3}};

INSTANTIATE_TEST_SUITE_P(Av1TemporalSvc,
                         SVCVideoEncoderTest,
                         ::testing::ValuesIn(kAv1SVCParams),
                         PrintTestParams);

INSTANTIATE_TEST_SUITE_P(Av1ManualSvc,
                         ManualSVCVideoEncoderTest,
                         ::testing::ValuesIn(kAv1SVCParams),
                         PrintTestParams);
#endif  // ENABLE_LIBAOM

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(H264VideoEncoderTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SVCVideoEncoderTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SoftwareVideoEncoderTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ManualSVCVideoEncoderTest);

TEST(SoftwareVideoEncoderTest, DefaultBitrate) {
  EXPECT_EQ(GetDefaultVideoEncodeBitrate({1280, 720}, 30u), 2'000'000u);
  EXPECT_EQ(GetDefaultVideoEncodeBitrate({0, 0}, 0u), 10000u);
  EXPECT_EQ(GetDefaultVideoEncodeBitrate({10000, 10000}, 10000), 1388888888u);
  EXPECT_EQ(GetDefaultVideoEncodeBitrate({1920, 1080}, 60u), 9'000'000u);
  EXPECT_EQ(GetDefaultVideoEncodeBitrate({1280, 720}, 1000u), 20'000'000u);
}

}  // namespace media
