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
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/ahardwarebuffer_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
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
#include "media/gpu/command_buffer_helper.h"
#include "media/parsers/h264_parser.h"
#include "media/parsers/vp9_parser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "ui/gl/gl_context.h"
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

class MockCommandBufferHelper : public CommandBufferHelper {
 public:
  explicit MockCommandBufferHelper(
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : CommandBufferHelper(std::move(task_runner)) {}

  MOCK_METHOD(void,
              WaitForSyncToken,
              (gpu::SyncToken sync_token, base::OnceClosure done_cb),
              (override));
  MOCK_METHOD(gpu::SharedImageManager*, GetSharedImageManager, (), (override));

 protected:
  ~MockCommandBufferHelper() override = default;
};

struct VideoParams {
  VideoCodecProfile profile;
  VideoPixelFormat pixel_format;
  bool use_gl_surface = false;
  bool use_shared_image = false;
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
        SetupSharedImages();
      } else {
        GTEST_SKIP() << "Not supported Android version. "
                     << "Surface input needs Android 15 or newer.";
      }
      enabled_features.push_back(kSurfaceInputForAndroidVEA);
    } else {
      disabled_features.push_back(kSurfaceInputForAndroidVEA);
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
    si_refs.clear();
    if (args.use_gl_surface) {
      if (context_state_) {
        context_state_->MakeCurrent(gl_surface_.get(), true);
        context_state_.reset();
        gl_context_.reset();
        gl_surface_.reset();
      }
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
      loop_->Quit();
    }
  }

  void BitstreamBufferReady(int32_t bitstream_buffer_id,
                            const BitstreamBufferMetadata& metadata) override {
    outputs_.push_back({bitstream_buffer_id, metadata});
    SendNewBuffer();
    if (!OnBufferReady()) {
      loop_->Quit();
    }
  }

  void NotifyErrorStatus(const EncoderStatus& status) override {
    CHECK(!status.is_ok());
    error_status_ = status;
    if (!OnError()) {
      loop_->Quit();
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

    CHECK(frame);
    frame->set_color_space(gfx::ColorSpace::CreateREC601());
    FillFourColors(*frame, color);
    return frame;
  }

  scoped_refptr<VideoFrame> WrapInSharedImageFrame(
      scoped_refptr<VideoFrame> software_frame) {
    CHECK_EQ(software_frame->format(), PIXEL_FORMAT_XBGR);
    gfx::Size size = software_frame->visible_rect().size();
    auto mailbox = gpu::Mailbox::Generate();
    auto color_space = gfx::ColorSpace::CreateSRGB();
    GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
    SkAlphaType alpha_type = kPremul_SkAlphaType;
    auto viz_format = viz::SinglePlaneFormat::kRGBA_8888;
    auto sync_token = gpu::SyncToken();
    gpu::SharedImageUsageSet usage = gpu::SHARED_IMAGE_USAGE_GLES2_READ |
                                     gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;

    // Create a SharedImage-backed frame from the software frame's data.
    // AHardwareBufferImageBackingFactory expects tightly packed data. Create a
    // buffer and copy the frame data into it.
    const size_t row_bytes = size.width() * 4;
    const size_t pixel_data_size = row_bytes * size.height();
    std::vector<uint8_t> pixel_data(pixel_data_size);
    libyuv::CopyPlane(software_frame->data(VideoFrame::Plane::kARGB),
                      software_frame->stride(VideoFrame::Plane::kARGB),
                      pixel_data.data(), row_bytes, row_bytes, size.height());

    auto backing = backing_factory_->CreateSharedImage(
        mailbox, viz_format, size, color_space, surface_origin, alpha_type,
        usage, "TestLabel", /*is_thread_safe=*/false, pixel_data);
    CHECK(backing);

    auto factory_ref = shared_image_manager_.Register(std::move(backing),
                                                      &memory_type_tracker_);
    CHECK(factory_ref);
    si_refs.push_back(std::move(factory_ref));

    gpu::SharedImageInfo si_info(viz_format, size, color_space, surface_origin,
                                 alpha_type, usage, "TestLabel");

    auto test_ssi = base::MakeRefCounted<gpu::TestSharedImageInterface>();
    auto client_shared_image = base::MakeRefCounted<gpu::ClientSharedImage>(
        mailbox, si_info, sync_token,
        base::MakeRefCounted<gpu::SharedImageInterfaceHolder>(test_ssi.get()),
        gfx::EMPTY_BUFFER);

    return VideoFrame::WrapSharedImage(
        PIXEL_FORMAT_XBGR, client_shared_image, sync_token, base::DoNothing(),
        size, gfx::Rect(size), size, software_frame->timestamp());
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

  void Run() {
    loop_->Run();
    loop_ = std::make_unique<base::RunLoop>();
  }

  std::unique_ptr<NullMediaLog> NullLog() {
    return std::make_unique<NullMediaLog>();
  }

  std::unique_ptr<VideoEncodeAccelerator> MakeNdkAccelerator() {
    auto runner = task_environment_.GetMainThreadTaskRunner();
    return base::WrapUnique<VideoEncodeAccelerator>(
        new NdkVideoEncodeAccelerator(runner));
  }

  void SetCommandBufferHelper() {
    if (GetParam().use_shared_image) {
      accelerator_->SetCommandBufferHelperCB(
          base::BindLambdaForTesting(
              [&, this]() { return command_buffer_helper_; }),
          fake_gpu_runner_);
    }
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

  void SetupSharedImages() {
    gpu::GpuPreferences gpu_preferences;
    gpu::GpuDriverBugWorkarounds gpu_workarounds;
    gpu::GpuFeatureInfo gpu_feature_info;
    gl_surface_ = gl::init::CreateOffscreenGLSurface(
        gl::GLSurfaceEGL::GetGLDisplayEGL(), gfx::Size());
    ASSERT_TRUE(gl_surface_);
    gl_context_ = gl::init::CreateGLContext(nullptr, gl_surface_.get(),
                                            gl::GLContextAttribs());
    ASSERT_TRUE(gl_context_);
    ASSERT_TRUE(gl_context_->MakeCurrent(gl_surface_.get()));

    context_state_ = base::MakeRefCounted<gpu::SharedContextState>(
        base::MakeRefCounted<gl::GLShareGroup>(), gl_surface_, gl_context_,
        /*use_virtualized_gl_contexts=*/false, base::DoNothing(),
        gpu::GrContextType::kGL);
    ASSERT_TRUE(context_state_->InitializeGL(
        gpu_preferences, base::MakeRefCounted<gpu::gles2::FeatureInfo>(
                             gpu_workarounds, gpu_feature_info)));

    backing_factory_ =
        std::make_unique<gpu::AHardwareBufferImageBackingFactory>(
            context_state_->feature_info(), gpu_preferences,
            context_state_->vk_context_provider());

    auto runner = task_environment_.GetMainThreadTaskRunner();
    auto command_buffer_helper =
        base::MakeRefCounted<MockCommandBufferHelper>(fake_gpu_runner_);
    ON_CALL(*command_buffer_helper, WaitForSyncToken)
        .WillByDefault([runner](gpu::SyncToken, base::OnceClosure done_cb) {
          runner->PostTask(FROM_HERE, std::move(done_cb));
        });
    ON_CALL(*command_buffer_helper, GetSharedImageManager())
        .WillByDefault(Return(&shared_image_manager_));
    command_buffer_helper_ = command_buffer_helper;
  }

  VideoCodec codec_;
  VideoCodecProfile profile_;
  VideoPixelFormat pixel_format_;
  bool use_gl_surface_ = false;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS};
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<base::RunLoop> loop_ = std::make_unique<base::RunLoop>();
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
  TestRandom random_color_{0};

  scoped_refptr<gl::GLSurface> gl_surface_;
  scoped_refptr<gl::GLContext> gl_context_;
  scoped_refptr<gpu::SharedContextState> context_state_;
  gpu::SharedImageManager shared_image_manager_{/*thread_safe=*/false};
  std::unique_ptr<gpu::SharedImageBackingFactory> backing_factory_;
  scoped_refptr<gpu::MemoryTracker> memory_tracker_ =
      base::MakeRefCounted<gpu::MemoryTracker>();
  gpu::MemoryTypeTracker memory_type_tracker_{memory_tracker_.get()};
  std::vector<std::unique_ptr<gpu::SharedImageRepresentationFactoryRef>>
      si_refs;
  scoped_refptr<CommandBufferHelper> command_buffer_helper_;
  scoped_refptr<base::SingleThreadTaskRunner> fake_gpu_runner_ =
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
};

class NdkVideoEncoderAcceleratorE2ETest
    : public NdkVideoEncoderAcceleratorTest {
 protected:
  std::unique_ptr<VideoDecoder> PrepareDecoder(
      gfx::Size size,
      VideoDecoder::OutputCB output_cb,
      std::vector<uint8_t> extra_data = std::vector<uint8_t>()) {
    VideoDecoderConfig config(
        codec_, profile_, VideoDecoderConfig::AlphaMode::kIsOpaque,
        VideoColorSpace::REC601(), VideoTransformation(), size, gfx::Rect(size),
        size, extra_data, EncryptionScheme::kUnencrypted);

    std::unique_ptr<VideoDecoder> decoder;
    if (codec_ == VideoCodec::kH264) {
#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
      decoder = std::make_unique<FFmpegVideoDecoder>(&media_log_);
#endif
    } else if (codec_ == VideoCodec::kVP8 || codec_ == VideoCodec::kVP9) {
#if BUILDFLAG(ENABLE_LIBVPX)
      decoder = std::make_unique<VpxVideoDecoder>();
#endif
    } else if (codec_ == VideoCodec::kAV1) {
#if BUILDFLAG(ENABLE_DAV1D_DECODER)
      decoder = std::make_unique<Dav1dVideoDecoder>(media_log_.Clone());
#endif
    }

    if (!decoder) {
      return nullptr;
    }

    decoder->Initialize(config, false, nullptr, DecoderStatusCB(),
                        std::move(output_cb), base::NullCallback());
    RunUntilIdle();
    return decoder;
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

  base::circular_deque<scoped_refptr<VideoFrame>> frames_to_encode_;
  std::vector<uint8_t> concatenated_stream_;
  int total_decoded_frames_ = 0;
  NullMediaLog media_log_;
};

TEST_P(NdkVideoEncoderAcceleratorTest, InitializeAndDestroy) {
  auto config = GetDefaultConfig();
  accelerator_ = MakeNdkAccelerator();
  EXPECT_CALL(*this, OnRequireBuffer()).WillOnce(Return(false));

  auto status = accelerator_->Initialize(config, this, NullLog());
  ASSERT_TRUE(status.is_ok())
      << EncoderStatusCodeToString(status.code()) << " " << status.message();
  SetCommandBufferHelper();

  Run();
  EXPECT_GE(id_to_buffer_.size(), 1u);
  accelerator_.reset();
  EXPECT_FALSE(error_status_.has_value());
}

TEST_P(NdkVideoEncoderAcceleratorTest, HandleEncodingError) {
  auto config = GetDefaultConfig();
  accelerator_ = MakeNdkAccelerator();
  EXPECT_CALL(*this, OnRequireBuffer()).WillOnce(Return(false));
  EXPECT_CALL(*this, OnError()).WillOnce(Return(false));

  auto status = accelerator_->Initialize(config, this, NullLog());
  ASSERT_TRUE(status.is_ok())
      << EncoderStatusCodeToString(status.code()) << " " << status.message();
  SetCommandBufferHelper();
  Run();

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
  EXPECT_CALL(*this, OnRequireBuffer()).WillOnce(Return(false));
  EXPECT_CALL(*this, OnBufferReady()).WillRepeatedly([this]() {
    if (outputs_.size() < total_frames_count) {
      return true;
    }
    return false;
  });

  auto status = accelerator_->Initialize(config, this, NullLog());
  ASSERT_TRUE(status.is_ok())
      << EncoderStatusCodeToString(status.code()) << " " << status.message();
  SetCommandBufferHelper();
  Run();

  auto duration = base::Milliseconds(16);
  for (auto frame_index = 0u; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * duration;
    uint32_t color = random_color_.Rand() & 0x00FFFFFF;
    auto frame =
        CreateFrame(config.input_visible_size, pixel_format_, timestamp, color);
    if (GetParam().use_shared_image) {
      frame = WrapInSharedImageFrame(frame);
    }

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
          loop_->Quit();
        }
      });

  auto decoder =
      PrepareDecoder(config.input_visible_size, std::move(decoder_output_cb));
  if (!decoder) {
    GTEST_SKIP() << "Decoder could not be created.";
  }

  EXPECT_CALL(*this, OnRequireBuffer()).WillOnce(Return(false));
  EXPECT_CALL(*this, OnBufferReady()).WillRepeatedly([this, &decoder]() {
    auto& output = outputs_.back();
    auto& mapping = id_to_buffer_[output.id]->GetMapping();
    auto data =
        mapping.GetMemoryAsSpan<uint8_t>().first(output.md.payload_size_bytes);
    concatenated_stream_.insert(concatenated_stream_.end(), data.begin(),
                                data.end());
    auto buffer = DecoderBuffer::CopyFrom(data);
    buffer->set_timestamp(output.md.timestamp);
    buffer->set_is_key_frame(output.md.key_frame);
    decoder->Decode(std::move(buffer), DecoderStatusCB());
    if (outputs_.size() == total_frames_count) {
      decoder->Decode(DecoderBuffer::CreateEOSBuffer(), DecoderStatusCB());
    }
    return true;
  });

  auto status = accelerator_->Initialize(config, this, NullLog());
  ASSERT_TRUE(status.is_ok())
      << EncoderStatusCodeToString(status.code()) << " " << status.message();
  SetCommandBufferHelper();
  Run();

  auto duration = base::Milliseconds(16);
  for (auto frame_index = 0u; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * duration;
    uint32_t color = random_color_.Rand() & 0x00FFFFFF;
    auto software_frame =
        CreateFrame(config.input_visible_size, pixel_format_, timestamp, color);
    scoped_refptr<VideoFrame> frame;
    if (GetParam().use_shared_image) {
      frame = WrapInSharedImageFrame(software_frame);
    } else {
      frame = software_frame;
    }

    frames_to_encode_.push_back(software_frame);
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
  if (info.param.use_shared_image) {
    result += "__SharedImage";
  }

  // GTest doesn't like spaces, but profile names have spaces, so we need
  // to replace them with underscores.
  std::replace(result.begin(), result.end(), ' ', '_');
  return result;
}

TEST_P(NdkVideoEncoderAcceleratorTest, Histograms) {
  const size_t total_frames_count = 5;
  auto config = GetDefaultConfig();
  accelerator_ = MakeNdkAccelerator();
  EXPECT_CALL(*this, OnRequireBuffer()).WillOnce(Return(false));
  EXPECT_CALL(*this, OnBufferReady()).WillRepeatedly([this]() {
    return outputs_.size() < total_frames_count;
  });

  base::HistogramTester histogram_tester;
  auto status = accelerator_->Initialize(config, this, NullLog());
  ASSERT_TRUE(status.is_ok());
  Run();

  histogram_tester.ExpectUniqueSample(
      "Media.VideoEncoder.NDKVEA.InitStatus." + GetCodecNameForUMA(codec_),
      EncoderStatus::Codes::kOk, 1);

  auto duration = base::Milliseconds(16);
  for (auto frame_index = 0u; frame_index < total_frames_count; frame_index++) {
    auto timestamp = frame_index * duration;
    uint32_t color = random_color_.Rand() & 0x00FFFFFF;
    auto frame =
        CreateFrame(config.input_visible_size, pixel_format_, timestamp, color);
    accelerator_->Encode(frame, true);
  }

  Run();

  // The EncodeStatus histogram is recorded in the destructor of the
  // accelerator.
  accelerator_.reset();

  histogram_tester.ExpectUniqueSample(
      "Media.VideoEncoder.NDKVEA.EncodeStatus." + GetCodecNameForUMA(codec_),
      EncoderStatus::Codes::kOk, 1);

  // Check EncodingLatency histogram
  auto latency_buckets = histogram_tester.GetAllSamples(
      "Media.VideoEncoder.NDKVEA.EncodingLatency." +
      GetCodecNameForUMA(codec_));
  size_t latency_samples = 0;
  for (const auto& bucket : latency_buckets) {
    EXPECT_GT(bucket.min, 0);
    latency_samples += bucket.count;
  }
  EXPECT_LE(latency_samples, total_frames_count);
  EXPECT_GT(latency_samples, 1u);
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

std::vector<VideoParams> EnableSharedImages(
    base::span<const VideoParams> params) {
  std::vector<VideoParams> result;
  for (auto param : params) {
    param.use_shared_image = true;
    param.use_gl_surface = true;
    result.push_back(param);
  }
  return result;
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

constexpr VideoParams kSIParams[] = {
    {H264PROFILE_BASELINE, PIXEL_FORMAT_XBGR},
    {H264PROFILE_MAIN, PIXEL_FORMAT_XBGR},
    {H264PROFILE_HIGH, PIXEL_FORMAT_XBGR},
    {VP9PROFILE_PROFILE0, PIXEL_FORMAT_XBGR},
};

INSTANTIATE_TEST_SUITE_P(SharedImagesNdkEncoderTests,
                         NdkVideoEncoderAcceleratorTest,
                         ::testing::ValuesIn(EnableSharedImages(kSIParams)),
                         PrintTestParams);

INSTANTIATE_TEST_SUITE_P(SharedImagesNdkEncoderE2ETests,
                         NdkVideoEncoderAcceleratorE2ETest,
                         ::testing::ValuesIn(EnableSharedImages(kSIParams)),
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
