// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/renderer_controller.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/limits.h"
#include "media/base/media_util.h"
#include "media/base/test_helpers.h"
#include "media/base/video_decoder_config.h"
#include "media/remoting/fake_remoter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace remoting {

namespace {

PipelineMetadata DefaultMetadata(VideoCodec codec) {
  PipelineMetadata data;
  data.has_audio = true;
  data.has_video = true;
  data.video_decoder_config = TestVideoConfig::Normal(codec);
  data.audio_decoder_config = TestAudioConfig::Normal();
  data.natural_size = gfx::Size(1920, 1080);
  return data;
}

const char kDefaultReceiver[] = "TestingChromeCast";

mojom::RemotingSinkMetadataPtr GetDefaultSinkMetadata(bool enable) {
  mojom::RemotingSinkMetadataPtr metadata = mojom::RemotingSinkMetadata::New();
  if (enable) {
    metadata->features.push_back(mojom::RemotingSinkFeature::RENDERING);
  } else {
    metadata->features.clear();
  }
  metadata->video_capabilities.push_back(
      mojom::RemotingSinkVideoCapability::CODEC_VP8);
  metadata->audio_capabilities.push_back(
      mojom::RemotingSinkAudioCapability::CODEC_BASELINE_SET);
  metadata->friendly_name = kDefaultReceiver;
  return metadata;
}

constexpr base::TimeDelta kDelayedStartDuration = base::Seconds(5);
constexpr double frame_rate = 30;
constexpr double high_pixel_rate = 3840 * 2160 * 30;
}  // namespace

class RendererControllerTest : public ::testing::Test,
                               public MediaObserverClient {
 public:
  RendererControllerTest()
      : controller_(FakeRemoterFactory::CreateController(false)) {}

  RendererControllerTest(const RendererControllerTest&) = delete;
  RendererControllerTest& operator=(const RendererControllerTest&) = delete;

  ~RendererControllerTest() override = default;

  void TearDown() final { RunUntilIdle(); }

  static void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  // MediaObserverClient implementation.
  void SwitchToRemoteRenderer(
      const std::string& remote_device_friendly_name) override {
    is_rendering_remotely_ = true;
    disable_pipeline_suspend_ = true;
    sink_name_ = remote_device_friendly_name;
  }

  void SwitchToLocalRenderer(ReasonToSwitchToLocal reason) override {
    is_rendering_remotely_ = false;
    disable_pipeline_suspend_ = false;
    sink_name_.clear();
  }

  double Duration() const override { return duration_in_sec_; }

  unsigned DecodedFrameCount() const override { return decoded_frames_; }

  void UpdateRemotePlaybackCompatibility(bool is_compatible) override {
    is_remote_playback_compatible_ = is_compatible;
  }

  void set_pixels_per_second_(double pixels) {
    controller_->pixels_per_second_ = pixels;
  }

  void InitializeControllerWithSink(
      const PipelineMetadata& pipeline_metadata,
      mojom::RemotingSinkMetadataPtr sink_metadata) {
    EXPECT_FALSE(is_rendering_remotely_);
    EXPECT_TRUE(sink_name_.empty());
    controller_->clock_ = &clock_;
    clock_.Advance(base::Seconds(1));
    controller_->SetClient(this);
    controller_->OnSinkAvailable(std::move(sink_metadata));
    controller_->OnRemotePlaybackDisabled(false);
    controller_->OnMetadataChanged(pipeline_metadata);
    controller_->OnPlaying();
    RunUntilIdle();
    PixelRateTimerEnds();
    RunUntilIdle();
    EXPECT_FALSE(is_rendering_remotely_);
    EXPECT_FALSE(disable_pipeline_suspend_);
  }

  void InitializeControllerAndBecomeDominant(
      const PipelineMetadata& pipeline_metadata,
      mojom::RemotingSinkMetadataPtr sink_metadata) {
    InitializeControllerWithSink(pipeline_metadata, std::move(sink_metadata));
    controller_->OnBecameDominantVisibleContent(true);
    RunUntilIdle();
  }

  void PixelRateTimerEnds() {
    EXPECT_TRUE(controller_->pixel_rate_timer_.IsRunning());
    decoded_frames_ = frame_rate * kDelayedStartDuration.InSeconds();
    clock_.Advance(kDelayedStartDuration);
    controller_->pixel_rate_timer_.FireNow();
  }

  void ExpectInRemoting() const {
    EXPECT_TRUE(is_rendering_remotely_);
    EXPECT_TRUE(disable_pipeline_suspend_);
    EXPECT_EQ(kDefaultReceiver, sink_name_);
  }

  void ExpectInLocalRendering() const {
    EXPECT_FALSE(is_rendering_remotely_);
    EXPECT_FALSE(disable_pipeline_suspend_);
    EXPECT_TRUE(sink_name_.empty());
  }

  bool ShouldBeRemoting() const { return controller_->ShouldBeRemoting(); }

  base::test::SingleThreadTaskEnvironment task_environment_;

 protected:
  bool is_rendering_remotely_ = false;
  bool disable_pipeline_suspend_ = false;
  bool is_remote_playback_compatible_ = false;
  size_t decoded_bytes_ = 0;
  unsigned decoded_frames_ = 0;
  base::SimpleTestTickClock clock_;
  std::string sink_name_;
  std::unique_ptr<RendererController> controller_;
  double duration_in_sec_ = 120;  // 2m duration.
};

TEST_F(RendererControllerTest, ShouldBeRemotingForDominantVisibleContent) {
  InitializeControllerAndBecomeDominant(DefaultMetadata(VideoCodec::kVP8),
                                        GetDefaultSinkMetadata(true));
  EXPECT_TRUE(ShouldBeRemoting());

  controller_->OnBecameDominantVisibleContent(false);
  RunUntilIdle();
  EXPECT_FALSE(ShouldBeRemoting());
}

TEST_F(RendererControllerTest, ShouldBeRemotingForRequestFromBrowser) {
  InitializeControllerAndBecomeDominant(DefaultMetadata(VideoCodec::kVP8),
                                        GetDefaultSinkMetadata(true));
  controller_->OnMediaRemotingRequested();
  RunUntilIdle();
  EXPECT_TRUE(ShouldBeRemoting());

  controller_->OnSinkGone();
  RunUntilIdle();
  EXPECT_FALSE(ShouldBeRemoting());
  RunUntilIdle();
}

TEST_F(RendererControllerTest, ToggleRendererOnDominantChange) {
  InitializeControllerAndBecomeDominant(DefaultMetadata(VideoCodec::kVP8),
                                        GetDefaultSinkMetadata(true));
  ExpectInRemoting();  // All requirements now satisfied.

  // Leaving fullscreen should shut down remoting.
  controller_->OnBecameDominantVisibleContent(false);
  RunUntilIdle();
  ExpectInLocalRendering();
}

TEST_F(RendererControllerTest, ToggleRendererOnDisableChange) {
  EXPECT_FALSE(is_rendering_remotely_);
  InitializeControllerAndBecomeDominant(DefaultMetadata(VideoCodec::kVP8),
                                        GetDefaultSinkMetadata(true));
  RunUntilIdle();
  ExpectInRemoting();  // All requirements now satisfied.

  // If the page disables remote playback (e.g., by setting the
  // disableRemotePlayback attribute), this should shut down remoting.
  controller_->OnRemotePlaybackDisabled(true);
  RunUntilIdle();
  ExpectInLocalRendering();
}

TEST_F(RendererControllerTest, NotStartForShortContent) {
  duration_in_sec_ = 20;
  InitializeControllerAndBecomeDominant(DefaultMetadata(VideoCodec::kVP8),
                                        GetDefaultSinkMetadata(true));
  ExpectInLocalRendering();
}

TEST_F(RendererControllerTest, ToggleRendererOnSinkCapabilities) {
  InitializeControllerAndBecomeDominant(DefaultMetadata(VideoCodec::kVP8),
                                        GetDefaultSinkMetadata(false));
  // An available sink that does not support remote rendering should not cause
  // the controller to toggle remote rendering on.
  ExpectInLocalRendering();
  controller_->OnSinkGone();  // Bye-bye useless sink!
  RunUntilIdle();
  ExpectInLocalRendering();

  // A sink that *does* support remote rendering *does* cause the controller to
  // toggle remote rendering on.
  controller_->OnSinkAvailable(GetDefaultSinkMetadata(true));
  RunUntilIdle();
  RunUntilIdle();
  ExpectInRemoting();  // All requirements now satisfied.
}

TEST_F(RendererControllerTest, ToggleRendererOnMediaRemotingRequest) {
  InitializeControllerWithSink(DefaultMetadata(VideoCodec::kVP8), nullptr);
  ExpectInLocalRendering();

  // Should not start media remoting when there is no sink.
  controller_->OnMediaRemotingRequested();
  RunUntilIdle();
  ExpectInLocalRendering();

  // Start media remoting when there are available sinks.
  controller_->OnSinkAvailable(GetDefaultSinkMetadata(false));
  RunUntilIdle();
  RunUntilIdle();
  ExpectInRemoting();
}

TEST_F(RendererControllerTest, WithVP9VideoCodec) {
  InitializeControllerAndBecomeDominant(DefaultMetadata(VideoCodec::kVP9),
                                        GetDefaultSinkMetadata(true));
  // An available sink that does not support VP9 video codec should not cause
  // the controller to toggle remote rendering on.
  ExpectInLocalRendering();

  controller_->OnSinkGone();  // Bye-bye useless sink!
  mojom::RemotingSinkMetadataPtr sink_metadata = GetDefaultSinkMetadata(true);
  sink_metadata->video_capabilities.push_back(
      mojom::RemotingSinkVideoCapability::CODEC_VP9);
  // A sink that *does* support VP9 video codec *does* cause the controller to
  // toggle remote rendering on.
  controller_->OnSinkAvailable(std::move(sink_metadata));
  RunUntilIdle();
  RunUntilIdle();
  ExpectInRemoting();  // All requirements now satisfied.
}

TEST_F(RendererControllerTest, WithHEVCVideoCodec) {
  InitializeControllerAndBecomeDominant(DefaultMetadata(VideoCodec::kHEVC),
                                        GetDefaultSinkMetadata(true));
  // An available sink that does not support HEVC video codec should not cause
  // the controller to toggle remote rendering on.
  ExpectInLocalRendering();

  controller_->OnSinkGone();  // Bye-bye useless sink!
  RunUntilIdle();
  ExpectInLocalRendering();
  mojom::RemotingSinkMetadataPtr sink_metadata = GetDefaultSinkMetadata(true);
  sink_metadata->video_capabilities.push_back(
      mojom::RemotingSinkVideoCapability::CODEC_HEVC);
  // A sink that *does* support HEVC video codec *does* cause the controller to
  // toggle remote rendering on.
  controller_->OnSinkAvailable(std::move(sink_metadata));
  RunUntilIdle();
  RunUntilIdle();
  ExpectInRemoting();  // All requirements now satisfied.
}

TEST_F(RendererControllerTest, WithAACAudioCodec) {
  const AudioDecoderConfig audio_config = AudioDecoderConfig(
      AudioCodec::kAAC, kSampleFormatPlanarF32, CHANNEL_LAYOUT_STEREO, 44100,
      EmptyExtraData(), EncryptionScheme::kUnencrypted);
  PipelineMetadata pipeline_metadata = DefaultMetadata(VideoCodec::kVP8);
  pipeline_metadata.audio_decoder_config = audio_config;
  InitializeControllerAndBecomeDominant(pipeline_metadata,
                                        GetDefaultSinkMetadata(true));
  // An available sink that does not support AAC audio codec should not cause
  // the controller to toggle remote rendering on.
  ExpectInLocalRendering();

  controller_->OnSinkGone();  // Bye-bye useless sink!
  RunUntilIdle();
  ExpectInLocalRendering();
  mojom::RemotingSinkMetadataPtr sink_metadata = GetDefaultSinkMetadata(true);
  sink_metadata->audio_capabilities.push_back(
      mojom::RemotingSinkAudioCapability::CODEC_AAC);
  // A sink that *does* support AAC audio codec *does* cause the controller to
  // toggle remote rendering on.
  controller_->OnSinkAvailable(std::move(sink_metadata));
  RunUntilIdle();
  RunUntilIdle();
  ExpectInRemoting();  // All requirements now satisfied.
}

TEST_F(RendererControllerTest, WithOpusAudioCodec) {
  const AudioDecoderConfig audio_config = AudioDecoderConfig(
      AudioCodec::kOpus, kSampleFormatPlanarF32, CHANNEL_LAYOUT_STEREO, 44100,
      EmptyExtraData(), EncryptionScheme::kUnencrypted);
  PipelineMetadata pipeline_metadata = DefaultMetadata(VideoCodec::kVP8);
  pipeline_metadata.audio_decoder_config = audio_config;
  InitializeControllerAndBecomeDominant(pipeline_metadata,
                                        GetDefaultSinkMetadata(true));
  // An available sink that does not support Opus audio codec should not cause
  // the controller to toggle remote rendering on.
  ExpectInLocalRendering();

  controller_->OnSinkGone();  // Bye-bye useless sink!
  RunUntilIdle();
  mojom::RemotingSinkMetadataPtr sink_metadata = GetDefaultSinkMetadata(true);
  sink_metadata->audio_capabilities.push_back(
      mojom::RemotingSinkAudioCapability::CODEC_OPUS);
  // A sink that *does* support Opus audio codec *does* cause the controller to
  // toggle remote rendering on.
  controller_->OnSinkAvailable(std::move(sink_metadata));
  RunUntilIdle();
  RunUntilIdle();
  ExpectInRemoting();  // All requirements now satisfied.
}

TEST_F(RendererControllerTest, StartFailedWithHighPixelRate) {
  InitializeControllerWithSink(DefaultMetadata(VideoCodec::kVP8),
                               GetDefaultSinkMetadata(true));
  set_pixels_per_second_(high_pixel_rate);

  controller_->OnBecameDominantVisibleContent(true);
  RunUntilIdle();
  ExpectInLocalRendering();
}

TEST_F(RendererControllerTest, StartSuccessWithHighPixelRate) {
  mojom::RemotingSinkMetadataPtr sink_metadata = GetDefaultSinkMetadata(true);
  sink_metadata->video_capabilities.push_back(
      mojom::RemotingSinkVideoCapability::SUPPORT_4K);
  InitializeControllerWithSink(DefaultMetadata(VideoCodec::kVP8),
                               std::move(sink_metadata));
  set_pixels_per_second_(high_pixel_rate);

  controller_->OnBecameDominantVisibleContent(true);
  RunUntilIdle();
  ExpectInRemoting();
}

TEST_F(RendererControllerTest, PacingTooSlowly) {
  InitializeControllerAndBecomeDominant(DefaultMetadata(VideoCodec::kVP8),
                                        GetDefaultSinkMetadata(true));
  RunUntilIdle();
  ExpectInRemoting();  // All requirements now satisfied.
  controller_->OnRendererFatalError(StopTrigger::PACING_TOO_SLOWLY);
  RunUntilIdle();
  ExpectInLocalRendering();
  controller_->OnSinkAvailable(GetDefaultSinkMetadata(true));
  RunUntilIdle();
  controller_->OnBecameDominantVisibleContent(false);
  RunUntilIdle();
  ExpectInLocalRendering();
  controller_->OnBecameDominantVisibleContent(true);
  RunUntilIdle();
  ExpectInRemoting();  // All requirements now satisfied.
}

TEST_F(RendererControllerTest, StartFailed) {
  controller_ = FakeRemoterFactory::CreateController(true);
  InitializeControllerAndBecomeDominant(DefaultMetadata(VideoCodec::kVP8),
                                        GetDefaultSinkMetadata(true));
  RunUntilIdle();
  ExpectInLocalRendering();
}

TEST_F(RendererControllerTest, SetClientNullptr) {
  controller_ = FakeRemoterFactory::CreateController(true);
  InitializeControllerAndBecomeDominant(DefaultMetadata(VideoCodec::kVP8),
                                        GetDefaultSinkMetadata(true));
  controller_->SetClient(nullptr);
  RunUntilIdle();
  ExpectInLocalRendering();
}

TEST_F(RendererControllerTest, OnFrozen) {
  InitializeControllerAndBecomeDominant(DefaultMetadata(VideoCodec::kVP8),
                                        GetDefaultSinkMetadata(true));

  RunUntilIdle();
  ExpectInRemoting();

  // Pausing needs to occur before freezing can be enabled.
  controller_->OnPaused();
  ExpectInRemoting();

  // Freezing should kick rendering back to local.
  controller_->OnFrozen();
  RunUntilIdle();
  ExpectInLocalRendering();
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(RendererControllerTest, RemotePlaybackHlsCompatibility) {
  controller_ = FakeRemoterFactory::CreateController(true);
  controller_->SetClient(this);

  controller_->OnDataSourceInitialized(GURL("http://example.com/foo.m3u8"));

  PipelineMetadata incompatible_metadata;
  incompatible_metadata.has_video = false;
  incompatible_metadata.has_audio = false;
  controller_->OnMetadataChanged(incompatible_metadata);
  EXPECT_FALSE(is_remote_playback_compatible_);

  // HLS is compatible with RemotePlayback regardless of the metadata we have.
  controller_->OnHlsManifestDetected();
  EXPECT_TRUE(is_remote_playback_compatible_);
}
#endif

}  // namespace remoting
}  // namespace media
