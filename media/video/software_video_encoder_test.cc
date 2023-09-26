// Copyright 2021 The Chromium Authors
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
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/filters/h264_to_annex_b_bitstream_converter.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/video/h264_parser.h"
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
  absl::optional<SVCScalabilityMode> scalability_mode;
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

    if (codec_ == VideoCodec::kH264 || codec_ == VideoCodec::kVP8) {
#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
      decoder_ = std::make_unique<FFmpegVideoDecoder>(&media_log_);
#endif
    } else if (codec_ == VideoCodec::kVP9) {
#if BUILDFLAG(ENABLE_LIBVPX)
      decoder_ = std::make_unique<VpxVideoDecoder>();
#endif
    } else if (codec_ == VideoCodec::kAV1) {
#if BUILDFLAG(ENABLE_DAV1D_DECODER)
      decoder_ = std::make_unique<Dav1dVideoDecoder>(&media_log_);
#endif
    }

    EXPECT_NE(decoder_, nullptr);
    decoder_->Initialize(config, false, nullptr, DecoderStatusCB(),
                         std::move(output_cb), base::NullCallback());
    RunUntilIdle();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }
  void RunUntilQuit() { task_environment_.RunUntilQuit(); }

  scoped_refptr<VideoFrame> CreateI420Frame(gfx::Size size,
                                            uint32_t color,
                                            base::TimeDelta timestamp) {
    auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_I420, size,
                                         gfx::Rect(size), size, timestamp);
    auto y = color & 0xFF;
    auto u = (color >> 8) & 0xFF;
    auto v = (color >> 16) & 0xFF;
    libyuv::I420Rect(frame->writable_data(VideoFrame::kYPlane),
                     frame->stride(VideoFrame::kYPlane),
                     frame->writable_data(VideoFrame::kUPlane),
                     frame->stride(VideoFrame::kUPlane),
                     frame->writable_data(VideoFrame::kVPlane),
                     frame->stride(VideoFrame::kVPlane),
                     frame->visible_rect().x(),       // x
                     frame->visible_rect().y(),       // y
                     frame->visible_rect().width(),   // width
                     frame->visible_rect().height(),  // height
                     y,                               // Y color
                     u,                               // U color
                     v);                              // V color
    return frame;
  }

  scoped_refptr<VideoFrame> CreateNV12Frame(gfx::Size size,
                                            uint32_t color,
                                            base::TimeDelta timestamp) {
    auto i420_frame = CreateI420Frame(size, color, timestamp);
    auto nv12_frame = VideoFrame::CreateFrame(PIXEL_FORMAT_NV12, size,
                                              gfx::Rect(size), size, timestamp);
    auto status = ConvertAndScaleFrame(*i420_frame, *nv12_frame, resize_buff_);
    EXPECT_TRUE(status.is_ok());
    return nv12_frame;
  }

  scoped_refptr<VideoFrame> CreateRGBFrame(gfx::Size size,
                                           uint32_t color,
                                           base::TimeDelta timestamp) {
    auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_XRGB, size,
                                         gfx::Rect(size), size, timestamp);

    libyuv::ARGBRect(frame->writable_data(VideoFrame::kARGBPlane),
                     frame->stride(VideoFrame::kARGBPlane),
                     frame->visible_rect().x(),       // dst_x
                     frame->visible_rect().y(),       // dst_y
                     frame->visible_rect().width(),   // width
                     frame->visible_rect().height(),  // height
                     color);

    return frame;
  }

  scoped_refptr<VideoFrame> CreateFrame(gfx::Size size,
                                        VideoPixelFormat format,
                                        base::TimeDelta timestamp,
                                        uint32_t color = 0x964050) {
    switch (format) {
      case PIXEL_FORMAT_I420:
        return CreateI420Frame(size, color, timestamp);
      case PIXEL_FORMAT_NV12:
        return CreateNV12Frame(size, color, timestamp);
      case PIXEL_FORMAT_XRGB:
        return CreateRGBFrame(size, color, timestamp);
      default:
        EXPECT_TRUE(false) << "not supported pixel format";
        return nullptr;
    }
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
        if (profile_ == VP9PROFILE_PROFILE2) {
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
      const uint8_t* data1 = frame1.visible_data(plane);
      int stride1 = frame1.stride(plane);
      const uint8_t* data2 = frame2.visible_data(plane);
      int stride2 = frame2.stride(plane);
      size_t rows = VideoFrame::Rows(plane, format, visible_size.height());
      int row_bytes = VideoFrame::RowBytes(plane, format, visible_size.width());

      for (size_t r = 0; r < rows; ++r) {
        for (int c = 0; c < row_bytes; ++c) {
          uint8_t b1 = data1[(stride1 * r) + c];
          uint8_t b2 = data2[(stride2 * r) + c];
          uint8_t diff = std::max(b1, b2) - std::min(b1, b2);
          if (diff > tolerance)
            diff_cnt++;
        }
      }
    }
    return diff_cnt;
  }

  VideoPixelFormat GetExpectedOutputPixelFormat(VideoCodecProfile profile) {
    return profile == VP9PROFILE_PROFILE2 ? PIXEL_FORMAT_YUV420P10
                                          : PIXEL_FORMAT_I420;
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

 protected:
  VideoCodec codec_;
  VideoCodecProfile profile_;
  VideoPixelFormat pixel_format_;
  std::vector<uint8_t> resize_buff_;

  MockMediaLog media_log_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<VideoEncoder> encoder_;
  std::unique_ptr<VideoDecoder> decoder_;
};

class H264VideoEncoderTest : public SoftwareVideoEncoderTest {};
class VpxVideoEncoderTest : public SoftwareVideoEncoderTest {};
class SVCVideoEncoderTest : public SoftwareVideoEncoderTest {};

TEST_P(SoftwareVideoEncoderTest, StopCallbackWrapping) {
  VideoEncoder::Options options;
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
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  bool output_called = false;
  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput, absl::optional<VideoEncoder::CodecDescription>) {
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

TEST_P(SoftwareVideoEncoderTest, ForceAllKeyFrames) {
  int outputs_count = 0;
  int frames = 10;
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  auto frame_duration = base::Seconds(1.0 / 60);

  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          absl::optional<VideoEncoder::CodecDescription> desc) {
        EXPECT_TRUE(output.key_frame);
        outputs_count++;
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(output_cb),
                       ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();

  for (int i = 0; i < frames; i++) {
    auto timestamp = i * frame_duration;
    auto frame = CreateFrame(options.frame_size, pixel_format_, timestamp);
    encoder_->Encode(frame, VideoEncoder::EncodeOptions(true),
                     ValidatingStatusCB(/* quit_run_loop_on_call */ true));
    RunUntilQuit();
  }

  encoder_->Flush(ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();
  EXPECT_EQ(outputs_count, frames);
}

TEST_P(SoftwareVideoEncoderTest, ResizeFrames) {
  int outputs_count = 0;
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  auto sec = base::Seconds(1);

  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          absl::optional<VideoEncoder::CodecDescription> desc) {
        outputs_count++;
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(output_cb),
                       ValidatingStatusCB(/* quit_run_loop_on_call */ true));

  RunUntilQuit();
  auto frame1 = CreateFrame(gfx::Size(320, 200), pixel_format_, 0 * sec);
  auto frame2 = CreateFrame(gfx::Size(800, 600), pixel_format_, 1 * sec);
  auto frame3 = CreateFrame(gfx::Size(720, 1280), pixel_format_, 2 * sec);
  encoder_->Encode(frame1, VideoEncoder::EncodeOptions(false),
                   ValidatingStatusCB());
  encoder_->Encode(frame2, VideoEncoder::EncodeOptions(false),
                   ValidatingStatusCB());
  encoder_->Encode(frame3, VideoEncoder::EncodeOptions(false),
                   ValidatingStatusCB());

  encoder_->Flush(ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();
  EXPECT_EQ(outputs_count, 3);
}

TEST_P(SoftwareVideoEncoderTest, OutputCountEqualsFrameCount) {
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
          absl::optional<VideoEncoder::CodecDescription> desc) {
        EXPECT_NE(output.data, nullptr);
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
    auto frame =
        CreateFrame(options.frame_size, pixel_format_, timestamp, color);
    color = (color << 1) + frame_index;
    encoder_->Encode(frame, VideoEncoder::EncodeOptions(false),
                     ValidatingStatusCB());
  }

  encoder_->Flush(ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();
  EXPECT_EQ(outputs_count, total_frames_count);
}

TEST_P(SoftwareVideoEncoderTest, PerFrameQpEncoding) {
  VideoEncoder::Options options;
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
          absl::optional<VideoEncoder::CodecDescription> desc) {
        EXPECT_NE(output.data, nullptr);
        outputs_count++;
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(output_cb),
                       ValidatingStatusCB(/* quit_run_loop_on_call */ true));

  RunUntilQuit();
  uint32_t color = 0x964050;
  int qp = qp_range.first;
  for (int frame_index = 0; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * frame_duration;
    auto frame =
        CreateFrame(options.frame_size, pixel_format_, timestamp, color);
    color = (color << 1) + frame_index;
    VideoEncoder::EncodeOptions encode_options(false);
    encode_options.quantizer = qp;
    qp++;
    encoder_->Encode(frame, encode_options, ValidatingStatusCB());
  }

  encoder_->Flush(ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();
  EXPECT_EQ(outputs_count, total_frames_count);
}

#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
TEST_P(SoftwareVideoEncoderTest, EncodeAndDecode) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(320, 200);
  options.bitrate = Bitrate::ConstantBitrate(1000000u);  // 1Mbps
  options.framerate = 25;
  if (codec_ == VideoCodec::kH264)
    options.avc.produce_annexb = true;
  options.keyframe_interval = options.framerate.value() * 3;  // every 3s
  std::vector<scoped_refptr<VideoFrame>> frames_to_encode;
  std::vector<scoped_refptr<VideoFrame>> decoded_frames;
  int total_frames_count =
      options.framerate.value() * 10;  // total duration 10s

  auto frame_duration = base::Seconds(1.0 / options.framerate.value());

  VideoEncoder::OutputCB encoder_output_cb = base::BindLambdaForTesting(
      [&, this](VideoEncoderOutput output,
                absl::optional<VideoEncoder::CodecDescription> desc) {
        auto buffer =
            DecoderBuffer::FromArray(std::move(output.data), output.size);
        buffer->set_timestamp(output.timestamp);
        buffer->set_is_key_frame(output.key_frame);
        decoder_->Decode(std::move(buffer), DecoderStatusCB());
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
    auto frame =
        CreateFrame(options.frame_size, pixel_format_, timestamp, color);
    frames_to_encode.push_back(frame);
    color = (color << 1) + frame_index;
    encoder_->Encode(frame, VideoEncoder::EncodeOptions(false),
                     ValidatingStatusCB(/* quit_run_loop_on_call */ true));
    RunUntilQuit();
  }

  encoder_->Flush(ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();
  DecodeAndWaitForStatus(DecoderBuffer::CreateEOSBuffer());
  EXPECT_EQ(decoded_frames.size(), frames_to_encode.size());
  for (auto i = 0u; i < decoded_frames.size(); i++) {
    auto original_frame = frames_to_encode[i];
    auto decoded_frame = decoded_frames[i];
    EXPECT_EQ(decoded_frame->timestamp(), original_frame->timestamp());
    EXPECT_EQ(decoded_frame->visible_rect().size(),
              original_frame->visible_rect().size());
    EXPECT_EQ(decoded_frame->format(), GetExpectedOutputPixelFormat(profile_));
    if (decoded_frame->format() == original_frame->format()) {
      EXPECT_LE(CountDifferentPixels(*decoded_frame, *original_frame),
                original_frame->visible_rect().width());
    }
  }
}

TEST_P(SVCVideoEncoderTest, EncodeClipTemporalSvc) {
  VideoEncoder::Options options;
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
          absl::optional<VideoEncoder::CodecDescription> desc) {
        chunks.push_back(std::move(output));
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(encoder_output_cb),
                       ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();

  uint32_t color = 0x964050;
  for (auto frame_index = 0u; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * frame_duration;
    auto frame =
        CreateFrame(options.frame_size, pixel_format_, timestamp, color);
    color = (color << 1) + frame_index;
    frames_to_encode.push_back(frame);
    encoder_->Encode(frame, VideoEncoder::EncodeOptions(false),
                     ValidatingStatusCB(/* quit_run_loop_on_call */ true));
    RunUntilQuit();
  }

  encoder_->Flush(ValidatingStatusCB(/* quit_run_loop_on_call */ true));
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
        NOTREACHED() << "Unsupported SVC: "
                     << GetScalabilityModeName(
                            options.scalability_mode.value());
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
      if (chunk.temporal_id <= max_layer && chunk.data) {
        auto buffer = DecoderBuffer::CopyFrom(chunk.data.get(), chunk.size);
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
  VideoEncoder::Options options;
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
          absl::optional<VideoEncoder::CodecDescription> desc) {
        chunks.push_back(std::move(output));
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(encoder_output_cb),
                       ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();

  uint32_t color = 0x964050;
  for (auto frame_index = 0u; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * frame_duration;

    const bool reconfigure = (frame_index == total_frames_count / 2);
    if (reconfigure) {
      encoder_->Flush(ValidatingStatusCB(/* quit_run_loop_on_call */ true));
      RunUntilQuit();

      // Ask encoder to change SVC mode, empty output callback
      // means the encoder should keep the old one.
      options.scalability_mode = SVCScalabilityMode::kL1T1;
      encoder_->ChangeOptions(
          options, VideoEncoder::OutputCB(),
          ValidatingStatusCB(/* quit_run_loop_on_call */ true));
      RunUntilQuit();
    }

    auto frame =
        CreateFrame(options.frame_size, pixel_format_, timestamp, color);
    color = (color << 1) + frame_index;
    frames_to_encode.push_back(frame);
    encoder_->Encode(frame, VideoEncoder::EncodeOptions(false),
                     ValidatingStatusCB(/* quit_run_loop_on_call */ true));
    RunUntilQuit();
  }

  encoder_->Flush(ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();
  EXPECT_EQ(chunks.size(), total_frames_count);
}

TEST_P(VpxVideoEncoderTest, ReconfigureWithResize) {
  int outputs_count = 0;
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(1024, 1024);

  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          absl::optional<VideoEncoder::CodecDescription> desc) {
        outputs_count++;
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(output_cb), ValidatingStatusCB());

  auto frame0 = CreateFrame(options.frame_size, pixel_format_, {});
  encoder_->Encode(frame0, VideoEncoder::EncodeOptions(false),
                   ValidatingStatusCB());

  options.frame_size = gfx::Size(1000, 608);
  encoder_->ChangeOptions(options, VideoEncoder::OutputCB(),
                          ValidatingStatusCB());

  auto frame1 = CreateFrame(options.frame_size, pixel_format_, {});
  encoder_->Encode(frame1, VideoEncoder::EncodeOptions(false),
                   ValidatingStatusCB());

  options.frame_size = gfx::Size(16, 720);
  encoder_->ChangeOptions(options, VideoEncoder::OutputCB(),
                          ValidatingStatusCB());

  auto frame2 = CreateFrame(options.frame_size, pixel_format_, {});
  encoder_->Encode(frame2, VideoEncoder::EncodeOptions(false),
                   ValidatingStatusCB(/* quit_run_loop_on_call */ true));

  RunUntilQuit();
  EXPECT_EQ(outputs_count, 3);
}

TEST_P(H264VideoEncoderTest, ReconfigureWithResize) {
  VideoEncoder::Options options;
  gfx::Size size1(320, 200), size2(400, 240);
  options.frame_size = size1;
  options.bitrate = Bitrate::ConstantBitrate(1000000u);  // 1Mbps
  options.framerate = 25;
  if (codec_ == VideoCodec::kH264)
    options.avc.produce_annexb = true;
  struct ChunkWithConfig {
    VideoEncoderOutput output;
    gfx::Size size;
  };
  std::vector<scoped_refptr<VideoFrame>> frames_to_encode;
  std::vector<scoped_refptr<VideoFrame>> decoded_frames;
  std::vector<ChunkWithConfig> chunks;
  size_t total_frames_count = 8;
  auto frame_duration = base::Seconds(1.0 / options.framerate.value());

  VideoEncoder::OutputCB encoder_output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          absl::optional<VideoEncoder::CodecDescription> desc) {
        chunks.push_back({std::move(output), options.frame_size});
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(encoder_output_cb),
                       ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();

  uint32_t color = 0x0080FF;
  for (auto frame_index = 0u; frame_index < total_frames_count; frame_index++) {
    const auto timestamp = frame_index * frame_duration;
    const bool reconfigure = (frame_index == total_frames_count / 2);

    if (reconfigure) {
      encoder_->Flush(ValidatingStatusCB(/* quit_run_loop_on_call */ true));
      RunUntilQuit();

      // Ask encoder to change encoded resolution, empty output callback
      // means the encoder should keep the old one.
      options.frame_size = size2;
      encoder_->ChangeOptions(
          options, VideoEncoder::OutputCB(),
          ValidatingStatusCB(/* quit_run_loop_on_call */ true));
      RunUntilQuit();
    }

    auto frame =
        CreateFrame(options.frame_size, pixel_format_, timestamp, color);
    frames_to_encode.push_back(frame);
    encoder_->Encode(frame, VideoEncoder::EncodeOptions(false),
                     ValidatingStatusCB(/* quit_run_loop_on_call */ true));
    RunUntilQuit();
  }
  encoder_->Flush(ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();

  EXPECT_EQ(chunks.size(), total_frames_count);
  gfx::Size current_size;
  for (auto& chunk : chunks) {
    VideoDecoder::OutputCB decoder_output_cb =
        base::BindLambdaForTesting([&](scoped_refptr<VideoFrame> frame) {
          decoded_frames.push_back(frame);
        });

    if (chunk.size != current_size) {
      if (decoder_)
        DecodeAndWaitForStatus(DecoderBuffer::CreateEOSBuffer());
      PrepareDecoder(chunk.size, std::move(decoder_output_cb));
      current_size = chunk.size;
    }
    auto& output = chunk.output;
    auto buffer = DecoderBuffer::FromArray(std::move(output.data), output.size);
    buffer->set_timestamp(output.timestamp);
    buffer->set_is_key_frame(output.key_frame);
    DecodeAndWaitForStatus(std::move(buffer));
  }
  DecodeAndWaitForStatus(DecoderBuffer::CreateEOSBuffer());

  EXPECT_EQ(decoded_frames.size(), frames_to_encode.size());
  std::vector<uint8_t> conversion_buffer;
  for (auto i = 0u; i < decoded_frames.size(); i++) {
    auto original_frame = frames_to_encode[i];
    auto decoded_frame = decoded_frames[i];
    EXPECT_EQ(decoded_frame->timestamp(), original_frame->timestamp());
    EXPECT_EQ(decoded_frame->visible_rect().size(),
              original_frame->visible_rect().size());
    if (decoded_frame->format() != original_frame->format()) {
      // The frame was converted from RGB to YUV, we can't easily compare to
      // the original frame, so we're going to compare with a new white frame.
      EXPECT_EQ(decoded_frame->format(), PIXEL_FORMAT_I420);
      auto size = decoded_frame->visible_rect().size();
      auto i420_frame =
          VideoFrame::CreateFrame(PIXEL_FORMAT_I420, size, gfx::Rect(size),
                                  size, decoded_frame->timestamp());
      EXPECT_TRUE(
          ConvertAndScaleFrame(*original_frame, *i420_frame, conversion_buffer)
              .is_ok());
      original_frame = i420_frame;
    }
    EXPECT_LE(CountDifferentPixels(*decoded_frame, *original_frame),
              original_frame->visible_rect().width());
  }
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
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  auto sec = base::Seconds(1);

  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          absl::optional<VideoEncoder::CodecDescription> desc) {
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

        EXPECT_NE(output.data, nullptr);
        outputs_count++;
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(output_cb),
                       ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();

  auto frame1 = CreateFrame(options.frame_size, pixel_format_, 0 * sec);
  auto frame2 = CreateFrame(options.frame_size, pixel_format_, 1 * sec);
  auto frame3 = CreateFrame(options.frame_size, pixel_format_, 2 * sec);
  encoder_->Encode(frame1, VideoEncoder::EncodeOptions(false),
                   ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();
  encoder_->Encode(frame2, VideoEncoder::EncodeOptions(false),
                   ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();
  encoder_->Encode(frame3, VideoEncoder::EncodeOptions(true),
                   ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();

  encoder_->Flush(ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();
  EXPECT_EQ(outputs_count, 3);
}

TEST_P(H264VideoEncoderTest, AnnexB) {
  int outputs_count = 0;
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(640, 480);
  options.avc.produce_annexb = true;
  auto sec = base::Seconds(1);

  VideoEncoder::OutputCB output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          absl::optional<VideoEncoder::CodecDescription> desc) {
        EXPECT_FALSE(desc.has_value());
        EXPECT_NE(output.data, nullptr);

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
                       std::move(output_cb),
                       ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();

  auto frame1 = CreateFrame(options.frame_size, pixel_format_, 0 * sec);
  auto frame2 = CreateFrame(options.frame_size, pixel_format_, 1 * sec);
  auto frame3 = CreateFrame(options.frame_size, pixel_format_, 2 * sec);
  encoder_->Encode(frame1, VideoEncoder::EncodeOptions(false),
                   ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();
  encoder_->Encode(frame2, VideoEncoder::EncodeOptions(false),
                   ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();
  encoder_->Encode(frame3, VideoEncoder::EncodeOptions(true),
                   ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();

  encoder_->Flush(ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();
  EXPECT_EQ(outputs_count, 3);
}

// This test is different from EncodeAndDecode:
// 1. It sets produce_annexb = false
// 2. It recreates the decoder each time there is new AVC extra data (SPS/PPS)
//    available.
TEST_P(H264VideoEncoderTest, EncodeAndDecodeWithConfig) {
  VideoEncoder::Options options;
  options.frame_size = gfx::Size(320, 200);
  options.bitrate = Bitrate::ConstantBitrate(1000000u);  // 1Mbps
  options.framerate = 25;
  options.avc.produce_annexb = false;
  struct ChunkWithConfig {
    VideoEncoderOutput output;
    absl::optional<VideoEncoder::CodecDescription> desc;
  };
  std::vector<scoped_refptr<VideoFrame>> frames_to_encode;
  std::vector<scoped_refptr<VideoFrame>> decoded_frames;
  std::vector<ChunkWithConfig> chunks;
  size_t total_frames_count = 30;
  auto frame_duration = base::Seconds(1.0 / options.framerate.value());

  VideoEncoder::OutputCB encoder_output_cb = base::BindLambdaForTesting(
      [&](VideoEncoderOutput output,
          absl::optional<VideoEncoder::CodecDescription> desc) {
        chunks.push_back({std::move(output), std::move(desc)});
      });

  encoder_->Initialize(profile_, options, /*info_cb=*/base::DoNothing(),
                       std::move(encoder_output_cb),
                       ValidatingStatusCB(/* quit_run_loop_on_call */ true));
  RunUntilQuit();

  uint32_t color = 0x964050;
  for (auto frame_index = 0u; frame_index < total_frames_count; frame_index++) {
    const auto timestamp = frame_index * frame_duration;
    const bool key_frame = (frame_index % 5) == 0;
    auto frame =
        CreateFrame(options.frame_size, pixel_format_, timestamp, color);
    frames_to_encode.push_back(frame);
    encoder_->Encode(frame, VideoEncoder::EncodeOptions(key_frame),
                     ValidatingStatusCB(/* quit_run_loop_on_call */ true));
    RunUntilQuit();
  }
  encoder_->Flush(ValidatingStatusCB(/* quit_run_loop_on_call */ true));
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
    auto buffer = DecoderBuffer::FromArray(std::move(output.data), output.size);
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

#if BUILDFLAG(ENABLE_OPENH264)
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
    {VideoCodec::kH264, H264PROFILE_BASELINE, PIXEL_FORMAT_I420, absl::nullopt},
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
#endif  // ENABLE_OPENH264

#if BUILDFLAG(ENABLE_LIBVPX)
SwVideoTestParams kVpxParams[] = {
    {VideoCodec::kVP9, VP9PROFILE_PROFILE0, PIXEL_FORMAT_I420},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE0, PIXEL_FORMAT_NV12},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE0, PIXEL_FORMAT_XRGB},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE2, PIXEL_FORMAT_I420},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE2, PIXEL_FORMAT_NV12},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE2, PIXEL_FORMAT_XRGB},
    {VideoCodec::kVP8, VP8PROFILE_ANY, PIXEL_FORMAT_I420},
    {VideoCodec::kVP8, VP8PROFILE_ANY, PIXEL_FORMAT_XRGB}};

INSTANTIATE_TEST_SUITE_P(VpxSpecific,
                         VpxVideoEncoderTest,
                         ::testing::ValuesIn(kVpxParams),
                         PrintTestParams);

INSTANTIATE_TEST_SUITE_P(VpxGeneric,
                         SoftwareVideoEncoderTest,
                         ::testing::ValuesIn(kVpxParams),
                         PrintTestParams);

SwVideoTestParams kVpxSVCParams[] = {
    {VideoCodec::kVP9, VP9PROFILE_PROFILE0, PIXEL_FORMAT_I420, absl::nullopt},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE0, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T1},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE0, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T2},
    {VideoCodec::kVP9, VP9PROFILE_PROFILE0, PIXEL_FORMAT_I420,
     SVCScalabilityMode::kL1T3},
    {VideoCodec::kVP8, VP8PROFILE_ANY, PIXEL_FORMAT_I420, absl::nullopt},
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
    {VideoCodec::kAV1, AV1PROFILE_PROFILE_MAIN, PIXEL_FORMAT_XRGB}};

INSTANTIATE_TEST_SUITE_P(Av1Generic,
                         SoftwareVideoEncoderTest,
                         ::testing::ValuesIn(kAv1Params),
                         PrintTestParams);

SwVideoTestParams kAv1SVCParams[] = {
    {VideoCodec::kAV1, AV1PROFILE_PROFILE_MAIN, PIXEL_FORMAT_I420,
     absl::nullopt},
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
#endif  // ENABLE_LIBAOM

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(H264VideoEncoderTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(VpxVideoEncoderTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SVCVideoEncoderTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SoftwareVideoEncoderTest);

TEST(SoftwareVideoEncoderTest, DefaultBitrate) {
  EXPECT_EQ(GetDefaultVideoEncodeBitrate({1280, 720}, 30u), 2'000'000u);
  EXPECT_EQ(GetDefaultVideoEncodeBitrate({0, 0}, 0u), 10000u);
  EXPECT_EQ(GetDefaultVideoEncodeBitrate({10000, 10000}, 10000), 1388888888u);
  EXPECT_EQ(GetDefaultVideoEncodeBitrate({1920, 1080}, 60u), 9'000'000u);
  EXPECT_EQ(GetDefaultVideoEncodeBitrate({1280, 720}, 1000u), 20'000'000u);
}

}  // namespace media
