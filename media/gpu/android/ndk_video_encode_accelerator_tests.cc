// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/ndk_video_encode_accelerator.h"

#include <algorithm>
#include <map>
#include <optional>
#include <vector>

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/test_helpers.h"
#include "media/base/test_random.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_converter.h"
#include "media/base/video_util.h"
#include "media/parsers/h264_parser.h"
#include "media/parsers/vp9_parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
#include "media/filters/ffmpeg_video_decoder.h"
#endif

#if BUILDFLAG(ENABLE_LIBVPX)
#include "media/filters/vpx_video_decoder.h"
#endif

#if BUILDFLAG(ENABLE_DAV1D_DECODER)
#include "media/filters/dav1d_video_decoder.h"
#endif

using testing::Return;

namespace media {

struct VideoParams {
  VideoCodecProfile profile;
  VideoPixelFormat pixel_format;
  bool use_gl_surface;
};

// We're putting this *after* VideoParams, so that it can be used with
// ::testing::ValuesIn without triggering -Wunguarded-availability warnings.
#pragma clang attribute push DEFAULT_REQUIRES_ANDROID_API( \
    NDK_MEDIA_CODEC_MIN_API)

class NdkVideoEncoderAcceleratorTest
    : public ::testing::TestWithParam<VideoParams>,
      public VideoEncodeAccelerator::Client {
 public:
  void SetUp() override {
    if (__builtin_available(android NDK_MEDIA_CODEC_MIN_API, *)) {
      // Negation results in compiler warning.
    } else {
      GTEST_SKIP() << "Not supported Android version";
    }

    auto args = GetParam();
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    enabled_features.push_back(kPlatformHEVCEncoderSupport);
#endif

    if (args.use_gl_surface) {
      if (__builtin_available(android 35, *)) {
        ASSERT_TRUE(gl::init::InitializeGLOneOff(gl::GpuPreference::kDefault));
      } else {
        GTEST_SKIP() << "Not supported Android version. "
                     << "Surface input needs Android 15 or newer.";
      }
      enabled_features.push_back(kEnableSurfaceInputForAndroidVEA);
    } else {
      disabled_features.push_back(kEnableSurfaceInputForAndroidVEA);
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);

    profile_ = args.profile;
    codec_ = VideoCodecProfileToVideoCodec(profile_);
    pixel_format_ = args.pixel_format;

    auto profiles = MakeNdkAccelerator()->GetSupportedProfiles();
    bool codec_supported = base::Contains(
        profiles, profile_, &VideoEncodeAccelerator::SupportedProfile::profile);

    if (!codec_supported) {
      GTEST_SKIP() << "Device doesn't have hw encoder for: "
                   << GetProfileName(profile_);
    }
  }

  void TearDown() override {
    accelerator_.reset();
    RunUntilIdle();
    auto args = GetParam();
    if (args.use_gl_surface) {
      gl::init::ShutdownGL(nullptr, false);
    }
  }

  // Implementation for VEA::Client
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override {
    output_buffer_size_ = output_buffer_size;
    input_buffer_size_ =
        VideoFrame::AllocationSize(PIXEL_FORMAT_I420, input_coded_size);
    SendNewBuffer();
    if (!OnRequireBuffer()) {
      loop_.Quit();
    }
  }

  void BitstreamBufferReady(int32_t bitstream_buffer_id,
                            const BitstreamBufferMetadata& metadata) override {
    outputs_.push_back({bitstream_buffer_id, metadata});
    SendNewBuffer();
    if (!OnBufferReady()) {
      loop_.Quit();
    }
  }

  void NotifyErrorStatus(const EncoderStatus& status) override {
    CHECK(!status.is_ok());
    error_status_ = status;
    if (!OnError()) {
      loop_.Quit();
    }
  }

  MOCK_METHOD(bool, OnRequireBuffer, ());
  MOCK_METHOD(bool, OnBufferReady, ());
  MOCK_METHOD(bool, OnError, ());

 protected:
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void SendNewBuffer() {
    auto buffer = output_pool_->MaybeAllocateBuffer(output_buffer_size_);
    if (!buffer) {
      FAIL() << "Can't allocate memory buffer";
    }
    const base::UnsafeSharedMemoryRegion& region = buffer->GetRegion();
    auto mapping = region.Map();
    std::ranges::fill(mapping.begin(), mapping.end(), 0);

    auto id = ++last_buffer_id_;
    accelerator_->UseOutputBitstreamBuffer(
        BitstreamBuffer(id, region.Duplicate(), region.GetSize()));
    id_to_buffer_[id] = std::move(buffer);
  }

  scoped_refptr<VideoFrame> CreateFrame(gfx::Size size,
                                        VideoPixelFormat format,
                                        base::TimeDelta timestamp,
                                        uint32_t color = 0) {
    auto frame =
        VideoFrame::CreateFrame(format, size, gfx::Rect(size), size, timestamp);
    if (!frame) {
      return nullptr;
    }
    frame->set_color_space(gfx::ColorSpace::CreateREC601());
    FillFourColors(*frame, color);
    return frame;
  }

  VideoEncodeAccelerator::Config GetDefaultConfig() {
    gfx::Size frame_size(640, 480);
    uint32_t framerate = 30;
    auto bitrate = Bitrate::ConstantBitrate(1000000u);
    auto config = VideoEncodeAccelerator::Config(
        pixel_format_, frame_size, profile_, bitrate, framerate,
        VideoEncodeAccelerator::Config::StorageType::kShmem,
        VideoEncodeAccelerator::Config::ContentType::kCamera);
    config.gop_length = 1000;
    config.required_encoder_type =
        VideoEncodeAccelerator::Config::EncoderType::kNoPreference;
    return config;
  }

  void Run() { loop_.Run(); }

  std::unique_ptr<NullMediaLog> NullLog() {
    return std::make_unique<NullMediaLog>();
  }

  std::unique_ptr<VideoEncodeAccelerator> MakeNdkAccelerator() {
    auto runner = task_environment_.GetMainThreadTaskRunner();
    return base::WrapUnique<VideoEncodeAccelerator>(
        new NdkVideoEncodeAccelerator(runner));
  }

  void ValidateStream(base::span<uint8_t> data) {
    EXPECT_GT(data.size(), 0u);
    switch (codec_) {
      case VideoCodec::kH264: {
        H264Parser parser;
        parser.SetStream(data.data(), data.size());

        int num_parsed_nalus = 0;
        while (true) {
          media::H264SliceHeader shdr;
          H264NALU nalu;
          H264Parser::Result res = parser.AdvanceToNextNALU(&nalu);
          if (res == H264Parser::kEOStream) {
            EXPECT_GT(num_parsed_nalus, 0);
            break;
          }
          EXPECT_EQ(res, H264Parser::kOk);
          ++num_parsed_nalus;

          int id;
          switch (nalu.nal_unit_type) {
            case H264NALU::kSPS: {
              EXPECT_EQ(parser.ParseSPS(&id), H264Parser::kOk);
              const H264SPS* sps = parser.GetSPS(id);
              VideoCodecProfile profile =
                  H264Parser::ProfileIDCToVideoCodecProfile(sps->profile_idc);
              EXPECT_EQ(profile, profile_);
              break;
            }

            case H264NALU::kPPS:
              EXPECT_EQ(parser.ParsePPS(&id), H264Parser::kOk);
              break;

            default:
              break;
          }
        }
        break;
      }
      case VideoCodec::kVP9: {
        Vp9Parser parser;
        parser.SetStream(data.data(), data.size(), nullptr);

        int num_parsed_frames = 0;
        while (true) {
          Vp9FrameHeader frame;
          gfx::Size size;
          std::unique_ptr<DecryptConfig> frame_decrypt_config;
          Vp9Parser::Result res =
              parser.ParseNextFrame(&frame, &size, &frame_decrypt_config);
          if (res == Vp9Parser::kEOStream) {
            EXPECT_GT(num_parsed_frames, 0);
            break;
          }
          EXPECT_EQ(res, Vp9Parser::kOk);
          ++num_parsed_frames;
        }
        break;
      }
      default: {
        EXPECT_TRUE(
            std::ranges::any_of(data, [](uint8_t x) { return x != 0; }));
      }
    }
  }

  VideoCodec codec_;
  VideoCodecProfile profile_;
  VideoPixelFormat pixel_format_;
  bool use_gl_surface_ = false;

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  base::RunLoop loop_;
  std::unique_ptr<VideoEncodeAccelerator> accelerator_;
  size_t output_buffer_size_ = 0;
  scoped_refptr<base::UnsafeSharedMemoryPool> output_pool_ =
      base::MakeRefCounted<base::UnsafeSharedMemoryPool>();
  std::map<int32_t, std::unique_ptr<base::UnsafeSharedMemoryPool::Handle>>
      id_to_buffer_;
  struct Output {
    int32_t id;
    BitstreamBufferMetadata md;
  };
  std::vector<Output> outputs_;
  std::optional<EncoderStatus> error_status_;
  size_t input_buffer_size_ = 0;
  int32_t last_buffer_id_ = 0;
  VideoFrameConverter frame_converter_;
  TestRandom random_color_{0};
};

class NdkVideoEncoderAcceleratorE2ETest
    : public NdkVideoEncoderAcceleratorTest {
 public:
  void TearDown() override {
    decoder_.reset();
    NdkVideoEncoderAcceleratorTest::TearDown();
  }

 protected:
  void PrepareDecoder(
      gfx::Size size,
      VideoDecoder::OutputCB output_cb,
      std::vector<uint8_t> extra_data = std::vector<uint8_t>()) {
    VideoDecoderConfig config(
        codec_, profile_, VideoDecoderConfig::AlphaMode::kIsOpaque,
        VideoColorSpace::REC601(), VideoTransformation(), size, gfx::Rect(size),
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

  std::unique_ptr<VideoDecoder> decoder_;
  base::circular_deque<scoped_refptr<VideoFrame>> frames_to_encode_;
  std::vector<uint8_t> concatenated_stream_;
  int total_decoded_frames_ = 0;
  NullMediaLog media_log_;
};

TEST_P(NdkVideoEncoderAcceleratorTest, InitializeAndDestroy) {
  auto config = GetDefaultConfig();
  accelerator_ = MakeNdkAccelerator();
  EXPECT_CALL(*this, OnRequireBuffer()).WillOnce(Return(false));

  bool result = accelerator_->Initialize(config, this, NullLog()).is_ok();
  ASSERT_TRUE(result);
  Run();
  EXPECT_GE(id_to_buffer_.size(), 1u);
  accelerator_.reset();
  EXPECT_FALSE(error_status_.has_value());
}

TEST_P(NdkVideoEncoderAcceleratorTest, HandleEncodingError) {
  auto config = GetDefaultConfig();
  accelerator_ = MakeNdkAccelerator();
  EXPECT_CALL(*this, OnRequireBuffer()).WillOnce(Return(true));
  EXPECT_CALL(*this, OnError()).WillOnce(Return(false));

  bool result = accelerator_->Initialize(config, this, NullLog()).is_ok();
  ASSERT_TRUE(result);

  auto size = config.input_visible_size;
  // A frame with unsupported pixel format works as a way to induce a error.
  auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_NV21, size, gfx::Rect(size),
                                       size, {});
  accelerator_->Encode(frame, true);

  Run();
  EXPECT_EQ(outputs_.size(), 0u);
  EXPECT_TRUE(error_status_.has_value());
}

TEST_P(NdkVideoEncoderAcceleratorTest, EncodeSeveralFrames) {
  const size_t total_frames_count = 10;
  const size_t key_frame_index = 7;
  auto config = GetDefaultConfig();
  accelerator_ = MakeNdkAccelerator();
  EXPECT_CALL(*this, OnRequireBuffer()).WillRepeatedly(Return(true));
  EXPECT_CALL(*this, OnBufferReady()).WillRepeatedly([this]() {
    if (outputs_.size() < total_frames_count)
      return true;
    return false;
  });

  bool result = accelerator_->Initialize(config, this, NullLog()).is_ok();
  ASSERT_TRUE(result);

  auto duration = base::Milliseconds(16);
  for (auto frame_index = 0u; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * duration;
    uint32_t color = random_color_.Rand() & 0x00FFFFFF;
    auto frame =
        CreateFrame(config.input_visible_size, pixel_format_, timestamp, color);
    bool key_frame = (frame_index == key_frame_index);
    accelerator_->Encode(frame, key_frame);
  }

  Run();
  EXPECT_FALSE(error_status_.has_value());
  EXPECT_GE(outputs_.size(), total_frames_count);
  // Here we'd like to test that an output with at `key_frame_index`
  // has a keyframe flag set to true, but because MediaCodec
  // is unreliable in inserting keyframes at our request we can't test
  // for it. In practice it usually works, just not always.

  std::vector<uint8_t> stream;
  for (auto& output : outputs_) {
    auto& mapping = id_to_buffer_[output.id]->GetMapping();
    EXPECT_GE(mapping.size(), output.md.payload_size_bytes);
    EXPECT_GT(output.md.payload_size_bytes, 0u);
    auto span =
        mapping.GetMemoryAsSpan<uint8_t>().first(output.md.payload_size_bytes);
    stream.insert(stream.end(), span.begin(), span.end());
  }
  ValidateStream(stream);
}

TEST_P(NdkVideoEncoderAcceleratorE2ETest, EncodeAndDecode) {
  auto config = GetDefaultConfig();
  const int total_frames_count = 10;
  accelerator_ = MakeNdkAccelerator();

  VideoDecoder::OutputCB decoder_output_cb =
      base::BindLambdaForTesting([&](scoped_refptr<VideoFrame> decoded_frame) {
        ASSERT_FALSE(frames_to_encode_.empty());
        auto original_frame = std::move(frames_to_encode_.front());
        frames_to_encode_.pop_front();

        EXPECT_EQ(decoded_frame->timestamp(), original_frame->timestamp());
        EXPECT_EQ(decoded_frame->visible_rect().size(),
                  original_frame->visible_rect().size());
        if (decoded_frame->format() == original_frame->format()) {
          EXPECT_LE(CountDifferentPixels(*decoded_frame, *original_frame, 10),
                    original_frame->visible_rect().width() * 2);
        }
        ++total_decoded_frames_;
        if (total_decoded_frames_ == total_frames_count) {
          loop_.Quit();
        }
      });

  PrepareDecoder(config.input_visible_size, std::move(decoder_output_cb));

  EXPECT_CALL(*this, OnRequireBuffer()).WillRepeatedly(Return(true));
  EXPECT_CALL(*this, OnBufferReady()).WillRepeatedly([this]() {
    auto& output = outputs_.back();
    auto& mapping = id_to_buffer_[output.id]->GetMapping();
    auto data =
        mapping.GetMemoryAsSpan<uint8_t>().first(output.md.payload_size_bytes);
    concatenated_stream_.insert(concatenated_stream_.end(), data.begin(),
                                data.end());
    auto buffer = DecoderBuffer::CopyFrom(data);
    buffer->set_timestamp(output.md.timestamp);
    buffer->set_is_key_frame(output.md.key_frame);
    decoder_->Decode(std::move(buffer), DecoderStatusCB());
    if (outputs_.size() == total_frames_count) {
      decoder_->Decode(DecoderBuffer::CreateEOSBuffer(), DecoderStatusCB());
    }
    return true;
  });

  bool result = accelerator_->Initialize(config, this, NullLog()).is_ok();
  ASSERT_TRUE(result);

  auto duration = base::Milliseconds(16);
  for (auto frame_index = 0u; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * duration;
    auto frame = CreateFrame(config.input_visible_size, pixel_format_,
                             timestamp, random_color_.Rand() & 0x00FFFFFF);
    frames_to_encode_.push_back(frame);
    accelerator_->Encode(frame, false);
  }

  Run();
  EXPECT_FALSE(error_status_.has_value());
  EXPECT_EQ(total_decoded_frames_, total_frames_count);
  if (HasFailure()) {
    std::string base64_stream = base::Base64Encode(concatenated_stream_);
    LOG(INFO) << "Concatenated stream for failed test, size: "
              << concatenated_stream_.size();
    constexpr size_t kMaxLogcatLineSize = 1024;
    for (size_t i = 0; i < base64_stream.length(); i += kMaxLogcatLineSize) {
      LOG(INFO) << base64_stream.substr(i, kMaxLogcatLineSize);
    }
  }
}

std::string PrintTestParams(const testing::TestParamInfo<VideoParams>& info) {
  auto result = GetProfileName(info.param.profile) + "__" +
                VideoPixelFormatToString(info.param.pixel_format);
  if (info.param.use_gl_surface) {
    result += "__Surface";
  }

  // GTest doesn't like spaces, but profile names have spaces, so we need
  // to replace them with underscores.
  std::replace(result.begin(), result.end(), ' ', '_');
  return result;
}

std::vector<VideoParams> GenerateSurfaceVariants(
    base::span<const VideoParams> base_params) {
  std::vector<VideoParams> all_params;
  for (const auto& base : base_params) {
    all_params.push_back({base.profile, base.pixel_format, false});
    all_params.push_back({base.profile, base.pixel_format, true});
  }
  return all_params;
}

constexpr VideoParams kBaseParams[] = {
    {VP8PROFILE_MIN, PIXEL_FORMAT_I420},
    {VP8PROFILE_MIN, PIXEL_FORMAT_NV12},
    {VP9PROFILE_PROFILE0, PIXEL_FORMAT_I420},
    {VP9PROFILE_PROFILE0, PIXEL_FORMAT_NV12},
    {AV1PROFILE_PROFILE_MAIN, PIXEL_FORMAT_I420},
    {AV1PROFILE_PROFILE_MAIN, PIXEL_FORMAT_NV12},
    {H264PROFILE_BASELINE, PIXEL_FORMAT_I420},
    {H264PROFILE_MAIN, PIXEL_FORMAT_I420},
    {H264PROFILE_HIGH, PIXEL_FORMAT_I420},
    {H264PROFILE_BASELINE, PIXEL_FORMAT_NV12},
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    {HEVCPROFILE_MAIN, PIXEL_FORMAT_I420},
    {HEVCPROFILE_MAIN, PIXEL_FORMAT_NV12},
#endif
};

INSTANTIATE_TEST_SUITE_P(
    BaseNdkEncoderTests,
    NdkVideoEncoderAcceleratorTest,
    ::testing::ValuesIn(GenerateSurfaceVariants(kBaseParams)),
    PrintTestParams);

constexpr VideoParams kE2EParams[] = {
    {H264PROFILE_BASELINE, PIXEL_FORMAT_I420},
    {H264PROFILE_BASELINE, PIXEL_FORMAT_NV12},
    {H264PROFILE_MAIN, PIXEL_FORMAT_NV12},
    {H264PROFILE_MAIN, PIXEL_FORMAT_I420},
    {VP9PROFILE_PROFILE0, PIXEL_FORMAT_I420},
    {VP9PROFILE_PROFILE0, PIXEL_FORMAT_NV12},
    {AV1PROFILE_PROFILE_MAIN, PIXEL_FORMAT_I420},
    {AV1PROFILE_PROFILE_MAIN, PIXEL_FORMAT_NV12},
};
INSTANTIATE_TEST_SUITE_P(
    E2ENdkEncoderTests,
    NdkVideoEncoderAcceleratorE2ETest,
    ::testing::ValuesIn(GenerateSurfaceVariants(kE2EParams)),
    PrintTestParams);

}  // namespace media
#pragma clang attribute pop
