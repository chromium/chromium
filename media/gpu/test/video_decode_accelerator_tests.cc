// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <memory>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/cpu.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
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
#include "media/gpu/test/video_decode_accelerator_test_suite.h"
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

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#include "media/gpu/chromeos/platform_video_frame_pool.h"
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_utils.h"
#endif  // BUILDFLAG(USE_V4L2_CODEC)

namespace media {
namespace test {

namespace {

media::test::VideoDecodeAcceleratorTestSuite* g_env;

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

// Set the frame rate for the decoder. This is required for the
// VideoDecoderPipeline to work.
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
    base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
    command_line.AppendSwitchASCII(
        switches::kHardwareVideoDecodeFrameRate,
        base::NumberToString(g_env->Video()->FrameRate()));
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

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

    // Increase the time out if
    // (1) video frames are output, or
    // (2) on Intel GLK, where mapping is very slow, or
    // (3) with V4L2 VISL driver where execution is very slow on ARM64 VM.
    if (g_env->GetFrameOutputMode() != FrameOutputMode::kNone ||
        IsSlowMappingDevice() || g_env->IsV4L2VirtualDriver()) {
      video_player->SetEventWaitTimeout(
          std::max(kDefaultEventWaitTimeout, g_env->Video()->Duration() * 10));
    }

    return video_player;
  }

  bool InitializeDecoderWithConfig(VideoDecoderConfig& decoder_config) {
    // TODO(https://crbugs.com/350994517): Enable this test for Windows once
    // PlatformVideoFramePool is implemented for that.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
    auto frame_pool = std::make_unique<PlatformVideoFramePool>();
    std::unique_ptr<VideoDecoder> decoder = VideoDecoderPipeline::Create(
        gpu::GpuDriverBugWorkarounds(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        std::move(frame_pool),
        /*frame_converter=*/nullptr,
        VideoDecoderPipeline::DefaultPreferredRenderableFourccs(),
        std::make_unique<NullMediaLog>(),
        /*oop_video_decoder=*/{},
        /*in_video_decoder_process=*/true);

    bool init_result = false;
    VideoDecoder::InitCB init_cb = base::BindLambdaForTesting(
        [&init_result](DecoderStatus result) { init_result = result.is_ok(); });
    decoder->Initialize(decoder_config, /*low_delay=*/false,
                        /*cdm_context=*/nullptr, std::move(init_cb),
                        base::BindRepeating(&VideoDecoderTest::AddModelFrame,
                                            base::Unretained(this)),
                        /*waiting_cb=*/base::NullCallback());
    return init_result;
#else
    return false;
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  }

 private:
  bool IsSlowMappingDevice() const {
    static const bool is_slow_mapping_device = []() {
      const base::CPU& cpuid = base::CPU::GetInstanceNoAllocation();
      constexpr int kPentiumAndLaterFamily = 0x06;
      constexpr int kGeminiLakeModelId = 0x7A;
      constexpr int kApolloLakeModelId = 0x5c;
      const bool is_glk_device = cpuid.family() == kPentiumAndLaterFamily &&
                                 cpuid.model() == kGeminiLakeModelId;
      const bool is_apl_device = cpuid.family() == kPentiumAndLaterFamily &&
                                 cpuid.model() == kApolloLakeModelId;
      return is_glk_device || is_apl_device;
    }();

    return is_slow_mapping_device;
  }

  // TODO(hiroh): Move this to Video class or video_frame_helpers.h.
  // TODO(hiroh): Create model frames once during the test.
  bool CreateModelFrames(const VideoBitstream* video) {
    if (video->Codec() != VideoCodec::kAV1) {
      LOG(ERROR) << "Frame validation by SSIM is allowed for AV1 streams only";
      return false;
    }

    Dav1dVideoDecoder decoder(
        std::make_unique<NullMediaLog>(),
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
        EncodedDataHelper::Create(video->Data(), video->Codec());
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

#if BUILDFLAG(USE_V4L2_CODEC)
// This test case calls Decode() a number of times and expect OK DecodeCBs. This
// test only makes sense for V4L2 (VA-API doesn't have an input queue).
TEST_F(VideoDecoderTest, Decode) {
  auto tvp = CreateDecoderListener(g_env->Video());

  tvp->Play();
  // We usually allocate at least 8 buffers for input queues.
  const size_t kNumDecodeBuffers = 8;
  EXPECT_TRUE(tvp->WaitForEvent(DecoderListener::Event::kDecoderBufferAccepted,
                                /*times=*/kNumDecodeBuffers));
}

// This test case sends all the frames and expects them to be accepted for
// decoding (as in, VideoDecoder::OutputCB should be called). Most of them
// should be decoded as well, but since this test doesn't exercise an
// End-of-Stream (a.k.a. "a flush"), some will likely be held onto by the
// VideoDecoder/driver as part of its decoding pipeline. We don't know how
// many (it depends also on the ImageProcessor, if any), so it's not a good
// idea to set expectations on the number of kFrameDecoded events.
TEST_F(VideoDecoderTest, AllDecoderBuffersAcceptedForDecoding) {
  auto tvp = CreateDecoderListener(g_env->Video());

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForEvent(DecoderListener::Event::kDecoderBufferAccepted,
                                /*times=*/g_env->Video()->NumFrames()));

  // This is a hack to allow Qualcomm devices (e.g. trogdor) to flush the pipes
  // after the last resolution change event that comes out when running the
  // resolution_change_500frames.vp9.ivf sequence. It should be fixed but since
  // a new V4L2StatefulVideoDecoder backend is in the making, let's just leave
  // the hack. See b/294611425.
  base::PlatformThread::Sleep(base::Milliseconds(100));
}
#endif

// Play video from start to end. Wait for the kFlushDone event at the end of the
// stream, that notifies us all frames have been decoded.
TEST_F(VideoDecoderTest, FlushAtEndOfStream) {
  auto tvp = CreateDecoderListener(g_env->Video());

  // This test case is used for video.ChromeStackDecoderVerification.
  // Mapping is very slow on some intel devices and hit the default timeout
  // in long 4k video verification. Increase the timeout more than 1080p video
  // to mitigate the issue.
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

#if BUILDFLAG(USE_V4L2_CODEC)
// Flush the decoder somewhere mid-stream, then continue as normal. This is a
// contrived use case to exercise important V4L2 stateful areas.
TEST_F(VideoDecoderTest, DISABLED_FlushMidStream) {
  if (!base::FeatureList::IsEnabled(kV4L2FlatStatefulVideoDecoder)) {
    GTEST_SKIP();
  }

  auto tvp = CreateDecoderListener(g_env->Video());

  tvp->Play();
  const size_t flush_location_in_frames =
      std::min(static_cast<size_t>(10), g_env->Video()->NumFrames() / 2);
  EXPECT_TRUE(tvp->WaitForFrameDecoded(flush_location_in_frames));
  tvp->Flush();
  EXPECT_TRUE(tvp->WaitForFlushDone());
  // GetFrameDecodedCount() is likely larger than |flush_location_in_frames|
  // because there are likely submitted encoded chunks ready to be decoded at
  // the time of Flush().
  EXPECT_GE(tvp->GetFrameDecodedCount(), flush_location_in_frames);
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  // Total flush count must be two: once mid-stream and once at the end.
  EXPECT_EQ(tvp->GetFlushDoneCount(), 2u);

  // The H264 bitstreams in our test set have B-frames; by Flush()ing carelessly
  // like we do here in this test, we're likely to lose needed references that
  // later B-frames will need. Those B-frames will be discarded.
  // TODO(mcasas): Flush at an IDR frame.
  const bool has_b_frames = g_env->Video()->Codec() == VideoCodec::kH264;
  if (!has_b_frames)
    EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  else
    EXPECT_LE(tvp->GetFrameDecodedCount(), g_env->Video()->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}
#endif

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

#if BUILDFLAG(USE_V4L2_CODEC)
  if (base::FeatureList::IsEnabled(kV4L2FlatStatefulVideoDecoder)) {
    GTEST_SKIP() << "Temporarily disabled due to b/298073737";
  }
#endif  // BUILDFLAG(USE_V4L2_CODEC)

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

  // kNewBuffersRequested is a specific kVDVDA event.
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
    EXPECT_EQ(tvps[i]->GetFlushDoneCount(), 1u) << "Decoder #" << i;
    EXPECT_EQ(tvps[i]->GetFrameDecodedCount(), g_env->Video()->NumFrames())
        << "Decoder #" << i;
    EXPECT_TRUE(tvps[i]->WaitForFrameProcessors()) << "Decoder #" << i;
  }
}

TEST_F(VideoDecoderTest, InitializeWithNonSupportedConfig) {
  const auto* video = g_env->Video();
  constexpr VideoCodecProfile kProfileNotSupportedByChromeOS =
      THEORAPROFILE_ANY;
  VideoDecoderConfig non_supported_decoder_config(
      video->Codec(), kProfileNotSupportedByChromeOS,
      VideoDecoderConfig::AlphaMode::kIsOpaque, VideoColorSpace(),
      kNoTransformation, video->Resolution(), gfx::Rect(video->Resolution()),
      video->Resolution(), EmptyExtraData(), EncryptionScheme::kUnencrypted);
  EXPECT_FALSE(InitializeDecoderWithConfig(non_supported_decoder_config));
}

}  // namespace test
}  // namespace media

int main(int argc, char** argv) {
  media::test::g_env =
      media::test::VideoDecodeAcceleratorTestSuite::Create(argc, argv);
  if (!media::test::g_env || !media::test::g_env->ValidVideoTestEnv()) {
    LOG(ERROR) << "Invalid video test environment";
    return EXIT_FAILURE;
  }

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&media::test::VideoDecodeAcceleratorTestSuite::Run,
                     base::Unretained(media::test::g_env)));
}
