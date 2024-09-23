// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <limits>
#include <optional>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/test_data_util.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/gpu/test/raw_video.h"
#include "media/gpu/test/video_encoder/bitstream_file_writer.h"
#include "media/gpu/test/video_encoder/bitstream_validator.h"
#include "media/gpu/test/video_encoder/decoder_buffer_validator.h"
#include "media/gpu/test/video_encoder/video_encoder.h"
#include "media/gpu/test/video_encoder/video_encoder_client.h"
#include "media/gpu/test/video_encoder/video_encoder_test_environment.h"
#include "media/gpu/test/video_frame_file_writer.h"
#include "media/gpu/test/video_frame_validator.h"
#include "media/gpu/test/video_test_environment.h"
#include "media/gpu/test/video_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace test {

namespace {

// Video encoder tests usage message. Make sure to also update the documentation
// under docs/media/gpu/video_encoder_test_usage.md when making changes here.
constexpr const char* usage_msg =
    R"(usage: video_encode_accelerator_tests
           [--codec=<codec>] [--svc_mode=<svc scalability mode>]
           [--bitrate_mode=(cbr|vbr)]
           [--reverse] [--bitrate=<bitrate>]
           [--disable_validator] [--psnr_threshold=<number>]
           [--output_bitstream] [--output_images=(all|corrupt)]
           [--output_format=(png|yuv)] [--output_folder=<filepath>]
           [--output_limit=<number>]
           [-v=<level>] [--vmodule=<config>]
           [--gtest_help] [--help]
           [<video path>] [<video metadata path>]
)";

// Video encoder tests help message.
constexpr const char* help_msg =
    R"""(Run the video encoder accelerator tests on the video specified by
<video path>. If no <video path> is given the default
"bear_320x192_40frames.yuv.webm" video will be used.

The <video metadata path> should specify the location of a json file
containing the video's metadata, such as frame checksums. By default
<video path>.json will be used.

The following arguments are supported:
   -v                   enable verbose mode, e.g. -v=2.
  --vmodule             enable verbose mode for the specified module,
                        e.g. --vmodule=*media/gpu*=2.
  --codec               codec profile to encode, "h264" (baseline),
                        "h264main, "h264high", "vp8", "vp9", "av1".
                        H264 Baseline is selected if unspecified.
  --num_spatial_layers  the number of spatial layers of the encoded
                        bitstream. A default value is 1. Only affected
                        if --codec=vp9 currently.
  --num_temporal_layers the number of temporal layers of the encoded
                        bitstream. A default value is 1.
  --svc_mode            SVC scalability mode. Spatial SVC encoding is only
                        supported with --codec=vp9 and only runs in NV12Dmabuf
                        test cases. The valid svc mode is "L1T1", "L1T2",
                        "L1T3", "L2T1_KEY", "L2T2_KEY", "L2T3_KEY", "L3T1_KEY",
                        "L3T2_KEY", "L3T3_KEY", "S2T1", "S2T2", "S2T3", "S3T1",
                        "S3T2", "S3T3". The default value is "L1T1".
  --bitrate             bitrate (bits in second) of a produced bitstram.
                        If not specified, a proper value for the video
                        resolution is selected by the test.
  --bitrate_mode        The rate control mode for encoding, one of "cbr"
                        (default) or "vbr".
  --reverse             the stream plays backwards if the stream reaches
                        end of stream. So the input stream to be encoded
                        is consecutive. By default this is false.
  --disable_validator   disable validation of encoded bitstream.
  --output_bitstream    save the output bitstream in either H264 AnnexB
                        format (for H264) or IVF format (for vp8 and
                        vp9) to <output_folder>/<testname>.
  --output_images       in addition to saving the full encoded,
                        bitstream it's also possible to dump individual
                        frames to <output_folder>/<testname>, possible
                        values are "all|corrupt"
  --output_format       set the format of images saved to disk,
                        supported formats are "png" (default) and
                        "yuv".
  --output_limit        limit the number of images saved to disk.
  --output_folder       set the basic folder used to store test
                        artifacts. The default is the current directory.

  --gtest_help          display the gtest help and exit.
  --help                display this help and exit.
)""";

// Default video to be used if no test video was specified.
constexpr base::FilePath::CharType kDefaultTestVideoPath[] =
    FILE_PATH_LITERAL("bear_320x192_40frames.yuv.webm");

// The number of frames to encode for bitrate check test cases.
// TODO(hiroh): Decrease this values to make the test faster.
constexpr size_t kNumFramesToEncodeForBitrateCheck = 300;
// Tolerance factor for how encoded bitrate can differ from requested bitrate.
constexpr double kBitrateTolerance = 0.15;
constexpr double kVariableBitrateTolerance = 0.3;
// The event timeout used in bitrate check tests because encoding 2160p and
// validating |kNumFramesToEncodeBitrateCheck| frames take much time.
constexpr base::TimeDelta kBitrateCheckEventTimeout = base::Seconds(180);

media::test::VideoEncoderTestEnvironment* g_env;

// Whether we validate the bitstream produced by the encoder.
bool g_enable_bitstream_validator = false;

// Declared PSNR threshold here, not in VideoEncoderTestEnvironment because it
// is specific in video_encode_accelerator_tests.
double g_psnr_threshold = PSNRVideoFrameValidator::kDefaultTolerance;

// Video encode test class. Performs setup and teardown for each single test.
class VideoEncoderTest : public ::testing::Test {
 public:
  // GetDefaultConfig() creates VideoEncoderClientConfig for SharedMemory input
  // encoding. This function must not be called in spatial SVC encoding.
  VideoEncoderClientConfig GetDefaultConfig() {
    const auto& spatial_layers = g_env->SpatialLayers();
    CHECK_LE(spatial_layers.size(), 1u);

    return VideoEncoderClientConfig(
        g_env->Video(), g_env->Profile(), spatial_layers,
        g_env->InterLayerPredMode(), g_env->ContentType(),
        g_env->BitrateAllocation(), g_env->Reverse());
  }

  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      const RawVideo* video,
      const VideoEncoderClientConfig& config,
      double validator_threshold = g_psnr_threshold) {
    LOG_ASSERT(video);

    auto video_encoder = VideoEncoder::Create(
        config, CreateBitstreamProcessors(video, config, validator_threshold));
    LOG_ASSERT(video_encoder);

    if (!video_encoder->Initialize(video))
      ADD_FAILURE();

    return video_encoder;
  }

 private:
  std::unique_ptr<BitstreamProcessor> CreateBitstreamValidator(
      const RawVideo* video,
      const VideoDecoderConfig& decoder_config,
      const size_t last_frame_index,
      const double validator_threshold,
      VideoFrameValidator::GetModelFrameCB get_model_frame_cb,
      std::optional<size_t> spatial_layer_index_to_decode,
      std::optional<size_t> temporal_layer_index_to_decode,
      SVCInterLayerPredMode inter_layer_pred_mode,
      const std::vector<gfx::Size>& spatial_layer_resolutions) {
    std::vector<std::unique_ptr<VideoFrameProcessor>> video_frame_processors;

    // Attach a video frame writer to store individual frames to disk if
    // requested.
    std::unique_ptr<VideoFrameProcessor> image_writer;
    auto frame_output_config = g_env->ImageOutputConfig();
    base::FilePath output_folder = base::FilePath(g_env->OutputFolder())
                                       .Append(g_env->GetTestOutputFilePath());
    if (frame_output_config.output_mode != FrameOutputMode::kNone) {
      base::FilePath::StringType output_file_prefix;
      if (spatial_layer_index_to_decode) {
        output_file_prefix +=
            (inter_layer_pred_mode == SVCInterLayerPredMode::kOff &&
                     spatial_layer_resolutions.size() > 1
                 ? FILE_PATH_LITERAL("S")
                 : FILE_PATH_LITERAL("L")) +
            base::FilePath::FromASCII(
                base::NumberToString(*spatial_layer_index_to_decode))
                .value();
      }
      if (temporal_layer_index_to_decode) {
        output_file_prefix +=
            FILE_PATH_LITERAL("T") +
            base::FilePath::FromASCII(
                base::NumberToString(*temporal_layer_index_to_decode))
                .value();
      }

      image_writer = VideoFrameFileWriter::Create(
          output_folder, frame_output_config.output_format,
          frame_output_config.output_limit, output_file_prefix);
      LOG_ASSERT(image_writer);
      if (frame_output_config.output_mode == FrameOutputMode::kAll)
        video_frame_processors.push_back(std::move(image_writer));
    }

    auto psnr_validator = PSNRVideoFrameValidator::Create(
        get_model_frame_cb, std::move(image_writer),
        VideoFrameValidator::ValidationMode::kAverage, validator_threshold);
    LOG_ASSERT(psnr_validator);
    video_frame_processors.push_back(std::move(psnr_validator));
    return BitstreamValidator::Create(
        decoder_config, last_frame_index, std::move(video_frame_processors),
        spatial_layer_index_to_decode, temporal_layer_index_to_decode,
        spatial_layer_resolutions);
  }

  std::vector<std::unique_ptr<BitstreamProcessor>> CreateBitstreamProcessors(
      const RawVideo* video,
      const VideoEncoderClientConfig& config,
      double validator_threshold) {
    std::vector<std::unique_ptr<BitstreamProcessor>> bitstream_processors;
    const gfx::Rect visible_rect(config.output_resolution);
    std::vector<gfx::Size> spatial_layer_resolutions;
    // |config.spatial_layers| is filled only in temporal layer or spatial layer
    // encoding.
    for (const auto& sl : config.spatial_layers)
      spatial_layer_resolutions.emplace_back(sl.width, sl.height);

    const VideoCodec codec =
        VideoCodecProfileToVideoCodec(config.output_profile);
    if (g_env->SaveOutputBitstream()) {
      if (!spatial_layer_resolutions.empty()) {
        CHECK_GE(config.num_spatial_layers, 1u);
        CHECK_GE(config.num_temporal_layers, 1u);
        for (size_t spatial_layer_index_to_write = 0;
             spatial_layer_index_to_write < config.num_spatial_layers;
             ++spatial_layer_index_to_write) {
          const gfx::Size& layer_size =
              spatial_layer_resolutions[spatial_layer_index_to_write];
          for (size_t temporal_layer_index_to_write = 0;
               temporal_layer_index_to_write < config.num_temporal_layers;
               ++temporal_layer_index_to_write) {
            bitstream_processors.emplace_back(BitstreamFileWriter::Create(
                g_env->OutputFilePath(codec, true, spatial_layer_index_to_write,
                                      temporal_layer_index_to_write),
                codec, layer_size, config.framerate,
                config.num_frames_to_encode, spatial_layer_index_to_write,
                temporal_layer_index_to_write, spatial_layer_resolutions));
            LOG_ASSERT(bitstream_processors.back());
          }
        }
      } else {
        bitstream_processors.emplace_back(BitstreamFileWriter::Create(
            g_env->OutputFilePath(codec), codec, visible_rect.size(),
            config.framerate, config.num_frames_to_encode));
        LOG_ASSERT(bitstream_processors.back());
      }
    }

    if (!g_enable_bitstream_validator) {
      return bitstream_processors;
    }

    bitstream_processors.emplace_back(DecoderBufferValidator::Create(
        config.output_profile, visible_rect, config.num_spatial_layers,
        config.num_temporal_layers, config.inter_layer_pred_mode));

    raw_data_helper_ = std::make_unique<RawDataHelper>(video, g_env->Reverse());
    if (!spatial_layer_resolutions.empty()) {
      CHECK_GE(config.num_spatial_layers, 1u);
      CHECK_GE(config.num_temporal_layers, 1u);
      for (size_t spatial_layer_index_to_decode = 0;
           spatial_layer_index_to_decode < config.num_spatial_layers;
           ++spatial_layer_index_to_decode) {
        const gfx::Size& layer_size =
            spatial_layer_resolutions[spatial_layer_index_to_decode];
        VideoDecoderConfig decoder_config(
            codec, config.output_profile,
            VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace(),
            kNoTransformation, layer_size, gfx::Rect(layer_size), layer_size,
            EmptyExtraData(), EncryptionScheme::kUnencrypted);
        VideoFrameValidator::GetModelFrameCB get_model_frame_cb =
            base::BindRepeating(&VideoEncoderTest::GetModelFrame,
                                base::Unretained(this), gfx::Rect(layer_size));
        for (size_t temporal_layer_index_to_decode = 0;
             temporal_layer_index_to_decode < config.num_temporal_layers;
             ++temporal_layer_index_to_decode) {
          bitstream_processors.emplace_back(CreateBitstreamValidator(
              video, decoder_config, config.num_frames_to_encode - 1,
              validator_threshold, get_model_frame_cb,
              spatial_layer_index_to_decode, temporal_layer_index_to_decode,
              config.inter_layer_pred_mode, spatial_layer_resolutions));
          LOG_ASSERT(bitstream_processors.back());
        }
      }
    } else {
      // Attach a bitstream validator to validate all encoded video frames. The
      // bitstream validator uses a software video decoder to validate the
      // encoded buffers by decoding them. Metrics such as the image's SSIM can
      // be calculated for additional quality checks.
      VideoDecoderConfig decoder_config(
          codec, config.output_profile,
          VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace(),
          kNoTransformation, visible_rect.size(), visible_rect,
          visible_rect.size(), EmptyExtraData(),
          EncryptionScheme::kUnencrypted);
      VideoFrameValidator::GetModelFrameCB get_model_frame_cb =
          base::BindRepeating(&VideoEncoderTest::GetModelFrame,
                              base::Unretained(this), visible_rect);
      bitstream_processors.emplace_back(CreateBitstreamValidator(
          video, decoder_config, config.num_frames_to_encode - 1,
          validator_threshold, get_model_frame_cb, std::nullopt, std::nullopt,
          config.inter_layer_pred_mode, /*spatial_layer_resolutions=*/{}));
      LOG_ASSERT(bitstream_processors.back());
    }
    return bitstream_processors;
  }

  scoped_refptr<const VideoFrame> GetModelFrame(const gfx::Rect& visible_rect,
                                                size_t frame_index) {
    LOG_ASSERT(raw_data_helper_);
    auto frame = raw_data_helper_->GetFrame(frame_index);
    if (!frame)
      return nullptr;
    if (visible_rect.size() == frame->visible_rect().size())
      return frame;
    return ScaleVideoFrame(frame.get(), visible_rect.size());
  }

  std::unique_ptr<RawDataHelper> raw_data_helper_;
};
}  // namespace

// Encode video from start to end. Wait for the kFlushDone event at the end of
// the stream, that notifies us all frames have been encoded.
TEST_F(VideoEncoderTest, FlushAtEndOfStream) {
  if (g_env->SpatialLayers().size() > 1)
    GTEST_SKIP() << "Skip SHMEM input test cases in spatial SVC encoding";

  auto encoder = CreateVideoEncoder(g_env->Video(), GetDefaultConfig());

  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());

  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

// Test initializing the video encoder. The test will be successful if the video
// encoder is capable of setting up the encoder for the specified codec and
// resolution. The test only verifies initialization and doesn't do any
// encoding.
TEST_F(VideoEncoderTest, Initialize) {
  if (g_env->SpatialLayers().size() > 1)
    GTEST_SKIP() << "Skip SHMEM input test cases in spatial SVC encoding";

  auto encoder = CreateVideoEncoder(g_env->Video(), GetDefaultConfig());

  EXPECT_EQ(encoder->GetEventCount(VideoEncoder::kInitialized), 1u);
}

// Create a video encoder and immediately destroy it without initializing. The
// video encoder will be automatically destroyed when the video encoder goes out
// of scope at the end of the test. The test will pass if no asserts or crashes
// are triggered upon destroying.
TEST_F(VideoEncoderTest, DestroyBeforeInitialize) {
  if (g_env->SpatialLayers().size() > 1)
    GTEST_SKIP() << "Skip SHMEM input test cases in spatial SVC encoding";

  auto video_encoder = VideoEncoder::Create(GetDefaultConfig());

  EXPECT_NE(video_encoder, nullptr);
}

// Test forcing key frames while encoding a video.
TEST_F(VideoEncoderTest, ForceKeyFrame) {
  if (g_env->SpatialLayers().size() > 1)
    GTEST_SKIP() << "Skip SHMEM input test cases in spatial SVC encoding";

  auto config = GetDefaultConfig();
  const size_t middle_frame = config.num_frames_to_encode;
  config.num_frames_to_encode *= 2;
  auto encoder = CreateVideoEncoder(g_env->Video(), config);

  // It is expected that our hw encoders don't produce key frames in a short
  // time span like a few hundred frames.
  encoder->EncodeUntil(VideoEncoder::kBitstreamReady, 1u);
  EXPECT_TRUE(encoder->WaitUntilIdle());
  EXPECT_EQ(encoder->GetEventCount(VideoEncoder::kKeyFrame), 1u);
  // Encode until the middle of stream and request force_keyframe.
  encoder->EncodeUntil(VideoEncoder::kFrameReleased, middle_frame);
  EXPECT_TRUE(encoder->WaitUntilIdle());
  // Check if there is no keyframe except the first frame.
  EXPECT_EQ(encoder->GetEventCount(VideoEncoder::kKeyFrame), 1u);
  encoder->ForceKeyFrame();

  // Encode until the end of stream.
  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  // Check if there are two key frames, first frame and one on ForceKeyFrame().
  EXPECT_EQ(encoder->GetEventCount(VideoEncoder::kKeyFrame), 2u);
  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), config.num_frames_to_encode);
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

// Test forcing key frame to the first and second frames.
TEST_F(VideoEncoderTest, ForceTheFirstAndSecondKeyFrames) {
  if (g_env->SpatialLayers().size() > 1) {
    GTEST_SKIP() << "Skip SHMEM input test cases in spatial SVC encoding";
  }

  auto config = GetDefaultConfig();
  CHECK_GT(config.num_frames_to_encode, 1u);

  // The two keyframes impairs the video quality. We use the default tolerance
  // in order to keep the psnr threshold high that is specified by
  // --psnr_threshold in video.EncodeAccel tast tests.
  auto encoder = CreateVideoEncoder(g_env->Video(), config,
                                    PSNRVideoFrameValidator::kDefaultTolerance);

  // Encode until the first frame and request force_keyframe.
  encoder->EncodeUntil(VideoEncoder::kFrameReleased, 1u);
  EXPECT_TRUE(encoder->WaitUntilIdle());
  encoder->ForceKeyFrame();
  // Check if the first and second frames are key frames.
  encoder->EncodeUntil(VideoEncoder::kBitstreamReady, 2u);
  EXPECT_TRUE(encoder->WaitUntilIdle());
  EXPECT_EQ(encoder->GetEventCount(VideoEncoder::kKeyFrame), 2u);

  // Encode until the end of stream.
  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), config.num_frames_to_encode);
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

// Encode video from start to end. Multiple buffer encodes will be queued in the
// encoder, without waiting for the result of the previous encode requests.
TEST_F(VideoEncoderTest, FlushAtEndOfStream_MultipleOutstandingEncodes) {
  if (g_env->SpatialLayers().size() > 1)
    GTEST_SKIP() << "Skip SHMEM input test cases in spatial SVC encoding";

  auto config = GetDefaultConfig();
  config.max_outstanding_encode_requests = 4;
  auto encoder = CreateVideoEncoder(g_env->Video(), config);

  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());

  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

// Encode multiple videos simultaneously from start to finish.
TEST_F(VideoEncoderTest, FlushAtEndOfStream_MultipleConcurrentEncodes) {
  if (g_env->SpatialLayers().size() > 1)
    GTEST_SKIP() << "Skip SHMEM input test cases in spatial SVC encoding";

  // Run two encoders for larger resolutions to avoid creating shared memory
  // buffers during the test on lower end devices.
  constexpr gfx::Size k1080p(1920, 1080);
  const size_t kMinSupportedConcurrentEncoders =
      g_env->Video()->Resolution().GetArea() >= k1080p.GetArea() ? 2 : 3;

  auto config = GetDefaultConfig();
  std::vector<std::unique_ptr<VideoEncoder>> encoders(
      kMinSupportedConcurrentEncoders);
  for (size_t i = 0; i < kMinSupportedConcurrentEncoders; ++i)
    encoders[i] = CreateVideoEncoder(g_env->Video(), config);

  for (size_t i = 0; i < kMinSupportedConcurrentEncoders; ++i)
    encoders[i]->Encode();

  for (size_t i = 0; i < kMinSupportedConcurrentEncoders; ++i) {
    EXPECT_TRUE(encoders[i]->WaitForFlushDone());
    EXPECT_EQ(encoders[i]->GetFlushDoneCount(), 1u);
    EXPECT_EQ(encoders[i]->GetFrameReleasedCount(),
              g_env->Video()->NumFrames());
    EXPECT_TRUE(encoders[i]->WaitForBitstreamProcessors());
  }
}

TEST_F(VideoEncoderTest, BitrateCheck) {
  if (g_env->SpatialLayers().size() > 1)
    GTEST_SKIP() << "Skip SHMEM input test cases in spatial SVC encoding";

  auto config = GetDefaultConfig();
  // TODO(b/181797390): Reconsider bitrate check for VBR encoding if this fails
  // on some boards.
  const bool vbr_encoding =
      config.bitrate_allocation.GetMode() == Bitrate::Mode::kVariable;
  const double tolerance =
      vbr_encoding ? kVariableBitrateTolerance : kBitrateTolerance;
  // Encode twice as many frame as kNumFramesToEncodeForBitrateCheck in VBR
  // encoding. This is a workaround the zork rate controller. See b/361109092.
  // TODO(b/195407733): Remove this workaround if we introduce the rate
  // controller to the H264 vaapi encoder.
  config.num_frames_to_encode = vbr_encoding
                                    ? kNumFramesToEncodeForBitrateCheck * 2
                                    : kNumFramesToEncodeForBitrateCheck * 3;

  auto encoder = CreateVideoEncoder(g_env->Video(), config);
  // Set longer event timeout than the default (30 sec) because encoding 2160p
  // and validating the stream take much time.
  encoder->SetEventWaitTimeout(kBitrateCheckEventTimeout);

  const uint32_t first_bitrate = config.bitrate_allocation.GetSumBps();
  const uint32_t first_framerate = config.framerate;
  if (vbr_encoding) {
    encoder->Encode();
    EXPECT_TRUE(encoder->WaitForFlushDone());
  } else {
    encoder->EncodeUntil(VideoEncoder::kFrameReleased,
                         kNumFramesToEncodeForBitrateCheck);
    EXPECT_TRUE(encoder->WaitUntilIdle());
    EXPECT_TRUE(encoder->WaitForEvent(VideoEncoder::kBitstreamReady,
                                      kNumFramesToEncodeForBitrateCheck));
  }
  EXPECT_NEAR(encoder->GetStats().Bitrate(), first_bitrate,
              tolerance * first_bitrate);

  if (!vbr_encoding) {
    // Change bitrate only.
    const uint32_t second_bitrate = first_bitrate * 3 / 2;
    const uint32_t second_framerate = first_framerate;
    encoder->ResetStats();
    encoder->UpdateBitrate(
        AllocateDefaultBitrateForTesting(
            config.num_spatial_layers, config.num_temporal_layers,
            Bitrate::ConstantBitrate(second_bitrate)),
        second_framerate);
    encoder->EncodeUntil(VideoEncoder::kFrameReleased,
                         kNumFramesToEncodeForBitrateCheck * 2);
    EXPECT_TRUE(encoder->WaitUntilIdle());
    EXPECT_TRUE(encoder->WaitForEvent(VideoEncoder::kBitstreamReady,
                                      kNumFramesToEncodeForBitrateCheck));
    EXPECT_NEAR(encoder->GetStats().Bitrate(), second_bitrate,
                tolerance * second_bitrate);

    // Change bitrate and framerate.
    const uint32_t third_bitrate = first_bitrate;
    const uint32_t third_framerate = std::max(first_framerate * 2 / 3, 10u);
    encoder->ResetStats();
    encoder->UpdateBitrate(
        AllocateDefaultBitrateForTesting(
            config.num_spatial_layers, config.num_temporal_layers,
            Bitrate::ConstantBitrate(third_bitrate)),
        third_framerate);
    encoder->Encode();
    EXPECT_TRUE(encoder->WaitForFlushDone());
    EXPECT_NEAR(encoder->GetStats().Bitrate(), third_bitrate,
                tolerance * third_bitrate);
  }

  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), config.num_frames_to_encode);
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
// TODO(https://crbugs.com/350994517): NV12 DMABuf test does not apply to
// Windows. There should be similar test for this with NV12 DXGI buffers added.
TEST_F(VideoEncoderTest, FlushAtEndOfStream_NV12Dmabuf) {
  RawVideo* nv12_video = g_env->GenerateNV12Video();
  VideoEncoderClientConfig config(
      nv12_video, g_env->Profile(), g_env->SpatialLayers(),
      g_env->InterLayerPredMode(), g_env->ContentType(),
      g_env->BitrateAllocation(), g_env->Reverse());
  config.input_storage_type =
      VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;

  auto encoder = CreateVideoEncoder(nv12_video, config);

  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), nv12_video->NumFrames());
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

// TODO(https://crbugs.com/350994517): These are for scaling and cropping,
// which requires GMB support. Enable these tests when GMB is supported
// for VEA tests on Windows.
// Downscaling is required in VideoEncodeAccelerator when zero-copy video
// capture is enabled. One example is simulcast, camera produces 360p VideoFrame
// and there are two VideoEncodeAccelerator for 360p and 180p. VideoEncoder for
// 180p is fed 360p and thus has to perform the scaling from 360p to 180p.
TEST_F(VideoEncoderTest, FlushAtEndOfStream_NV12DmabufScaling) {
  if (g_env->SpatialLayers().size() > 1)
    GTEST_SKIP() << "Skip simulcast test case for spatial SVC encoding";

  constexpr gfx::Size kMinOutputResolution(240, 180);
  const gfx::Size output_resolution =
      gfx::Size(g_env->Video()->Resolution().width() / 2,
                g_env->Video()->Resolution().height() / 2);
  if (!gfx::Rect(output_resolution).Contains(gfx::Rect(kMinOutputResolution))) {
    GTEST_SKIP() << "Skip test if video resolution is too small, "
                 << "output_resolution=" << output_resolution.ToString()
                 << ", minimum output resolution="
                 << kMinOutputResolution.ToString();
  }

  auto* nv12_video = g_env->GenerateNV12Video();
  // Set 1/4 of the original bitrate because the area of |output_resolution| is
  // 1/4 of the original resolution.
  uint32_t new_target_bitrate = g_env->BitrateAllocation().GetSumBps() / 4;
  // TODO(b/181797390): Reconsider if this peak bitrate is reasonable.
  const Bitrate new_bitrate =
      g_env->BitrateAllocation().GetMode() == Bitrate::Mode::kConstant
          ? Bitrate::ConstantBitrate(new_target_bitrate)
          : Bitrate::VariableBitrate(new_target_bitrate,
                                     new_target_bitrate * 2);

  auto spatial_layers = g_env->SpatialLayers();
  size_t num_temporal_layers = 1u;
  if (!spatial_layers.empty()) {
    CHECK_EQ(spatial_layers.size(), 1u);
    spatial_layers[0].width = output_resolution.width();
    spatial_layers[0].height = output_resolution.height();
    spatial_layers[0].bitrate_bps /= 4;
    num_temporal_layers = spatial_layers[0].num_of_temporal_layers;
  }
  VideoEncoderClientConfig config(
      nv12_video, g_env->Profile(), spatial_layers, SVCInterLayerPredMode::kOff,
      g_env->ContentType(),
      AllocateDefaultBitrateForTesting(/*num_spatial_layers=*/1u,
                                       num_temporal_layers, new_bitrate),
      g_env->Reverse());
  config.output_resolution = output_resolution;
  config.input_storage_type =
      VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;

  // The encoded resolution is 1/4 of the input resolution and thus the
  // compression quality is reduced. Since the appropriate threshold for the
  // small resolution is unknown, so we use the default tolerance in this
  // scaling test case.
  auto encoder = CreateVideoEncoder(nv12_video, config,
                                    PSNRVideoFrameValidator::kDefaultTolerance);
  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), nv12_video->NumFrames());
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

// Encode VideoFrames with cropping the rectangle (0, 60, size).
// Cropping is required in VideoEncodeAccelerator when zero-copy video
// capture is enabled. One example is when 640x360 capture recording is
// requested, a camera cannot produce the resolution and instead produces
// 640x480 frames with visible_rect=0, 60, 640x360.
TEST_F(VideoEncoderTest, FlushAtEndOfStream_NV12DmabufCroppingTopAndBottom) {
  constexpr int kGrowHeight = 120;
  const gfx::Size original_resolution = g_env->Video()->Resolution();
  const gfx::Rect expanded_visible_rect(0, kGrowHeight / 2,
                                        original_resolution.width(),
                                        original_resolution.height());
  const gfx::Size expanded_resolution(
      original_resolution.width(), original_resolution.height() + kGrowHeight);
  constexpr gfx::Size kMaxExpandedResolution(1920, 1080);
  if (!gfx::Rect(kMaxExpandedResolution)
           .Contains(gfx::Rect(expanded_resolution))) {
    GTEST_SKIP() << "Expanded video resolution is too large, "
                 << "expanded_resolution=" << expanded_resolution.ToString()
                 << ", maximum expanded resolution="
                 << kMaxExpandedResolution.ToString();
  }

  auto nv12_expanded_video = g_env->GenerateNV12Video()->CreateExpandedVideo(
      expanded_resolution, expanded_visible_rect);
  ASSERT_TRUE(nv12_expanded_video);
  VideoEncoderClientConfig config(
      nv12_expanded_video.get(), g_env->Profile(), g_env->SpatialLayers(),
      g_env->InterLayerPredMode(), g_env->ContentType(),
      g_env->BitrateAllocation(), g_env->Reverse());
  config.output_resolution = original_resolution;
  config.input_storage_type =
      VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;

  auto encoder = CreateVideoEncoder(nv12_expanded_video.get(), config);
  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), nv12_expanded_video->NumFrames());
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

// Encode VideoFrames with cropping the rectangle (60, 0, size).
// Cropping is required in VideoEncodeAccelerator when zero-copy video
// capture is enabled. One example is when 640x360 capture recording is
// requested, a camera cannot produce the resolution and instead produces
// 760x360 frames with visible_rect=60, 0, 640x360.
TEST_F(VideoEncoderTest, FlushAtEndOfStream_NV12DmabufCroppingRightAndLeft) {
  constexpr int kGrowWidth = 120;
  const gfx::Size original_resolution = g_env->Video()->Resolution();
  const gfx::Rect expanded_visible_rect(kGrowWidth / 2, 0,
                                        original_resolution.width(),
                                        original_resolution.height());
  const gfx::Size expanded_resolution(original_resolution.width() + kGrowWidth,
                                      original_resolution.height());
  constexpr gfx::Size kMaxExpandedResolution(1920, 1080);
  if (!gfx::Rect(kMaxExpandedResolution)
           .Contains(gfx::Rect(expanded_resolution))) {
    GTEST_SKIP() << "Expanded video resolution is too large, "
                 << "expanded_resolution=" << expanded_resolution.ToString()
                 << ", maximum expanded resolution="
                 << kMaxExpandedResolution.ToString();
  }

  auto nv12_expanded_video = g_env->GenerateNV12Video()->CreateExpandedVideo(
      expanded_resolution, expanded_visible_rect);
  ASSERT_TRUE(nv12_expanded_video);
  VideoEncoderClientConfig config(
      nv12_expanded_video.get(), g_env->Profile(), g_env->SpatialLayers(),
      g_env->InterLayerPredMode(), g_env->ContentType(),
      g_env->BitrateAllocation(), g_env->Reverse());
  config.output_resolution = original_resolution;
  config.input_storage_type =
      VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;

  auto encoder = CreateVideoEncoder(nv12_expanded_video.get(), config);
  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), nv12_expanded_video->NumFrames());
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

// This tests deactivate and activating spatial layers during encoding.
TEST_F(VideoEncoderTest, DeactivateAndActivateSpatialLayers) {
  const auto& spatial_layers = g_env->SpatialLayers();
  if (spatial_layers.size() <= 1)
    GTEST_SKIP() << "Skip (de)activate spatial layers test for simple encoding";

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  RawVideo* video = g_env->GenerateNV12Video();
#else
  // TODO(b/211783271): Add support for I420 SHM input.
  RawVideo* video = g_env->Video();
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

  const size_t bottom_spatial_idx = 0;
  const size_t top_spatial_idx = spatial_layers.size() - 1;
  auto deactivate_spatial_layer =
      [](VideoBitrateAllocation bitrate_allocation,
         size_t deactivate_sid) -> VideoBitrateAllocation {
    for (size_t i = 0; i < VideoBitrateAllocation::kMaxTemporalLayers; ++i)
      bitrate_allocation.SetBitrate(deactivate_sid, i, 0u);
    return bitrate_allocation;
  };

  const auto& default_allocation = g_env->BitrateAllocation();
  std::vector<VideoBitrateAllocation> bitrate_allocations;

  // Deactivate the top layer.
  bitrate_allocations.emplace_back(
      deactivate_spatial_layer(default_allocation, top_spatial_idx));

  // Activate the top layer.
  bitrate_allocations.emplace_back(default_allocation);

  // Deactivate the bottom layer (and top layer if there is still a spatial
  // layer).
  auto bitrate_allocation =
      deactivate_spatial_layer(default_allocation, bottom_spatial_idx);
  if (bottom_spatial_idx + 1 < top_spatial_idx) {
    bitrate_allocation =
        deactivate_spatial_layer(bitrate_allocation, top_spatial_idx);
  }
  bitrate_allocations.emplace_back(bitrate_allocation);

  // Deactivate the layers except bottom layer.
  bitrate_allocation = default_allocation;
  for (size_t i = bottom_spatial_idx + 1; i < spatial_layers.size(); ++i)
    bitrate_allocation = deactivate_spatial_layer(bitrate_allocation, i);
  bitrate_allocations.emplace_back(bitrate_allocation);

  VideoEncoderClientConfig config(
      video, g_env->Profile(), g_env->SpatialLayers(),
      g_env->InterLayerPredMode(), g_env->ContentType(),
      g_env->BitrateAllocation(), g_env->Reverse());

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  config.input_storage_type =
      VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;
#else
  // TODO(https://crbugs.com/350994517): Enable GMB for Windows.
  config.input_storage_type =
      VideoEncodeAccelerator::Config::StorageType::kShmem;
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

  std::vector<size_t> num_frames_to_encode(bitrate_allocations.size());
  for (size_t i = 0; i < num_frames_to_encode.size(); ++i)
    num_frames_to_encode[i] = config.num_frames_to_encode * (i + 1);
  config.num_frames_to_encode =
      num_frames_to_encode.back() + config.num_frames_to_encode;

  auto encoder = CreateVideoEncoder(video, config);

  for (size_t i = 0; i < bitrate_allocations.size(); ++i) {
    encoder->EncodeUntil(VideoEncoder::kFrameReleased, num_frames_to_encode[i]);
    EXPECT_TRUE(encoder->WaitUntilIdle());
    encoder->UpdateBitrate(bitrate_allocations[i], config.framerate);
  }

  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), config.num_frames_to_encode);
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

#if BUILDFLAG(USE_VAAPI)
TEST_F(VideoEncoderTest, FlushAtEndOfStream_NV12Dmabuf_EnableDropFrame) {
  const VideoCodec codec = VideoCodecProfileToVideoCodec(g_env->Profile());
  if (codec != media::VideoCodec::kVP8 && codec != media::VideoCodec::kVP9 &&
      codec != media::VideoCodec::kAV1) {
    GTEST_SKIP() << "VideoEncodeAccelerator on this device doesn't support drop"
                 << "frame with codec=" << GetCodecName(codec);
  }
  if (g_env->BitrateAllocation().GetMode() == Bitrate::Mode::kVariable) {
    GTEST_SKIP() << "Drop frame doesn't support in VBR encoding";
  }

  RawVideo* nv12_video = g_env->GenerateNV12Video();
  VideoEncoderClientConfig config(
      nv12_video, g_env->Profile(), g_env->SpatialLayers(),
      g_env->InterLayerPredMode(), g_env->ContentType(),
      g_env->BitrateAllocation(), g_env->Reverse());
  config.input_storage_type =
      VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer;
  constexpr uint8_t kDropFrameThreshold = 80;
  config.drop_frame_thresh = kDropFrameThreshold;
  auto encoder = CreateVideoEncoder(nv12_video, config);

  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());

  auto stats = encoder->GetStats();
  VLOG(0) << "Dropped frames: " << stats.num_dropped_frames << " / "
          << stats.total_num_encoded_frames;
}
#endif  // BUILDFLAG(USE_VAAPI)

}  // namespace test
}  // namespace media

int main(int argc, char** argv) {
  // Set the default test data path.
  media::test::RawVideo::SetTestDataPath(media::GetTestDataPath());

  // Print the help message if requested. This needs to be done before
  // initializing gtest, to overwrite the default gtest help message.
  base::CommandLine::Init(argc, argv);
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  LOG_ASSERT(cmd_line);
  if (cmd_line->HasSwitch("help")) {
    std::cout << media::test::usage_msg << "\n" << media::test::help_msg;
    return 0;
  }

  // Check if a video was specified on the command line.
  base::CommandLine::StringVector args = cmd_line->GetArgs();
  base::FilePath video_path =
      (args.size() >= 1) ? base::FilePath(args[0])
                         : base::FilePath(media::test::kDefaultTestVideoPath);
  base::FilePath video_metadata_path =
      (args.size() >= 2) ? base::FilePath(args[1]) : base::FilePath();
  std::string codec = "h264";
  std::string svc_mode = "L1T1";
  bool output_bitstream = false;
  std::optional<uint32_t> output_bitrate;
  bool reverse = false;
  media::Bitrate::Mode bitrate_mode = media::Bitrate::Mode::kConstant;
  media::test::FrameOutputConfig frame_output_config;
  base::FilePath output_folder =
      base::FilePath(base::FilePath::kCurrentDirectory);
  std::vector<base::test::FeatureRef> disabled_features;

  // Parse command line arguments.
  media::test::g_enable_bitstream_validator = true;
  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first.find("gtest_") == 0 ||               // Handled by GoogleTest
        it->first == "v" || it->first == "vmodule") {  // Handled by Chrome
      continue;
    }

    if (it->first == "num_temporal_layers" ||
        it->first == "num_spatial_layers") {
      std::cout << "--num_temporal_layers and --num_spatial_layers have been "
                << "removed. Please use --svc_mode";
      return EXIT_FAILURE;
    }

    if (it->first == "codec") {
      codec = cmd_line->GetSwitchValueASCII("codec");
    } else if (it->first == "svc_mode") {
      svc_mode = cmd_line->GetSwitchValueASCII("svc_mode");
    } else if (it->first == "bitrate_mode") {
      auto brc_mode_str = cmd_line->GetSwitchValueASCII("bitrate_mode");
      if (brc_mode_str == "vbr") {
        bitrate_mode = media::Bitrate::Mode::kVariable;
      } else if (brc_mode_str != "cbr") {
        std::cout << "unknown bitrate mode \"" << brc_mode_str
                  << "\", possible values are \"cbr|vbr\"\n";
        return EXIT_FAILURE;
      }
    } else if (it->first == "disable_validator") {
      media::test::g_enable_bitstream_validator = false;
    } else if (it->first == "psnr_threshold") {
      if (!base::StringToDouble(it->second, &media::test::g_psnr_threshold)) {
        std::cout << "invalid number \"" << it->second << "\n";
        return EXIT_FAILURE;
      }
    } else if (it->first == "output_bitstream") {
      output_bitstream = true;
    } else if (it->first == "bitrate") {
      unsigned value;
      if (!base::StringToUint(it->second, &value)) {
        std::cout << "invalid bitrate " << it->second << "\n"
                  << media::test::usage_msg;
        return EXIT_FAILURE;
      }
      output_bitrate = base::checked_cast<uint32_t>(value);
    } else if (it->first == "reverse") {
      reverse = true;
    } else if (it->first == "output_images") {
      auto output_mode_str = cmd_line->GetSwitchValueASCII("output_images");
      if (output_mode_str == "all") {
        frame_output_config.output_mode = media::test::FrameOutputMode::kAll;
      } else if (output_mode_str == "corrupt") {
        frame_output_config.output_mode =
            media::test::FrameOutputMode::kCorrupt;
      } else {
        std::cout << "unknown image output mode \"" << output_mode_str
                  << "\", possible values are \"all|corrupt\"\n";
        return EXIT_FAILURE;
      }
    } else if (it->first == "output_format") {
      auto output_format_str = cmd_line->GetSwitchValueASCII("output_format");
      if (output_format_str == "png") {
        frame_output_config.output_format =
            media::test::VideoFrameFileWriter::OutputFormat::kPNG;
      } else if (output_format_str == "yuv") {
        frame_output_config.output_format =
            media::test::VideoFrameFileWriter::OutputFormat::kYUV;
      } else {
        std::cout << "unknown frame output format \"" << output_format_str
                  << "\", possible values are \"png|yuv\"\n";
        return EXIT_FAILURE;
      }
    } else if (it->first == "output_limit") {
      if (!base::StringToUint64(it->second,
                                &frame_output_config.output_limit)) {
        std::cout << "invalid number \"" << it->second << "\n";
        return EXIT_FAILURE;
      }
    } else if (it->first == "output_folder") {
      output_folder = base::FilePath(it->second);
    } else {
      std::cout << "unknown option: --" << it->first << "\n"
                << media::test::usage_msg;
      return EXIT_FAILURE;
    }
  }

  disabled_features.push_back(media::kGlobalVaapiLock);

  testing::InitGoogleTest(&argc, argv);

  // Set up our test environment.
  media::test::VideoEncoderTestEnvironment* test_environment =
      media::test::VideoEncoderTestEnvironment::Create(
          media::test::VideoEncoderTestEnvironment::TestType::kValidation,
          video_path, video_metadata_path, output_folder, codec, svc_mode,
          media::VideoEncodeAccelerator::Config::ContentType::kCamera,
          output_bitstream, output_bitrate, bitrate_mode, reverse,
          frame_output_config, /*enabled_features=*/{}, disabled_features);

  if (!test_environment)
    return EXIT_FAILURE;

  media::test::g_env = static_cast<media::test::VideoEncoderTestEnvironment*>(
      testing::AddGlobalTestEnvironment(test_environment));

  return RUN_ALL_TESTS();
}
