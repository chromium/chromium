// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/test_data_util.h"
#include "media/gpu/test/video_frame_file_writer.h"
#include "media/gpu/test/video_frame_validator.h"
#include "media/gpu/test/video_player/frame_renderer_dummy.h"
#include "media/gpu/test/video_player/frame_renderer_thumbnail.h"
#include "media/gpu/test/video_player/video.h"
#include "media/gpu/test/video_player/video_decoder_client.h"
#include "media/gpu/test/video_player/video_player.h"
#include "media/gpu/test/video_player/video_player_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace test {

namespace {

// Video decoder tests usage message. Make sure to also update the documentation
// under docs/media/gpu/video_decoder_test_usage.md when making changes here.
constexpr const char* usage_msg =
    "usage: video_decode_accelerator_tests\n"
    "           [-v=<level>] [--vmodule=<config>] [--disable_validator]\n"
    "           [--output_frames=(all|corrupt)] [--output_format=(png|yuv)]\n"
    "           [--output_limit=<number>] [--output_folder=<folder>]\n"
    "           [--use_vd] [--gtest_help] [--help]\n"
    "           [<video path>] [<video metadata path>]\n";

// Video decoder tests help message.
constexpr const char* help_msg =
    "Run the video decode accelerator tests on the video specified by\n"
    "<video path>. If no <video path> is given the default\n"
    "\"test-25fps.h264\" video will be used.\n"
    "\nThe <video metadata path> should specify the location of a json file\n"
    "containing the video's metadata, such as frame checksums. By default\n"
    "<video path>.json will be used.\n"
    "\nThe following arguments are supported:\n"
    "   -v                  enable verbose mode, e.g. -v=2.\n"
    "  --vmodule            enable verbose mode for the specified module,\n"
    "                       e.g. --vmodule=*media/gpu*=2.\n\n"
    "  --disable_validator  disable frame validation.\n"
    "  --use_vd             use the new VD-based video decoders, instead of\n"
    "                       the default VDA-based video decoders.\n\n"
    "  --output_frames      write the selected video frames to disk, possible\n"
    "                       values are \"all|corrupt\".\n"
    "  --output_format      set the format of frames saved to disk, supported\n"
    "                       formats are \"png\" (default) and \"yuv\".\n"
    "  --output_limit       limit the number of frames saved to disk.\n"
    "  --output_folder      set the folder used to store frames, defaults to\n"
    "                       \"<testname>\".\n\n"
    "  --gtest_help         display the gtest help and exit.\n"
    "  --help               display this help and exit.\n";

media::test::VideoPlayerTestEnvironment* g_env;

// Video decode test class. Performs setup and teardown for each single test.
class VideoDecoderTest : public ::testing::Test {
 public:
  std::unique_ptr<VideoPlayer> CreateVideoPlayer(
      const Video* video,
      VideoDecoderClientConfig config = VideoDecoderClientConfig(),
      std::unique_ptr<FrameRenderer> frame_renderer =
          FrameRendererDummy::Create()) {
    LOG_ASSERT(video);
    std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors;

    // Force allocate mode if import mode is not supported.
    if (!g_env->ImportSupported())
      config.allocation_mode = AllocationMode::kAllocate;

    base::FilePath output_folder = base::FilePath(g_env->OutputFolder())
                                       .Append(g_env->GetTestOutputFilePath());

    // Write all video frames to the '<testname>' folder if the frame output
    // mode is 'all'. Only supported if import mode is supported and enabled.
    if (g_env->GetFrameOutputMode() == FrameOutputMode::kAll &&
        config.allocation_mode == AllocationMode::kImport) {
      frame_processors.push_back(VideoFrameFileWriter::Create(
          output_folder, g_env->GetFrameOutputFormat(),
          g_env->GetFrameOutputLimit()));
      VLOG(0) << "Writing video frames to: " << output_folder;
    }

    // Use the video frame validator to validate decoded video frames if
    // enabled. If the frame output mode is 'corrupt', a frame writer will be
    // attached to forward corrupted frames to. Only supported if import mode
    // is supported and enabled.
    if (g_env->IsValidatorEnabled() &&
        config.allocation_mode == AllocationMode::kImport) {
      std::unique_ptr<VideoFrameFileWriter> frame_writer;
      if (g_env->GetFrameOutputMode() == FrameOutputMode::kCorrupt) {
        frame_writer = VideoFrameFileWriter::Create(
            output_folder, g_env->GetFrameOutputFormat(),
            g_env->GetFrameOutputLimit());
      }
      frame_processors.push_back(media::test::VideoFrameValidator::Create(
          video->FrameChecksums(), PIXEL_FORMAT_I420, std::move(frame_writer)));
    }

    // Use the new VD-based video decoders if requested.
    config.use_vd = g_env->UseVD();

    auto video_player = VideoPlayer::Create(
        config, g_env->GetGpuMemoryBufferFactory(), std::move(frame_renderer),
        std::move(frame_processors));
    LOG_ASSERT(video_player);
    LOG_ASSERT(video_player->Initialize(video));

    // Increase event timeout when outputting video frames.
    if (g_env->GetFrameOutputMode() != FrameOutputMode::kNone) {
      video_player->SetEventWaitTimeout(std::max(
          kDefaultEventWaitTimeout, g_env->Video()->GetDuration() * 10));
    }
    return video_player;
  }
};

}  // namespace

// Play video from start to end. Wait for the kFlushDone event at the end of the
// stream, that notifies us all frames have been decoded.
TEST_F(VideoDecoderTest, FlushAtEndOfStream) {
  auto tvp = CreateVideoPlayer(g_env->Video());

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Flush the decoder immediately after initialization.
TEST_F(VideoDecoderTest, FlushAfterInitialize) {
  auto tvp = CreateVideoPlayer(g_env->Video());

  tvp->Flush();
  EXPECT_TRUE(tvp->WaitForFlushDone());
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetFlushDoneCount(), 2u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Reset the decoder immediately after initialization.
TEST_F(VideoDecoderTest, ResetAfterInitialize) {
  auto tvp = CreateVideoPlayer(g_env->Video());

  tvp->Reset();
  EXPECT_TRUE(tvp->WaitForResetDone());
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetResetDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Reset the decoder when the middle of the stream is reached.
TEST_F(VideoDecoderTest, ResetMidStream) {
  auto tvp = CreateVideoPlayer(g_env->Video());

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFrameDecoded(g_env->Video()->NumFrames() / 2));
  tvp->Reset();
  EXPECT_TRUE(tvp->WaitForResetDone());
  size_t numFramesDecoded = tvp->GetFrameDecodedCount();
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetResetDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(),
            numFramesDecoded + g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Reset the decoder when the end of the stream is reached.
TEST_F(VideoDecoderTest, ResetEndOfStream) {
  auto tvp = CreateVideoPlayer(g_env->Video());

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  tvp->Reset();
  EXPECT_TRUE(tvp->WaitForResetDone());
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetResetDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFlushDoneCount(), 2u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames() * 2);
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Reset the decoder immediately when the end-of-stream flush starts, without
// waiting for a kFlushDone event.
TEST_F(VideoDecoderTest, ResetBeforeFlushDone) {
  auto tvp = CreateVideoPlayer(g_env->Video());

  // Reset when a kFlushing event is received.
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());
  tvp->Reset();
  EXPECT_TRUE(tvp->WaitForResetDone());

  // Reset will cause the decoder to drop everything it's doing, including the
  // ongoing flush operation. However the flush might have been completed
  // already by the time reset is called. So depending on the timing of the
  // calls we should see 0 or 1 flushes, and the last few video frames might
  // have been dropped.
  EXPECT_LE(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetResetDoneCount(), 1u);
  EXPECT_LE(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Reset the decoder immediately when encountering the first config info in a
// H.264 video stream. After resetting the video is played until the end.
TEST_F(VideoDecoderTest, ResetAfterFirstConfigInfo) {
  // This test is only relevant for H.264 video streams.
  if (g_env->Video()->Profile() < H264PROFILE_MIN ||
      g_env->Video()->Profile() > H264PROFILE_MAX)
    GTEST_SKIP();

  auto tvp = CreateVideoPlayer(g_env->Video());

  tvp->PlayUntil(VideoPlayerEvent::kConfigInfo);
  EXPECT_TRUE(tvp->WaitForEvent(VideoPlayerEvent::kConfigInfo));
  tvp->Reset();
  EXPECT_TRUE(tvp->WaitForResetDone());
  size_t numFramesDecoded = tvp->GetFrameDecodedCount();
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetResetDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(),
            numFramesDecoded + g_env->Video()->NumFrames());
  EXPECT_GE(tvp->GetEventCount(VideoPlayerEvent::kConfigInfo), 1u);
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Play video from start to end. Multiple buffer decodes will be queued in the
// decoder, without waiting for the result of the previous decode requests.
TEST_F(VideoDecoderTest, FlushAtEndOfStream_MultipleOutstandingDecodes) {
  VideoDecoderClientConfig config;
  config.max_outstanding_decode_requests = 4;
  auto tvp = CreateVideoPlayer(g_env->Video(), config);

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Play multiple videos simultaneously from start to finish.
TEST_F(VideoDecoderTest, FlushAtEndOfStream_MultipleConcurrentDecodes) {
  // The minimal number of concurrent decoders we expect to be supported.
  constexpr size_t kMinSupportedConcurrentDecoders = 3;

  std::vector<std::unique_ptr<VideoPlayer>> tvps(
      kMinSupportedConcurrentDecoders);
  for (size_t i = 0; i < kMinSupportedConcurrentDecoders; ++i)
    tvps[i] = CreateVideoPlayer(g_env->Video());

  for (size_t i = 0; i < kMinSupportedConcurrentDecoders; ++i)
    tvps[i]->Play();

  for (size_t i = 0; i < kMinSupportedConcurrentDecoders; ++i) {
    EXPECT_TRUE(tvps[i]->WaitForFlushDone());
    EXPECT_EQ(tvps[i]->GetFlushDoneCount(), 1u);
    EXPECT_EQ(tvps[i]->GetFrameDecodedCount(), g_env->Video()->NumFrames());
    EXPECT_TRUE(tvps[i]->WaitForFrameProcessors());
  }
}

// Play a video from start to finish. Thumbnails of the decoded frames will be
// rendered into a image, whose checksum is compared to a golden value. This
// test is only run on older platforms that don't support the video frame
// validator, which requires import mode. If no thumbnail checksums are present
// in the video metadata the test will be skipped. This test will be deprecated
// once all devices support import mode.
TEST_F(VideoDecoderTest, FlushAtEndOfStream_RenderThumbnails) {
  if (!g_env->IsValidatorEnabled() || g_env->ImportSupported() ||
      g_env->Video()->ThumbnailChecksums().empty()) {
    GTEST_SKIP();
  }

  base::FilePath output_folder = base::FilePath(g_env->OutputFolder())
                                     .Append(g_env->GetTestOutputFilePath());
  VideoDecoderClientConfig config;
  config.allocation_mode = AllocationMode::kAllocate;
  auto tvp = CreateVideoPlayer(
      g_env->Video(), config,
      FrameRendererThumbnail::Create(g_env->Video()->ThumbnailChecksums(),
                                     output_folder));

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
  EXPECT_TRUE(static_cast<FrameRendererThumbnail*>(tvp->GetFrameRenderer())
                  ->ValidateThumbnail());
}

// Play a video from start to finish, using allocate mode. This test is only run
// on platforms that support import mode, as on allocate-mode only platforms all
// tests are run in allocate mode. The test will be skipped when --use_vd is
// specified as the new video decoders only support import mode.
// TODO(dstaessens): Deprecate after switching to new VD-based video decoders.
TEST_F(VideoDecoderTest, FlushAtEndOfStream_Allocate) {
  if (!g_env->ImportSupported() || g_env->UseVD())
    GTEST_SKIP();

  VideoDecoderClientConfig config;
  config.allocation_mode = AllocationMode::kAllocate;
  auto tvp = CreateVideoPlayer(g_env->Video(), config);

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Test initializing the video decoder for the specified video. Initialization
// will be successful if the video decoder is capable of decoding the test
// video's configuration (e.g. codec and resolution). The test only verifies
// initialization and doesn't decode the video.
TEST_F(VideoDecoderTest, Initialize) {
  auto tvp = CreateVideoPlayer(g_env->Video());
  EXPECT_EQ(tvp->GetEventCount(VideoPlayerEvent::kInitialized), 1u);
}

// Test video decoder re-initialization. Re-initialization is only supported by
// the media::VideoDecoder interface, so the test will be skipped if --use_vd
// is not specified.
TEST_F(VideoDecoderTest, Reinitialize) {
  if (!g_env->UseVD())
    GTEST_SKIP();

  // Create and initialize the video decoder.
  auto tvp = CreateVideoPlayer(g_env->Video());
  EXPECT_EQ(tvp->GetEventCount(VideoPlayerEvent::kInitialized), 1u);

  // Re-initialize the video decoder, without having played the video.
  EXPECT_TRUE(tvp->Initialize(g_env->Video()));
  EXPECT_EQ(tvp->GetEventCount(VideoPlayerEvent::kInitialized), 2u);

  // Play the video from start to end.
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());
  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());

  // Try re-initializing the video decoder again.
  EXPECT_TRUE(tvp->Initialize(g_env->Video()));
  EXPECT_EQ(tvp->GetEventCount(VideoPlayerEvent::kInitialized), 3u);
}

// Create a video decoder and immediately destroy it without initializing. The
// video decoder will be automatically destroyed when the video player goes out
// of scope at the end of the test. The test will pass if no asserts or crashes
// are triggered upon destroying.
TEST_F(VideoDecoderTest, DestroyBeforeInitialize) {
  VideoDecoderClientConfig config = VideoDecoderClientConfig();
  config.use_vd = g_env->UseVD();
  auto tvp = VideoPlayer::Create(config, g_env->GetGpuMemoryBufferFactory(),
                                 FrameRendererDummy::Create());
  EXPECT_NE(tvp, nullptr);
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
      (args.size() >= 1) ? base::FilePath(args[0]) : base::FilePath();
  base::FilePath video_metadata_path =
      (args.size() >= 2) ? base::FilePath(args[1]) : base::FilePath();

  // Parse command line arguments.
  bool enable_validator = true;
  media::test::FrameOutputConfig frame_output_config;
  base::FilePath::StringType output_folder = base::FilePath::kCurrentDirectory;
  bool use_vd = false;
  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first.find("gtest_") == 0 ||               // Handled by GoogleTest
        it->first == "v" || it->first == "vmodule") {  // Handled by Chrome
      continue;
    }

    if (it->first == "disable_validator") {
      enable_validator = false;
    } else if (it->first == "output_frames") {
      if (it->second == "all") {
        frame_output_config.output_mode = media::test::FrameOutputMode::kAll;
      } else if (it->second == "corrupt") {
        frame_output_config.output_mode =
            media::test::FrameOutputMode::kCorrupt;
      } else {
        std::cout << "unknown frame output mode \"" << it->second
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
      output_folder = it->second;
    } else if (it->first == "use_vd") {
      use_vd = true;
    } else {
      std::cout << "unknown option: --" << it->first << "\n"
                << media::test::usage_msg;
      return EXIT_FAILURE;
    }
  }

  testing::InitGoogleTest(&argc, argv);

  // Set up our test environment.
  media::test::VideoPlayerTestEnvironment* test_environment =
      media::test::VideoPlayerTestEnvironment::Create(
          video_path, video_metadata_path, enable_validator, use_vd,
          base::FilePath(output_folder), frame_output_config);
  if (!test_environment)
    return EXIT_FAILURE;

  media::test::g_env = static_cast<media::test::VideoPlayerTestEnvironment*>(
      testing::AddGlobalTestEnvironment(test_environment));

  return RUN_ALL_TESTS();
}
