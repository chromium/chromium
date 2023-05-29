// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_switches.h"
#include "media/base/media_util.h"
#include "media/base/test_data_util.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_transformation.h"
#include "media/filters/dav1d_video_decoder.h"
#include "media/gpu/test/video_bitstream.h"
#include "media/gpu/test/video_frame_file_writer.h"
#include "media/gpu/test/video_frame_validator.h"
#include "media/gpu/test/video_player/decoder_listener.h"
#include "media/gpu/test/video_player/decoder_wrapper.h"
#include "media/gpu/test/video_player/frame_renderer_dummy.h"
#include "media/gpu/test/video_player/video_player_test_environment.h"
#include "media/gpu/test/video_test_helpers.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

namespace media {
namespace test {

namespace {

// Video decoder tests usage message. Make sure to also update the documentation
// under docs/media/gpu/video_decoder_test_usage.md when making changes here.
constexpr const char* usage_msg =
    R"(usage: video_decode_accelerator_tests
           [-v=<level>] [--vmodule=<config>]
           [--validator_type=(none|md5|ssim)]
           [--output_frames=(all|corrupt)] [--output_format=(png|yuv)]
           [--output_limit=<number>] [--output_folder=<folder>]
           [--linear_output] ([--use-legacy]|[--use_vd_vda])
           [--use-gl=<backend>] [--ozone-platform=<platform>]
           [--disable_vaapi_lock]
           [--gtest_help] [--help]
           [<video path>] [<video metadata path>]
)";

// Video decoder tests help message.
const std::string help_msg =
    std::string(
        R"""(Run the video decode accelerator tests on the video specified by
<video path>. If no <video path> is given the default
"test-25fps.h264" video will be used.

The <video metadata path> should specify the location of a json file
containing the video's metadata, such as frame checksums. By default
<video path>.json will be used.

The following arguments are supported:
   -v                   enable verbose mode, e.g. -v=2.
  --vmodule             enable verbose mode for the specified module,
                        e.g. --vmodule=*media/gpu*=2.

  --validator_type      validate decoded frames, possible values are
                        md5 (default, compare against md5hash of expected
                        frames), ssim (compute SSIM against expected
                        frames, currently allowed for AV1 streams only)
                        and none (disable frame validation).
  --use-legacy          use the legacy VDA-based video decoders.
  --use_vd_vda          use the new VD-based video decoders with a
                        wrapper that translates to the VDA interface,
                        used to test interaction with older components
  --linear_output       use linear buffers as the final output of the
                        decoder which may require the use of an image
                        processor internally. This flag only works in
                        conjunction with --use_vd_vda.
                        Disabled by default.
  --output_frames       write the selected video frames to disk, possible
                        values are "all|corrupt".
  --output_format       set the format of frames saved to disk, supported
                        formats are "png" (default) and "yuv".
  --output_limit        limit the number of frames saved to disk.
  --output_folder       set the folder used to store frames, defaults to
                        "<testname>".
  --use-gl              specify which GPU backend to use, possible values
                        include desktop (GLX), egl (GLES w/ ANGLE), and
                        swiftshader (software rendering)
  --ozone-platform      specify which Ozone platform to use, possible values
                        depend on build configuration but normally include
                        x11, drm, wayland, and headless
  --disable_vaapi_lock  disable the global VA-API lock if applicable,
                        i.e., only on devices that use the VA-API with a libva
                        backend that's known to be thread-safe and only in
                        portions of the Chrome stack that should be able to
                        deal with the absence of the lock
                        (not the VaapiVideoDecodeAccelerator).)""") +
#if defined(ARCH_CPU_ARM_FAMILY)
    R"""(
  --disable-libyuv      use hw format conversion instead of libYUV.
                        libYUV will be used by default, unless the
                        video decoder format is not supported;
                        in that case the code will try to use the
                        v4l2 image processor.)""" +
#endif  // defined(ARCH_CPU_ARM_FAMILY)
    R"""(
  --gtest_help          display the gtest help and exit.
  --help                display this help and exit.
)""";

media::test::VideoPlayerTestEnvironment* g_env;

// Video decode test class. Performs setup and teardown for each single test.
class VideoDecoderTest : public ::testing::Test {
 public:
  std::unique_ptr<DecoderListener> CreateDecoderListener(
      const VideoBitstream* video,
      DecoderWrapperConfig config = DecoderWrapperConfig(),
      std::unique_ptr<FrameRendererDummy> frame_renderer =
          FrameRendererDummy::Create()) {
    LOG_ASSERT(video);
    std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors;

    base::FilePath output_folder = base::FilePath(g_env->OutputFolder())
                                       .Append(g_env->GetTestOutputFilePath());

    // Write all video frames to the '<testname>' folder if the frame output
    // mode is 'all'.
    if (g_env->GetFrameOutputMode() == FrameOutputMode::kAll) {
      frame_processors.push_back(VideoFrameFileWriter::Create(
          output_folder, g_env->GetFrameOutputFormat(),
          g_env->GetFrameOutputLimit()));
      VLOG(0) << "Writing video frames to: " << output_folder;
    }

    // Use the video frame validator to validate decoded video frames if
    // enabled. If the frame output mode is 'corrupt', a frame writer will be
    // attached to forward corrupted frames to.
    if (g_env->IsValidatorEnabled()) {
      std::unique_ptr<VideoFrameFileWriter> frame_writer;
      if (g_env->GetFrameOutputMode() == FrameOutputMode::kCorrupt) {
        frame_writer = VideoFrameFileWriter::Create(
            output_folder, g_env->GetFrameOutputFormat(),
            g_env->GetFrameOutputLimit());
      }
      if (g_env->Video()->BitDepth() != 8u &&
          g_env->Video()->BitDepth() != 10u) {
        LOG(ERROR) << "Unsupported bit depth: "
                   << base::strict_cast<int>(g_env->Video()->BitDepth());
        ADD_FAILURE();
      }
      const VideoPixelFormat validation_format =
          g_env->Video()->BitDepth() == 10 ? PIXEL_FORMAT_YUV420P10
                                           : PIXEL_FORMAT_I420;
      if (g_env->GetValidatorType() ==
          VideoPlayerTestEnvironment::ValidatorType::kMD5) {
        frame_processors.push_back(media::test::MD5VideoFrameValidator::Create(
            video->FrameChecksums(), validation_format,
            std::move(frame_writer)));
      } else {
        DCHECK_EQ(g_env->GetValidatorType(),
                  VideoPlayerTestEnvironment::ValidatorType::kSSIM);
        if (!CreateModelFrames(g_env->Video())) {
          LOG(ERROR) << "Failed creating model frames";
          ADD_FAILURE();
        }
        constexpr double kSSIMTolerance = 0.915;
        frame_processors.push_back(media::test::SSIMVideoFrameValidator::Create(
            base::BindRepeating(&VideoDecoderTest::GetModelFrame,
                                base::Unretained(this)),
            std::move(frame_writer),
            VideoFrameValidator::ValidationMode::kThreshold, kSSIMTolerance));
      }
    }

    base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
    command_line.AppendSwitchASCII(
        switches::kHardwareVideoDecodeFrameRate,
        base::NumberToString(g_env->Video()->FrameRate()));

    config.implementation = g_env->GetDecoderImplementation();
    config.linear_output = g_env->ShouldOutputLinearBuffers();
#if BUILDFLAG(USE_VAAPI)
    // VP9 verification "frm_resize" and "sub8x8_sf" vectors utilize
    // keyframeless resolution changes, see:
    // https://www.webmproject.org/vp9/levels/#test-descriptions.
    config.ignore_resolution_changes_to_smaller_vp9 =
        g_env->Video()->HasKeyFrameLessResolutionChange();
#endif

    auto video_player = DecoderListener::Create(
        config, std::move(frame_renderer), std::move(frame_processors));
    LOG_ASSERT(video_player);
    LOG_ASSERT(video_player->Initialize(video));

    // Increase event timeout when outputting video frames.
    if (g_env->GetFrameOutputMode() != FrameOutputMode::kNone) {
      video_player->SetEventWaitTimeout(
          std::max(kDefaultEventWaitTimeout, g_env->Video()->Duration() * 10));
    }
    return video_player;
  }

 private:
  // TODO(hiroh): Move this to Video class or video_frame_helpers.h.
  // TODO(hiroh): Create model frames once during the test.
  bool CreateModelFrames(const VideoBitstream* video) {
    if (video->Codec() != VideoCodec::kAV1) {
      LOG(ERROR) << "Frame validation by SSIM is allowed for AV1 streams only";
      return false;
    }

    Dav1dVideoDecoder decoder(
        /*media_log=*/nullptr,
        OffloadableVideoDecoder::OffloadState::kOffloaded);
    VideoDecoderConfig decoder_config(
        video->Codec(), video->Profile(),
        VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace(),
        kNoTransformation, video->Resolution(), gfx::Rect(video->Resolution()),
        video->Resolution(), EmptyExtraData(), EncryptionScheme::kUnencrypted);

    bool init_success = false;
    VideoDecoder::InitCB init_cb = base::BindOnce(
        [](bool* init_success, DecoderStatus result) {
          *init_success = result.is_ok();
        },
        &init_success);
    decoder.Initialize(decoder_config, /*low_delay=*/false,
                       /*cdm_context=*/nullptr, std::move(init_cb),
                       base::BindRepeating(&VideoDecoderTest::AddModelFrame,
                                           base::Unretained(this)),
                       /*waiting_cb=*/base::NullCallback());
    if (!init_success)
      return false;
    auto encoded_data_helper =
        std::make_unique<EncodedDataHelper>(video->Data(), video->Codec());
    DCHECK(encoded_data_helper);
    while (!encoded_data_helper->ReachEndOfStream()) {
      bool decode_success = false;
      media::VideoDecoder::DecodeCB decode_cb = base::BindOnce(
          [](bool* decode_success, DecoderStatus status) {
            *decode_success = status.is_ok();
          },
          &decode_success);
      scoped_refptr<DecoderBuffer> bitstream_buffer =
          encoded_data_helper->GetNextBuffer();
      if (!bitstream_buffer) {
        LOG(ERROR) << "Failed to get next video stream data";
        return false;
      }
      decoder.Decode(std::move(bitstream_buffer), std::move(decode_cb));
      if (!decode_success)
        return false;
    }
    bool flush_success = false;
    media::VideoDecoder::DecodeCB flush_cb = base::BindOnce(
        [](bool* flush_success, DecoderStatus status) {
          *flush_success = status.is_ok();
        },
        &flush_success);
    decoder.Decode(DecoderBuffer::CreateEOSBuffer(), std::move(flush_cb));

    return flush_success && model_frames_.size() == video->NumFrames();
  }

  void AddModelFrame(scoped_refptr<VideoFrame> frame) {
    model_frames_.push_back(std::move(frame));
  }

  scoped_refptr<const VideoFrame> GetModelFrame(size_t frame_index) {
    CHECK_LT(frame_index, model_frames_.size());
    return model_frames_[frame_index];
  }
  std::vector<scoped_refptr<VideoFrame>> model_frames_;
};

}  // namespace

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
TEST_F(VideoDecoderTest, GetSupportedConfigs) {
  if (g_env->GetDecoderImplementation() != DecoderImplementation::kVD) {
    GTEST_SKIP() << "Re-initialization is only supported by the "
                    "media::VideoDecoder interface;";
  }
  const media::VideoDecoderType decoder_type =
#if BUILDFLAG(USE_VAAPI)
      media::VideoDecoderType::kVaapi;
#elif BUILDFLAG(USE_V4L2_CODEC)
      media::VideoDecoderType::kV4L2;
#else
      media::VideoDecoderType::kUnknown;
#endif
  const auto supported_configs = VideoDecoderPipeline::GetSupportedConfigs(
      decoder_type, gpu::GpuDriverBugWorkarounds());
  ASSERT_FALSE(supported_configs->empty());

  const bool contains_h264 =
      std::find_if(supported_configs->begin(), supported_configs->end(),
                   [](SupportedVideoDecoderConfig config) {
                     return config.profile_min >= H264PROFILE_MIN &&
                            config.profile_max <= H264PROFILE_MAX;
                   }) != supported_configs->end();
  // Every hardware video decoder in ChromeOS supports some kind of H.264.
  EXPECT_TRUE(contains_h264);
}
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

// Test initializing the video decoder for the specified video. Initialization
// will be successful if the video decoder is capable of decoding the test
// video's configuration (e.g. codec and resolution). The test only verifies
// initialization and doesn't decode the video.
TEST_F(VideoDecoderTest, Initialize) {
  auto tvp = CreateDecoderListener(g_env->Video());
  EXPECT_EQ(tvp->GetEventCount(DecoderListener::Event::kInitialized), 1u);
}

// Test video decoder simple re-initialization: Initialize, then without
// playing, re-initialize.
TEST_F(VideoDecoderTest, Reinitialize) {
  if (g_env->GetDecoderImplementation() != DecoderImplementation::kVD) {
    GTEST_SKIP() << "Re-initialization is only supported by the "
                    "media::VideoDecoder interface;";
  }
  // Create and initialize the video decoder.
  auto tvp = CreateDecoderListener(g_env->Video());

  EXPECT_EQ(tvp->GetEventCount(DecoderListener::Event::kInitialized), 1u);

  // Re-initialize the video decoder, without having played the video.
  EXPECT_TRUE(tvp->Initialize(g_env->Video()));
  EXPECT_EQ(tvp->GetEventCount(DecoderListener::Event::kInitialized), 2u);
}

// Test video decoder simple re-initialization, then re-initialization after a
// successful play.
TEST_F(VideoDecoderTest, ReinitializeThenPlayThenInitialize) {
  if (g_env->GetDecoderImplementation() != DecoderImplementation::kVD) {
    GTEST_SKIP() << "Re-initialization is only supported by the "
                    "media::VideoDecoder interface;";
  }

  // Create and initialize the video decoder.
  auto tvp = CreateDecoderListener(g_env->Video());
  EXPECT_EQ(tvp->GetEventCount(DecoderListener::Event::kInitialized), 1u);

  // Re-initialize the video decoder, without having played the video.
  EXPECT_TRUE(tvp->Initialize(g_env->Video()));
  EXPECT_EQ(tvp->GetEventCount(DecoderListener::Event::kInitialized), 2u);

  // Play the video from start to end.
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());
  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());

  // Try re-initializing the video decoder again.
  EXPECT_TRUE(tvp->Initialize(g_env->Video()));
  EXPECT_EQ(tvp->GetEventCount(DecoderListener::Event::kInitialized), 3u);
}

// Create a video decoder and immediately destroy it without initializing. The
// video decoder will be automatically destroyed when the video player goes out
// of scope at the end of the test. The test will pass if no asserts or crashes
// are triggered upon destroying.
TEST_F(VideoDecoderTest, DestroyBeforeInitialize) {
  DecoderWrapperConfig config = DecoderWrapperConfig();
  config.implementation = g_env->GetDecoderImplementation();
  auto tvp = DecoderListener::Create(config, FrameRendererDummy::Create());
  EXPECT_NE(tvp, nullptr);
}

// Play video from start to end. Wait for the kFlushDone event at the end of the
// stream, that notifies us all frames have been decoded.
TEST_F(VideoDecoderTest, FlushAtEndOfStream) {
  auto tvp = CreateDecoderListener(g_env->Video());

  // This test case is used for video.ChromeStackDecoderVerification.
  // Mapping is very slow on some intel devices and hit the default timeout
  // in long 4k video verification. Increase the timeout more than 1080p video
  // to mitigate the issue. See b/230378122 for the discussion.
  // 180 seconds are selected as it is long enough to pass the existing tests.
  constexpr gfx::Size k1080p(1920, 1080);
  if (g_env->Video()->Resolution().GetArea() > k1080p.GetArea()) {
    tvp->SetEventWaitTimeout(base::Seconds(180));
  }

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Flush the decoder immediately after initialization.
TEST_F(VideoDecoderTest, FlushAfterInitialize) {
  auto tvp = CreateDecoderListener(g_env->Video());

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
  auto tvp = CreateDecoderListener(g_env->Video());

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
  auto tvp = CreateDecoderListener(g_env->Video());

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFrameDecoded(g_env->Video()->NumFrames() / 2));
  tvp->Reset();
  EXPECT_TRUE(tvp->WaitForResetDone());
  size_t numFramesDecoded = tvp->GetFrameDecodedCount();
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetResetDoneCount(), 1u);
  // In the case of a very short clip the decoder may be able
  // to decode all the frames before a reset is sent.
  // A flush occurs after the last frame, so in this situation
  // there will be 2 flushes that occur.
  EXPECT_TRUE(tvp->GetFlushDoneCount() == 1u || tvp->GetFlushDoneCount() == 2u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(),
            numFramesDecoded + g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Reset the decoder when the end of the stream is reached.
TEST_F(VideoDecoderTest, ResetEndOfStream) {
  auto tvp = CreateDecoderListener(g_env->Video());

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  tvp->Reset();
  EXPECT_TRUE(tvp->WaitForResetDone());
  tvp->Play();
  ASSERT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetResetDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFlushDoneCount(), 2u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames() * 2);
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Reset the decoder immediately when the end-of-stream flush starts, without
// waiting for a kFlushDone event.
TEST_F(VideoDecoderTest, ResetBeforeFlushDone) {
  auto tvp = CreateDecoderListener(g_env->Video());

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
// H.264/HEVC video stream. After resetting the video is played until the end.
TEST_F(VideoDecoderTest, ResetAfterFirstConfigInfo) {
  // This test is only relevant for H.264/HEVC video streams.
  if (g_env->Video()->Codec() != media::VideoCodec::kH264 &&
      g_env->Video()->Codec() != media::VideoCodec::kHEVC)
    GTEST_SKIP();

  auto tvp = CreateDecoderListener(g_env->Video());

  tvp->PlayUntil(DecoderListener::Event::kConfigInfo);
  EXPECT_TRUE(tvp->WaitForEvent(DecoderListener::Event::kConfigInfo));
  tvp->Reset();
  EXPECT_TRUE(tvp->WaitForResetDone());
  size_t numFramesDecoded = tvp->GetFrameDecodedCount();
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetResetDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(),
            numFramesDecoded + g_env->Video()->NumFrames());
  EXPECT_GE(tvp->GetEventCount(DecoderListener::Event::kConfigInfo), 1u);
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

TEST_F(VideoDecoderTest, ResolutionChangeAbortedByReset) {
  if (g_env->GetDecoderImplementation() != DecoderImplementation::kVDVDA)
    GTEST_SKIP();

  auto tvp = CreateDecoderListener(g_env->Video());

  tvp->PlayUntil(DecoderListener::Event::kNewBuffersRequested);
  EXPECT_TRUE(tvp->WaitForEvent(DecoderListener::Event::kNewBuffersRequested));

  // TODO(b/192523692): Add a new test case that continues passing input buffers
  // between the resolution change has been aborted and resetting the decoder.

  tvp->Reset();
  EXPECT_TRUE(tvp->WaitForResetDone());

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetResetDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Play video from start to end. Multiple buffer decodes will be queued in the
// decoder, without waiting for the result of the previous decode requests.
TEST_F(VideoDecoderTest, FlushAtEndOfStream_MultipleOutstandingDecodes) {
  DecoderWrapperConfig config;
  config.max_outstanding_decode_requests = 4;
  auto tvp = CreateDecoderListener(g_env->Video(), config);

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

  std::vector<std::unique_ptr<DecoderListener>> tvps(
      kMinSupportedConcurrentDecoders);
  for (size_t i = 0; i < kMinSupportedConcurrentDecoders; ++i)
    tvps[i] = CreateDecoderListener(g_env->Video());

  for (size_t i = 0; i < kMinSupportedConcurrentDecoders; ++i)
    tvps[i]->Play();

  for (size_t i = 0; i < kMinSupportedConcurrentDecoders; ++i) {
    EXPECT_TRUE(tvps[i]->WaitForFlushDone());
    EXPECT_EQ(tvps[i]->GetFlushDoneCount(), 1u);
    EXPECT_EQ(tvps[i]->GetFrameDecodedCount(), g_env->Video()->NumFrames());
    EXPECT_TRUE(tvps[i]->WaitForFrameProcessors());
  }
}

}  // namespace test
}  // namespace media

int main(int argc, char** argv) {
  // Set the default test data path.
  media::test::VideoBitstream::SetTestDataPath(media::GetTestDataPath());

  // Print the help message if requested. This needs to be done before
  // initializing gtest, to overwrite the default gtest help message.
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
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
  auto validator_type =
      media::test::VideoPlayerTestEnvironment::ValidatorType::kMD5;
  media::test::FrameOutputConfig frame_output_config;
  base::FilePath::StringType output_folder = base::FilePath::kCurrentDirectory;
  bool use_legacy = false;
  bool use_vd_vda = false;
  bool linear_output = false;
  std::vector<base::test::FeatureRef> disabled_features;
  std::vector<base::test::FeatureRef> enabled_features;

  media::test::DecoderImplementation implementation =
      media::test::DecoderImplementation::kVD;
  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first.find("gtest_") == 0 ||  // Handled by GoogleTest
                                          // Options below are handled by Chrome
        it->first == "ozone-platform" || it->first == "use-gl" ||
        it->first == "v" || it->first == "vmodule" ||
        it->first == "enable-features" || it->first == "disable-features") {
      continue;
    }

    if (it->first == "validator_type") {
      if (it->second == "none") {
        validator_type =
            media::test::VideoPlayerTestEnvironment::ValidatorType::kNone;
      } else if (it->second == "md5") {
        validator_type =
            media::test::VideoPlayerTestEnvironment::ValidatorType::kMD5;
      } else if (it->second == "ssim") {
        validator_type =
            media::test::VideoPlayerTestEnvironment::ValidatorType::kSSIM;
      } else {
        std::cout << "unknown validator type \"" << it->second
                  << "\", possible values are \"none|md5|ssim\"\n";
        return EXIT_FAILURE;
      }
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
    } else if (it->first == "use-legacy") {
      use_legacy = true;
      implementation = media::test::DecoderImplementation::kVDA;
    } else if (it->first == "use_vd_vda") {
      use_vd_vda = true;
      implementation = media::test::DecoderImplementation::kVDVDA;
    } else if (it->first == "linear_output") {
      linear_output = true;
    } else if (it->first == "disable_vaapi_lock") {
      disabled_features.push_back(media::kGlobalVaapiLock);
#if defined(ARCH_CPU_ARM_FAMILY)
    } else if (it->first == "disable-libyuv") {
      enabled_features.clear();
#endif  // defined(ARCH_CPU_ARM_FAMILY)
    } else {
      std::cout << "unknown option: --" << it->first << "\n"
                << media::test::usage_msg;
      return EXIT_FAILURE;
    }
  }

  if (use_legacy && use_vd_vda) {
    std::cout << "--use-legacy and --use_vd_vda cannot be enabled together.\n"
              << media::test::usage_msg;
    return EXIT_FAILURE;
  }
  if (linear_output && !use_vd_vda) {
    std::cout << "--linear_output must be used with the VDVDA (--use_vd_vda)\n"
                 "implementation.\n"
              << media::test::usage_msg;
    return EXIT_FAILURE;
  }

  testing::InitGoogleTest(&argc, argv);

  // Add the command line flag for HEVC testing which will be checked by the
  // video decoder to allow clear HEVC decoding.
  cmd_line->AppendSwitch("enable-clear-hevc-for-testing");

#if defined(ARCH_CPU_ARM_FAMILY)
  // On some platforms bandwidth compression is fully opaque and can not be
  // read by the cpu.  This prevents MD5 computation as that is done by the
  // cpu.
  cmd_line->AppendSwitch("disable-buffer-bw-compression");
#endif

#if BUILDFLAG(USE_V4L2_CODEC)
  std::unique_ptr<base::FeatureList> feature_list =
      std::make_unique<base::FeatureList>();
  feature_list->InitializeFromCommandLine(
      cmd_line->GetSwitchValueASCII(switches::kEnableFeatures),
      cmd_line->GetSwitchValueASCII(switches::kDisableFeatures));
  if (feature_list->IsFeatureOverridden("V4L2FlatStatelessVideoDecoder")) {
    enabled_features.push_back(media::kV4L2FlatStatelessVideoDecoder);
  }
  if (feature_list->IsFeatureOverridden("V4L2FlatStatefulVideoDecoder")) {
    enabled_features.push_back(media::kV4L2FlatStatefulVideoDecoder);
  }
#endif

  // Set up our test environment.
  media::test::VideoPlayerTestEnvironment* test_environment =
      media::test::VideoPlayerTestEnvironment::Create(
          video_path, video_metadata_path, validator_type, implementation,
          linear_output, base::FilePath(output_folder), frame_output_config,
          enabled_features, disabled_features);
  if (!test_environment)
    return EXIT_FAILURE;

  media::test::g_env = static_cast<media::test::VideoPlayerTestEnvironment*>(
      testing::AddGlobalTestEnvironment(test_environment));

  return RUN_ALL_TESTS();
}
