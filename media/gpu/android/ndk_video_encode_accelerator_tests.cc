// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/ndk_video_encode_accelerator.h"

#include <map>
#include <optional>
#include <vector>

#include "base/android/build_info.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/test_helpers.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_converter.h"
#include "media/base/video_util.h"
#include "media/parsers/h264_parser.h"
#include "media/video/fake_gpu_memory_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"

#pragma clang attribute push DEFAULT_REQUIRES_ANDROID_API( \
    NDK_MEDIA_CODEC_MIN_API)
using testing::Return;

namespace media {

struct VideoParams {
  VideoCodecProfile profile;
  VideoPixelFormat pixel_format;
};

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

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    feature_list_.InitAndEnableFeature(kPlatformHEVCEncoderSupport);
#endif

    auto args = GetParam();
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

  void TearDown() override {}

  // Implementation for VEA::Client
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override {
    output_buffer_size_ = output_buffer_size;
    input_buffer_size_ =
        VideoFrame::AllocationSize(PIXEL_FORMAT_I420, input_coded_size);
    SendNewBuffer();
    if (!OnRequireBuffer())
      loop_.Quit();
  }

  void BitstreamBufferReady(int32_t bitstream_buffer_id,
                            const BitstreamBufferMetadata& metadata) override {
    outputs_.push_back({bitstream_buffer_id, metadata});
    SendNewBuffer();
    if (!OnBufferReady())
      loop_.Quit();
  }

  void NotifyErrorStatus(const EncoderStatus& status) override {
    CHECK(!status.is_ok());
    error_status_ = status;
    if (!OnError())
      loop_.Quit();
  }

  MOCK_METHOD(bool, OnRequireBuffer, ());
  MOCK_METHOD(bool, OnBufferReady, ());
  MOCK_METHOD(bool, OnError, ());

 protected:
  void SendNewBuffer() {
    auto buffer = output_pool_->MaybeAllocateBuffer(output_buffer_size_);
    if (!buffer) {
      FAIL() << "Can't allocate memory buffer";
    }

    const base::UnsafeSharedMemoryRegion& region = buffer->GetRegion();
    auto mapping = region.Map();
    memset(mapping.memory(), 0, mapping.size());

    auto id = ++last_buffer_id_;
    accelerator_->UseOutputBitstreamBuffer(
        BitstreamBuffer(id, region.Duplicate(), region.GetSize()));
    id_to_buffer_[id] = std::move(buffer);
  }

  scoped_refptr<VideoFrame> CreateI420Frame(gfx::Size size,
                                            uint32_t color,
                                            base::TimeDelta timestamp) {
    auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_I420, size,
                                         gfx::Rect(size), size, timestamp);
    auto y = color & 0xFF;
    auto u = (color >> 8) & 0xFF;
    auto v = (color >> 16) & 0xFF;
    libyuv::I420Rect(frame->writable_data(VideoFrame::Plane::kY),
                     frame->stride(VideoFrame::Plane::kY),
                     frame->writable_data(VideoFrame::Plane::kU),
                     frame->stride(VideoFrame::Plane::kU),
                     frame->writable_data(VideoFrame::Plane::kV),
                     frame->stride(VideoFrame::Plane::kV),
                     0,                               // left
                     0,                               // top
                     frame->visible_rect().width(),   // right
                     frame->visible_rect().height(),  // bottom
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
    auto status = frame_converter_.ConvertAndScale(*i420_frame, *nv12_frame);
    EXPECT_TRUE(status.is_ok());
    return nv12_frame;
  }

  scoped_refptr<VideoFrame> CreateRGBFrame(gfx::Size size,
                                           uint32_t color,
                                           base::TimeDelta timestamp) {
    auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_XRGB, size,
                                         gfx::Rect(size), size, timestamp);

    libyuv::ARGBRect(frame->writable_data(VideoFrame::Plane::kARGB),
                     frame->stride(VideoFrame::Plane::kARGB),
                     0,                               // left
                     0,                               // top
                     frame->visible_rect().width(),   // right
                     frame->visible_rect().height(),  // bottom
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
      default: {
        EXPECT_TRUE(
            base::ranges::any_of(data, [](uint8_t x) { return x != 0; }));
      }
    }
  }

  VideoCodec codec_;
  VideoCodecProfile profile_;
  VideoPixelFormat pixel_format_;

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
};

TEST_P(NdkVideoEncoderAcceleratorTest, InitializeAndDestroy) {
  auto config = GetDefaultConfig();
  accelerator_ = MakeNdkAccelerator();
  EXPECT_CALL(*this, OnRequireBuffer()).WillOnce(Return(false));

  bool result = accelerator_->Initialize(config, this, NullLog());
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

  bool result = accelerator_->Initialize(config, this, NullLog());
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

  bool result = accelerator_->Initialize(config, this, NullLog());
  ASSERT_TRUE(result);

  uint32_t color = 0x964050;
  auto duration = base::Milliseconds(16);
  for (auto frame_index = 0u; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * duration;
    auto frame =
        CreateFrame(config.input_visible_size, pixel_format_, timestamp, color);
    color = (color << 1) + frame_index;
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

  for (auto& output : outputs_) {
    auto& mapping = id_to_buffer_[output.id]->GetMapping();
    EXPECT_GE(mapping.size(), output.md.payload_size_bytes);
    EXPECT_GT(output.md.payload_size_bytes, 0u);
    auto span = mapping.GetMemoryAsSpan<uint8_t>();
    ValidateStream(span);
  }
}

std::string PrintTestParams(const testing::TestParamInfo<VideoParams>& info) {
  auto result = GetProfileName(info.param.profile) + "__" +
                VideoPixelFormatToString(info.param.pixel_format);

  // GTest doesn't like spaces, but profile names have spaces, so we need
  // to replace them with underscores.
  std::replace(result.begin(), result.end(), ' ', '_');
  return result;
}

VideoParams kParams[] = {
    {VP8PROFILE_MIN, PIXEL_FORMAT_I420},
    {VP8PROFILE_MIN, PIXEL_FORMAT_NV12},
    {H264PROFILE_BASELINE, PIXEL_FORMAT_I420},
    {H264PROFILE_MAIN, PIXEL_FORMAT_I420},
    {H264PROFILE_HIGH, PIXEL_FORMAT_I420},
    {H264PROFILE_BASELINE, PIXEL_FORMAT_NV12},
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    {HEVCPROFILE_MAIN, PIXEL_FORMAT_I420},
    {HEVCPROFILE_MAIN, PIXEL_FORMAT_NV12},
#endif
};

INSTANTIATE_TEST_SUITE_P(AllNdkEncoderTests,
                         NdkVideoEncoderAcceleratorTest,
                         ::testing::ValuesIn(kParams),
                         PrintTestParams);

}  // namespace media
#pragma clang attribute pop
