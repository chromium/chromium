// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <limits>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/media_util.h"
#include "media/base/test_data_util.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/test/video.h"
#include "media/gpu/test/video_encoder/bitstream_file_writer.h"
#include "media/gpu/test/video_encoder/bitstream_validator.h"
#include "media/gpu/test/video_encoder/decoder_buffer_validator.h"
#include "media/gpu/test/video_encoder/video_encoder.h"
#include "media/gpu/test/video_encoder/video_encoder_client.h"
#include "media/gpu/test/video_encoder/video_encoder_test_environment.h"
#include "media/gpu/test/video_frame_file_writer.h"
#include "media/gpu/test/video_frame_helpers.h"
#include "media/gpu/test/video_frame_validator.h"
#include "media/gpu/test/video_test_environment.h"
#include "media/gpu/test/video_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace test {

namespace {

// Video encoder tests usage message. Make sure to also update the documentation
// under docs/media/gpu/video_encoder_test_usage.md when making changes here.
// TODO(dstaessens): Add video_encoder_test_usage.md
constexpr const char* usage_msg =
    "usage: video_encode_accelerator_tests\n"
    "           [--codec=<codec>] [--num_temporal_layers=<number>]\n"
    "           [--disable_validator] [--output_bitstream]\n"
    "           [--output_images=(all|corrupt)] [--output_format=(png|yuv)]\n"
    "           [--output_folder=<filepath>] [--output_limit=<number>]\n"
    "           [-v=<level>] [--vmodule=<config>] [--gtest_help] [--help]\n"
    "           [<video path>] [<video metadata path>]\n";

// Video encoder tests help message.
constexpr const char* help_msg =
    "Run the video encoder accelerator tests on the video specified by\n"
    "<video path>. If no <video path> is given the default\n"
    "\"bear_320x192_40frames.yuv.webm\" video will be used.\n"
    "\nThe <video metadata path> should specify the location of a json file\n"
    "containing the video's metadata, such as frame checksums. By default\n"
    "<video path>.json will be used.\n"
    "\nThe following arguments are supported:\n"
    "  --codec               codec profile to encode, \"h264\" (baseline),\n"
    "                        \"h264main, \"h264high\", \"vp8\" and \"vp9\".\n"
    "                        H264 Baseline is selected if unspecified.\n"
    "  --num_temporal_layers the number of temporal layers of the encoded\n"
    "                        bitstream. Only used in --codec=vp9 currently.\n"
    "  --disable_validator   disable validation of encoded bitstream.\n"
    "  --output_bitstream    save the output bitstream in either H264 AnnexB\n"
    "                        format (for H264) or IVF format (for vp8 and\n"
    "                        vp9) to <output_folder>/<testname>.\n"
    "  --output_images       in addition to saving the full encoded,\n"
    "                        bitstream it's also possible to dump individual\n"
    "                        frames to <output_folder>/<testname>, possible\n"
    "                        values are \"all|corrupt\"\n"
    "  --output_format       set the format of images saved to disk,\n"
    "                        supported formats are \"png\" (default) and\n"
    "                        \"yuv\".\n"
    "  --output_limit        limit the number of images saved to disk.\n"
    "  --output_folder       set the basic folder used to store test\n"
    "                        artifacts. The default is the current directory.\n"
    "   -v                   enable verbose mode, e.g. -v=2.\n"
    "  --vmodule             enable verbose mode for the specified module,\n"
    "                        e.g. --vmodule=*media/gpu*=2.\n\n"
    "  --gtest_help          display the gtest help and exit.\n"
    "  --help                display this help and exit.\n";

// Default video to be used if no test video was specified.
constexpr base::FilePath::CharType kDefaultTestVideoPath[] =
    FILE_PATH_LITERAL("bear_320x192_40frames.yuv.webm");

// The number of frames to encode for bitrate check test cases.
// TODO(hiroh): Decrease this values to make the test faster.
constexpr size_t kNumFramesToEncodeForBitrateCheck = 300;
// Tolerance factor for how encoded bitrate can differ from requested bitrate.
constexpr double kBitrateTolerance = 0.1;

media::test::VideoEncoderTestEnvironment* g_env;

// Video encode test class. Performs setup and teardown for each single test.
class VideoEncoderTest : public ::testing::Test {
 public:
  VideoEncoderClientConfig GetDefaultConfig() {
    return VideoEncoderClientConfig(g_env->Video(), g_env->Profile(),
                                    g_env->NumTemporalLayers(),
                                    g_env->Bitrate());
  }

  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      Video* video,
      const VideoEncoderClientConfig& config) {
    LOG_ASSERT(video);

    auto video_encoder =
        VideoEncoder::Create(config, g_env->GetGpuMemoryBufferFactory(),
                             CreateBitstreamProcessors(video, config));
    LOG_ASSERT(video_encoder);

    if (!video_encoder->Initialize(video))
      ADD_FAILURE();

    return video_encoder;
  }

 private:
  std::vector<std::unique_ptr<BitstreamProcessor>> CreateBitstreamProcessors(
      Video* video,
      const VideoEncoderClientConfig& config) {
    std::vector<std::unique_ptr<BitstreamProcessor>> bitstream_processors;
    const gfx::Rect visible_rect(video->Resolution());
    const VideoCodec codec =
        VideoCodecProfileToVideoCodec(config.output_profile);
    if (g_env->SaveOutputBitstream()) {
      base::FilePath::StringPieceType extension =
          codec == VideoCodec::kCodecH264 ? FILE_PATH_LITERAL("h264")
                                          : FILE_PATH_LITERAL("ivf");
      auto output_bitstream_filepath =
          g_env->OutputFolder()
              .Append(g_env->GetTestOutputFilePath())
              .Append(video->FilePath().BaseName().ReplaceExtension(extension));
      auto bitstream_writer = BitstreamFileWriter::Create(
          output_bitstream_filepath, codec, visible_rect.size(),
          config.framerate, config.num_frames_to_encode);
      LOG_ASSERT(bitstream_writer);
      bitstream_processors.emplace_back(std::move(bitstream_writer));
    }

    if (!g_env->IsBitstreamValidatorEnabled()) {
      return bitstream_processors;
    }

    switch (codec) {
      case kCodecH264:
        bitstream_processors.emplace_back(
            new H264Validator(config.output_profile, visible_rect));
        break;
      case kCodecVP8:
        bitstream_processors.emplace_back(new VP8Validator(visible_rect));
        break;
      case kCodecVP9:
        bitstream_processors.emplace_back(new VP9Validator(
            config.output_profile, visible_rect, config.num_temporal_layers));
        break;
      default:
        LOG(ERROR) << "Unsupported profile: "
                   << GetProfileName(config.output_profile);
        break;
    }

    // Attach a bitstream validator to validate all encoded video frames. The
    // bitstream validator uses a software video decoder to validate the
    // encoded buffers by decoding them. Metrics such as the image's SSIM can
    // be calculated for additional quality checks.
    VideoDecoderConfig decoder_config(
        codec, config.output_profile, VideoDecoderConfig::AlphaMode::kIsOpaque,
        VideoColorSpace(), kNoTransformation, visible_rect.size(), visible_rect,
        visible_rect.size(), EmptyExtraData(), EncryptionScheme::kUnencrypted);
    std::vector<std::unique_ptr<VideoFrameProcessor>> video_frame_processors;
    raw_data_helper_ = RawDataHelper::Create(video);
    if (!raw_data_helper_) {
      LOG(ERROR) << "Failed to create raw data helper";
      return bitstream_processors;
    }

    VideoFrameValidator::GetModelFrameCB get_model_frame_cb =
        base::BindRepeating(&VideoEncoderTest::GetModelFrame,
                            base::Unretained(this));

    // Attach a video frame writer to store individual frames to disk if
    // requested.
    std::unique_ptr<VideoFrameProcessor> image_writer;
    auto frame_output_config = g_env->ImageOutputConfig();
    base::FilePath output_folder = base::FilePath(g_env->OutputFolder())
                                       .Append(g_env->GetTestOutputFilePath());
    if (frame_output_config.output_mode != FrameOutputMode::kNone) {
      image_writer = VideoFrameFileWriter::Create(
          output_folder, frame_output_config.output_format,
          frame_output_config.output_limit);
      LOG_ASSERT(image_writer);
      if (frame_output_config.output_mode == FrameOutputMode::kAll)
        video_frame_processors.push_back(std::move(image_writer));
    }
    auto ssim_validator = SSIMVideoFrameValidator::Create(
        get_model_frame_cb, std::move(image_writer),
        VideoFrameValidator::ValidationMode::kAverage);
    LOG_ASSERT(ssim_validator);
    video_frame_processors.push_back(std::move(ssim_validator));
    auto bitstream_validator = BitstreamValidator::Create(
        decoder_config, config.num_frames_to_encode - 1,
        std::move(video_frame_processors));
    LOG_ASSERT(bitstream_validator);
    bitstream_processors.emplace_back(std::move(bitstream_validator));
    return bitstream_processors;
  }

  scoped_refptr<const VideoFrame> GetModelFrame(size_t frame_index) {
    LOG_ASSERT(raw_data_helper_);
    return raw_data_helper_->GetFrame(frame_index %
                                      g_env->Video()->NumFrames());
  }

  std::unique_ptr<RawDataHelper> raw_data_helper_;
};

}  // namespace

// TODO(dstaessens): Add more test scenarios:
// - Forcing key frames

// Encode video from start to end. Wait for the kFlushDone event at the end of
// the stream, that notifies us all frames have been encoded.
TEST_F(VideoEncoderTest, FlushAtEndOfStream) {
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
  auto encoder = CreateVideoEncoder(g_env->Video(), GetDefaultConfig());

  EXPECT_EQ(encoder->GetEventCount(VideoEncoder::kInitialized), 1u);
}

// Create a video encoder and immediately destroy it without initializing. The
// video encoder will be automatically destroyed when the video encoder goes out
// of scope at the end of the test. The test will pass if no asserts or crashes
// are triggered upon destroying.
TEST_F(VideoEncoderTest, DestroyBeforeInitialize) {
  auto video_encoder = VideoEncoder::Create(GetDefaultConfig(),
                                            g_env->GetGpuMemoryBufferFactory());

  EXPECT_NE(video_encoder, nullptr);
}

// Encode video from start to end. Multiple buffer encodes will be queued in the
// encoder, without waiting for the result of the previous encode requests.
TEST_F(VideoEncoderTest, FlushAtEndOfStream_MultipleOutstandingEncodes) {
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
  // The minimal number of concurrent encoders we expect to be supported.
  constexpr size_t kMinSupportedConcurrentEncoders = 3;

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
  auto config = GetDefaultConfig();
  config.num_frames_to_encode = kNumFramesToEncodeForBitrateCheck;
  auto encoder = CreateVideoEncoder(g_env->Video(), config);

  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());

  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), config.num_frames_to_encode);
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
  EXPECT_NEAR(encoder->GetStats().Bitrate(), config.bitrate,
              kBitrateTolerance * config.bitrate);
}

TEST_F(VideoEncoderTest, DynamicBitrateChange) {
  auto config = GetDefaultConfig();
  config.num_frames_to_encode = kNumFramesToEncodeForBitrateCheck * 2;
  auto encoder = CreateVideoEncoder(g_env->Video(), config);

  // Encode the video with the first bitrate.
  const uint32_t first_bitrate = config.bitrate;
  encoder->EncodeUntil(VideoEncoder::kFrameReleased,
                       kNumFramesToEncodeForBitrateCheck);
  encoder->WaitForEvent(VideoEncoder::kFrameReleased,
                        kNumFramesToEncodeForBitrateCheck);
  EXPECT_NEAR(encoder->GetStats().Bitrate(), first_bitrate,
              kBitrateTolerance * first_bitrate);

  // Encode the video with the second bitrate.
  const uint32_t second_bitrate = first_bitrate * 3 / 2;
  encoder->ResetStats();
  encoder->UpdateBitrate(second_bitrate, config.framerate);
  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_NEAR(encoder->GetStats().Bitrate(), second_bitrate,
              kBitrateTolerance * second_bitrate);

  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), config.num_frames_to_encode);
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

TEST_F(VideoEncoderTest, DynamicFramerateChange) {
  auto config = GetDefaultConfig();
  config.num_frames_to_encode = kNumFramesToEncodeForBitrateCheck * 2;
  auto encoder = CreateVideoEncoder(g_env->Video(), config);

  // Encode the video with the first framerate.
  const uint32_t first_framerate = config.framerate;

  encoder->EncodeUntil(VideoEncoder::kFrameReleased,
                       kNumFramesToEncodeForBitrateCheck);
  encoder->WaitForEvent(VideoEncoder::kFrameReleased,
                        kNumFramesToEncodeForBitrateCheck);
  EXPECT_NEAR(encoder->GetStats().Bitrate(), config.bitrate,
              kBitrateTolerance * config.bitrate);

  // Encode the video with the second framerate.
  const uint32_t second_framerate = first_framerate * 3 / 2;
  encoder->ResetStats();
  encoder->UpdateBitrate(config.bitrate, second_framerate);
  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_NEAR(encoder->GetStats().Bitrate(), config.bitrate,
              kBitrateTolerance * config.bitrate);

  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), config.num_frames_to_encode);
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}

TEST_F(VideoEncoderTest, FlushAtEndOfStream_NV12Dmabuf) {
  auto nv12_video = g_env->Video()->ConvertToNV12();
  ASSERT_TRUE(nv12_video);

  VideoEncoderClientConfig config(nv12_video.get(), g_env->Profile(),
                                  g_env->NumTemporalLayers(), g_env->Bitrate());
  config.input_storage_type =
      VideoEncodeAccelerator::Config::StorageType::kDmabuf;

  auto encoder = CreateVideoEncoder(nv12_video.get(), config);

  encoder->Encode();
  EXPECT_TRUE(encoder->WaitForFlushDone());
  EXPECT_EQ(encoder->GetFlushDoneCount(), 1u);
  EXPECT_EQ(encoder->GetFrameReleasedCount(), nv12_video->NumFrames());
  EXPECT_TRUE(encoder->WaitForBitstreamProcessors());
}
}  // namespace test
}  // namespace media

int main(int argc, char** argv) {
  // Set the default test data path.
  media::test::Video::SetTestDataPath(media::GetTestDataPath());

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
  size_t num_temporal_layers = 1u;
  bool output_bitstream = false;
  media::test::FrameOutputConfig frame_output_config;
  base::FilePath output_folder =
      base::FilePath(base::FilePath::kCurrentDirectory);

  // Parse command line arguments.
  bool enable_bitstream_validator = true;
  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first.find("gtest_") == 0 ||               // Handled by GoogleTest
        it->first == "v" || it->first == "vmodule") {  // Handled by Chrome
      continue;
    }

    if (it->first == "codec") {
      codec = it->second;
    } else if (it->first == "num_temporal_layers") {
      if (!base::StringToSizeT(it->second, &num_temporal_layers)) {
        std::cout << "invalid number of temporal layers: " << it->second
                  << "\n";
        return EXIT_FAILURE;
      }
    } else if (it->first == "disable_validator") {
      enable_bitstream_validator = false;
    } else if (it->first == "output_bitstream") {
      output_bitstream = true;
    } else if (it->first == "output_images") {
      if (it->second == "all") {
        frame_output_config.output_mode = media::test::FrameOutputMode::kAll;
      } else if (it->second == "corrupt") {
        frame_output_config.output_mode =
            media::test::FrameOutputMode::kCorrupt;
      } else {
        std::cout << "unknown image output mode \"" << it->second
                  << "\", possible values are \"all|corrupt\"\n";
        return EXIT_FAILURE;
      }
    } else if (it->first == "output_format") {
      if (it->second == "png") {
        frame_output_config.output_format =
            media::test::VideoFrameFileWriter::OutputFormat::kPNG;
      } else if (it->second == "yuv") {
        frame_output_config.output_format =
            media::test::VideoFrameFileWriter::OutputFormat::kYUV;
      } else {
        std::cout << "unknown frame output format \"" << it->second
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

  testing::InitGoogleTest(&argc, argv);

  // Set up our test environment.
  media::test::VideoEncoderTestEnvironment* test_environment =
      media::test::VideoEncoderTestEnvironment::Create(
          video_path, video_metadata_path, enable_bitstream_validator,
          output_folder, codec, num_temporal_layers, output_bitstream,
          frame_output_config);

  if (!test_environment)
    return EXIT_FAILURE;

  media::test::g_env = static_cast<media::test::VideoEncoderTestEnvironment*>(
      testing::AddGlobalTestEnvironment(test_environment));

  return RUN_ALL_TESTS();
}
